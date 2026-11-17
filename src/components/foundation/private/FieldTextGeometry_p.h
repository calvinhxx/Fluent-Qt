#pragma once

#include <QMargins>
#include <QRect>
#include <QStyle>

namespace fluent::detail {

// Margins specify logical leading/trailing text edges; native editors handle internal padding
// and trailing adornments reserve their own space.
inline QRect fieldTextRect(const QRect& bounds, Qt::LayoutDirection direction, const QMargins& margins)
{
    return QStyle::visualRect(direction, bounds, bounds.marginsRemoved(margins));
}

} // namespace fluent::detail
