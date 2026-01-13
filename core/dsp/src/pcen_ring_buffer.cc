#include "core/dsp/pcen_ring_buffer.h"

#include <algorithm>
#include <cstring>

namespace core::dsp {

    PcenRingBuffer::PcenRingBuffer(int n_mels, int capacity_frames)
        : n_mels_(n_mels),
        capacity_frames_(capacity_frames),
        data_(static_cast<std::size_t>(n_mels)* static_cast<std::size_t>(capacity_frames), 0.0f) {
    }

    void PcenRingBuffer::PushFrame(const float* frame) {
        if (!frame || n_mels_ <= 0 || capacity_frames_ <= 0) return;

        std::lock_guard<std::mutex> lk(mu_);
        const std::size_t off = static_cast<std::size_t>(write_idx_) * static_cast<std::size_t>(n_mels_);
        std::memcpy(data_.data() + off, frame, static_cast<std::size_t>(n_mels_) * sizeof(float));

        write_idx_ = (write_idx_ + 1) % capacity_frames_;
        size_frames_ = std::min(size_frames_ + 1, capacity_frames_);
    }

    std::vector<float> PcenRingBuffer::SnapshotLast(int last_frames, int* out_frames) const {
        if (out_frames) *out_frames = 0;
        if (n_mels_ <= 0 || capacity_frames_ <= 0) return {};

        std::lock_guard<std::mutex> lk(mu_);
        const int avail = size_frames_;
        const int want = std::max(0, std::min(last_frames, avail));
        if (out_frames) *out_frames = want;
        if (want == 0) return {};

        std::vector<float> out(static_cast<std::size_t>(want) * static_cast<std::size_t>(n_mels_), 0.0f);

        // Oldest among the last "want" frames:
        // write_idx_ points to next write position => newest is write_idx_-1.
        const int start = (write_idx_ - want + capacity_frames_) % capacity_frames_;

        for (int i = 0; i < want; ++i) {
            const int src_frame = (start + i) % capacity_frames_;
            const std::size_t src_off = static_cast<std::size_t>(src_frame) * static_cast<std::size_t>(n_mels_);
            const std::size_t dst_off = static_cast<std::size_t>(i) * static_cast<std::size_t>(n_mels_);
            std::memcpy(out.data() + dst_off, data_.data() + src_off, static_cast<std::size_t>(n_mels_) * sizeof(float));
        }
        return out;
    }

}  // namespace core::dsp
