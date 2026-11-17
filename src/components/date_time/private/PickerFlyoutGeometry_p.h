#ifndef FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"

namespace fluent::date_time::detail {

struct PickerFieldColors {
    QColor background;
    QColor border = Qt::transparent;
    QColor divider;
    QColor text;
};

// Resolve field background, border, segment divider, and text colors for DatePicker and TimePicker.
// zh_CN: 为 DatePicker 和 TimePicker 解析字段背景、边框、分隔线与文字颜色。
inline PickerFieldColors resolvePickerFieldColors(fluent::basicinput::Button::ButtonStyle style,
    const fluent::FluentElement::Colors& colors,
    bool enabled,
    bool hovered,
    bool pressed)
{
    PickerFieldColors resolved;
    if (style == fluent::basicinput::Button::Accent) {
        if (!enabled) {
            resolved.background = colors.controlDisabled;
            resolved.border = colors.strokeDivider;
            resolved.divider = colors.strokeDivider;
            resolved.text = colors.textDisabled;
        } else {
            resolved.background = pressed ? colors.accentTertiary
                : hovered
                ? colors.accentSecondary
                : colors.accentDefault;
            resolved.border = pressed ? Qt::transparent : colors.strokeStrong;
            resolved.divider = colors.textOnAccent;
            resolved.divider.setAlpha(64);
            resolved.text = colors.textOnAccent;
        }
        return resolved;
    }

    if (style == fluent::basicinput::Button::Subtle) {
        resolved.divider = colors.strokeDivider;
        if (!enabled) {
            resolved.background = Qt::transparent;
            resolved.text = colors.textDisabled;
        } else {
            resolved.background = pressed ? colors.subtleTertiary : hovered ? colors.subtleSecondary : Qt::transparent;
            resolved.text = (hovered || pressed) ? colors.textPrimary : colors.textSecondary;
        }
        return resolved;
    }

    resolved.divider = colors.strokeDivider;
    if (!enabled) {
        resolved.background = colors.controlDisabled;
        resolved.text = colors.textDisabled;
    } else {
        resolved.background = pressed ? colors.subtleTertiary
            : hovered
            ? colors.subtleSecondary
            : colors.controlDefault;
        resolved.text = (hovered || pressed) ? colors.textPrimary : colors.textSecondary;
    }
    return resolved;
}

// Keep the active wheel row on the closed field's visual axis. When the card
// cannot fit at that position, containment takes precedence over alignment.
// zh_CN: 让滚轮当前行与收起字段保持同轴；空间不足时优先保证卡片完整可见。
inline QPoint alignedWheelFlyoutPosition(const QWidget* owner,
    const QSize& outerSize,
    int shadowMargin,
    int selectedRowCenterY,
    bool clampToWindow,
    int windowMargin = 4)
{
    QWidget* top = overlay::presentedTopLevel(owner);
    if (!top)
        return QPoint();

    const QRect anchorRect = overlay::presentedRectInTopLevel(owner);
    const QSize cardSize = overlay::visibleCardSize(outerSize, shadowMargin);
    QPoint cardTopLeft(anchorRect.left(), anchorRect.center().y() - selectedRowCenterY);
    if (clampToWindow) {
        cardTopLeft = overlay::clampCardTopLeft(cardTopLeft, cardSize, overlay::overlaySurfaceRect(top), windowMargin);
    }
    return overlay::outerTopLeftForVisibleCard(cardTopLeft, shadowMargin);
}

} // namespace fluent::date_time::detail

#endif // FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H
