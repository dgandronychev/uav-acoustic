#pragma once

#include <QObject>
#include <QVariantList>
#include <QTimer>

#include <memory>

#include "core/telemetry/telemetry_bus.h"
#include "core/dsp/pcen_ring_buffer.h"

namespace qt_bridge {

class PcenImageProvider;

class TelemetryProvider final : public QObject {
  Q_OBJECT

  Q_PROPERTY(double pDetectLatest READ pDetectLatest NOTIFY updated)
  Q_PROPERTY(QString fsmState READ fsmState NOTIFY updated)
  Q_PROPERTY(QVariantList timeline READ timeline NOTIFY updated)
  Q_PROPERTY(int frameId READ frameId NOTIFY updated)

 public:
  TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
                    std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb,
                    PcenImageProvider* img_provider,
                    QObject* parent = nullptr);

  // Keep both names to avoid your past "Start/start" mismatch
  Q_INVOKABLE void Start(int fps);
  Q_INVOKABLE void start(int fps) { Start(fps); }

  double pDetectLatest() const { return p_detect_; }
  QString fsmState() const { return fsm_state_; }
  QVariantList timeline() const { return timeline_; }
  int frameId() const { return frame_id_; }

 signals:
  void updated();

 private slots:
  void OnTick();

 private:
  std::shared_ptr<core::telemetry::TelemetryBus> bus_;
  std::shared_ptr<core::dsp::PcenRingBuffer> pcen_rb_;
  PcenImageProvider* img_provider_ = nullptr;

  QTimer timer_;

  double p_detect_ = 0.0;
  QString fsm_state_ = "IDLE";
  QVariantList timeline_;
  int frame_id_ = 0;
};

}  // namespace qt_bridge
