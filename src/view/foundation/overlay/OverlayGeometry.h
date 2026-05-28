#ifndef OVERLAYGEOMETRY_H
#define OVERLAYGEOMETRY_H

#include <QMargins>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QSize>

#include "design/Spacing.h"

namespace view::overlay {

constexpr int defaultShadowMargin()
{
    return ::Spacing::Standard;
}

inline QMargins uniformShadowMargins(int shadowMargin = defaultShadowMargin())
{
    return QMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
}

inline QSize visibleCardSize(const QSize& outerSize, int shadowMargin = defaultShadowMargin())
{
    return QSize(qMax(0, outerSize.width() - shadowMargin * 2),
                 qMax(0, outerSize.height() - shadowMargin * 2));
}

inline QSize outerSizeForVisibleCard(const QSize& visibleSize, int shadowMargin = defaultShadowMargin())
{
    return QSize(visibleSize.width() + shadowMargin * 2,
                 visibleSize.height() + shadowMargin * 2);
}

inline QRect visibleCardRect(const QRect& outerRect, int shadowMargin = defaultShadowMargin())
{
    return outerRect.marginsRemoved(uniformShadowMargins(shadowMargin));
}

inline QRect visibleCardGeometry(const QRect& outerGeometry, int shadowMargin = defaultShadowMargin())
{
    return outerGeometry.marginsRemoved(uniformShadowMargins(shadowMargin));
}

inline QPoint outerTopLeftForVisibleCard(const QPoint& cardTopLeft, int shadowMargin = defaultShadowMargin())
{
    return cardTopLeft - QPoint(shadowMargin, shadowMargin);
}

inline QRect outerGeometryForVisibleCard(const QRect& cardGeometry, int shadowMargin = defaultShadowMargin())
{
    return QRect(outerTopLeftForVisibleCard(cardGeometry.topLeft(), shadowMargin),
                 outerSizeForVisibleCard(cardGeometry.size(), shadowMargin));
}

inline bool visibleCardContains(const QRect& outerRect, const QPoint& localPoint,
                                int shadowMargin = defaultShadowMargin())
{
    return visibleCardRect(outerRect, shadowMargin).contains(localPoint);
}

inline QPoint clampCardTopLeft(const QPoint& cardTopLeft, const QSize& cardSize,
                               const QRect& bounds, int windowMargin)
{
    if (bounds.isEmpty())
        return cardTopLeft;

    const int maxX = bounds.right() - cardSize.width() + 1 - windowMargin;
    const int maxY = bounds.bottom() - cardSize.height() + 1 - windowMargin;
    return QPoint(qBound(bounds.left() + windowMargin, cardTopLeft.x(), maxX),
                  qBound(bounds.top() + windowMargin, cardTopLeft.y(), maxY));
}

inline QPainterPath roundedRectPath(const QRectF& rect, qreal radius)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;
    if (radius <= 0.0) {
        path.addRect(rect);
        return path;
    }
    path.addRoundedRect(rect, radius, radius);
    return path;
}

inline QPainterPath roundedCornerRectPath(const QRectF& rect, qreal radius,
                                          bool roundTopLeft, bool roundTopRight,
                                          bool roundBottomRight, bool roundBottomLeft)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;

    radius = qMin<qreal>(radius, qMin(rect.width(), rect.height()) / 2.0);
    if (radius <= 0.0) {
        path.addRect(rect);
        return path;
    }

    const qreal left = rect.left();
    const qreal top = rect.top();
    const qreal right = rect.right();
    const qreal bottom = rect.bottom();
    const qreal diameter = radius * 2.0;

    path.moveTo(roundTopLeft ? QPointF(left + radius, top) : QPointF(left, top));

    if (roundTopRight) {
        path.lineTo(right - radius, top);
        path.arcTo(QRectF(right - diameter, top, diameter, diameter), 90, -90);
    } else {
        path.lineTo(right, top);
    }

    if (roundBottomRight) {
        path.lineTo(right, bottom - radius);
        path.arcTo(QRectF(right - diameter, bottom - diameter, diameter, diameter), 0, -90);
    } else {
        path.lineTo(right, bottom);
    }

    if (roundBottomLeft) {
        path.lineTo(left + radius, bottom);
        path.arcTo(QRectF(left, bottom - diameter, diameter, diameter), 270, -90);
    } else {
        path.lineTo(left, bottom);
    }

    if (roundTopLeft) {
        path.lineTo(left, top + radius);
        path.arcTo(QRectF(left, top, diameter, diameter), 180, -90);
    } else {
        path.lineTo(left, top);
    }

    path.closeSubpath();
    return path;
}

} // namespace view::overlay

#endif // OVERLAYGEOMETRY_H