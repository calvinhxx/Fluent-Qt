#ifndef FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

#include "components/foundation/overlay/OverlayGeometry.h"

namespace fluent::date_time::detail {

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
    QWidget* top = owner ? owner->window() : nullptr;
    if (!top)
        return QPoint();

    const QRect anchorRect(owner->mapTo(top, QPoint(0, 0)), owner->size());
    const QSize cardSize = overlay::visibleCardSize(outerSize, shadowMargin);
    QPoint cardTopLeft(anchorRect.left(),
                       anchorRect.center().y() - selectedRowCenterY);
    if (clampToWindow) {
        cardTopLeft = overlay::clampCardTopLeft(
            cardTopLeft,
            cardSize,
            overlay::overlaySurfaceRect(top),
            windowMargin);
    }
    return overlay::outerTopLeftForVisibleCard(cardTopLeft, shadowMargin);
}

} // namespace fluent::date_time::detail

#endif // FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERFLYOUTGEOMETRY_P_H
