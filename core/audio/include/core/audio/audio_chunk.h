#pragma once
#include <cstdint>
#include <span>

namespace core::audio {

	struct AudioChunk {
		std::int64_t t0_ns = 0;
		int sample_rate = 0;
		int channels = 1;
		int frames = 0;
		std::span<const float> interleaved; // frames*channels
	};

}  // namespace core::audio
