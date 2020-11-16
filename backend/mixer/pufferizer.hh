#ifndef PUFFERIZER_HH
#define PUFFERIZER_HH

#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "wav_header.hh"

class Pufferizer {
  private:
	// The prefix of the files for it to write
	std::string prefix_;

	// The path to the files to write
	std::string dest_;

	// The location of the left channel
	std::ifstream l_in_;

	// The location of the right channel
	std::ifstream r_in_;

	// The number of the next file to write
	size_t next_file_{0};

	// This is the default header that gets written to every output file
	WavHeader header_{

	};

	// This method converts floating point samples to 16 bit PCM samples
	// It's entirely based on code from the following link
	// https://github.com/stanford-stagecast/audio/blob/main/src/audio/alsa_devices.cc#L372-L376
	inline int16_t float_to_sample( const float sample_f );

  public:
	// The constructor takes in strings specifiying where to read data from
	// and write data to and populates the members accordingly
	Pufferizer(const std::string& prefix,
		const std::string& destination,
		const std::string& left_input,
		const std::string& right_input);

	// reads in enough data to write one 5 second wav file
	void pufferize_once();

	bool eof() const;
};


#endif
