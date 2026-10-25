#pragma once

#include <QEvent>

#include "OverlayGeometry.h"
#include "OverlayPresentation_p.h"

namespace fluent::overlay {

inline bool fieldPopupAnchorGeometryMayChange(QObject* watched, QEvent* event, QWidget* anchor, const QWidget* popup)
{
    const auto* widget = qobject_cast<QWidget*>(watched);
    if (event && event->type() == QEvent::Wheel && widget && (widget == popup || popup->isAncestorOf(widget)))
        return false;
    return anchorGeometryMayChange(watched, event, anchor);
}

// Field dropdowns share leading-edge alignment and visible-card spacing.
// General flyouts retain their independently chosen centered placements.
// zh_CN: 输入类下拉框共用起始边对齐及可见卡片间距，通用 Flyout 保留自身位置语义。
struct FieldPopupSpace {
    QRect bounds;
    int below = 0;
    int above = 0;

    bool opensAbove(int desiredHeight) const { return below < desiredHeight && above > below; }
    int capacity(int desiredHeight) const { return opensAbove(desiredHeight) ? above : below; }
    int boundedWidth(int desiredWidth) const { return qMax(1, qMin(desiredWidth, bounds.width())); }
};

inline FieldPopupSpace fieldPopupSpace(const QRect& anchor,
    const QRect& surface,
    int gap = ::Spacing::Gap::Normal,
    int margin = ::Spacing::XSmall)
{
    const QRect bounds = surface.adjusted(margin, margin, -margin, -margin);
    return {bounds, qMax(0, bounds.bottom() - anchor.bottom() - gap), qMax(0, anchor.top() - gap - bounds.top())};
}

inline QPoint fieldPopupCardTopLeft(const QRect& anchor,
    const QSize& cardSize,
    const QRect& surface,
    Qt::LayoutDirection direction,
    bool above,
    int gap = ::Spacing::Gap::Normal,
    int margin = ::Spacing::XSmall,
    bool clamp = true)
{
    const int x = direction == Qt::RightToLeft ? anchor.right() + 1 - cardSize.width() : anchor.left();
    const int y = above ? anchor.top() - gap - cardSize.height() : anchor.bottom() + 1 + gap;
    return clamp ? clampCardTopLeft(QPoint(x, y), cardSize, surface, margin) : QPoint(x, y);
}

} // namespace fluent::overlay
