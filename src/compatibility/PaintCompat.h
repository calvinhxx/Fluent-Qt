#pragma once

// Pixmap sizing, device-pixel-ratio handling, and color components.

#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

// Explicit bitmap caches retain their requested DPR metadata. Compensate only
// when the painter ignored a sub-unit DPR; newer Qt already has the right matrix.
// zh_CN: 显式位图缓存保留原 DPR 元数据；仅在画笔忽略小于 1 的 DPR 时补偿，新 Qt 的矩阵保持不变。
/**
 * @brief Painter for explicit image/pixmap caches, including DPR below 1 on older Qt.
 * zh_CN: 用于显式图像缓存的画笔，兼容旧 Qt 上小于 1 的 DPR。
 *
 * Construct with the cache device after setting its DPR. Do not use for QWidget
 * painting or restart with begin(); the compensation belongs to this paint session.
 * zh_CN: 先设置缓存 DPR 再构造画笔；不用于 QWidget 或通过 begin() 重启绘制。
 */
class FluentRasterCachePainter final : public QPainter {
public:
    explicit FluentRasterCachePainter(QPaintDevice* target) : QPainter(target)
    {
        if (!isActive() || !target || (target->devType() != QInternal::Image && target->devType() != QInternal::Pixmap))
            return;
        const qreal dpr = target->devicePixelRatioF();
        const QTransform matrix = deviceTransform();
        if (dpr > 0.0 && dpr < 1.0 && qFuzzyCompare(matrix.m11(), 1.0) && qFuzzyCompare(matrix.m22(), 1.0))
            scale(dpr, dpr);
    }
};

// Qt 5.15/6.2 clamp a paint device's DPR to 1. Qt effect filters also assume
// at least unit density. Keep explicit render targets at >= 1 and apply the
// remaining scale in the painter; allocation and dirty tiles stay unchanged.
// zh_CN: Qt 5.15/6.2 将画布 DPR 钳制到 1，特效滤镜也假定至少单位密度。
// 显式渲染目标保持 DPR >= 1，剩余缩放交给画笔；不扩大分配尺寸或脏图块。
/**
 * @brief Sets an explicit render target DPR; pair with fluentApplyRenderTargetScale().
 * zh_CN: 设置显式渲染目标的 DPR；配合 fluentApplyRenderTargetScale() 使用。
 */
template <class Device> inline void fluentSetRenderTargetDevicePixelRatio(Device& device, qreal dpr)
{
    device.setDevicePixelRatio(qMax(qreal(1), dpr));
}

/**
 * @brief Applies the remaining scale to a normalized explicit render target.
 * zh_CN: 对已归一化的显式渲染目标补充剩余缩放；每次绘制开始时调用一次。
 */
inline void fluentApplyRenderTargetScale(QPainter& painter, qreal dpr)
{
    if (dpr > 0.0 && dpr < 1.0)
        painter.scale(dpr, dpr);
}

inline QSize fluentPixmapLogicalSize(const QPixmap& pixmap)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return pixmap.deviceIndependentSize().toSize();
#else
    return QSize(qRound(pixmap.width() / pixmap.devicePixelRatioF()),
        qRound(pixmap.height() / pixmap.devicePixelRatioF()));
#endif
}

/**
 * @brief Draws a pixmap at its logical size, including DPR below 1.
 * zh_CN: 按逻辑尺寸绘制位图，包含小于 1 的 DPR。
 */
inline void fluentDrawPixmapAt(QPainter& painter, const QPointF& position, const QPixmap& pixmap)
{
    const qreal dpr = pixmap.devicePixelRatioF();
    // Qt's untransformed raster fast path ignores source DPR below 1.
    // zh_CN: Qt 无缩放矩阵时的位图快速路径会忽略小于 1 的源 DPR。
    if (dpr > 0.0 && dpr < 1.0)
        painter.drawPixmap(QRectF(position, QSizeF(pixmap.size()) / dpr), pixmap, QRectF(pixmap.rect()));
    else
        painter.drawPixmap(position, pixmap);
}

/**
 * @brief Draws an image at its logical size, including DPR below 1.
 * zh_CN: 按逻辑尺寸绘制图像，包含小于 1 的 DPR。
 */
inline void fluentDrawImageAt(QPainter& painter, const QPointF& position, const QImage& image)
{
    const qreal dpr = image.devicePixelRatioF();
    if (dpr > 0.0 && dpr < 1.0)
        painter.drawImage(QRectF(position, QSizeF(image.size()) / dpr), image, QRectF(image.rect()));
    else
        painter.drawImage(position, image);
}

inline QRectF fluentPixmapSourceRectForDraw(const QRectF& logicalSource, const QPixmap& pixmap)
{
    // QPainter::drawPixmap source rectangles are in pixmap pixel coordinates on
    // both Qt 5 and Qt 6, including when devicePixelRatio != 1.
    // zh_CN: Qt 5/6 的 drawPixmap 源矩形都使用 pixmap 像素坐标，即使
    // devicePixelRatio != 1 也如此。
    const qreal dpr = qMax<qreal>(1.0e-6, pixmap.devicePixelRatioF());
    return QRectF(logicalSource.left() * dpr, logicalSource.top() * dpr, logicalSource.width() * dpr,
        logicalSource.height() * dpr);
}

inline QPixmap fluentIconPixmapForLogicalExtent(const QIcon& icon,
    const QSize& logicalExtent,
    qreal devicePixelRatio = 1.0,
    QWindow* targetWindow = nullptr)
{
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatio);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5 applies the application's High-DPI factor inside QIcon::pixmap().
    // Passing a physical extent here would therefore scale the icon twice.
    QPixmap pixmap = targetWindow ? icon.pixmap(targetWindow, logicalExtent) : icon.pixmap(logicalExtent);
    const QSize physicalExtent(qMax(1, qRound(logicalExtent.width() * dpr)),
        qMax(1, qRound(logicalExtent.height() * dpr)));
    if (!pixmap.isNull() && pixmap.size() != physicalExtent) {
        pixmap = pixmap.scaled(physicalExtent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
#else
    Q_UNUSED(targetWindow);
    return icon.pixmap(logicalExtent, dpr);
#endif
}

/**
 * @brief Effective device pixel ratio for the painter's current paint device.
 * zh_CN: 绘制器当前绘制设备的有效设备像素比。
 */
inline qreal fluentPainterDevicePixelRatio(const QPainter& painter)
{
    if (!painter.device())
        return 1.0;
    const qreal dpr = painter.device()->devicePixelRatioF();
    return (dpr > 0.0) ? dpr : 1.0;
}

/**
 * @brief Draws a pixmap into a logical rect using the painter device DPR.
 * zh_CN: 按绘制器设备 DPR 将 pixmap 画进逻辑矩形。
 *
 * Scales the source to the physical pixel extent of `logicalRect`, sets the
 * pixmap DPR, and centers the result. Prefer this over `drawPixmap(rect,
 * source)` when painting Gallery thumbnails or other 1x assets into HiDPI /
 * fractional-scale surfaces.
 * zh_CN: 将源图缩放到 `logicalRect` 的物理像素范围、设置 pixmap DPR 并居中绘制。
 * 在 HiDPI / 分数缩放下绘制缩略图或其他 1x 素材时，优先于 `drawPixmap(rect, source)`。
 */
inline void fluentDrawPixmapInLogicalRect(QPainter& painter, const QRectF& logicalRect, const QPixmap& source)
{
    if (logicalRect.isEmpty() || source.isNull())
        return;

    const qreal dpr = qMax<qreal>(1.0, fluentPainterDevicePixelRatio(painter));
    const QSize target(qMax(1, qRound(logicalRect.width() * dpr)), qMax(1, qRound(logicalRect.height() * dpr)));
    QPixmap scaled = source;
    if (scaled.size() != target) {
        scaled = source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    scaled.setDevicePixelRatio(dpr);

    const QSizeF logicalSize = QSizeF(scaled.size()) / dpr;
    const QPointF topLeft(logicalRect.x() + (logicalRect.width() - logicalSize.width()) * 0.5,
        logicalRect.y() + (logicalRect.height() - logicalSize.height()) * 0.5);
    painter.drawPixmap(topLeft, scaled);
}

inline void fluentDrawPixmapInLogicalRect(QPainter& painter, const QRect& logicalRect, const QPixmap& source)
{
    fluentDrawPixmapInLogicalRect(painter, QRectF(logicalRect), source);
}

/**
 * @brief Draws a pixmap with cover-crop scaling into a logical rect.
 * zh_CN: 以 cover（裁切铺满）方式将 pixmap 画进逻辑矩形。
 *
 * Scales uniformly so the rect is fully covered, then centers the crop. Used by
 * photo cards and similar media tiles.
 * zh_CN: 等比放大至铺满矩形后居中裁切；用于照片卡等媒体瓦片。
 */
inline void fluentDrawCoverPixmapInLogicalRect(QPainter& painter, const QRectF& logicalRect, const QPixmap& source)
{
    if (logicalRect.isEmpty() || source.isNull())
        return;

    const qreal sourceDpr = qMax<qreal>(1.0e-6, source.devicePixelRatioF());
    const QSizeF sourceSize = QSizeF(source.size()) / sourceDpr;
    if (sourceSize.isEmpty())
        return;

    const qreal scale = qMax(logicalRect.width() / sourceSize.width(), logicalRect.height() / sourceSize.height());
    const QSizeF visible(logicalRect.width() / scale, logicalRect.height() / scale);
    const QRectF crop((sourceSize.width() - visible.width()) * 0.5, (sourceSize.height() - visible.height()) * 0.5,
        visible.width(), visible.height());
    painter.drawPixmap(logicalRect, source, fluentPixmapSourceRectForDraw(crop, source));
}

// QColor::getHsvF / getRgbF / getHslF component pointer type.
// zh_CN: QColor::getHsvF / getRgbF / getHslF 分量指针类型。
// Qt 6 takes float*; Qt 5 takes qreal* (= double*).
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentColorComponent = float;
#else
using FluentColorComponent = qreal;
#endif
