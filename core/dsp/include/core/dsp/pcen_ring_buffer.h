#pragma once

#include <vector>
#include <mutex>

namespace core::dsp {

	// Ring buffer of PCEN frames (each frame has n_mels floats)
	class PcenRingBuffer {
	public:
		PcenRingBuffer(int n_mels, int capacity_frames);

		void PushFrame(const float* frame); // size n_mels
		std::vector<float> SnapshotLast(int last_frames, int* out_frames) const;

		int n_mels() const { return n_mels_; }
		int capacity_frames() const { return capacity_frames_; }

	private:
		int n_mels_{ 0 };
		int capacity_frames_{ 0 };

		mutable std::mutex mu_;
		std::vector<float> data_; // capacity_frames * n_mels
		int write_idx_{ 0 };        // frame index (0..capacity_frames-1)
		int size_frames_{ 0 };      // filled frames
	};

}  // namespace core::dsp
