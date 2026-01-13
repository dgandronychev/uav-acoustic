// core/dsp/include/core/dsp/mel_filterbank.h
#pragma once

#include <vector>

namespace core::dsp {

	struct MelFilterbankConfig {
		int sample_rate = 16000;
		int n_fft = 512;
		int n_mels = 64;
		float f_min = 50.0f;
		float f_max = 8000.0f;
	};

	class MelFilterbank {
	public:
		explicit MelFilterbank(const MelFilterbankConfig& cfg);

		int n_mels() const { return cfg_.n_mels; }
		int n_freqs() const { return n_freqs_; }

		// power: size n_freqs = n_fft/2+1
		// out_mel: size n_mels
		void Apply(const float* power, float* out_mel) const;

	private:
		MelFilterbankConfig cfg_{};
		int n_freqs_ = 0;

		// Dense weights [n_mels][n_freqs]
		std::vector<float> w_;  // row-major: w_[m*n_freqs + k]

		static float HzToMel(float hz);
		static float MelToHz(float mel);

		void Build();
	};

}  // namespace core::dsp
