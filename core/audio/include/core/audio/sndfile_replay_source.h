#pragma once

#include "core/audio/i_audio_source.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace core::audio {

	class SndfileReplaySource : public IAudioSource {
	public:
		explicit SndfileReplaySource(std::string path);
		~SndfileReplaySource() override;

		bool Open(const AudioSourceConfig& cfg) override;
		void Close() override;
		std::optional<AudioChunk> Read() override;

		int file_sample_rate() const { return file_sr_; }
		int file_channels() const { return file_ch_; }

	private:
		std::int64_t now_ns() const;
		void resetToLoopStart();

		std::string path_;
		AudioSourceConfig cfg_{};

		void* snd_{ nullptr };
		int file_sr_{ 0 };
		int file_ch_{ 0 };

		std::vector<float> buf_;
		std::int64_t t0_ns_{ 0 };
	};

}  // namespace core::audio
