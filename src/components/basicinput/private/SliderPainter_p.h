#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_SLIDERPAINTER_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_SLIDERPAINTER_P_H

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "components/foundation/FluentElement.h"

class QPainter;

namespace fluent::basicinput::detail {

// Visual state of one slider thumb; ratios are animation progress in [0, 1].
// zh_CN: 单个滑块手柄的视觉状态；比例为 [0, 1] 的动画进度。
struct SliderThumbVisual {
    QPointF center;
    qreal diameter = 20.0;
    qreal hoverRatio = 0.0;
    qreal pressRatio = 0.0;
    bool keyboardFocused = false;
    bool enabled = true;
};

// Track and value-fill colors shared by Slider and RangeSlider.
// zh_CN: Slider 与 RangeSlider 共用的轨道与数值填充颜色。
QColor sliderTrackColor(const FluentElement::Colors& colors, bool enabled);
QColor sliderFillColor(const FluentElement::Colors& colors, bool enabled);
QColor sliderTickColor(const FluentElement::Colors& colors);

// Paints a fully rounded bar; empty rectangles paint nothing.
// zh_CN: 绘制全圆角条；空矩形不绘制。
void paintSliderBar(QPainter& painter, const QRectF& rect, const QColor& color);

// Paints the Fluent thumb: solid outer circle with a ring and an accent inner dot
// that grows from 0.45 (rest) to 0.7 (hover) of the radius.
// zh_CN: 绘制 Fluent 手柄：带细环的实心外圆，以及半径从 0.45（静止）增长到 0.7（悬停）的强调色内圆。
void paintSliderThumb(QPainter& painter, const FluentElement::Colors& colors, const SliderThumbVisual& thumb);

// Tick values using QSlider's automatic interval fallback. Returns no ticks when
// there would be 100 or more, matching the existing Slider density limit.
// zh_CN: 使用 QSlider 的自动间隔回退规则计算刻度值；刻度数达到 100 及以上时返回空列表，
// 与现有 Slider 的密度上限一致。
QVector<int> sliderTickValues(int minimum, int maximum, int tickInterval, int singleStep, int pageStep);

} // namespace fluent::basicinput::detail

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_SLIDERPAINTER_P_H
