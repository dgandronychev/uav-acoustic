#pragma once
#include <vector>
#include <complex>
#include <cstdint>

#include "core/dsp/mel_filterbank.h"

namespace core::dsp {

struct PcenConfig {
  int sample_rate = 16000;
  int n_fft = 512;      // power of two
  int win_length = 400; // 25ms @16k
  int hop_length = 160; // 10ms @16k
  int n_mels = 64;

  // PCEN params (reasonable defaults)
  float eps = 1e-6f;
  float alpha = 0.98f;
  float delta = 2.0f;
  float r = 0.5f;
  float s = 0.025f;   // smoothing coefficient (0..1), ~tau
  float floor = 1e-12f;

  float f_min = 50.0f;
  float f_max = 7600.0f;
};

// Streaming: feed PCM float mono, get PCEN mel frames at hop rate.
class PcenExtractor {
 public:
  explicit PcenExtractor(const PcenConfig& cfg);

  int n_mels() const { return cfg_.n_mels; }

  // Push mono samples. Returns number of produced frames.
  // Frames are appended to `out_frames` as row-major: [frame0(mels), frame1(mels), ...]
  int Process(const float* mono, int n, std::vector<float>* out_frames);

 private:
  PcenConfig cfg_;
  MelFilterbank mel_;

  std::vector<float> in_fifo_;
  std::vector<float> window_;
  std::vector<std::complex<float>> fft_buf_;
  std::vector<float> power_;
  std::vector<float> mel_energy_;
  std::vector<float> pcen_m_;
  std::vector<float> pcen_frame_;

  void ComputeHann();
  void OneFrame(const float* frame_win, float* out_pcen);
};

}  // namespace core::dsp
