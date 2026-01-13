#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>

#include <vector>

namespace qt_bridge {

	class PcenImageProvider : public QQuickImageProvider {
	public:
		PcenImageProvider();

		// frames: packed [n_frames x n_mels], newest at end (как обычно из SnapshotLast)
		void SetPcenMatrix(const std::vector<float>& frames, int n_mels, int n_frames);

		QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

	private:
		static QRgb ColorMap(float t);

		// Apply log1p + percentile normalization to 0..1
		void BuildNormalizedImageLocked(QImage* out) const;

		mutable QMutex mu_;

		int n_mels_ = 0;
		int n_frames_ = 0;
		std::vector<float> frames_;  // packed
	};

}  // namespace qt_bridge
