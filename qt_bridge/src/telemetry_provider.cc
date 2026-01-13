#include "qt_bridge/telemetry_provider.h"

#include <QtGlobal>
#include <QImage>
#include <cmath>

namespace qt_bridge {

TelemetryProvider::TelemetryProvider(std::shared_ptr<core::telemetry::TelemetryBus> bus,
                                     PcenImageProvider* img_provider,
                                     QObject* parent)
    : QObject(parent),
      bus_(std::move(bus)),
      img_provider_(img_provider) {
  timer_.setTimerType(Qt::PreciseTimer);
  connect(&timer_, &QTimer::timeout, this, &TelemetryProvider::onTick);
}

void TelemetryProvider::start(int ui_fps) {
  if (ui_fps <= 0) ui_fps = 12;
  const int interval_ms = qMax(1, 1000 / ui_fps);
  timer_.start(interval_ms);
}

void TelemetryProvider::stop() {
  timer_.stop();
}

double TelemetryProvider::pDetectLatest() const {
  if (!last_) return 0.0;
  return static_cast<double>(last_->p_detect_latest);
}

int TelemetryProvider::fsmState() const {
  if (!last_) return static_cast<int>(core::telemetry::FsmState::IDLE);
  return static_cast<int>(last_->fsm_state);
}

QVariantList TelemetryProvider::timeline() const {
  return timeline_;
}

void TelemetryProvider::onTick() {
  if (!bus_) return;

  auto snap = bus_->Latest();
  if (!snap) return;
  if (snap == last_) return;

  last_ = std::move(snap);
  timeline_ = buildTimelineQml(*last_);

  // Step-0: mock heatmap (позже подключим реальный PCEN ring buffer)
  updateMockHeatmap();

  emit updated();
}

QVariantList TelemetryProvider::buildTimelineQml(const core::telemetry::TelemetrySnapshot& s) const {
  QVariantList out;
  out.reserve(s.timeline_n);

  for (int i = 0; i < s.timeline_n; ++i) {
    const auto& p = s.timeline[static_cast<std::size_t>(i)];
    QVariantMap m;
    m["t_ns"] = QVariant::fromValue<qint64>(static_cast<qint64>(p.t_ns));
    m["p"] = p.p_detect;
    out.push_back(m);
  }
  return out;
}

void TelemetryProvider::updateMockHeatmap() {
  if (!img_provider_) return;

  // Простая "движущаяся" картинка, чтобы сразу было видно обновление.
  constexpr int W = 600;
  constexpr int H = 200;

  QImage img(W, H, QImage::Format_Grayscale8);
  if (img.isNull()) return;

  const double phase = last_ ? (last_->p_detect_latest) : 0.0;
  for (int y = 0; y < H; ++y) {
    uchar* line = img.scanLine(y);
    for (int x = 0; x < W; ++x) {
      const double v = 0.5 + 0.5 * std::sin(0.03 * x + 0.08 * y + 10.0 * phase);
      line[x] = static_cast<uchar>(std::lround(v * 255.0));
    }
  }

  img_provider_->SetImage(img);
}

}  // namespace qt_bridge
