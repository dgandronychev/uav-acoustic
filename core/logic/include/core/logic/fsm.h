#pragma once

#include <cstdint>

#include "core/telemetry/telemetry_snapshot.h"  // FsmState

namespace core::logic {

	struct FsmParams {
		// thresholds
		float p_on = 0.65f;  // start when p_detect >= p_on for t_confirm
		float p_off = 0.45f;  // end when p_detect <= p_off for t_release (hysteresis)

		// timings (ms)
		int t_confirm_ms = 500;   // how long above p_on to confirm start
		int t_release_ms = 800;   // how long below p_off to confirm end
		int cooldown_ms = 1200;  // ignore re-trigger just after end
	};

	struct FsmOutput {
		core::telemetry::FsmState state = core::telemetry::FsmState::IDLE;
		bool detect_start = false;
		bool detect_end = false;
		std::int64_t last_start_ns = 0;
		std::int64_t last_end_ns = 0;
	};

	class DetectorFsm {
	public:
		explicit DetectorFsm(FsmParams params = {});
		void Reset();

		// Call every tick with monotonic time in ns and latest probability in [0..1]
		FsmOutput Update(std::int64_t now_ns, float p_detect);

		core::telemetry::FsmState state() const { return state_; }
		std::int64_t last_start_ns() const { return last_start_ns_; }
		std::int64_t last_end_ns() const { return last_end_ns_; }

	private:
		static std::int64_t MsToNs(int ms) { return static_cast<std::int64_t>(ms) * 1'000'000LL; }

		FsmParams params_;

		core::telemetry::FsmState state_ = core::telemetry::FsmState::IDLE;

		// timers
		std::int64_t above_on_since_ns_ = 0;
		std::int64_t below_off_since_ns_ = 0;
		std::int64_t cooldown_until_ns_ = 0;

		// last events
		std::int64_t last_start_ns_ = 0;
		std::int64_t last_end_ns_ = 0;
	};

}  // namespace core::logic
