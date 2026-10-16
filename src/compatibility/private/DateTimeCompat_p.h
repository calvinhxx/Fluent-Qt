#pragma once

#include <QDateTime>
#if QT_CONFIG(timezone)
#include <QTimeZone>
#endif

namespace compatibility::detail {

inline bool dateTimeZonesEqual(const QDateTime& left, const QDateTime& right)
{
#if QT_CONFIG(timezone)
    return left.timeZone() == right.timeZone();
#else
    return left.offsetFromUtc() == right.offsetFromUtc();
#endif
}

} // namespace compatibility::detail
