#include "qt_bridge/pcen_image_provider.h"

namespace qt_bridge {

PcenImageProvider::PcenImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

void PcenImageProvider::SetImage(const QImage& img) {
  QWriteLocker wl(&lock_);
  image_ = img;
}

QImage PcenImageProvider::requestImage(const QString&, QSize* size, const QSize& requestedSize) {
  QReadLocker rl(&lock_);
  QImage out = image_;
  if (size) *size = out.size();
  if (!requestedSize.isValid() || out.isNull()) return out;
  return out.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

}  // namespace qt_bridge
