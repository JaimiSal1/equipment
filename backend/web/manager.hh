#ifndef MANAGER_HH
#define MANAGER_HH

#include <memory>
#include <map>
#include <string_view>
#include <unordered_map>

#include <boost/property_tree/ptree_fwd.hpp>
#include "net.hh"

#include "cuelist.hh"
#include "transmitter.hh"

class Dispatcher;

class Manager : public std::enable_shared_from_this<Manager> {
private:
  std::shared_ptr<Dispatcher> dispatcher_;
  net::io_context& ioc_;
  std::unordered_map<unsigned, CueList> lists_{};
  boost::asio::steady_timer timer_;
  std::chrono::milliseconds tick_time_{};
  Transmitter transmitter_;

  void set_levels(CueList& list, boost::property_tree::ptree values);
  void save_cue(CueList& list, unsigned q, float time);
  void restore_cue(CueList& list, unsigned q);
  void delete_cue(CueList& list, unsigned q);
  void list_cues(CueList& list);
  void get_lists();
  void go_cue(CueList& list);
  void back_cue(CueList& list);

  void reset_channel(CueList& list, unsigned channel);
  void track_channel(CueList& list, unsigned channel);
  void block_channel(CueList& list, unsigned channel);

public:
  Manager(std::shared_ptr<Dispatcher> dispatcher, net::io_context& ioc);

  void begin();
  void on_update(std::string_view update);
  void get_levels(CueList &list);
  void tick();
};

#endif
