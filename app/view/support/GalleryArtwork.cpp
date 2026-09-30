#include "GalleryArtwork.h"

#include <cmath>

#include <QCache>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace fluent::gallery {

QPixmap galleryArtworkPixmap(const QString& resource, const QSize& logicalSize,
                             qreal devicePixelRatio)
{
    if (resource.isEmpty() || logicalSize.isEmpty() || !std::isfinite(devicePixelRatio))
        return {};

    const qreal dpr = qMax(qreal(1), devicePixelRatio);
    const QSize physicalSize(qMax(1, qRound(logicalSize.width() * dpr)),
                             qMax(1, qRound(logicalSize.height() * dpr)));
    const QString key = QStringLiteral("fluentqt-gallery-svg:%1:%2x%3:%4")
                            .arg(resource)
                            .arg(physicalSize.width())
                            .arg(physicalSize.height())
                            .arg(QString::number(dpr, 'g', 17));
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap))
        return pixmap;

    // Parse each SVG only on a cache miss. Qt bounds the shared pixmap cache by bytes;
    // this separate LRU bounds parsed documents across page creation and teardown.
    // zh_CN: 仅在缓存未命中时解析 SVG。Qt 按字节限制位图缓存，另以 LRU 限制跨页面的解析文档数量。
    static QCache<QString, QSvgRenderer> renderers(128);
    QSvgRenderer* renderer = renderers.object(resource);
    if (!renderer) {
        renderer = new QSvgRenderer(resource);
        if (!renderer->isValid()) {
            delete renderer;
            return {};
        }
        renderers.insert(resource, renderer);
    }

    pixmap = QPixmap(physicalSize);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer->render(&painter, QRectF(QPointF(), physicalSize));
    }
    pixmap.setDevicePixelRatio(dpr);
    QPixmapCache::insert(key, pixmap);
    return pixmap;
}

void drawGalleryArtwork(QPainter& painter, const QRectF& rect, const QString& resource)
{
    // QWidget::render can redirect painting to a denser image while device() still
    // reports the widget's DPR. The effective transform includes that backing scale.
    // zh_CN: QWidget::render 重定向到高分辨率图像时，device() 仍可能报告 widget 的 DPR；
    // 使用有效变换以包含重定向目标的像素比例。
    const QTransform transform = painter.deviceTransform();
    const qreal dpr = qMax(painter.device()->devicePixelRatioF(),
                           qMax(std::hypot(transform.m11(), transform.m12()),
                                std::hypot(transform.m21(), transform.m22())));
    const QPixmap pixmap = galleryArtworkPixmap(resource, rect.size().toSize(), dpr);
    if (pixmap.isNull())
        return;
    const QSizeF size = QSizeF(pixmap.size()) / pixmap.devicePixelRatioF();
    painter.drawPixmap(rect.center() - QPointF(size.width() / 2, size.height() / 2), pixmap);
}

} // namespace fluent::gallery
