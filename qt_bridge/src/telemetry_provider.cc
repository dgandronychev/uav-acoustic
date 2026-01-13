#include "qt_bridge/telemetry_provider.h"
#include "qt_bridge/pcen_image_provider.h"

#include <QtGlobal>
#include <QVariantMap>

#include <algorithm>

namespace qt_bridge {

static QString FsmToString(core::telemetry::FsmState st) {
  switch (st) {
    case core::telemetry::FsmState::IDLE: return "IDLE";
    case core::telemetry::FsmState::CANDIDATE: return "CANDIDATE";
    case core::telemetry::FsmState::ACTIVE: return "ACTIVE";
    case core::telemetry::FsmState::COOLDOWN: return "COOLDOWN";
    default: return "UNKNOWN";
  }
}

TelemetryProvider::TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
                                     std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb,
                                     PcenImageProvider* img_provider,
                                     QObject* parent)
    : QObject(parent),
      bus_(std::move(bus)),
      pcen_rb_(std::move(pcen_rb)),
      img_provider_(img_provider) {
  connect(&timer_, &QTimer::timeout, this, &TelemetryProvider::OnTick);
}

void TelemetryProvider::Start(int fps) {
  if (fps <= 0) fps = 12;
  timer_.start(1000 / fps);
}

void TelemetryProvider::OnTick() {
  auto snap = bus_->Latest();
  if (snap) {
    p_detect_ = snap->p_detect_latest;
    fsm_state_ = FsmToString(snap->fsm_state);

    // timeline
    QVariantList tl;
    tl.reserve(snap->timeline_n);
    for (int i = 0; i < snap->timeline_n; ++i) {
      QVariantMap p;
      p["t"] = static_cast<qint64>(snap->timeline[static_cast<std::size_t>(i)].t_ns);
      p["p"] = snap->timeline[static_cast<std::size_t>(i)].p_detect;
      tl.push_back(p);
    }
    timeline_ = std::move(tl);
  }

  // --- PCEN heatmap from ring buffer ---
  // We render exactly 128 time columns to match UI expectation.
  if (pcen_rb_ && img_provider_) {
    int got = 0;
    auto mat = pcen_rb_->SnapshotLast(128, &got);  // got x mels
    if (got > 0) {
      img_provider_->SetPcenMatrix(std::move(mat), got, pcen_rb_->n_mels());
      frame_id_++;  // forces QML image refresh via query param
    }
  }

  emit updated();
}

}  // namespace qt_bridge
