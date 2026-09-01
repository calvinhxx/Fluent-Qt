#pragma once

#include <QPainterPath>
#include <QRect>
#include <QVector>

#include "design/CornerRadius.h"

namespace fluent::navigation::detail {

constexpr int kSelectedTabTopRadius = CornerRadius::Overlay;
constexpr int kSelectedTabShoulderWidth = CornerRadius::Overlay + CornerRadius::Control;
constexpr int kSelectedTabShoulderHeight = CornerRadius::Overlay;

struct TabSeparatorGeometryItem {
    QRect rect;
    bool selected = false;
    bool filled = false;
};

inline QPainterPath selectedTabPath(const QRect& rect)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;

    constexpr qreal quarterCurveControl = 0.5522847498;
    const int topRadius = qMin(kSelectedTabTopRadius, qMin(rect.width(), rect.height()) / 2);
    const int shoulderWidth = qMin(kSelectedTabShoulderWidth, rect.width() / 2);
    const int shoulderHeight = qMin(kSelectedTabShoulderHeight, rect.height() / 2);
    const qreal left = rect.left();
    const qreal right = rect.right();
    const qreal top = rect.top();
    const qreal bottom = rect.bottom() + 1.0;

    path.moveTo(left - shoulderWidth, bottom);
    path.cubicTo(left - shoulderWidth * (1.0 - quarterCurveControl), bottom, left,
                 bottom - shoulderHeight * (1.0 - quarterCurveControl), left,
                 bottom - shoulderHeight);
    path.lineTo(left, top + topRadius);
    path.quadTo(left, top, left + topRadius, top);
    path.lineTo(right - topRadius, top);
    path.quadTo(right, top, right, top + topRadius);
    path.lineTo(right, bottom - shoulderHeight);
    path.cubicTo(right, bottom - shoulderHeight * (1.0 - quarterCurveControl),
                 right + shoulderWidth * (1.0 - quarterCurveControl), bottom, right + shoulderWidth,
                 bottom);
    path.closeSubpath();
    return path;
}

inline QVector<QRect> tabSeparatorRects(const QVector<TabSeparatorGeometryItem>& items,
                                        bool dragActive)
{
    QVector<QRect> separators;
    if (dragActive)
        return separators;

    constexpr int separatorHeight = 16;
    for (int index = 0; index + 1 < items.size(); ++index) {
        const TabSeparatorGeometryItem& first = items.at(index);
        const TabSeparatorGeometryItem& second = items.at(index + 1);
        if (first.selected || second.selected || first.filled || second.filled)
            continue;
        if (first.rect.isEmpty() || second.rect.isEmpty())
            continue;

        const QRect& left = first.rect.left() < second.rect.left() ? first.rect : second.rect;
        const QRect& right = first.rect.left() < second.rect.left() ? second.rect : first.rect;
        const int height = qMin(separatorHeight, qMin(left.height(), right.height()));
        const int top =
            qMax(left.top(), right.top()) + (qMin(left.height(), right.height()) - height) / 2;
        separators.append(QRect(right.left(), top, 1, height));
    }
    return separators;
}

} // namespace fluent::navigation::detail
