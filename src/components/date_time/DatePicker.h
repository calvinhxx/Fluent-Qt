#ifndef DATEPICKER_H
#define DATEPICKER_H

#include <QDate>
#include <QLocale>
#include <QPointer>
#include <Qt>
#include <QVector>

#include "../basicinput/Button.h"

namespace fluent::date_time {

class DatePickerFlyout;

/**
 * @brief Button-style date picker that supports clearing the selection.
 * zh_CN: 按钮式日期选择器，支持清空已选日期。
 *
 * Manages date bounds, field visibility, day/month/year formatting, and the
 * wheel flyout for compact form entry.
 * zh_CN: 支持配置可选日期范围、字段显隐及年月日显示格式，通过下拉滚轮面板选择日期。
 */
class DatePicker : public fluent::basicinput::Button {
    Q_OBJECT
    /**
     * @brief Current date value displayed by the picker.
     * zh_CN: 当前显示的日期值。
     */
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    /**
     * @brief Selected date; an invalid date represents no selection.
     * zh_CN: 选中日期；无效日期表示未选择。
     */
    Q_PROPERTY(QDate selectedDate
        READ selectedDate
        WRITE setSelectedDate
        RESET clearSelectedDate
        NOTIFY selectedDateChanged)
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
     * zh_CN: 下拉滚轮面板是否打开。
     */
    Q_PROPERTY(bool dropDownOpen READ isDropDownOpen NOTIFY dropDownOpenChanged)
    /**
     * @brief Locale used for month and weekday names.
     * zh_CN: 用于月份和星期名称的区域设置。
     */
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    /**
     * @brief Custom accessible name for the confirm button.
     * zh_CN: 确认按钮的自定义无障碍名称。
     */
    Q_PROPERTY(QString confirmButtonAccessibleName
        READ confirmButtonAccessibleName
        WRITE setConfirmButtonAccessibleName
        NOTIFY confirmButtonAccessibleNameChanged)
    /**
     * @brief Custom accessible name for the cancel button.
     * zh_CN: 取消按钮的自定义无障碍名称。
     */
    Q_PROPERTY(QString cancelButtonAccessibleName
        READ cancelButtonAccessibleName
        WRITE setCancelButtonAccessibleName
        NOTIFY cancelButtonAccessibleNameChanged)

public:
    enum class DateField { Month, Day, Year };
    Q_ENUM(DateField)

    enum class MonthFormat { FullMonthName, AbbreviatedMonthName, NumericMonth, TwoDigitMonth };
    Q_ENUM(MonthFormat)

    enum class DayFormat { DayInteger, TwoDigitDay, DayIntegerWithAbbreviatedWeekday };
    Q_ENUM(DayFormat)

    enum class YearFormat { FullYear, TwoDigitYear };
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

    QLocale locale() const { return QWidget::locale(); }

    Qt::Alignment fieldTextAlignment(DateField field) const;

    bool isDropDownOpen() const { return m_dropDownOpen; }
    bool isOpen() const { return isDropDownOpen(); }

    QString fieldDisplayText(DateField field) const;
    QString placeholderText(DateField field) const;
    QString confirmButtonAccessibleName() const { return m_confirmButtonAccessibleName; }
    QString cancelButtonAccessibleName() const { return m_cancelButtonAccessibleName; }

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
    void setLocale(const QLocale& locale);
    /**
     * @brief Sets custom placeholder text for one date field.
     * zh_CN: 为指定日期字段设置自定义占位文本。
     */
    void setPlaceholderText(DateField field, const QString& text);
    void setConfirmButtonAccessibleName(const QString& name);
    void setCancelButtonAccessibleName(const QString& name);
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
    void localeChanged(const QLocale& locale);
    void placeholderTextChanged(DatePicker::DateField field, const QString& text);
    void confirmButtonAccessibleNameChanged(const QString& name);
    void cancelButtonAccessibleNameChanged(const QString& name);
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
    int preferredFieldWidth(DateField field) const;

    QString formatField(DateField field, const QDate& date) const;
    QDate normalizeDate(int year, int month, int day) const;
    QDate clampDate(const QDate& date) const;
    QDate shiftedDate(const QDate& date, DateField field, int offset) const;

    void setDropDownOpen(bool open);
    void applyPendingDate(const QDate& date);
    void handleFlyoutClosed();

    QPointer<DatePickerFlyout> m_flyout;

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
    QLocale m_observedLocale;
    QString m_monthPlaceholderText;
    QString m_dayPlaceholderText;
    QString m_yearPlaceholderText;
    QString m_confirmButtonAccessibleName;
    QString m_cancelButtonAccessibleName;
    Qt::Alignment m_monthTextAlignment = Qt::AlignLeft;
    Qt::Alignment m_dayTextAlignment = Qt::AlignHCenter;
    Qt::Alignment m_yearTextAlignment = Qt::AlignHCenter;
};

} // namespace fluent::date_time

Q_DECLARE_METATYPE(fluent::date_time::DatePicker::DateField)
Q_DECLARE_METATYPE(fluent::date_time::DatePicker::MonthFormat)
Q_DECLARE_METATYPE(fluent::date_time::DatePicker::DayFormat)
Q_DECLARE_METATYPE(fluent::date_time::DatePicker::YearFormat)

#endif // DATEPICKER_H
