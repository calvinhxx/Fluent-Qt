#ifndef OVERLAYGEOMETRY_H
#define OVERLAYGEOMETRY_H

#include <QEvent>
#include <QMargins>
#include <QObject>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

#include "design/Spacing.h"

namespace fluent::overlay {

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

/**
 * @brief Returns whether an application event can change an anchor's mapped geometry.
 * zh_CN: 判断应用事件是否可能改变锚点映射后的几何位置。
 */
inline bool anchorGeometryMayChange(QObject* watched, QEvent* event, QWidget* anchor)
{
    if (!event || !anchor)
        return false;
    if (event->type() == QEvent::Wheel)
        return true;

    auto* changedWidget = qobject_cast<QWidget*>(watched);
    if (!changedWidget
        || (changedWidget != anchor && !changedWidget->isAncestorOf(anchor))) {
        return false;
    }

    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ParentChange:
    case QEvent::LayoutRequest:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Returns whether any part of an anchor remains visible through its ancestor chain.
 * zh_CN: 判断锚点经过祖先裁剪后是否仍有可见区域。
 */
inline bool isAnchorVisibleInTopLevel(QWidget* anchor)
{
    if (!anchor)
        return false;
    QWidget* top = anchor->window();
    if (!top || !anchor->isVisibleTo(top))
        return false;

    QRect visibleRect(anchor->mapTo(top, QPoint(0, 0)), anchor->size());
    for (QWidget* ancestor = anchor->parentWidget(); ancestor;
         ancestor = ancestor->parentWidget()) {
        if (!ancestor->isVisibleTo(top))
            return false;
        const QRect ancestorRect(ancestor->mapTo(top, QPoint(0, 0)), ancestor->size());
        visibleRect = visibleRect.intersected(ancestorRect);
        if (visibleRect.isEmpty())
            return false;
        if (ancestor == top)
            break;
    }
    return !visibleRect.isEmpty();
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

} // namespace fluent::overlay

#endif // OVERLAYGEOMETRY_H
