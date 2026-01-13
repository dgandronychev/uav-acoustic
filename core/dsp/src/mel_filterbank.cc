// core/dsp/src/mel_filterbank.cc
#include "core/dsp/mel_filterbank.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace core::dsp {

    static inline int ClampInt(int v, int lo, int hi) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    float MelFilterbank::HzToMel(float hz) {
        return 2595.0f * std::log10(1.0f + hz / 700.0f);
    }

    float MelFilterbank::MelToHz(float mel) {
        return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
    }

    MelFilterbank::MelFilterbank(const MelFilterbankConfig& cfg) : cfg_(cfg) {
        n_freqs_ = cfg_.n_fft / 2 + 1;
        w_.assign(static_cast<std::size_t>(cfg_.n_mels * n_freqs_), 0.0f);
        Build();
    }

    void MelFilterbank::Build() {
        const float f_min = std::max(0.0f, cfg_.f_min);
        const float f_max = std::min(cfg_.f_max, 0.5f * static_cast<float>(cfg_.sample_rate));

        const float mel_min = HzToMel(f_min);
        const float mel_max = HzToMel(f_max);

        // mel points
        std::vector<float> mel_pts(static_cast<std::size_t>(cfg_.n_mels + 2));
        for (int i = 0; i < cfg_.n_mels + 2; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(cfg_.n_mels + 1);
            mel_pts[static_cast<std::size_t>(i)] = mel_min + t * (mel_max - mel_min);
        }

        // hz points
        std::vector<float> hz_pts(static_cast<std::size_t>(cfg_.n_mels + 2));
        for (int i = 0; i < cfg_.n_mels + 2; ++i) {
            hz_pts[static_cast<std::size_t>(i)] = MelToHz(mel_pts[static_cast<std::size_t>(i)]);
        }

        // fft bin points
        std::vector<int> bin(static_cast<std::size_t>(cfg_.n_mels + 2));
        for (int i = 0; i < cfg_.n_mels + 2; ++i) {
            const float hz = hz_pts[static_cast<std::size_t>(i)];
            const int b = static_cast<int>(std::floor((cfg_.n_fft + 1) * hz / cfg_.sample_rate));
            bin[static_cast<std::size_t>(i)] = ClampInt(b, 0, n_freqs_ - 1);
        }

        // build triangles
        for (int m = 0; m < cfg_.n_mels; ++m) {
            const int left = bin[static_cast<std::size_t>(m)];
            const int center = bin[static_cast<std::size_t>(m + 1)];
            const int right = bin[static_cast<std::size_t>(m + 2)];

            if (right <= left) continue;

            // rising
            for (int k = left; k < center; ++k) {
                const float denom = static_cast<float>(center - left);
                const float val = (denom > 0.0f) ? (static_cast<float>(k - left) / denom) : 0.0f;
                w_[static_cast<std::size_t>(m * n_freqs_ + k)] = val;
            }
            // falling
            for (int k = center; k <= right; ++k) {
                const float denom = static_cast<float>(right - center);
                const float val = (denom > 0.0f) ? (static_cast<float>(right - k) / denom) : 0.0f;
                w_[static_cast<std::size_t>(m * n_freqs_ + k)] =
                    std::max(w_[static_cast<std::size_t>(m * n_freqs_ + k)], val);
            }
        }
    }

    void MelFilterbank::Apply(const float* power, float* out_mel) const {
        std::memset(out_mel, 0, static_cast<std::size_t>(cfg_.n_mels) * sizeof(float));
        for (int m = 0; m < cfg_.n_mels; ++m) {
            float acc = 0.0f;
            const float* row = &w_[static_cast<std::size_t>(m * n_freqs_)];
            for (int k = 0; k < n_freqs_; ++k) {
                acc += row[k] * power[k];
            }
            out_mel[m] = acc;
        }
    }

}  // namespace core::dsp
