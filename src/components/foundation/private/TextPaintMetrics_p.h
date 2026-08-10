#ifndef FLUENTTEXTPAINTMETRICS_P_H
#define FLUENTTEXTPAINTMETRICS_P_H

#include <QFontMetricsF>
#include <QRectF>
#include <QString>

namespace fluent::painting {

// QPainter's AlignVCenter centers the font line box, whose ascent and descent
// are intentionally asymmetric. Center the glyph ink instead so a selected
// row's text stays aligned with its center indicator across native and browser
// font backends. This is a private paint helper, not widget geometry policy.
// zh_CN: QPainter 的 AlignVCenter 居中的是上下不对称的字体行框。这里改为按字形
// 实际墨迹居中，使选中行文字在原生与浏览器字体后端都能和中心指示条对齐。
inline qreal verticallyCenteredTextInkOffset(const QRectF& targetRect,
                                              const QFontMetricsF& metrics,
                                              const QString& text)
{
    if (!targetRect.isValid() || targetRect.isEmpty() || text.isEmpty())
        return 0.0;

    const QRectF ink = metrics.tightBoundingRect(text);
    if (!ink.isValid() || ink.isEmpty())
        return 0.0;

    const qreal alignedBaseline = targetRect.top()
        + (targetRect.height() - metrics.height()) / 2.0
        + metrics.ascent();
    const qreal alignedInkCenter = alignedBaseline + ink.center().y();
    return targetRect.center().y() - alignedInkCenter;
}

inline QRectF verticallyCenteredTextInkRect(const QRectF& targetRect,
                                            const QFontMetricsF& metrics,
                                            const QString& text)
{
    return targetRect.translated(
        0.0, verticallyCenteredTextInkOffset(targetRect, metrics, text));
}

inline qreal alignedTextInkCenterY(const QRectF& alignedRect,
                                   const QFontMetricsF& metrics,
                                   const QString& text)
{
    const QRectF ink = metrics.tightBoundingRect(text);
    const qreal baseline = alignedRect.top()
        + (alignedRect.height() - metrics.height()) / 2.0
        + metrics.ascent();
    return baseline + ink.center().y();
}

} // namespace fluent::painting

#endif // FLUENTTEXTPAINTMETRICS_P_H
