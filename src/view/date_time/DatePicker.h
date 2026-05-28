#ifndef DATEPICKER_H
#define DATEPICKER_H

#include <QDate>
#include <Qt>
#include <QVector>

#include "view/basicinput/Button.h"

namespace view::date_time {

class DatePickerFlyout;

/**
 * @brief Button-like date picker with nullable selected-date semantics.
 * zh_CN: 支持可空选中日期语义的按钮式日期选择器。
 *
 * DatePicker controls date bounds, visible fields, day/month/year formatting,
 * committed date text, and dropdown state for compact form entry scenarios.
 * zh_CN: DatePicker 管理日期边界、字段可见性、日/月/年格式、提交后的日期文本
 * 和下拉打开态，适合紧凑表单输入。
 */
class DatePicker : public view::basicinput::Button {
    Q_OBJECT
    /**
     * @brief Current date value displayed by the picker.
     * zh_CN: picker 当前显示的日期值。
     */
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    /**
     * @brief Selected date; an invalid date represents no selection.
     * zh_CN: 选中日期；无效日期表示未选择。
     */
    Q_PROPERTY(QDate selectedDate READ selectedDate WRITE setSelectedDate RESET clearSelectedDate NOTIFY selectedDateChanged)
    /**
     * @brief Minimum selectable date.
     * zh_CN: 可选择的最小日期。
     */
    Q_PROPERTY(QDate minimumDate READ minimumDate WRITE setMinimumDate NOTIFY minimumDateChanged)
    /**
     * @brief Maximum selectable date.
     * zh_CN: 可选择的最大日期。
     */
    Q_PROPERTY(QDate maximumDate READ maximumDate WRITE setMaximumDate NOTIFY maximumDateChanged)
    /**
     * @brief Whether the month field is visible.
     * zh_CN: 月份字段是否可见。
     */
    Q_PROPERTY(bool monthVisible READ monthVisible WRITE setMonthVisible NOTIFY monthVisibleChanged)
    /**
     * @brief Whether the day field is visible.
     * zh_CN: 日期字段是否可见。
     */
    Q_PROPERTY(bool dayVisible READ dayVisible WRITE setDayVisible NOTIFY dayVisibleChanged)
    /**
     * @brief Whether the year field is visible.
     * zh_CN: 年份字段是否可见。
     */
    Q_PROPERTY(bool yearVisible READ yearVisible WRITE setYearVisible NOTIFY yearVisibleChanged)
    /**
     * @brief Format used for the month field.
     * zh_CN: 月份字段使用的格式。
     */
    Q_PROPERTY(MonthFormat monthFormat READ monthFormat WRITE setMonthFormat NOTIFY monthFormatChanged)
    /**
     * @brief Format used for the day field.
     * zh_CN: 日期字段使用的格式。
     */
    Q_PROPERTY(DayFormat dayFormat READ dayFormat WRITE setDayFormat NOTIFY dayFormatChanged)
    /**
     * @brief Format used for the year field.
     * zh_CN: 年份字段使用的格式。
     */
    Q_PROPERTY(YearFormat yearFormat READ yearFormat WRITE setYearFormat NOTIFY yearFormatChanged)
    /**
     * @brief Whether the picker dropdown is open.
     * zh_CN: picker 下拉面板是否打开。
     */
    Q_PROPERTY(bool dropDownOpen READ isDropDownOpen NOTIFY dropDownOpenChanged)

public:
    enum class DateField {
        Month,
        Day,
        Year
    };
    Q_ENUM(DateField)

    enum class MonthFormat {
        FullMonthName,
        AbbreviatedMonthName,
        NumericMonth,
        TwoDigitMonth
    };
    Q_ENUM(MonthFormat)

    enum class DayFormat {
        DayInteger,
        TwoDigitDay,
        DayIntegerWithAbbreviatedWeekday
    };
    Q_ENUM(DayFormat)

    enum class YearFormat {
        FullYear,
        TwoDigitYear
    };
    Q_ENUM(YearFormat)

    explicit DatePicker(QWidget* parent = nullptr);
    ~DatePicker() override;

    QDate date() const { return m_selectedDate.isValid() ? m_selectedDate : m_date; }
    QDate selectedDate() const { return m_selectedDate; }
    QDate minimumDate() const { return m_minimumDate; }
    QDate maximumDate() const { return m_maximumDate; }

    bool monthVisible() const { return m_monthVisible; }
    bool dayVisible() const { return m_dayVisible; }
    bool yearVisible() const { return m_yearVisible; }

    MonthFormat monthFormat() const { return m_monthFormat; }
    DayFormat dayFormat() const { return m_dayFormat; }
    YearFormat yearFormat() const { return m_yearFormat; }

    Qt::Alignment fieldTextAlignment(DateField field) const;

    bool isDropDownOpen() const { return m_dropDownOpen; }
    bool isOpen() const { return isDropDownOpen(); }

    QString fieldDisplayText(DateField field) const;
    QString placeholderText(DateField field) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setDate(const QDate& date);
    void setSelectedDate(const QDate& date);
    void clearSelectedDate();
    void setMinimumDate(const QDate& date);
    void setMaximumDate(const QDate& date);
    void setDateRange(const QDate& minimumDate, const QDate& maximumDate);
    void setMonthVisible(bool visible);
    void setDayVisible(bool visible);
    void setYearVisible(bool visible);
    void setMonthFormat(MonthFormat format);
    void setDayFormat(DayFormat format);
    void setYearFormat(YearFormat format);
    void setFieldTextAlignment(DateField field, Qt::Alignment alignment);
    void openPicker();
    void closePicker();

signals:
    void dateChanged(const QDate& date);
    void selectedDateChanged(const QDate& date);
    void minimumDateChanged(const QDate& date);
    void maximumDateChanged(const QDate& date);
    void monthVisibleChanged(bool visible);
    void dayVisibleChanged(bool visible);
    void yearVisibleChanged(bool visible);
    void monthFormatChanged(DatePicker::MonthFormat format);
    void dayFormatChanged(DatePicker::DayFormat format);
    void yearFormatChanged(DatePicker::YearFormat format);
    void dropDownOpenChanged(bool open);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class DatePickerFlyout;

    struct FieldSegment {
        DateField field;
        QRect rect;
    };

    int visibleFieldCount() const;
    bool isFieldVisible(DateField field) const;
    bool setFieldVisible(DateField field, bool visible);
    QVector<FieldSegment> fieldSegments() const;
    QRect fieldSurfaceRect() const;

    QString formatField(DateField field, const QDate& date) const;
    QDate normalizeDate(int year, int month, int day) const;
    QDate clampDate(const QDate& date) const;
    QDate shiftedDate(const QDate& date, DateField field, int offset) const;

    void setDropDownOpen(bool open);
    void applyPendingDate(const QDate& date);
    void handleFlyoutClosed();

    DatePickerFlyout* m_flyout = nullptr;

    QDate m_date;
    QDate m_selectedDate;
    QDate m_minimumDate;
    QDate m_maximumDate;

    bool m_monthVisible = true;
    bool m_dayVisible = true;
    bool m_yearVisible = true;
    bool m_dropDownOpen = false;

    MonthFormat m_monthFormat = MonthFormat::FullMonthName;
    DayFormat m_dayFormat = DayFormat::DayInteger;
    YearFormat m_yearFormat = YearFormat::FullYear;
    Qt::Alignment m_monthTextAlignment = Qt::AlignLeft;
    Qt::Alignment m_dayTextAlignment = Qt::AlignHCenter;
    Qt::Alignment m_yearTextAlignment = Qt::AlignHCenter;
};

} // namespace view::date_time

Q_DECLARE_METATYPE(view::date_time::DatePicker::DateField)
Q_DECLARE_METATYPE(view::date_time::DatePicker::MonthFormat)
Q_DECLARE_METATYPE(view::date_time::DatePicker::DayFormat)
Q_DECLARE_METATYPE(view::date_time::DatePicker::YearFormat)

#endif // DATEPICKER_H
