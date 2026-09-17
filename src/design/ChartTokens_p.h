#pragma once

#include <QColor>
#include <QVector>

// Chart-only semantic roles from the approved Charts study. Existing chart
// palette users (for example Avatar) retain their published theme behavior.
namespace fluent::charts::tokens {
inline QVector<QColor> series(bool dark, const QColor& accent)
{
    return dark ? QVector<QColor>{accent, QColor("#5DD3C8"), QColor("#C7A6EE"), QColor("#F5BE65")}
                : QVector<QColor>{accent, QColor("#008084"), QColor("#7F56AB"), QColor("#A85D00")};
}
inline QColor grid(bool dark)
{
    return QColor(dark ? "#3B3B3B" : "#ECECEC");
}
inline QColor border(bool dark)
{
    return QColor(dark ? "#404040" : "#E5E5E5");
}
inline QColor baseline(bool dark)
{
    return QColor(dark ? "#686868" : "#D1D1D1");
}
} // namespace fluent::charts::tokens
