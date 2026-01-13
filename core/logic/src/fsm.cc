#include "core/logic/fsm.h"

#include <algorithm>

namespace core::logic {

    DetectorFsm::DetectorFsm(FsmParams params) : params_(params) {
        Reset();
    }

    void DetectorFsm::Reset() {
        state_ = core::telemetry::FsmState::IDLE;
        above_on_since_ns_ = 0;
        below_off_since_ns_ = 0;
        cooldown_until_ns_ = 0;
        last_start_ns_ = 0;
        last_end_ns_ = 0;
    }

    FsmOutput DetectorFsm::Update(std::int64_t now_ns, float p_detect) {
        FsmOutput out;
        out.state = state_;
        out.last_start_ns = last_start_ns_;
        out.last_end_ns = last_end_ns_;

        // clamp
        p_detect = std::clamp(p_detect, 0.0f, 1.0f);

        const auto confirm_ns = MsToNs(params_.t_confirm_ms);
        const auto release_ns = MsToNs(params_.t_release_ms);
        const auto cooldown_ns = MsToNs(params_.cooldown_ms);

        switch (state_) {
        case core::telemetry::FsmState::IDLE: {
            if (now_ns < cooldown_until_ns_) {
                // still cooling down
                out.state = state_;
                return out;
            }

            if (p_detect >= params_.p_on) {
                if (above_on_since_ns_ == 0) above_on_since_ns_ = now_ns;
                if ((now_ns - above_on_since_ns_) >= confirm_ns) {
                    // START
                    state_ = core::telemetry::FsmState::ACTIVE;
                    last_start_ns_ = now_ns;
                    out.detect_start = true;

                    // reset counters
                    above_on_since_ns_ = 0;
                    below_off_since_ns_ = 0;
                }
            }
            else {
                above_on_since_ns_ = 0;
            }
            break;
        }

        case core::telemetry::FsmState::ACTIVE: {
            if (p_detect <= params_.p_off) {
                if (below_off_since_ns_ == 0) below_off_since_ns_ = now_ns;
                if ((now_ns - below_off_since_ns_) >= release_ns) {
                    // END
                    state_ = core::telemetry::FsmState::COOLDOWN;
                    last_end_ns_ = now_ns;
                    out.detect_end = true;

                    cooldown_until_ns_ = now_ns + cooldown_ns;

                    // reset counters
                    below_off_since_ns_ = 0;
                    above_on_since_ns_ = 0;
                }
            }
            else {
                below_off_since_ns_ = 0;
            }
            break;
        }

        case core::telemetry::FsmState::COOLDOWN: {
            if (now_ns >= cooldown_until_ns_) {
                state_ = core::telemetry::FsmState::IDLE;
            }
            break;
        }

                                                // Если у вас в enum есть CANDIDATE — можно не использовать в этой версии,
                                                // или добавить отдельное состояние. Сейчас делаем максимально стабильный MVP.
        case core::telemetry::FsmState::CANDIDATE: {
            // не используем, но на всякий случай
            state_ = core::telemetry::FsmState::IDLE;
            break;
        }
        }

        out.state = state_;
        out.last_start_ns = last_start_ns_;
        out.last_end_ns = last_end_ns_;
        return out;
    }

}  // namespace core::logic
