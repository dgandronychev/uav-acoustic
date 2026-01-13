#pragma once

#include <cstdint>

namespace core::detect {

    /**
     * @brief Energy VAD-like mock detector.
     *
     * Pipeline:
     *  1) chunk RMS energy -> logE
     *  2) running normalization (EWMA mean/var on logE)
     *  3) z-score -> sigmoid => p_raw
     *  4) EMA smoothing => p_smooth
     *  5) hysteresis + confirm/release timers => ACTIVE/IDLE
     */
    class MockDetector {
    public:
        struct Config {
            int sample_rate = 16000;
            int frame_ms = 20;

            // Normalization (EWMA over log-energy).
            // Larger => adapts faster to new noise floor.
            float norm_alpha = 0.01f;

            // Smoothing of probability.
            float p_ema_alpha = 0.20f;

            // Sigmoid mapping from z-score to probability.
            float sigmoid_k = 1.2f;      // slope
            float sigmoid_bias = 0.0f;   // shift

            // Hysteresis thresholds (applied on smoothed probability).
            float p_on = 0.65f;
            float p_off = 0.45f;

            // Debounce in milliseconds.
            int t_confirm_ms = 200;  // need p>=p_on for this duration => ACTIVE
            int t_release_ms = 300;  // need p<=p_off for this duration => IDLE

            // Safety floor for variance.
            float var_floor = 1e-4f;

            // Small epsilon.
            float eps = 1e-12f;
        };

        enum class State : std::uint8_t { IDLE = 0, ACTIVE = 1 };

        explicit MockDetector(const Config& cfg);

        /**
         * @brief Process mono float chunk (frames).
         * @return current smoothed probability.
         */
        float Process(const float* mono, int frames);

        State state() const { return state_; }
        float p_smooth() const { return p_smooth_; }

    private:
        float ComputeLogEnergy_(const float* mono, int frames) const;
        float Sigmoid_(float x) const;

        Config cfg_;

        // Running stats for log-energy normalization.
        bool stats_init_ = false;
        float mean_ = 0.0f;
        float var_ = 1.0f;

        // Smoothed probability.
        float p_smooth_ = 0.0f;

        // Hysteresis state machine.
        State state_ = State::IDLE;
        int above_cnt_frames_ = 0;
        int below_cnt_frames_ = 0;

        int frames_per_chunk_ = 0;
        int confirm_frames_ = 0;
        int release_frames_ = 0;
    };

}  // namespace core::detect
