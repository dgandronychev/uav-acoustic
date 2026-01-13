#include "qt_bridge/pcen_image_provider.h"

#include <algorithm>
#include <cmath>

namespace qt_bridge {

    static inline unsigned char clamp_u8(int v) {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return static_cast<unsigned char>(v);
    }

    PcenImageProvider::PcenImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image) {
    }

    void PcenImageProvider::SetPcenMatrix(std::vector<float> m, int rows, int cols) {
        QMutexLocker lk(&mu_);
        mat_ = std::move(m);
        rows_ = rows;
        cols_ = cols;
        cached_ = QImage(); // drop cache
    }

    QImage PcenImageProvider::BuildHeatmapLocked() const {
        if (rows_ <= 0 || cols_ <= 0 || static_cast<int>(mat_.size()) != rows_ * cols_) {
            // fallback image
            QImage img(256, 128, QImage::Format_RGB32);
            img.fill(qRgb(20, 20, 20));
            return img;
        }

        // Normalize (robust-ish): find min/max
        float mn = mat_[0], mx = mat_[0];
        for (float v : mat_) { mn = std::min(mn, v); mx = std::max(mx, v); }
        const float denom = (mx > mn) ? (mx - mn) : 1.0f;

        // image: x=time(frames), y=mels (top = high freq)
        QImage img(rows_, cols_, QImage::Format_RGB32);

        for (int x = 0; x < rows_; ++x) {
            for (int y = 0; y < cols_; ++y) {
                const int mel = (cols_ - 1 - y);
                const float v = mat_[x * cols_ + mel];
                float t = (v - mn) / denom;
                t = std::clamp(t, 0.0f, 1.0f);

                // Simple colormap: dark->green->yellow->white
                const int r = static_cast<int>(255.0f * std::pow(t, 0.55f));
                const int g = static_cast<int>(255.0f * std::pow(t, 0.35f));
                const int b = static_cast<int>(255.0f * std::pow(t, 1.80f));

                img.setPixel(x, y, qRgb(clamp_u8(r), clamp_u8(g), clamp_u8(b)));
            }
        }
        return img;
    }

    QImage PcenImageProvider::requestImage(const QString&, QSize* size, const QSize& requestedSize) {
        QMutexLocker lk(&mu_);
        if (cached_.isNull()) {
            cached_ = BuildHeatmapLocked();
        }

        QImage out = cached_;
        if (requestedSize.isValid()) {
            out = out.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        if (size) *size = out.size();
        return out;
    }

}  // namespace qt_bridge
