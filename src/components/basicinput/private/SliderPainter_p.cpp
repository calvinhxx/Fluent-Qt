#include "SliderPainter_p.h"

#include <QPainter>
#include <QPen>

namespace fluent::basicinput::detail {

QColor sliderTrackColor(const FluentElement::Colors& colors, bool enabled)
{
    // WinUI paints the unfilled track with the strong control fill so the
    // selected portion stays readable against it.
    // zh_CN: WinUI 用强控件填充绘制未填充轨道，使已选部分始终可辨。
    return enabled ? colors.strokeStrong : colors.textDisabled;
}

QColor sliderFillColor(const FluentElement::Colors& colors, bool enabled)
{
    return enabled ? colors.accentDefault : colors.accentDisabled;
}

QColor sliderTickColor(const FluentElement::Colors& colors)
{
    return colors.textSecondary;
}

void paintSliderBar(QPainter& painter, const QRectF& rect, const QColor& color)
{
    if (rect.width() <= 0.0 || rect.height() <= 0.0)
        return;
    const qreal radius = qMin(rect.width(), rect.height()) / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(rect, radius, radius);
}

void paintSliderThumb(QPainter& painter, const FluentElement::Colors& colors, const SliderThumbVisual& thumb)
{
    const qreal radius = thumb.diameter / 2.0;
    const qreal innerRadius = radius * (0.45 + 0.25 * thumb.hoverRatio);

    // The outer circle uses the solid background so it masks the track behind it;
    // the ring and inner dot carry the state.
    // zh_CN: 外圆使用实色背景遮住其后的轨道；细环与内圆表达状态。
    QColor ringColor = colors.strokeStrong;
    QColor innerColor = colors.accentDefault;
    if (!thumb.enabled) {
        innerColor = colors.textDisabled;
        ringColor = colors.strokeDivider;
    } else if (thumb.pressRatio > 0.5) {
        innerColor = colors.accentTertiary;
    } else if (thumb.hoverRatio > 0.5) {
        innerColor = colors.accentSecondary;
    }

    const bool focusRing = thumb.enabled && thumb.keyboardFocused;
    painter.setBrush(colors.bgSolid);
    painter.setPen(QPen(focusRing ? colors.accentDefault : ringColor, focusRing ? 2 : 1));
    painter.drawEllipse(thumb.center, radius, radius);

    painter.setBrush(innerColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(thumb.center, innerRadius, innerRadius);
}

QVector<int> sliderTickValues(int minimum, int maximum, int tickInterval, int singleStep, int pageStep)
{
    QVector<int> values;
    const qint64 range = static_cast<qint64>(maximum) - minimum;
    qint64 interval = tickInterval;
    if (interval <= 0) {
        interval = qMax(1, singleStep);
        if (range / interval >= 100 && pageStep > 0)
            interval = pageStep;
    }
    const qint64 steps = range / interval;
    if (steps <= 0 || steps >= 100)
        return values;
    values.reserve(static_cast<int>(steps) + 1);
    for (qint64 i = 0; i <= steps; ++i)
        values.append(static_cast<int>(static_cast<qint64>(minimum) + i * interval));
    return values;
}

} // namespace fluent::basicinput::detail
