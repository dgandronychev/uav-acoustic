#include "core/fsm/event_fsm.h"
#include <algorithm>

namespace core::fsm {

    EventFsm::EventFsm(const EventFsmConfig& cfg) : cfg_(cfg) {}

    void EventFsm::Reset() {
        state_ = core::telemetry::FsmState::IDLE;
        confirm_ms_ = 0;
        release_ms_ = 0;
        cooldown_left_ms_ = 0;
    }

    EventFsmUpdate EventFsm::Update(float p, int dt_ms) {
        EventFsmUpdate out;
        out.state = state_;

        const float p_on = cfg_.p_on;
        const float p_off = cfg_.p_off;

        dt_ms = std::max(0, dt_ms);

        switch (state_) {
        case core::telemetry::FsmState::IDLE: {
            confirm_ms_ = 0;
            release_ms_ = 0;

            if (p >= p_on) {
                state_ = core::telemetry::FsmState::CANDIDATE;
                confirm_ms_ = dt_ms;
            }
            break;
        }

        case core::telemetry::FsmState::CANDIDATE: {
            release_ms_ = 0;

            if (p >= p_on) {
                confirm_ms_ += dt_ms;
                if (confirm_ms_ >= cfg_.t_confirm_ms) {
                    state_ = core::telemetry::FsmState::ACTIVE;
                    out.started = true;
                    confirm_ms_ = 0;
                }
            }
            else {
                // fell below p_on before confirm -> back to IDLE
                state_ = core::telemetry::FsmState::IDLE;
                confirm_ms_ = 0;
            }
            break;
        }

        case core::telemetry::FsmState::ACTIVE: {
            confirm_ms_ = 0;

            if (p <= p_off) {
                release_ms_ += dt_ms;
                if (release_ms_ >= cfg_.t_release_ms) {
                    state_ = core::telemetry::FsmState::COOLDOWN;
                    cooldown_left_ms_ = cfg_.cooldown_ms;
                    out.ended = true;
                    release_ms_ = 0;
                }
            }
            else {
                // still active
                release_ms_ = 0;
            }
            break;
        }

        case core::telemetry::FsmState::COOLDOWN: {
            confirm_ms_ = 0;
            release_ms_ = 0;

            cooldown_left_ms_ -= dt_ms;
            if (cooldown_left_ms_ <= 0) {
                cooldown_left_ms_ = 0;
                state_ = core::telemetry::FsmState::IDLE;
            }
            break;
        }
        }

        out.state = state_;
        return out;
    }

}  // namespace core::fsm
