#include "manager.hh"
#include "dispatcher.hh"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <iostream>
#include <queue>
#include <optional>

using namespace std;

static const unsigned CYCLE_TIME_MS = 25;

namespace json = boost::property_tree;

static const string GET_LISTS = "get-lists";
static const string GET_LEVELS = "get-levels";
static const string SET_LEVELS = "set-levels";
static const string RESET_CHANNEL = "reset-channel";
static const string TRACK_CHANNEL = "track-channel";
static const string BLOCK_CHANNEL = "block-channel";
static const string SAVE_CUE = "save-cue";
static const string RESTORE_CUE = "restore-cue";
static const string GO_CUE = "go-cue";
static const string BACK_CUE = "back-cue";
static const string DELETE_CUE = "delete-cue";
static const string LIST_CUES = "list-cues";

Manager::Manager(shared_ptr<Dispatcher> dispatcher, net::io_context &ioc)
    : dispatcher_(dispatcher), ioc_(ioc),
      timer_(ioc_, net::chrono::milliseconds(CYCLE_TIME_MS)),
      transmitter_(ioc, net::ip::make_address("127.0.0.1"), 0) {
  tick_time_ = chrono::duration_cast<chrono::milliseconds>(
      chrono::system_clock::now().time_since_epoch());
  timer_.async_wait(boost::bind(&Manager::tick, this));
}

void Manager::begin() { dispatcher_->subscribe(weak_from_this()); }

string status_message(CueList::cue_status_t status) {
  switch (status) {
  case (CueList::MANUAL):
    return "manual";
  case (CueList::LOWERED):
    return "lowered";
  case (CueList::RAISED):
    return "raised";
  case (CueList::TRACKED):
    return "tracked";
  case (CueList::BLOCKED):
    return "blocked";
  }
  cerr << "Unknown status: " << status << endl;
  return "unknown";
}

void Manager::get_levels(CueList& list_) {
  json::ptree root;
  json::ptree values;
  json::ptree cue_info;

  Transmitter::universe_t universe = { 0 };

  for (auto info : list_.current_levels()) {
    json::ptree current;
    current.put("channel", info.channel);
    current.put("value", info.level);
    current.put("status", status_message(info.status));
    if (info.channel < 512) {
      universe[info.channel] = clamp(info.level, 0, 255);
    }
    values.push_back(make_pair("", current));
  }

  {
    cue_info.put("current", list_.cue());
    cue_info.put("fade_time", list_.fade_time());
    cue_info.put("fade_progress", list_.fade_progress().value_or(1));
    cue_info.put("fading", list_.fade_progress().has_value());
    cue_info.put("last", list_.last_cue());
    cue_info.put("next", list_.next_cue());
    cue_info.put("previous", list_.previous_cue());
  }

  root.put("list", list_.number());
  root.put("type", GET_LEVELS);
  root.put_child("cue", cue_info);
  root.put_child("values", values);

  stringstream ss;
  json::write_json(ss, root);
  transmitter_.update(universe);
  dispatcher_->do_update(string_view(ss.str()));
}

void Manager::set_levels(CueList& list_, boost::property_tree::ptree values) {
  for (auto &x : values) {
    json::ptree node = x.second;
    int channel = node.get<int>("channel");
    int value = node.get<int>("value");
    list_.set_level(channel, value);
  }
  get_levels(list_);
}

void Manager::save_cue(CueList& list_, unsigned q, float time) {
  list_.record_cue(q, time);
  list_cues(list_);
  get_levels(list_);
}

void Manager::tick() {
  chrono::milliseconds now =
      chrono::duration_cast<chrono::milliseconds>(
          chrono::system_clock::now().time_since_epoch());
  for (auto& x : lists_) {
    CueList& list_ = x.second;
    if (list_.fade_progress().has_value()) {
      list_.tick((now - tick_time_).count());
      get_levels(list_);
    }
  }
  transmitter_.tick((now - tick_time_).count());
  tick_time_ = now;
  timer_.expires_at(timer_.expires_at() +
                    net::chrono::milliseconds(CYCLE_TIME_MS));
  timer_.async_wait(boost::bind(&Manager::tick, this));
}

void Manager::restore_cue(CueList& list_, unsigned q) {
  list_.go_to_cue(q);
  get_levels(list_);
}

void Manager::go_cue(CueList& list_) {
  list_.go();
  get_levels(list_);
}

void Manager::back_cue(CueList& list_) {
  list_.back();
  get_levels(list_);
}

void Manager::reset_channel(CueList& list_, unsigned channel) {
  list_.set_level(channel, nullopt);
  get_levels(list_);
}

void Manager::track_channel(CueList& list_, unsigned channel) {
  list_.track(channel);
  get_levels(list_);
}

void Manager::block_channel(CueList& list_, unsigned channel) {
  list_.block(channel);
  get_levels(list_);
}

void Manager::delete_cue(CueList& list_, unsigned q) {
  list_.delete_cue(q);
  get_levels(list_);
  list_cues(list_);
}

void Manager::list_cues(CueList& list_) {
  json::ptree root;
  json::ptree cues;
  for (auto &x : list_.cue_info()) {
    json::ptree current;
    current.put("number", x.number);
    current.put("time", x.fade_time);
    cues.push_back(make_pair("", current));
  }
  root.put("type", LIST_CUES);
  root.put("cue", list_.cue());
  root.put_child("cues", cues);

  stringstream ss;
  json::write_json(ss, root);
  dispatcher_->do_update(string_view(ss.str()));
}

void Manager::get_lists() {
  json::ptree root;
  json::ptree lists;
  for (auto& x : lists_) {
    CueList& l = x.second;
    json::ptree current;
    current.put("number", l.number());
    current.put("name", l.name());
    lists.push_back(make_pair("", current));
  }
  root.put("type", GET_LISTS);
  root.put_child("lists", lists);

  stringstream ss;
  json::write_json(ss, root);
  dispatcher_->do_update(string_view(ss.str()));
}

void Manager::on_update(string_view update) {
  stringstream ss;
  ss << update;
  json::ptree pt;
  try {
    json::read_json(ss, pt);
    string type = pt.get<string>("type");
    cout << "Got update of type: " << type << endl;

    if (type == GET_LISTS) {
      get_lists();
      return;
    }

    unsigned list_id = pt.get<unsigned>("list_id");

    if (lists_.count(list_id) != 1) {
      CueList l(list_id, "untitled");
      lists_.insert({list_id, l});
    }
    auto found = lists_.find(list_id);
    CueList& list = (*found).second;

    if (type == GET_LEVELS) {
      get_levels(list);
    } else if (type == SET_LEVELS) {
      set_levels(list, pt.get_child("values"));
    } else if (type == RESET_CHANNEL) {
      reset_channel(list, pt.get<unsigned>("channel"));
    } else if (type == TRACK_CHANNEL) {
      track_channel(list, pt.get<unsigned>("channel"));
    } else if (type == BLOCK_CHANNEL) {
      block_channel(list, pt.get<unsigned>("channel"));
    } else if (type == SAVE_CUE) {
      save_cue(list, pt.get<int>("cue"), pt.get<float>("time"));
    } else if (type == RESTORE_CUE) {
      restore_cue(list, pt.get<int>("cue"));
    } else if (type == LIST_CUES) {
      list_cues(list);
    } else if (type == GO_CUE) {
      go_cue(list);
    } else if (type == BACK_CUE) {
      back_cue(list);
    } else if (type == DELETE_CUE) {
      delete_cue(list, pt.get<unsigned>("cue"));
    } else {
      cerr << "Invalid type: " << type << endl;
    }
  } catch (json::json_parser::json_parser_error &e) {
    cerr << "Ignoring invalid JSON. [" << e.what() << "]" << endl;
    return;
  } catch (json::ptree_bad_path &e) {
    cerr << "Ignoring incomplete JSON. [" << e.what() << "]" << endl;
    return;
  } catch (json::ptree_bad_data &e) {
    cerr << "Ignoring unparsable JSON. [" << e.what() << "]" << endl;
    return;
  }
}
