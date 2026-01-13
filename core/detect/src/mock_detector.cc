#include "core/detect/mock_detector.h"
#include <algorithm>
#include <cmath>

namespace core::detect {

    static float Sigmoid(float x) {
        // защита от переполнения exp
        x = std::clamp(x, -20.0f, 20.0f);
        return 1.0f / (1.0f + std::exp(-x));
    }

    MockDetector::MockDetector(const MockDetectorConfig& cfg) : cfg_(cfg) {}

    float MockDetector::Update(float energy) {
        energy = std::max(cfg_.min_energy, energy);

        if (!inited_) {
            fast_ = energy;
            slow_ = energy;
            inited_ = true;
        }
        else {
            fast_ = (1.0f - cfg_.ema_fast) * fast_ + cfg_.ema_fast * energy;
            slow_ = (1.0f - cfg_.ema_slow) * slow_ + cfg_.ema_slow * energy;
        }

        // отношение "сейчас/фон"
        const float ratio = fast_ / std::max(cfg_.min_energy, slow_);

        // переводим в вероятность: ratio ~ 1 => около 0, ratio > bias => растёт
        const float x = cfg_.k * (std::log(ratio) - std::log(cfg_.bias));
        return Sigmoid(x);
    }

}  // namespace core::detect
