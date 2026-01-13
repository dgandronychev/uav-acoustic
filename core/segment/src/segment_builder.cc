#include "core/segment/segment_builder.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "core/dsp/pcen_ring_buffer.h"

namespace fs = std::filesystem;

namespace core::segment {

    SegmentBuilder::SegmentBuilder(std::shared_ptr<const core::dsp::PcenRingBuffer> rb, Config cfg)
        : rb_(std::move(rb)), cfg_(std::move(cfg)) {
    }

    int SegmentBuilder::pre_frames() const {
        return std::max(0, cfg_.pre_roll_ms / std::max(1, cfg_.hop_ms));
    }
    int SegmentBuilder::post_frames() const {
        return std::max(0, cfg_.post_roll_ms / std::max(1, cfg_.hop_ms));
    }
    int SegmentBuilder::max_event_frames() const {
        return std::max(1, cfg_.max_event_ms / std::max(1, cfg_.hop_ms));
    }

    void SegmentBuilder::OnFramePushed(std::int64_t t_ns) {
        last_frame_t_ns_ = t_ns;
        frame_index_++;

        // если событие слишком длинное — принудительно завершаем как END (без внешней FSM)
        if (in_event_) {
            const std::int64_t dur_frames = frame_index_ - event_start_frame_;
            if (dur_frames > max_event_frames()) {
                OnEventEnd(t_ns);
            }
        }

        TryFinalizeIfReady();
    }

    void SegmentBuilder::OnEventStart(std::int64_t t_ns) {
        if (in_event_ || pending_finalize_) return;

        in_event_ = true;
        event_start_t_ns_ = t_ns;
        event_start_frame_ = frame_index_;
    }

    void SegmentBuilder::OnEventEnd(std::int64_t t_ns) {
        if (!in_event_ || pending_finalize_) return;

        in_event_ = false;
        event_end_t_ns_ = t_ns;
        event_end_frame_ = frame_index_;

        pending_finalize_ = true;
        finalize_at_frame_ = event_end_frame_ + post_frames();
    }

    bool SegmentBuilder::HasReadySegment() const {
        return has_ready_;
    }

    SegmentBuilder::SegmentInfo SegmentBuilder::PopReadySegment() {
        has_ready_ = false;
        return ready_;
    }

    void SegmentBuilder::TryFinalizeIfReady() {
        if (!pending_finalize_) return;
        if (frame_index_ < finalize_at_frame_) return;

        const int n_mels = cfg_.n_mels;
        const int pre = pre_frames();
        const int post = post_frames();

        // сегмент начинается: start_frame - pre
        const std::int64_t seg_begin_frame = std::max<std::int64_t>(0, event_start_frame_ - pre);
        // сегмент заканчивается: end_frame + post  (finalize_at_frame_ соответствует этому)
        const std::int64_t seg_end_frame = finalize_at_frame_;

        const std::int64_t seg_frames_ll = std::max<std::int64_t>(1, seg_end_frame - seg_begin_frame);
        const int seg_frames = static_cast<int>(std::min<std::int64_t>(seg_frames_ll, 5000)); // safety

        // Берём последние seg_frames из ring buffer на момент finalize.
        // Если ring buffer меньше — SnapshotLast вернёт меньше.
        int available = 0;
        std::vector<float> data = rb_->SnapshotLast(seg_frames, &available);
        const int got_frames = std::max(0, available);

        // Сохраняем
        EnsureOutDir();
        const std::string path = MakeFileName(event_start_t_ns_, event_end_t_ns_);

        SegmentInfo info;
        info.path = path;
        info.t_start_ns = event_start_t_ns_;
        info.t_end_ns = event_end_t_ns_;
        info.frames = got_frames;
        info.n_mels = n_mels;

        if (got_frames > 0 && static_cast<int>(data.size()) >= got_frames * n_mels) {
            (void)SaveCsv(path, data, got_frames, n_mels);
        }

        ready_ = info;
        has_ready_ = true;

        // reset
        pending_finalize_ = false;
        event_start_frame_ = -1;
        event_end_frame_ = -1;
        finalize_at_frame_ = -1;
        event_start_t_ns_ = 0;
        event_end_t_ns_ = 0;
    }

    std::string SegmentBuilder::EnsureOutDir() {
        std::error_code ec;
        fs::create_directories(cfg_.out_dir, ec);
        return cfg_.out_dir;
    }

    std::string SegmentBuilder::MakeFileName(std::int64_t t_start_ns, std::int64_t t_end_ns) const {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s/seg_%lld_%lld.csv",
            cfg_.out_dir.c_str(),
            static_cast<long long>(t_start_ns),
            static_cast<long long>(t_end_ns));
        return std::string(buf);
    }

    bool SegmentBuilder::SaveCsv(const std::string& path,
        const std::vector<float>& data,
        int frames,
        int n_mels) const {
        std::ofstream f(path, std::ios::out);
        if (!f) return false;

        // формат: одна строка = один фрейм; колонки = mels
        for (int i = 0; i < frames; ++i) {
            const float* row = &data[static_cast<std::size_t>(i * n_mels)];
            for (int m = 0; m < n_mels; ++m) {
                f << row[m];
                if (m + 1 < n_mels) f << ",";
            }
            f << "\n";
        }
        return true;
    }

}  // namespace core::segment
