#ifndef FLUENTSURFACEPAINTER_P_H
#define FLUENTSURFACEPAINTER_P_H

#include <QColor>
#include <QPainter>

#include "components/foundation/private/DpiPaintMetrics_p.h"

namespace fluent::painting {

struct RoundedSurfacePaint {
    QColor fill;
    QColor border;
    qreal borderWidth = 1.0;
    qreal radius = 0.0;
};

inline void paintRoundedSurface(QPainter& painter,
                                const QRectF& outerRect,
                                const RoundedSurfacePaint& surface)
{
    const bool hasFill = surface.fill.isValid() && surface.fill.alpha() > 0;
    const bool hasBorder = surface.border.isValid() && surface.border.alpha() > 0
        && surface.borderWidth > 0.0;
    if ((!hasFill && !hasBorder) || !outerRect.isValid() || outerRect.isEmpty())
        return;

    const DpiPaintMetrics metrics(painter);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, surface.radius > 0.0);

    if (hasBorder) {
        const DeviceAlignedStroke stroke = metrics.alignedStroke(
            outerRect, surface.borderWidth);
        painter.setPen(QPen(surface.border, stroke.width));
        painter.setBrush(hasFill ? QBrush(surface.fill) : Qt::NoBrush);
        painter.drawRoundedRect(stroke.rect, surface.radius, surface.radius);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(surface.fill);
        painter.drawRoundedRect(metrics.alignedOuterRect(outerRect),
                                surface.radius,
                                surface.radius);
    }

    painter.restore();
}

} // namespace fluent::painting

#endif // FLUENTSURFACEPAINTER_P_H
