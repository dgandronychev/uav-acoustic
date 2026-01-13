#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core::dsp {
    class PcenRingBuffer;
}

namespace core::segment {

    class SegmentBuilder {
    public:
        struct Config {
            int n_mels = 64;

            // hop_ms должен совпадать с PCEN hop (например 10ms при hop_length=160 @16kHz)
            int hop_ms = 10;

            // Сколько держим "до" и "после" события
            int pre_roll_ms = 2000;   // 2s до START
            int post_roll_ms = 2000;  // 2s после END

            // Защита от слишком длинных событий
            int max_event_ms = 12000; // максимум 12s активного
            std::string out_dir = "segments";
        };

        struct SegmentInfo {
            std::string path;
            std::int64_t t_start_ns = 0;
            std::int64_t t_end_ns = 0;
            int frames = 0;
            int n_mels = 0;
        };

        SegmentBuilder(std::shared_ptr<const core::dsp::PcenRingBuffer> rb, Config cfg);

        // Вызывайте при каждом PushFrame (после того как фрейм оказался в ring buffer)
        void OnFramePushed(std::int64_t t_ns);

        // FSM события
        void OnEventStart(std::int64_t t_ns);
        void OnEventEnd(std::int64_t t_ns);

        // Готов ли новый сегмент (после post-roll)
        bool HasReadySegment() const;
        SegmentInfo PopReadySegment();

    private:
        int pre_frames() const;
        int post_frames() const;
        int max_event_frames() const;

        void TryFinalizeIfReady();

        std::string EnsureOutDir();
        std::string MakeFileName(std::int64_t t_start_ns, std::int64_t t_end_ns) const;
        bool SaveCsv(const std::string& path, const std::vector<float>& data, int frames, int n_mels) const;

    private:
        std::shared_ptr<const core::dsp::PcenRingBuffer> rb_;
        Config cfg_;

        // Счетчик PCEN фреймов (монотонный)
        std::int64_t frame_index_ = 0;
        std::int64_t last_frame_t_ns_ = 0;

        // Состояние события
        bool in_event_ = false;
        bool pending_finalize_ = false;

        std::int64_t event_start_frame_ = -1;
        std::int64_t event_end_frame_ = -1;
        std::int64_t finalize_at_frame_ = -1;

        std::int64_t event_start_t_ns_ = 0;
        std::int64_t event_end_t_ns_ = 0;

        // Очередь готовых сегментов (обычно 0/1)
        bool has_ready_ = false;
        SegmentInfo ready_;
    };

}  // namespace core::segment
