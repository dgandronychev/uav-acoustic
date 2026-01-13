#pragma once
#include <QQuickImageProvider>
#include <QImage>
#include <QReadWriteLock>

namespace qt_bridge {

class PcenImageProvider final : public QQuickImageProvider {
 public:
  PcenImageProvider();

  void SetImage(const QImage& img);
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

 private:
  QReadWriteLock lock_;
  QImage image_;
};

}  // namespace qt_bridge
