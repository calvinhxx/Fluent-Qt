#pragma once

// Pixmap sizing, device-pixel-ratio handling, and color components.

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

inline QSize fluentPixmapLogicalSize(const QPixmap& pixmap)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return pixmap.deviceIndependentSize().toSize();
#else
    return QSize(qRound(pixmap.width() / pixmap.devicePixelRatioF()),
        qRound(pixmap.height() / pixmap.devicePixelRatioF()));
#endif
}

inline QRectF fluentPixmapSourceRectForDraw(const QRectF& logicalSource, const QPixmap& pixmap)
{
    // QPainter::drawPixmap source rectangles are in pixmap pixel coordinates on
    // both Qt 5 and Qt 6, including when devicePixelRatio != 1.
    // zh_CN: Qt 5/6 的 drawPixmap 源矩形都使用 pixmap 像素坐标，即使
    // devicePixelRatio != 1 也如此。
    const qreal dpr = qMax<qreal>(1.0, pixmap.devicePixelRatioF());
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
