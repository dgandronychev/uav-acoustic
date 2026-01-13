#include "core/detect/mock_detector.h"

#include <algorithm>
#include <cmath>

namespace core::detect {

    MockDetector::MockDetector(const Config& cfg) : cfg_(cfg) {
        frames_per_chunk_ = std::max(1, (cfg_.sample_rate * cfg_.frame_ms) / 1000);

        confirm_frames_ = std::max(1, cfg_.t_confirm_ms / std::max(1, cfg_.frame_ms));
        release_frames_ = std::max(1, cfg_.t_release_ms / std::max(1, cfg_.frame_ms));

        // reasonable init
        mean_ = 0.0f;
        var_ = 1.0f;
        p_smooth_ = 0.0f;
        state_ = State::IDLE;
        above_cnt_frames_ = 0;
        below_cnt_frames_ = 0;
    }

    float MockDetector::ComputeLogEnergy_(const float* mono, int frames) const {
        if (!mono || frames <= 0) return -30.0f;

        // RMS energy
        double sum2 = 0.0;
        for (int i = 0; i < frames; ++i) {
            const double v = static_cast<double>(mono[i]);
            sum2 += v * v;
        }
        const double mean2 = sum2 / static_cast<double>(frames);
        const double rms = std::sqrt(mean2 + static_cast<double>(cfg_.eps));

        // log-energy in natural log scale (stable for normalization)
        const double loge = std::log(rms + static_cast<double>(cfg_.eps));
        return static_cast<float>(loge);
    }

    float MockDetector::Sigmoid_(float x) const {
        // standard logistic
        const float y = 1.0f / (1.0f + std::exp(-x));
        return y;
    }

    float MockDetector::Process(const float* mono, int frames) {
        // 1) log-energy
        const float loge = ComputeLogEnergy_(mono, frames);

        // 2) update running mean/var (EWMA)
        if (!stats_init_) {
            mean_ = loge;
            var_ = 1.0f;
            stats_init_ = true;
        }
        else {
            const float a = std::clamp(cfg_.norm_alpha, 0.0001f, 0.5f);
            const float diff = loge - mean_;
            mean_ = (1.0f - a) * mean_ + a * loge;

            // EWMA variance of residual
            const float new_var = (1.0f - a) * var_ + a * (diff * diff);
            var_ = std::max(cfg_.var_floor, new_var);
        }

        // 3) z-score -> sigmoid => p_raw
        const float z = (loge - mean_) / std::sqrt(var_ + cfg_.eps);
        const float x = cfg_.sigmoid_k * (z - cfg_.sigmoid_bias);
        const float p_raw = Sigmoid_(x);

        // 4) EMA smoothing
        const float pa = std::clamp(cfg_.p_ema_alpha, 0.01f, 0.99f);
        p_smooth_ = (1.0f - pa) * p_smooth_ + pa * p_raw;

        // 5) hysteresis + confirm/release
        if (state_ == State::IDLE) {
            if (p_smooth_ >= cfg_.p_on) {
                above_cnt_frames_++;
                if (above_cnt_frames_ >= confirm_frames_) {
                    state_ = State::ACTIVE;
                    above_cnt_frames_ = 0;
                    below_cnt_frames_ = 0;
                }
            }
            else {
                above_cnt_frames_ = 0;
            }
        }
        else {  // ACTIVE
            if (p_smooth_ <= cfg_.p_off) {
                below_cnt_frames_++;
                if (below_cnt_frames_ >= release_frames_) {
                    state_ = State::IDLE;
                    below_cnt_frames_ = 0;
                    above_cnt_frames_ = 0;
                }
            }
            else {
                below_cnt_frames_ = 0;
            }
        }

        return p_smooth_;
    }

}  // namespace core::detect
