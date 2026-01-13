#pragma once

#include <cstdint>
#include <cstddef>

namespace core::telemetry {

	enum class FsmState : int {
		IDLE = 0,
		CANDIDATE = 1,
		ACTIVE = 2,
		COOLDOWN = 3,
	};

	struct TimelinePoint {
		std::int64_t t_ns = 0;
		float p_detect = 0.0f;
	};

	struct TelemetrySnapshot {
		std::int64_t t_ns = 0;

		float p_detect_latest = 0.0f;
		FsmState fsm_state = FsmState::IDLE;

		// New: event flags (edge events)
		bool event_started = false;
		bool event_ended = false;

		// Timeline (fixed-size buffer like before)
		static constexpr std::size_t kMaxTimeline = 128;
		TimelinePoint timeline[kMaxTimeline]{};
		int timeline_n = 0;
	};

}  // namespace core::telemetry
