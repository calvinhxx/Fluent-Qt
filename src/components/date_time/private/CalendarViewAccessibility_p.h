#ifndef FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_CALENDARVIEWACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_CALENDARVIEWACCESSIBILITY_P_H

namespace fluent::date_time {

class CalendarView;

namespace detail {

// Installs the private CalendarView accessibility factory once per process.
// The adapter remains an implementation detail and is not installed as API.
void ensureCalendarViewAccessibilityFactory();

// Notify assistive technology only after real CalendarView state changes.
void notifyCalendarViewAccessibilityReset(CalendarView* view);
void notifyCalendarViewAccessibilitySelection(CalendarView* view);
void notifyCalendarViewAccessibilityFocus(CalendarView* view);

} // namespace detail
} // namespace fluent::date_time

#endif // FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_CALENDARVIEWACCESSIBILITY_P_H
