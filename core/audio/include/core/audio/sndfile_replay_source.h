#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <sndfile.h>
#include "core/audio/i_audio_source.h"

struct SF_INFO_tag;

namespace core::audio {

// Replay source based on libsndfile:
// supports FLAC/WAV/OGG/Vorbis/... depending on build.
// Reads float samples (normalized), outputs interleaved float chunks.
class SndfileReplaySource final : public IAudioSource {
 public:
  explicit SndfileReplaySource(std::string path);
  ~SndfileReplaySource() override;

  bool Open(const AudioSourceConfig& cfg) override;
  void Close() override;
  std::optional<AudioChunk> Read() override;

  [[nodiscard]] int FileSampleRate() const noexcept { return file_sr_; }
  [[nodiscard]] int FileChannels() const noexcept { return file_ch_; }

 private:
  void resetToLoopStart();
  std::int64_t now_ns() const;

  std::string path_;
  AudioSourceConfig cfg_{};

  SNDFILE* snd_{ nullptr };
  SF_INFO_tag* info_ = nullptr;

  int file_sr_ = 0;
  int file_ch_ = 0;

  std::int64_t t0_ns_ = 0;

  std::vector<float> buf_;
};

}  // namespace core::audio
