#include "qt_bridge/pcen_image_provider.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace qt_bridge {

    void PcenImageProvider::SetPcenMatrix(const std::vector<float>& frames, int n_mels, int n_frames) {
        QMutexLocker lk(&mu_);
        n_mels_ = n_mels;
        n_frames_ = n_frames;
        frames_ = frames;
    }

    QImage PcenImageProvider::GetLatestImage() const {
        QImage img;
        QMutexLocker lk(&mu_);
        if (n_mels_ <= 0 || n_frames_ <= 0 || frames_.empty()) {
            img = QImage(128, 64, QImage::Format_RGB32);
            img.fill(Qt::black);
        }
        else {
            img = QImage(n_frames_, n_mels_, QImage::Format_RGB32);
            BuildNormalizedImageLocked(&img);
        }
        return img;
    }

    static inline float safe_log1p(float v) {
        if (v < 0.0f) v = 0.0f;
        return std::log1p(v);
    }

    void PcenImageProvider::BuildNormalizedImageLocked(QImage* out) const {
        const int W = n_frames_;
        const int H = n_mels_;

        std::vector<float> tmp;
        tmp.resize(static_cast<std::size_t>(W) * static_cast<std::size_t>(H));

        for (int x = 0; x < W; ++x) {
            for (int y = 0; y < H; ++y) {
                const std::size_t idx = static_cast<std::size_t>(x) * static_cast<std::size_t>(H) + static_cast<std::size_t>(y);
                tmp[idx] = safe_log1p(frames_[idx]);
            }
        }
        const std::size_t N = tmp.size();
        if (N == 0) {
            out->fill(Qt::black);
            return;
        }


        auto tmp2 = tmp;
        const std::size_t k_lo = static_cast<std::size_t>(0.02 * static_cast<double>(N));
        const std::size_t k_hi = static_cast<std::size_t>(0.98 * static_cast<double>(N));
        const std::size_t lo_i = std::min(k_lo, N - 1);
        const std::size_t hi_i = std::min(k_hi, N - 1);

        std::nth_element(tmp2.begin(), tmp2.begin() + lo_i, tmp2.end());
        const float lo = tmp2[lo_i];

        std::nth_element(tmp2.begin(), tmp2.begin() + hi_i, tmp2.end());
        const float hi = tmp2[hi_i];

        float denom = (hi - lo);
        if (!(denom > 1e-12f)) denom = 1.0f;

        // QImage origin is top-left. Keep low mel at bottom by inverting Y.
        for (int x = 0; x < W; ++x) {
            for (int y = 0; y < H; ++y) {
                const std::size_t idx = static_cast<std::size_t>(x) * static_cast<std::size_t>(H) + static_cast<std::size_t>(y);
                float t = (tmp[idx] - lo) / denom;
                t = std::clamp(t, 0.0f, 1.0f);
                const int yy = (H - 1) - y;
                out->setPixel(x, yy, ColorMap(t));
            }
        }
    }
    QRgb PcenImageProvider::ColorMap(float t) {
        // Simple "parula-ish" gradient: blue -> cyan -> green -> yellow
        t = std::clamp(t, 0.0f, 1.0f);

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        if (t < 0.25f) {
            const float k = t / 0.25f;
            r = 0.05f * k;
            g = 0.15f * k;
            b = 0.40f + 0.60f * k;
        }
        else if (t < 0.50f) {
            const float k = (t - 0.25f) / 0.25f;
            r = 0.05f;
            g = 0.15f + 0.75f * k;
            b = 1.0f - 0.70f * k;
        }
        else if (t < 0.80f) {
            const float k = (t - 0.50f) / 0.30f;
            r = 0.05f + 0.95f * k;
            g = 0.90f + 0.10f * k;
            b = 0.30f * (1.0f - k);
        }
        else {
            const float k = (t - 0.80f) / 0.20f;
            r = 1.0f;
            g = 1.0f;
            b = 0.0f + 0.25f * k;
        }
        auto to8 = [](float x) -> int {
            const int v = static_cast<int>(x * 255.0f + 0.5f);
            return std::clamp(v, 0, 255);
            };
        return qRgb(to8(r), to8(g), to8(b));
    }
}  // namespace qt_bridge
