#pragma once

namespace core::detect {

	// Минимальный интерфейс детектора.
	// На MVP достаточно "скормить" 1 PCEN-фрейм (n_mels) и получить p in [0..1].
	class IDetector {
	public:
		virtual ~IDetector() = default;

		// pcen_frame: массив длиной n_mels
		// return: вероятность p_detect [0..1]
		virtual float Update(const float* pcen_frame, int n_mels) = 0;

		virtual void Reset() = 0;
	};

}  // namespace core::detect
