#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "core/telemetry/telemetry_bus.h"
#include "qt_bridge/pcen_image_provider.h"

namespace qt_bridge {

class TelemetryProvider final : public QObject {
  Q_OBJECT

  Q_PROPERTY(double pDetectLatest READ pDetectLatest NOTIFY updated)
  Q_PROPERTY(int fsmState READ fsmState NOTIFY updated)
  Q_PROPERTY(QVariantList timeline READ timeline NOTIFY updated)

 public:
  TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
                    PcenImageProvider* img_provider,
                    QObject* parent = nullptr);

  Q_INVOKABLE void start(int ui_fps = 12);
  Q_INVOKABLE void stop();

  [[nodiscard]] double pDetectLatest() const;
  [[nodiscard]] int fsmState() const;
  [[nodiscard]] QVariantList timeline() const;

 signals:
  void updated();

 private slots:
  void onTick();

 private:
  QVariantList buildTimelineQml(const core::telemetry::TelemetrySnapshot& s) const;
  void updateMockHeatmap();  // Step-0: пока пустая/простая

  std::shared_ptr<core::telemetry::TelemetryBus> bus_;
  PcenImageProvider* img_provider_ = nullptr;
  QTimer timer_;

  core::telemetry::TelemetryBus::SnapshotPtr last_;
  QVariantList timeline_;
};

}  // namespace qt_bridge
