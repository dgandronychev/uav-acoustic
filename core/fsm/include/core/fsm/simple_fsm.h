#pragma once

#include <cstdint>

#include "core/telemetry/telemetry_snapshot.h"

namespace core::fsm {

struct FsmConfig {
  float p_on  = 0.65f;   // включение
  float p_off = 0.45f;   // выключение (гистерезис)

  int t_confirm_ms = 250;   // сколько держать выше p_on чтобы перейти в ACTIVE
  int t_release_ms = 500;   // сколько держать ниже p_off чтобы выйти из ACTIVE
  int cooldown_ms  = 800;   // пауза после события
};

class SimpleFsm {
 public:
  explicit SimpleFsm(const FsmConfig& cfg) : cfg_(cfg) {}

  core::telemetry::FsmState state() const { return state_; }

  // now_ms — монотонное время (например steady_clock)
  core::telemetry::FsmState Update(float p, std::int64_t now_ms) {
    switch (state_) {
      case core::telemetry::FsmState::IDLE: {
        if (p >= cfg_.p_on) {
          state_ = core::telemetry::FsmState::CANDIDATE;
          t_mark_ms_ = now_ms;
        }
      } break;

      case core::telemetry::FsmState::CANDIDATE: {
        if (p < cfg_.p_on) {
          state_ = core::telemetry::FsmState::IDLE;
        } else if (now_ms - t_mark_ms_ >= cfg_.t_confirm_ms) {
          state_ = core::telemetry::FsmState::ACTIVE;
          t_mark_ms_ = now_ms;
        }
      } break;

      case core::telemetry::FsmState::ACTIVE: {
        if (p <= cfg_.p_off) {
          // начали "выпускать"
          if (!releasing_) {
            releasing_ = true;
            t_release_ms_ = now_ms;
          } else if (now_ms - t_release_ms_ >= cfg_.t_release_ms) {
            state_ = core::telemetry::FsmState::COOLDOWN;
            t_mark_ms_ = now_ms;
            releasing_ = false;
          }
        } else {
          releasing_ = false;
        }
      } break;

      case core::telemetry::FsmState::COOLDOWN: {
        if (now_ms - t_mark_ms_ >= cfg_.cooldown_ms) {
          state_ = core::telemetry::FsmState::IDLE;
        }
      } break;
    }
    return state_;
  }

 private:
  FsmConfig cfg_;
  core::telemetry::FsmState state_ = core::telemetry::FsmState::IDLE;

  std::int64_t t_mark_ms_ = 0;

  bool releasing_ = false;
  std::int64_t t_release_ms_ = 0;
};

}  // namespace core::fsm
