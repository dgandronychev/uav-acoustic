#pragma once

#include <QImage>
#include <QMutex>

#include <vector>

namespace qt_bridge {

	class PcenImageProvider {
	public:
		PcenImageProvider() = default;

		void SetPcenMatrix(const std::vector<float>& frames, int n_mels, int n_frames);
		QImage GetLatestImage() const;

	private:
		static QRgb ColorMap(float t);
		void BuildNormalizedImageLocked(QImage* out) const;
		mutable QMutex mu_;
		int n_mels_ = 0;
		int n_frames_ = 0;
		std::vector<float> frames_;
	};
		

}  // namespace qt_bridge
