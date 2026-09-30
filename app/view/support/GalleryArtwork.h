#ifndef GALLERYARTWORK_H
#define GALLERYARTWORK_H

#include <QPixmap>
#include <QSize>
#include <QString>

class QPainter;
class QRectF;

namespace fluent::gallery {

/**
 * @brief Renders bundled SVG artwork at the requested display resolution using bounded caches.
 * zh_CN: 按显示分辨率渲染内置 SVG 素材，并使用有容量上限的缓存。
 */
QPixmap galleryArtworkPixmap(const QString& resource, const QSize& logicalSize,
                             qreal devicePixelRatio);

/**
 * @brief Draws SVG artwork in logical coordinates at the paint device's pixel ratio.
 * zh_CN: 在逻辑坐标中按绘制设备的像素比例显示 SVG 素材。
 */
void drawGalleryArtwork(QPainter& painter, const QRectF& rect, const QString& resource);

} // namespace fluent::gallery

#endif // GALLERYARTWORK_H
