#ifndef FLUENTTEXTPAINTCOMPAT_H
#define FLUENTTEXTPAINTCOMPAT_H

#include <QFontMetricsF>
#include <QGlyphRun>
#include <QPaintDevice>
#include <QPainter>
#include <QRawFont>
#include <QRectF>
#include <QString>
#include <QTextLayout>
#include <QtMath>

namespace fluent::painting {

/**
 * @brief Returns a centered font baseline, independent of the displayed characters.
 * zh_CN: 返回按字体度量居中的基线，不随显示字符变化。
 */
inline qreal verticallyCenteredTextBaseline(const QRectF& targetRect, const QFontMetricsF& metrics)
{
    return targetRect.top() + (targetRect.height() - metrics.height()) / 2.0 + metrics.ascent();
}

/**
 * @brief Preserves Qt's integer geometry rounding for existing point-based text painters.
 * zh_CN: 为使用基线坐标绘字的现有调用保留 Qt 整数几何的取整方式。
 */
inline int verticallyCenteredTextBaseline(const QRect& targetRect, const QFontMetrics& metrics)
{
    return targetRect.top() + (targetRect.height() - metrics.height()) / 2 + metrics.ascent();
}

/**
 * @brief Encodes a target DPR for a forwarding paint device's metric override.
 * zh_CN: 将目标 DPR 编码为转发绘制设备的 metric 返回值。
 */
inline int devicePixelRatioMetric(QPaintDevice::PaintDeviceMetric metric, qreal dpr)
{
    switch (metric) {
    case QPaintDevice::PdmDevicePixelRatio:
        return qRound(dpr);
    case QPaintDevice::PdmDevicePixelRatioScaled: {
        const qreal scale = QPaintDevice::devicePixelRatioFScale();
        const int scaled = qRound(dpr * scale);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // Qt bypasses encoded metrics when the scaled value denotes exactly 1 or 2.
        // Keep nearby fractional ratios on the encoded path instead of snapping them.
        // zh_CN: Qt 对 1/2 倍跳过浮点度量；邻近的小数比例仍须读取完整精度。
        if ((scaled == scale || scaled == 2 * scale) && dpr != scaled / scale)
            return scaled + (dpr < scaled / scale ? -1 : 1);
#endif
        return scaled;
    }
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
    const qreal scale = QPaintDevice::devicePixelRatioFScale();
    const int scaled = qFloor(dpr * scale);
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
    return scaled / scale;
#else
    // QImage truncates the scaled metric; QPaintDevice checks its 1x/2x shortcuts first.
    // zh_CN: QImage 截断缩放度量，QPaintDevice 优先检查其 1/2 倍快捷路径。
    if (scaled == scale || scaled == 2 * scale)
        return scaled / scale;
    return dpr;
#endif
}

/**
 * @brief Returns the vertical translation that centers visible glyph ink in a target rectangle.
 * zh_CN: 返回将可见字形墨迹垂直居中到目标矩形所需的平移量。
 * Intended for isolated badges, not labels sharing a row with icons or other text.
 * zh_CN: 适用于独立徽标；与图标或其他文字同行的标签应使用字体行框居中。
 */
inline qreal
verticallyCenteredTextInkOffset(const QRectF& targetRect, const QFontMetricsF& metrics, const QString& text)
{
    if (!targetRect.isValid() || targetRect.isEmpty() || text.isEmpty())
        return 0.0;

    const QRectF ink = metrics.tightBoundingRect(text);
    if (!ink.isValid() || ink.isEmpty())
        return 0.0;

    // QPainter centers the shaped line, whose fallback fonts can be taller than
    // the primary font. Measure that line instead of using the primary ascent.
    // zh_CN: QPainter 居中的是塑形后的行框，回退字体可能更高；应测量该行而非仅用主字体上升部。
    const QRectF line = metrics.boundingRect(text);
    return line.center().y() - ink.center().y();
}

/**
 * @brief Returns a text rectangle translated so its visible glyph ink is vertically centered.
 * zh_CN: 返回经过垂直平移的文本矩形，使其可见字形墨迹居中。
 */
inline QRectF verticallyCenteredTextInkRect(const QRectF& targetRect, const QFontMetricsF& metrics, const QString& text)
{
    return targetRect.translated(0.0, verticallyCenteredTextInkOffset(targetRect, metrics, text));
}

/**
 * @brief Centers shaped glyph ink, including text that combines fallback fonts.
 * zh_CN: 按塑形后的字形墨迹居中，支持同一段文字混用回退字体。
 */
inline QRectF verticallyCenteredTextInkRect(const QRectF& targetRect, const QPainter& painter, const QString& text)
{
    if (!targetRect.isValid() || targetRect.isEmpty() || text.isEmpty())
        return targetRect;

    QTextLayout layout(text, painter.font(), painter.device());
    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    const QTextLine line = layout.createLine();
    layout.endLayout();
    if (!line.isValid())
        return targetRect;

    // Metrics-only tight bounds can omit descenders when a fallback run raises
    // the line ascent. Keep each shaped glyph's position and actual font.
    // zh_CN: 回退字形抬高行上升部时，纯字体度量可能漏掉下伸部；保留每个塑形字形的位置和实际字体。
    QRectF ink;
    for (const QGlyphRun& run : layout.glyphRuns()) {
        const QRawFont font = run.rawFont();
        const auto glyphs = run.glyphIndexes();
        const auto positions = run.positions();
        for (int index = 0; index < glyphs.size(); ++index) {
            const QRectF bounds = font.boundingRect(glyphs[index]).translated(positions[index]);
            if (!bounds.isEmpty())
                ink |= bounds;
        }
    }
    if (!ink.isValid() || ink.isEmpty())
        return targetRect;
    return targetRect.translated(0.0, (line.ascent() + line.descent()) / 2.0 - ink.center().y());
}

/**
 * @brief Returns the visible glyph-ink center for text drawn with vertical-center alignment.
 * zh_CN: 返回使用垂直居中对齐绘制文本时的可见字形墨迹中心。
 */
inline qreal alignedTextInkCenterY(const QRectF& alignedRect, const QFontMetricsF& metrics, const QString& text)
{
    const QRectF ink = metrics.tightBoundingRect(text);
    const QRectF line = metrics.boundingRect(text);
    const qreal baseline = line.isValid()
        ? alignedRect.center().y() - line.center().y()
        : verticallyCenteredTextBaseline(alignedRect, metrics);
    return baseline + ink.center().y();
}

/**
 * @brief Draws control text centered on the font line box, so labels in a row share one baseline.
 * zh_CN: 按字体行框垂直居中绘制控件文字，使同一行的标签共用基线。
 * The caller owns the font, pen, slot and visual horizontal alignment. Qt retains shaping,
 * font fallback and multiline behavior; this helper does not elide text or change control geometry.
 * Isolated badges that center visible ink use verticallyCenteredTextInkRect() instead.
 * zh_CN: 调用方提供字体、画笔、区域和视觉水平对齐；字形塑形、字体回退和多行行为交给 Qt，
 * 此函数不截断文字，也不改变控件布局。按可见墨迹居中的独立徽标改用 verticallyCenteredTextInkRect()。
 */
inline void drawVerticallyCenteredText(QPainter& painter,
    const QRectF& targetRect,
    const QString& text,
    Qt::Alignment horizontalAlignment = Qt::AlignLeft)
{
    if (targetRect.isEmpty() || text.isEmpty())
        return;
    painter.drawText(targetRect, (horizontalAlignment & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter, text);
}

} // namespace fluent::painting

#endif // FLUENTTEXTPAINTCOMPAT_H
