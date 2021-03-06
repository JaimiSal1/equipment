#include "pufferizer.hh"

using namespace std;

Pufferizer::Pufferizer(const string& prefix,
					   const string& destination,
					   const string& left_input,
					   const string& right_input)
	: prefix_{prefix}
	, dest_{destination}
	, l_in_{left_input}
	, r_in_{right_input} {
		fill(overlap_, overlap_ + OVERLAP_SIZE_ * 2, 0.0f);
	}

bool Pufferizer::eof() const {
	 return l_in_.eof() && r_in_.eof();
}

inline int16_t Pufferizer::float_to_sample( const float sample_f ) {
	constexpr float maxval = uint64_t( 1 ) << 15;
	return lrint( clamp( sample_f, -1.0f, 1.0f ) * maxval );
}

void Pufferizer::pufferize_once() {
	// open the correct file
	ofstream os{dest_ + "/" + prefix_ + to_string(next_file_ * 432000) + ".wav"};

	// write the wav header
	os << header_;

	// write the overlapping data
	for (size_t i = 0; i < 2 * OVERLAP_SIZE_; i++) {
		os.write(reinterpret_cast<char*>(&overlap_[i]), sizeof(int16_t));
	}

	// read from the input and write the data
	char tmp[sizeof(float)];
	int16_t sample;
	for (size_t i = 0; i < TOTAL_SIZE_ - 2 * OVERLAP_SIZE_; i++) {
		l_in_.read(tmp, sizeof(tmp));
		sample = float_to_sample(*reinterpret_cast<float*>(tmp));
		os.write(reinterpret_cast<char*>(&sample), sizeof(int16_t));
		r_in_.read(tmp, sizeof(tmp));
		sample = float_to_sample(*reinterpret_cast<float*>(tmp));
		os.write(reinterpret_cast<char*>(&sample), sizeof(int16_t));
	}

	// read from the input and write the overlapping data to the output and to overlap_
	for (size_t i = 0; i < 2 * OVERLAP_SIZE_; i += 2) {
		l_in_.read(tmp, sizeof(tmp));
		sample = float_to_sample(*reinterpret_cast<float*>(tmp));
		os.write(reinterpret_cast<char*>(&sample), sizeof(int16_t));
		overlap_[i] = sample;

		r_in_.read(tmp, sizeof(tmp));
		sample = float_to_sample(*reinterpret_cast<float*>(tmp));
		os.write(reinterpret_cast<char*>(&sample), sizeof(int16_t));
		overlap_[i+1] = sample;
	}

	next_file_++;
}
