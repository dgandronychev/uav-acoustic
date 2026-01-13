#include "qt_bridge/pcen_image_provider.h"

#include <algorithm>
#include <cmath>
#include <random>

#include <QColor>
#include <QSize>

namespace qt_bridge {

    namespace {
        constexpr int kH = 64;
        constexpr int kW = 128;
    }  // namespace

    PcenImageProvider::PcenImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image) {
    }

    QImage PcenImageProvider::requestImage(const QString& /*id*/, QSize* size,
        const QSize& requestedSize) {
        QMutexLocker lk(&mu_);
        EnsureImageLocked();

        QImage out = img_;

        if (requestedSize.isValid()) {
            out = out.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
        if (size) *size = out.size();
        return out;
    }

    void PcenImageProvider::EnsureImageLocked() {
        if (!img_.isNull() && img_.width() == kW && img_.height() == kH) return;

        img_ = QImage(kW, kH, QImage::Format_ARGB32);
        img_.fill(QColor(0, 0, 0));
        step_ = 0;
    }

    QRgb PcenImageProvider::ColorMap(float v) const {
        v = std::clamp(v, 0.0f, 1.0f);

        // Simple "inferno-like" ramp without LUT (fast & ok for mock):
        // black -> purple -> orange -> yellow/white.
        const float x = v;
        const float r = std::clamp(1.5f * x, 0.0f, 1.0f);
        const float g = std::clamp(1.5f * (x - 0.35f), 0.0f, 1.0f);
        const float b = std::clamp(1.8f * (0.6f - x), 0.0f, 1.0f);

        return qRgba(static_cast<int>(255.0f * r),
            static_cast<int>(255.0f * g),
            static_cast<int>(255.0f * b),
            255);
    }

    void PcenImageProvider::UpdateMockHeatmap() {
        QMutexLocker lk(&mu_);
        EnsureImageLocked();

        // Scroll left by 1 column
        for (int y = 0; y < kH; ++y) {
            QRgb* row = reinterpret_cast<QRgb*>(img_.scanLine(y));
            std::memmove(row, row + 1, sizeof(QRgb) * (kW - 1));
        }

        // Deterministic noise
        static thread_local std::mt19937 rng(1234567u);
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);

        // New rightmost column
        const int x = kW - 1;
        const float t = static_cast<float>(step_) * 0.10f;
        ++step_;

        for (int y = 0; y < kH; ++y) {
            // y=0 top, y=kH-1 bottom
            const float fy = static_cast<float>(y) / static_cast<float>(kH - 1);

            // A few moving bands + noise to look like a spectrogram/PCEN
            const float band1 = 0.55f * (0.5f + 0.5f * std::sin(10.0f * fy + t));
            const float band2 = 0.35f * (0.5f + 0.5f * std::sin(22.0f * fy - 0.7f * t));
            const float ridge = 0.25f * std::exp(-std::pow((fy - 0.35f - 0.10f * std::sin(0.4f * t)), 2.0f) / (2.0f * 0.01f));

            const float noise = 0.18f * uni(rng);

            float v = band1 + band2 + ridge + noise;

            // Slight vertical rolloff
            v *= (0.85f + 0.15f * (1.0f - fy));

            // Normalize-ish
            v = std::clamp(v, 0.0f, 1.0f);

            img_.setPixel(x, (kH - 1) - y, ColorMap(v));  // invert Y for nicer look
        }
    }

}  // namespace qt_bridge
