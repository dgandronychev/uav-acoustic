#include "core/audio/sndfile_replay_source.h"

#include <chrono>
#include <thread>

#include <sndfile.h>

namespace core::audio {

    SndfileReplaySource::SndfileReplaySource(std::string path)
        : path_(std::move(path)) {
    }

    SndfileReplaySource::~SndfileReplaySource() { Close(); }

    std::int64_t SndfileReplaySource::now_ns() const {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }

    bool SndfileReplaySource::Open(const AudioSourceConfig& cfg) {
        cfg_ = cfg;
        Close();

        SF_INFO sfinfo{};
        SNDFILE* f = sf_open(path_.c_str(), SFM_READ, &sfinfo);
        if (!f) return false;

        snd_ = f;
        file_sr_ = sfinfo.samplerate;
        file_ch_ = sfinfo.channels;

        // Принимаем фактические параметры файла.
        // Downmix/resample делаем вне источника (в main), чтобы быстрее получить MVP.
        cfg_.sample_rate = file_sr_;
        cfg_.channels = file_ch_;

        const int frames_per_chunk = (cfg_.sample_rate * cfg_.chunk_ms) / 1000;
        const int samples_per_chunk = frames_per_chunk * cfg_.channels;
        buf_.assign(static_cast<std::size_t>(samples_per_chunk), 0.0f);

        t0_ns_ = now_ns();
        return true;
    }

    void SndfileReplaySource::Close() {
        if (snd_) {
            sf_close(reinterpret_cast<SNDFILE*>(snd_));
            snd_ = nullptr;
        }
        buf_.clear();
    }

    void SndfileReplaySource::resetToLoopStart() {
        if (!snd_) return;
        sf_seek(reinterpret_cast<SNDFILE*>(snd_), 0, SEEK_SET);
        t0_ns_ = now_ns();
    }

    std::optional<AudioChunk> SndfileReplaySource::Read() {
        if (!snd_) return std::nullopt;

        const int frames_per_chunk = (cfg_.sample_rate * cfg_.chunk_ms) / 1000;

        const sf_count_t got_frames =
            sf_readf_float(reinterpret_cast<SNDFILE*>(snd_), buf_.data(), frames_per_chunk);

        if (got_frames <= 0) {
            if (cfg_.loop) {
                resetToLoopStart();
                return Read();
            }
            return std::nullopt;
        }

        if (cfg_.realtime) {
            const std::int64_t expected_ns = t0_ns_ + static_cast<std::int64_t>(cfg_.chunk_ms) * 1'000'000LL;
            const std::int64_t now = now_ns();
            if (expected_ns > now) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(expected_ns - now));
            }
            t0_ns_ = expected_ns;
        }

        AudioChunk chunk;
        chunk.t0_ns = now_ns();
        chunk.sample_rate = cfg_.sample_rate;
        chunk.channels = cfg_.channels;
        chunk.frames = static_cast<int>(got_frames);
        chunk.interleaved = std::span<const float>(buf_.data(),
            static_cast<std::size_t>(static_cast<int>(got_frames) * cfg_.channels));
        return chunk;
    }

}  // namespace core::audio
