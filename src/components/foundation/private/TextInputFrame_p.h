#pragma once

#include <QPainter>
#include <QPainterPath>
#include "components/foundation/FluentElement.h"
#include "DpiPaintMetrics_p.h"

namespace fluent::detail {

struct TextInputFrameState {
    bool enabled = true;
    bool readOnly = false;
    bool focused = false;
    bool hovered = false;
    bool pressed = false;
    bool dark = false;
    int normalWidth = 1;
    int focusedWidth = 2;
};

// Share field colors and physical-pixel stroke alignment; each editor owns its
// bounds, interaction state, text and header layout.
// zh_CN: 统一字段颜色和物理像素描边对齐；边界、交互状态、文本与标题仍由各编辑器负责。
inline void paintTextInputFrame(QPainter& painter,
    const QRectF& bounds,
    const FluentElement::Colors& colors,
    qreal radius,
    const TextInputFrameState& state)
{
    const painting::DpiPaintMetrics metrics(painter);
    const auto border = metrics.alignedStroke(bounds, 1.0);
    QColor background, stroke, bottom;
    int bottomWidth = state.normalWidth;
    if (!state.enabled) {
        background = colors.controlDisabled;
        stroke = bottom = colors.strokeDivider;
    } else if (state.readOnly) {
        background = colors.controlAltSecondary;
        stroke = colors.strokeDefault;
        bottom = colors.strokeDivider;
    } else if (state.focused) {
        background = state.dark ? colors.bgSolid : colors.controlDefault;
        stroke = colors.strokeSecondary;
        bottom = colors.accentDefault;
        bottomWidth = state.focusedWidth;
    } else if (state.pressed || state.hovered) {
        background = state.pressed ? colors.controlTertiary : colors.controlSecondary;
        stroke = bottom = colors.strokeSecondary;
    } else {
        background = colors.controlDefault;
        stroke = colors.strokeDefault;
        bottom = colors.strokeDivider;
    }
    QPainterPath frame;
    frame.addRoundedRect(border.rect, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawPath(frame);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(stroke, border.width));
    painter.drawPath(frame);
    if (state.enabled && !state.readOnly) {
        const auto underline = metrics.alignedStroke(bounds, bottomWidth);
        QPen pen(bottom, underline.width);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        QPainterPath line;
        line.moveTo(border.rect.left() + radius, underline.rect.bottom());
        line.lineTo(border.rect.right() - radius, underline.rect.bottom());
        painter.drawPath(line);
    }
}

} // namespace fluent::detail
