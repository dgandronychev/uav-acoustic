#pragma once

#include <algorithm>
#include <cmath>

namespace core::detect {

    // Детектор по RMS энергии в dBFS + сглаживание → вероятность 0..1.
    struct EnergyDetectorConfig {
        // dBFS пороги (подстройка под твой микрофон/записи)
        float db_min = -70.0f;  // ниже этого считаем "тишина"
        float db_max = -25.0f;  // выше этого считаем "точно сигнал"

        // EMA сглаживание вероятности
        float ema = 0.08f;      // 0..1 (больше = быстрее)

        // защитный пол для RMS
        float rms_floor = 1e-8f;
    };

    class EnergyDetector {
    public:
        explicit EnergyDetector(const EnergyDetectorConfig& cfg) : cfg_(cfg) {}

        // mono: float PCM [-1..1], n = samples
        float Update(const float* mono, int n) {
            if (!mono || n <= 0) return p_;

            // RMS
            double acc = 0.0;
            for (int i = 0; i < n; ++i) {
                const double s = mono[i];
                acc += s * s;
            }
            const double mean = acc / static_cast<double>(n);
            const float rms = static_cast<float>(std::sqrt(mean));
            const float rms_c = std::max(cfg_.rms_floor, rms);

            // dBFS
            const float db = 20.0f * std::log10(rms_c);

            // map db -> [0..1]
            float x = 0.0f;
            if (db <= cfg_.db_min) x = 0.0f;
            else if (db >= cfg_.db_max) x = 1.0f;
            else x = (db - cfg_.db_min) / (cfg_.db_max - cfg_.db_min);

            // EMA smoothing
            p_ = (1.0f - cfg_.ema) * p_ + cfg_.ema * x;
            last_db_ = db;
            return p_;
        }

        float last_db() const { return last_db_; }

    private:
        EnergyDetectorConfig cfg_;
        float p_ = 0.0f;
        float last_db_ = -120.0f;
    };

}  // namespace core::detect
