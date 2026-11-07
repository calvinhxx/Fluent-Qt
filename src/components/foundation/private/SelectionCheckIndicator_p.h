#pragma once

#include <QPainter>
#include "components/foundation/FluentElement.h"
#include "design/Typography.h"

namespace fluent::detail {

// Shared by model-backed multi-selection surfaces; no widget is retained per row.
// zh_CN: 模型驱动多选表面共用，不为每一行保留独立控件。
inline void paintSelectionCheckIndicator(QPainter& painter,
    const QRect& rect,
    const FluentElement::Colors& colors,
    qreal radius,
    bool selected,
    bool hovered,
    bool enabled)
{
    const QColor fill = !enabled ? colors.controlDisabled
        : selected
        ? (hovered ? colors.accentSecondary : colors.accentDefault)
        : (hovered ? colors.controlSecondary : colors.controlDefault);
    const QColor stroke = !enabled ? colors.strokeDivider
        : selected
        ? QColor(Qt::transparent)
        : (hovered ? colors.strokeStrong : colors.strokeDefault);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(rect, radius, radius);
    if (stroke != Qt::transparent) {
        painter.setPen(QPen(stroke, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }
    if (selected) {
        painter.setPen(enabled ? colors.textOnAccent : colors.textDisabled);
        Typography::Icons::paintGlyph(painter, QRectF(rect), Typography::Icons::CheckMark,
            Typography::IconSize::Compact, Qt::AlignCenter);
    }
}

} // namespace fluent::detail
