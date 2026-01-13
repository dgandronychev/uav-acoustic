#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace core::telemetry {

enum class FsmState : std::uint8_t {
  IDLE = 0,
  CANDIDATE = 1,
  ACTIVE = 2,
  COOLDOWN = 3
};

struct DoaEstimate {
  bool valid = false;
  float azimuth_deg = 0.0f;
  float elevation_deg = 0.0f;
  float conf = 0.0f;
};

struct TimelinePoint {
  std::int64_t t_ns = 0;
  float p_detect = 0.0f;
  DoaEstimate doa;
};

struct CrnnTopK {
  struct Item { int class_id = -1; float score = 0.0f; };
  std::int64_t t_start_ns = 0;
  std::int64_t t_end_ns = 0;
  int predicted_id = -1;
  float confidence = 0.0f;
  bool is_unknown = true;
  std::vector<Item> topk;
};

struct TelemetrySnapshot {
  std::int64_t t_ns = 0;

  // Heatmap metadata (pixels live in qt_bridge provider cache)
  int pcen_T = 0;
  int pcen_F = 0;
  std::int64_t pcen_t_start_ns = 0;
  std::int64_t pcen_t_end_ns = 0;

  static constexpr int kMaxTimeline = 256;
  int timeline_n = 0;
  std::array<TimelinePoint, kMaxTimeline> timeline{};

  FsmState fsm_state = FsmState::IDLE;
  float p_on = 0.6f;
  float p_off = 0.4f;

  float p_detect_latest = 0.0f;
  DoaEstimate doa_latest;

  bool has_crnn = false;
  CrnnTopK crnn;
};

}  // namespace core::telemetry
