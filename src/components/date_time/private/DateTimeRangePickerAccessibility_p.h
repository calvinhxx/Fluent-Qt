#ifndef FLUENTQT_DATE_TIME_PRIVATE_DATETIMERANGEPICKERACCESSIBILITY_P_H
#define FLUENTQT_DATE_TIME_PRIVATE_DATETIMERANGEPICKERACCESSIBILITY_P_H

namespace fluent::date_time {
class DateTimeRangePicker;
namespace detail {
void ensureDateTimeRangePickerAccessibilityFactory();
void notifyDateTimeRangePickerValueChanged(DateTimeRangePicker* picker, bool validityChanged);
void notifyDateTimeRangePickerDescriptionChanged(DateTimeRangePicker* picker);
void notifyDateTimeRangePickerOpenChanged(DateTimeRangePicker* picker);
} // namespace detail
} // namespace fluent::date_time

#endif
