#pragma once
#include <cstdint>
#include "core/telemetry/telemetry_snapshot.h"

namespace core::fsm {

	struct EventFsmConfig {
		float p_on = 0.65f;
		float p_off = 0.45f;

		int t_confirm_ms = 200;   // IDLE->CANDIDATE->ACTIVE confirm time above p_on
		int t_release_ms = 300;   // ACTIVE release time below p_off
		int cooldown_ms = 800;    // COOLDOWN duration after END
	};

	struct EventFsmUpdate {
		core::telemetry::FsmState state = core::telemetry::FsmState::IDLE;
		bool started = false;
		bool ended = false;
	};

	class EventFsm {
	public:
		explicit EventFsm(const EventFsmConfig& cfg);

		void Reset();
		EventFsmUpdate Update(float p, int dt_ms);

		core::telemetry::FsmState state() const { return state_; }

	private:
		EventFsmConfig cfg_;
		core::telemetry::FsmState state_ = core::telemetry::FsmState::IDLE;

		int confirm_ms_ = 0;
		int release_ms_ = 0;
		int cooldown_left_ms_ = 0;
	};

}  // namespace core::fsm
