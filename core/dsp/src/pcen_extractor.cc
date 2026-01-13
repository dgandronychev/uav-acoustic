#include "core/dsp/pcen_extractor.h"
#include "core/dsp/fft_radix2.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace core::dsp {

    PcenExtractor::PcenExtractor(const PcenConfig& cfg)
        : cfg_(cfg),
        mel_(MelFilterbankConfig{
            .sample_rate = cfg.sample_rate,
            .n_fft = cfg.n_fft,
            .n_mels = cfg.n_mels,
            .f_min = cfg.f_min,
            .f_max = cfg.f_max,
            }) {
        in_fifo_.clear();
        window_.assign(static_cast<std::size_t>(cfg_.win_length), 0.0f);
        fft_buf_.assign(static_cast<std::size_t>(cfg_.n_fft), { 0.0f, 0.0f });
        power_.assign(static_cast<std::size_t>(cfg_.n_fft / 2 + 1), 0.0f);
        mel_energy_.assign(static_cast<std::size_t>(cfg_.n_mels), 0.0f);
        pcen_m_.assign(static_cast<std::size_t>(cfg_.n_mels), 0.0f);
        pcen_frame_.assign(static_cast<std::size_t>(cfg_.n_mels), 0.0f);

        ComputeHann();
    }

    void PcenExtractor::ComputeHann() {
        // Hann on win_length, later placed in n_fft with zero-padding.
        for (int i = 0; i < cfg_.win_length; ++i) {
            const float x = static_cast<float>(i) / static_cast<float>(cfg_.win_length - 1);
            window_[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(2.0f * kPi * x);
        }
    }

    void PcenExtractor::OneFrame(const float* frame_win, float* out_pcen) {
        // Build FFT input
        std::fill(fft_buf_.begin(), fft_buf_.end(), std::complex<float>(0.0f, 0.0f));
        for (int i = 0; i < cfg_.win_length; ++i) {
            const float v = frame_win[i] * window_[static_cast<std::size_t>(i)];
            fft_buf_[static_cast<std::size_t>(i)] = std::complex<float>(v, 0.0f);
        }

        // FFT in-place
        FftRadix2(fft_buf_);

        // Power spectrum (one-sided)
        const int n_freqs = cfg_.n_fft / 2 + 1;
        const float inv_n = 1.0f / static_cast<float>(cfg_.n_fft); // mild scaling
        for (int k = 0; k < n_freqs; ++k) {
            const auto c = fft_buf_[static_cast<std::size_t>(k)] * inv_n;
            const float p = (c.real() * c.real() + c.imag() * c.imag());
            power_[static_cast<std::size_t>(k)] = std::max(cfg_.floor, p);
        }

        // Mel energies
        mel_.Apply(power_.data(), mel_energy_.data());

        // PCEN smoothing + compression
        const float delta_r = std::pow(cfg_.delta, cfg_.r);
        for (int m = 0; m < cfg_.n_mels; ++m) {
            const float E = std::max(cfg_.floor, mel_energy_[static_cast<std::size_t>(m)]);
            float& M = pcen_m_[static_cast<std::size_t>(m)];
            M = (1.0f - cfg_.s) * M + cfg_.s * E;

            const float denom = std::pow(cfg_.eps + M, cfg_.alpha);
            const float x = (E / denom) + cfg_.delta;
            out_pcen[m] = std::pow(x, cfg_.r) - delta_r;
        }
    }

    int PcenExtractor::Process(const float* mono, int n, std::vector<float>* out_frames) {
        if (!out_frames) return 0;

        // Append to fifo
        const std::size_t old = in_fifo_.size();
        in_fifo_.resize(old + static_cast<std::size_t>(n));
        std::memcpy(in_fifo_.data() + old, mono, static_cast<std::size_t>(n) * sizeof(float));

        int produced = 0;

        // While we can take a window
        while (static_cast<int>(in_fifo_.size()) >= cfg_.win_length) {
            OneFrame(in_fifo_.data(), pcen_frame_.data());
            out_frames->insert(out_frames->end(), pcen_frame_.begin(), pcen_frame_.end());
            produced++;

            // Pop hop_length
            if (cfg_.hop_length <= 0) break;
            if (static_cast<int>(in_fifo_.size()) <= cfg_.hop_length) {
                in_fifo_.clear();
                break;
            }
            in_fifo_.erase(in_fifo_.begin(), in_fifo_.begin() + cfg_.hop_length);
        }

        return produced;
    }

}  // namespace core::dsp
