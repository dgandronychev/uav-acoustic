#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>
#include <vector>

namespace qt_bridge {

	class PcenImageProvider : public QQuickImageProvider {
	public:
		PcenImageProvider();

		// rows x cols matrix in row-major (rows=frames, cols=mels)
		void SetPcenMatrix(std::vector<float> m, int rows, int cols);

		QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

	private:
		QImage BuildHeatmapLocked() const;

	private:
		mutable QMutex mu_;
		std::vector<float> mat_;
		int rows_{ 0 };
		int cols_{ 0 };
		mutable QImage cached_;
	};

}  // namespace qt_bridge
