#ifndef FLUENTTEXTPAINTCOMPAT_H
#define FLUENTTEXTPAINTCOMPAT_H

#include <QFontMetricsF>
#include <QPaintDevice>
#include <QRectF>
#include <QString>
#include <QtMath>

namespace fluent::painting {

/**
 * @brief Encodes a target DPR for a forwarding paint device's metric override.
 * zh_CN: 将目标 DPR 编码为转发绘制设备的 metric 返回值。
 */
inline int devicePixelRatioMetric(QPaintDevice::PaintDeviceMetric metric, qreal dpr)
{
    switch (metric) {
    case QPaintDevice::PdmDevicePixelRatio:
        return qRound(dpr);
    case QPaintDevice::PdmDevicePixelRatioScaled:
        return qRound(dpr * QPaintDevice::devicePixelRatioFScale());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    case QPaintDevice::PdmDevicePixelRatioF_EncodedA:
    case QPaintDevice::PdmDevicePixelRatioF_EncodedB:
        return QPaintDevice::encodeMetricF(metric, dpr);
#endif
    default:
        return 0;
    }
}

/**
 * @brief Returns the DPR precision used by QImage's raster paint device.
 * zh_CN: 返回 QImage 栅格绘制设备实际采用精度下的 DPR。
 */
inline qreal imageRasterDpr(qreal dpr)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    const qreal scale = QPaintDevice::devicePixelRatioFScale();
    return qFloor(dpr * scale) / scale;
#else
    return dpr;
#endif
}

/**
 * @brief Returns the vertical translation that centers visible glyph ink in a target rectangle.
 * zh_CN: 返回将可见字形墨迹垂直居中到目标矩形所需的平移量。
 */
inline qreal verticallyCenteredTextInkOffset(const QRectF& targetRect, const QFontMetricsF& metrics,
                                             const QString& text)
{
    if (!targetRect.isValid() || targetRect.isEmpty() || text.isEmpty())
        return 0.0;

    const QRectF ink = metrics.tightBoundingRect(text);
    if (!ink.isValid() || ink.isEmpty())
        return 0.0;

    const qreal alignedBaseline =
        targetRect.top() + (targetRect.height() - metrics.height()) / 2.0 + metrics.ascent();
    const qreal alignedInkCenter = alignedBaseline + ink.center().y();
    return targetRect.center().y() - alignedInkCenter;
}

/**
 * @brief Returns a text rectangle translated so its visible glyph ink is vertically centered.
 * zh_CN: 返回经过垂直平移的文本矩形，使其可见字形墨迹居中。
 */
inline QRectF verticallyCenteredTextInkRect(const QRectF& targetRect, const QFontMetricsF& metrics,
                                            const QString& text)
{
    return targetRect.translated(0.0, verticallyCenteredTextInkOffset(targetRect, metrics, text));
}

/**
 * @brief Returns the visible glyph-ink center for text drawn with vertical-center alignment.
 * zh_CN: 返回使用垂直居中对齐绘制文本时的可见字形墨迹中心。
 */
inline qreal alignedTextInkCenterY(const QRectF& alignedRect, const QFontMetricsF& metrics,
                                   const QString& text)
{
    const QRectF ink = metrics.tightBoundingRect(text);
    const qreal baseline =
        alignedRect.top() + (alignedRect.height() - metrics.height()) / 2.0 + metrics.ascent();
    return baseline + ink.center().y();
}

} // namespace fluent::painting

#endif // FLUENTTEXTPAINTCOMPAT_H
