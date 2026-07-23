#ifndef CALENDARDATEPICKER_H
#define CALENDARDATEPICKER_H

#include <QDate>
#include <QLocale>
#include <QString>

#include "components/basicinput/Button.h"

namespace fluent::date_time {

class CalendarDatePickerPopup;
class CalendarView;

/**
 * @brief Button-like date entry that opens a CalendarView popup.
 * zh_CN: 打开 CalendarView 弹层的按钮式日期入口。
 *
 * CalendarDatePicker stores placeholder text, selected date, range, display format,
 * first-day setting, and popup open state while delegating calendar rendering to CalendarView.
 * zh_CN: CalendarDatePicker 管理占位文本、选中日期、日期范围、显示格式、一周起始日
 * 和弹层打开态，日历绘制交给 CalendarView。
 */
class CalendarDatePicker : public fluent::basicinput::Button {
    Q_OBJECT
    /**
     * @brief Text shown on the button when no date has been selected.
     * zh_CN: 尚未选择日期时显示在按钮上的占位文本。
     */
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    /**
     * @brief Current date value displayed by the picker.
     * zh_CN: picker 当前显示的日期值。
     */
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    /**
     * @brief Minimum date accepted by the calendar surface.
     * zh_CN: 日历表面接受的最小日期。
     */
    Q_PROPERTY(QDate minDate READ minDate WRITE setMinDate NOTIFY minDateChanged)
    /**
     * @brief Maximum date accepted by the calendar surface.
     * zh_CN: 日历表面接受的最大日期。
     */
    Q_PROPERTY(QDate maxDate READ maxDate WRITE setMaxDate NOTIFY maxDateChanged)
    /**
     * @brief Date display format used by the picker button.
     * zh_CN: picker 按钮使用的日期显示格式。
     */
    Q_PROPERTY(QString displayFormat READ displayFormat WRITE setDisplayFormat NOTIFY displayFormatChanged)
    /**
     * @brief First day used when laying out calendar weeks.
     * zh_CN: 日历周布局使用的一周起始日。
     */
    Q_PROPERTY(Qt::DayOfWeek firstDayOfWeek READ firstDayOfWeek WRITE setFirstDayOfWeek NOTIFY firstDayOfWeekChanged)
    /**
     * @brief Whether the calendar flyout is open.
     * zh_CN: 日历浮层是否打开。
     */
    Q_PROPERTY(bool calendarOpen READ isCalendarOpen WRITE setCalendarOpen NOTIFY calendarOpenChanged)

public:
    explicit CalendarDatePicker(QWidget* parent = nullptr);
    ~CalendarDatePicker() override;

    QString placeholderText() const { return m_placeholderText; }
    void setPlaceholderText(const QString& text);

    QDate date() const { return m_date; }
    void setDate(const QDate& date);

    QDate minDate() const { return m_minDate; }
    void setMinDate(const QDate& date);

    QDate maxDate() const { return m_maxDate; }
    void setMaxDate(const QDate& date);

    QString displayFormat() const { return m_displayFormat; }
    void setDisplayFormat(const QString& format);

    Qt::DayOfWeek firstDayOfWeek() const { return m_firstDayOfWeek; }
    void setFirstDayOfWeek(Qt::DayOfWeek day);

    bool isCalendarOpen() const { return m_calendarOpen; }
    bool isOpen() const { return isCalendarOpen(); }
    void setCalendarOpen(bool open);

    QString displayText() const;
    QDate visibleMonth() const;
    CalendarView* calendarView() const;
    bool isDateSelectable(const QDate& date) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void openCalendar();
    void closeCalendar();
    void clearDate();

signals:
    void placeholderTextChanged(const QString& text);
    void dateChanged(const QDate& date);
    void minDateChanged(const QDate& date);
    void maxDateChanged(const QDate& date);
    void displayFormatChanged(const QString& format);
    void firstDayOfWeekChanged(Qt::DayOfWeek day);
    void calendarOpenChanged(bool open);

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class CalendarDatePickerPopup;

    QDate normalizeDateForRange(const QDate& date) const;
    void enforceSelectedDateInRange();
    void ensurePopup();
    void handlePopupOpenChanged(bool open);
    void selectDateFromCalendar(const QDate& date);
    QDate defaultVisibleMonth() const;
    void refreshButtonText();

    QString m_placeholderText;
    QDate m_date;
    QDate m_minDate;
    QDate m_maxDate;
    QString m_displayFormat;
    Qt::DayOfWeek m_firstDayOfWeek = QLocale().firstDayOfWeek();

    bool m_calendarOpen = false;

    CalendarDatePickerPopup* m_popup = nullptr;
};

} // namespace fluent::date_time

#endif // CALENDARDATEPICKER_H
