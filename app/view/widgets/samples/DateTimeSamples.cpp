#include "DateTimeSamples.h"

#include <QDate>
#include <QTime>

#include "components/date_time/CalendarDatePicker.h"
#include "components/date_time/CalendarView.h"
#include "components/date_time/DatePicker.h"
#include "components/date_time/DateTimeRangePicker.h"
#include "components/date_time/TimePicker.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::date_time::CalendarDatePicker;
using fluent::date_time::CalendarView;
using fluent::date_time::DatePicker;
using fluent::date_time::DateTimeRangePicker;
using fluent::date_time::TimePicker;
using samples::makeSample;

QVector<GallerySample> calendarDatePickerSamples()
{
    return {makeSample(
        QStringLiteral("calendar-date-picker-basic"),
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
        })};
}

QVector<GallerySample> calendarViewSamples()
{
    return {makeSample(QStringLiteral("calendar-view-basic"),
                       QStringLiteral("Browse and select dates"),
                       QStringLiteral("Click the title to zoom out to months and years."),
                       QStringLiteral("auto* calendar = new CalendarView(this);\n"
                                      "calendar->setSelectedDate(QDate::currentDate());"),
                       [](QWidget* parent) {
                           auto* calendar = new CalendarView(parent);
                           calendar->setSelectedDate(QDate::currentDate());
                           return calendar;
                       })};
}

QVector<GallerySample> datePickerSamples()
{
    return {makeSample(
        QStringLiteral("date-picker-basic"), QStringLiteral("Spinning-field date picker"),
        QStringLiteral("Clicking opens looping day, month, and year columns."),
        QStringLiteral("auto* picker = new DatePicker(this);\n"
                       "picker->setPlaceholderText(DatePicker::DateField::Month, \"month\");\n"
                       "picker->setPlaceholderText(DatePicker::DateField::Day, \"day\");\n"
                       "picker->setPlaceholderText(DatePicker::DateField::Year, \"year\");\n"
                       "picker->setConfirmButtonAccessibleName(\"Accept date\");\n"
                       "picker->setCancelButtonAccessibleName(\"Cancel date\");\n"
                       "picker->setDate(QDate::currentDate());"),
        [](QWidget* parent) {
            auto* picker = new DatePicker(parent);
            picker->setPlaceholderText(DatePicker::DateField::Month, QStringLiteral("month"));
            picker->setPlaceholderText(DatePicker::DateField::Day, QStringLiteral("day"));
            picker->setPlaceholderText(DatePicker::DateField::Year, QStringLiteral("year"));
            picker->setConfirmButtonAccessibleName(QStringLiteral("Accept date"));
            picker->setCancelButtonAccessibleName(QStringLiteral("Cancel date"));
            picker->setDate(QDate::currentDate());
            return picker;
        })};
}

QVector<GallerySample> dateTimeRangePickerSamples()
{
    return {makeSample(
                QStringLiteral("date-time-range-dates"), QStringLiteral("Filter by a date range"),
                QStringLiteral("Open the compact entry, select two dates, or choose a whole-day "
                               "preset. Query inclusion belongs to the application."),
                QStringLiteral("auto* picker = new DateTimeRangePicker(this);\n"
                               "picker->setStartLabelText(\"Start\");\n"
                               "picker->setEndLabelText(\"End\");\n"
                               "picker->setTimeEnabled(false);\n"
                               "picker->setRange(QDateTime(QDate(2025, 10, 1), QTime(0, 0)),\n"
                               "                 QDateTime(QDate(2025, 11, 1), QTime(0, 0)));"),
                [](QWidget* parent) {
                    auto* picker = new DateTimeRangePicker(parent);
                    picker->setStartLabelText(QStringLiteral("Start"));
                    picker->setEndLabelText(QStringLiteral("End"));
                    picker->setTimeEnabled(false);
                    picker->setRange(QDateTime(QDate(2025, 10, 1), QTime(0, 0)),
                                     QDateTime(QDate(2025, 11, 1), QTime(0, 0)));
                    return picker;
                },
                true),
            makeSample(
                QStringLiteral("date-time-range-time"), QStringLiteral("Select dates and times"),
                QStringLiteral("Pick an endpoint in its compact flyout, or select a range on the "
                               "calendar. Edit times in place."),
                QStringLiteral("auto* picker = new DateTimeRangePicker(this);\n"
                               "picker->setStartLabelText(\"Start\");\n"
                               "picker->setEndLabelText(\"End\");\n"
                               "picker->setRange(QDateTime(QDate(2025, 10, 1), QTime(10, 10)),\n"
                               "                 QDateTime(QDate(2025, 10, 3), QTime(15, 10)));"),
                [](QWidget* parent) {
                    auto* picker = new DateTimeRangePicker(parent);
                    picker->setStartLabelText(QStringLiteral("Start"));
                    picker->setEndLabelText(QStringLiteral("End"));
                    picker->setRange(QDateTime(QDate(2025, 10, 1), QTime(10, 10)),
                                     QDateTime(QDate(2025, 10, 3), QTime(15, 10)));
                    return picker;
                },
                true),
            makeSample(
                QStringLiteral("date-time-range-open-ended"),
                QStringLiteral("Leave either bound open"),
                QStringLiteral("Use the clear buttons independently. An empty endpoint is an "
                               "invalid QDateTime, rather than today's date."),
                QStringLiteral(
                    "auto* picker = new DateTimeRangePicker(this);\n"
                    "picker->setStartLabelText(\"Since\");\n"
                    "picker->setEndLabelText(\"Until\");\n"
                    "picker->startDatePicker()->setPlaceholderText(\"No lower bound\");\n"
                    "picker->endDatePicker()->setPlaceholderText(\"No upper bound\");\n"
                    "picker->endTimePicker()->setPlaceholderText(TimePicker::TimeField::Hour, "
                    "\"hh\");\n"
                    "picker->endTimePicker()->setPlaceholderText(TimePicker::TimeField::Minute, "
                    "\"mm\");\n"
                    "picker->setStartDateTime(QDateTime(QDate(2025, 10, 1), QTime(10, 10)));\n"
                    "connect(picker, &DateTimeRangePicker::rangeChanged, this,\n"
                    "        [](const QDateTime& start, const QDateTime& end) { /* build the query "
                    "*/ });"),
                [](QWidget* parent) {
                    auto* picker = new DateTimeRangePicker(parent);
                    picker->setStartLabelText(QStringLiteral("Since"));
                    picker->setEndLabelText(QStringLiteral("Until"));
                    picker->startDatePicker()->setPlaceholderText(QStringLiteral("No lower bound"));
                    picker->endDatePicker()->setPlaceholderText(QStringLiteral("No upper bound"));
                    picker->endTimePicker()->setPlaceholderText(TimePicker::TimeField::Hour,
                                                                QStringLiteral("hh"));
                    picker->endTimePicker()->setPlaceholderText(TimePicker::TimeField::Minute,
                                                                QStringLiteral("mm"));
                    picker->setStartDateTime(QDateTime(QDate(2025, 10, 1), QTime(10, 10)));
                    QObject::connect(picker, &DateTimeRangePicker::rangeChanged, picker,
                                     [](const QDateTime&, const QDateTime&) {});
                    return picker;
                },
                true)};
}

QVector<GallerySample> timePickerSamples()
{
    return {makeSample(
        QStringLiteral("time-picker-basic"), QStringLiteral("Spinning-field time picker"),
        QStringLiteral("Clicking opens looping hour and minute columns."),
        QStringLiteral("auto* picker = new TimePicker(this);\n"
                       "picker->setPlaceholderText(TimePicker::TimeField::Hour, \"hour\");\n"
                       "picker->setPlaceholderText(TimePicker::TimeField::Minute, \"minute\");\n"
                       "picker->setPlaceholderText(TimePicker::TimeField::Period, \"AM/PM\");\n"
                       "picker->setConfirmButtonAccessibleName(\"Accept time\");\n"
                       "picker->setCancelButtonAccessibleName(\"Cancel time\");\n"
                       "picker->setTime(QTime(9, 30));\n"
                       "picker->setMinuteIncrement(5);"),
        [](QWidget* parent) {
            auto* picker = new TimePicker(parent);
            picker->setPlaceholderText(TimePicker::TimeField::Hour, QStringLiteral("hour"));
            picker->setPlaceholderText(TimePicker::TimeField::Minute, QStringLiteral("minute"));
            picker->setPlaceholderText(TimePicker::TimeField::Period, QStringLiteral("AM/PM"));
            picker->setConfirmButtonAccessibleName(QStringLiteral("Accept time"));
            picker->setCancelButtonAccessibleName(QStringLiteral("Cancel time"));
            picker->setTime(QTime(9, 30));
            picker->setMinuteIncrement(5);
            return picker;
        })};
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
    if (routeId == QStringLiteral("date-time-range-picker"))
        return dateTimeRangePickerSamples();
    if (routeId == QStringLiteral("time-picker"))
        return timePickerSamples();
    return {};
}

} // namespace fluent::gallery
