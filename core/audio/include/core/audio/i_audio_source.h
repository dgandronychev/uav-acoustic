#pragma once
#include <cstdint>
#include <optional>

#include "core/audio/audio_chunk.h"

namespace core::audio {

struct AudioSourceConfig {
  int sample_rate = 16000;
  int channels = 1;
  int chunk_ms = 20;      // 20ms typical
  bool realtime = true;   // sleep to emulate real-time
  bool loop = true;       // loop file
};

class IAudioSource {
 public:
  virtual ~IAudioSource() = default;

  virtual bool Open(const AudioSourceConfig& cfg) = 0;
  virtual void Close() = 0;

  // Возвращает следующий чанк, либо nullopt если конец и loop=false.
  virtual std::optional<AudioChunk> Read() = 0;
};

}  // namespace core::audio
