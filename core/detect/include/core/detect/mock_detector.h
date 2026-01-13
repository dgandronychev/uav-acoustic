#pragma once
#include <cstdint>

namespace core::detect {

	struct MockDetectorConfig {
		float ema_fast = 0.20f;     // быстрая EMA по энергии
		float ema_slow = 0.01f;     // медленная EMA (фон)
		float k = 6.0f;             // крутизна sigmoid
		float bias = 1.0f;          // смещение порога (выше -> меньше срабатываний)
		float min_energy = 1e-12f;  // защита от нулей
	};

	class MockDetector {
	public:
		explicit MockDetector(const MockDetectorConfig& cfg);

		// energy: любая положительная метрика энергии (например mean(PCEN) или mean(mel_energy))
		float Update(float energy);

	private:
		MockDetectorConfig cfg_;
		float fast_ = 0.0f;
		float slow_ = 0.0f;
		bool inited_ = false;
	};

}  // namespace core::detect
