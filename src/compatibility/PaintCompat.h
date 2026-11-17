#pragma once

// Pixmap sizing, device-pixel-ratio handling, and color components.

#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

/**
 * @brief Painter for explicit image/pixmap caches that compensates when older Qt ignores DPR < 1.0.
 * zh_CN: 用于显式图像缓存的画笔，在旧版 Qt 忽略小于 1.0 的 DPR 时自动补齐缩放。
 *
 * Construct with the cache device after setting its DPR; do not use for QWidgets or restart with begin().
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

/**
 * @brief Sets an explicit render-target DPR clamped to >= 1.0 (required by Qt 5.15/6.2 and effect filters); pair with fluentApplyRenderTargetScale().
 * zh_CN: 为显式渲染目标设置不低于 1.0 的 DPR（适配 Qt 5.15/6.2 及特效滤镜），需配合 fluentApplyRenderTargetScale() 使用。
 */
template <class Device> inline void fluentSetRenderTargetDevicePixelRatio(Device& device, qreal dpr)
{
    device.setDevicePixelRatio(qMax(qreal(1), dpr));
}

/**
 * @brief Applies the remaining sub-1.0 scale once at the start of painting a normalized render target.
 * zh_CN: 对已归一化 DPR 的显式渲染目标在绘制开始时补充小于 1.0 的剩余缩放。
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
 * @brief Draws a pixmap centered in a logical rect at the painter device's DPR (preferred for 1x thumbnails on HiDPI/fractional scales).
 * zh_CN: 按绘制设备 DPR 将位图居中绘制到逻辑矩形内（适合 HiDPI/分数缩放下的 1x 缩略图等素材）。
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
 * @brief Draws a pixmap scaled uniformly to cover a logical rect with centered cropping.
 * zh_CN: 等比缩放并居中裁切铺满逻辑矩形（cover 模式）。
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

/**
 * @brief QColor::getHsvF / getRgbF / getHslF component pointer type (float on Qt 6, qreal on Qt 5).
 * zh_CN: QColor::getHsvF / getRgbF / getHslF 的颜色分量类型（Qt 6 为 float，Qt 5 为 qreal）。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentColorComponent = float;
#else
using FluentColorComponent = qreal;
#endif
