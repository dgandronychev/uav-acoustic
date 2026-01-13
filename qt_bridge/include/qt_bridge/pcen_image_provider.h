#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

namespace qt_bridge {

	// Mock PCEN heatmap provider: 64x128 (H x W), scrolling.
	class PcenImageProvider final : public QQuickImageProvider {
	public:
		PcenImageProvider();

		// QQuickImageProvider
		QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

		// Called from UI timer thread (TelemetryProvider).
		void UpdateMockHeatmap();

	private:
		void EnsureImageLocked();
		QRgb ColorMap(float v) const;  // v in [0..1]

	private:
		mutable QMutex mu_;
		QImage img_;          // Format_ARGB32, size 128x64 (W x H)
		int step_ = 0;        // animation step
	};

}  // namespace qt_bridge
