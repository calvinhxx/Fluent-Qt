#include "DateTimeSamples.h"

#include <QDate>
#include <QTime>

#include "components/date_time/CalendarDatePicker.h"
#include "components/date_time/CalendarView.h"
#include "components/date_time/DatePicker.h"
#include "components/date_time/TimePicker.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::date_time::CalendarDatePicker;
using fluent::date_time::CalendarView;
using fluent::date_time::DatePicker;
using fluent::date_time::TimePicker;
using samples::makeSample;

QVector<GallerySample> calendarDatePickerSamples()
{
    return {
        makeSample(QStringLiteral("calendar-date-picker-basic"),
                   QStringLiteral("Pick a date from a drop-down calendar"),
                   QStringLiteral("The button opens a calendar flyout; the picked date becomes the label."),
                   QStringLiteral("auto* picker = new CalendarDatePicker(this);\n"
                                  "picker->setPlaceholderText(\"Pick a date\");\n"
                                  "connect(picker, &CalendarDatePicker::dateChanged,\n"
                                  "        this, [](const QDate& date) { /* use date */ });"),
                   [](QWidget* parent) {
                       auto* picker = new CalendarDatePicker(parent);
                       picker->setPlaceholderText(QStringLiteral("Pick a date"));
                       return picker;
                   })
    };
}

QVector<GallerySample> calendarViewSamples()
{
    return {
        makeSample(QStringLiteral("calendar-view-basic"),
                   QStringLiteral("Browse and select dates"),
                   QStringLiteral("Click the title to zoom out to months and years."),
                   QStringLiteral("auto* calendar = new CalendarView(this);\n"
                                  "calendar->setSelectedDate(QDate::currentDate());"),
                   [](QWidget* parent) {
                       auto* calendar = new CalendarView(parent);
                       calendar->setSelectedDate(QDate::currentDate());
                       return calendar;
                   })
    };
}

QVector<GallerySample> datePickerSamples()
{
    return {
        makeSample(QStringLiteral("date-picker-basic"),
                   QStringLiteral("Spinning-field date picker"),
                   QStringLiteral("Clicking opens looping day, month, and year columns."),
                   QStringLiteral("auto* picker = new DatePicker(this);\n"
                                  "picker->setDate(QDate::currentDate());"),
                   [](QWidget* parent) {
                       auto* picker = new DatePicker(parent);
                       picker->setDate(QDate::currentDate());
                       return picker;
                   })
    };
}

QVector<GallerySample> timePickerSamples()
{
    return {
        makeSample(QStringLiteral("time-picker-basic"),
                   QStringLiteral("Spinning-field time picker"),
                   QStringLiteral("Clicking opens looping hour and minute columns."),
                   QStringLiteral("auto* picker = new TimePicker(this);\n"
                                  "picker->setTime(QTime(9, 30));\n"
                                  "picker->setMinuteIncrement(5);"),
                   [](QWidget* parent) {
                       auto* picker = new TimePicker(parent);
                       picker->setTime(QTime(9, 30));
                       picker->setMinuteIncrement(5);
                       return picker;
                   })
    };
}

} // namespace

QVector<GallerySample> dateTimeSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("calendar-date-picker"))
        return calendarDatePickerSamples();
    if (routeId == QStringLiteral("calendar-view"))
        return calendarViewSamples();
    if (routeId == QStringLiteral("date-picker"))
        return datePickerSamples();
    if (routeId == QStringLiteral("time-picker"))
        return timePickerSamples();
    return {};
}

} // namespace fluent::gallery
