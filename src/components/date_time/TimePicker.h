#ifndef TIMEPICKER_H
#define TIMEPICKER_H

#include <QTime>
#include <QLocale>
#include <QPointer>
#include <QVector>
#include <Qt>

#include "../basicinput/Button.h"

namespace fluent::date_time {

class TimePickerFlyout;

/**
 * @brief Button-style time picker that supports clearing the selection.
 * zh_CN: 按钮式时间选择器，支持清空已选时间。
 *
 * Displays formatted time on a compact button and supports minute increments,
 * 12/24-hour clock mode, and a wheel flyout.
 * zh_CN: 在紧凑按钮上格式化显示时间，支持配置分钟步长、12/24 小时制以及下拉滚轮面板。
 */
class TimePicker : public fluent::basicinput::Button {
    Q_OBJECT
    /**
     * @brief Selected time if valid; otherwise the default time used to initialize the wheel.
     * zh_CN: 已选时间；未选择时返回用于初始化滚轮面板的默认时间。
     */
    Q_PROPERTY(QTime time READ time WRITE setTime NOTIFY timeChanged)
    /**
     * @brief Selected time; an invalid time represents no selection.
     * zh_CN: 选中时间；无效时间表示未选择。
     */
    Q_PROPERTY(QTime selectedTime
        READ selectedTime
        WRITE setSelectedTime
        RESET clearSelectedTime
        NOTIFY selectedTimeChanged)
    /**
     * @brief Minute step used by the time picker.
     * zh_CN: 时间选择器使用的分钟步长。
     */
    Q_PROPERTY(int minuteIncrement READ minuteIncrement WRITE setMinuteIncrement NOTIFY minuteIncrementChanged)
    /**
     * @brief Clock mode used for time display and editing.
     * zh_CN: 时间显示和编辑使用的时钟模式。
     */
    Q_PROPERTY(ClockIdentifier clockIdentifier
        READ clockIdentifier
        WRITE setClockIdentifier
        NOTIFY clockIdentifierChanged)
    /**
     * @brief Whether the picker dropdown is open.
     * zh_CN: 下拉滚轮面板是否打开。
     */
    Q_PROPERTY(bool dropDownOpen READ isDropDownOpen NOTIFY dropDownOpenChanged)
    /**
     * @brief Locale used for AM/PM period text.
     * zh_CN: 用于上午/下午时段文案的区域设置。
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
    enum class TimeField { Hour, Minute, Period };
    Q_ENUM(TimeField)

    enum class ClockIdentifier { TwelveHourClock, TwentyFourHourClock };
    Q_ENUM(ClockIdentifier)

    explicit TimePicker(QWidget* parent = nullptr);
    ~TimePicker() override;

    QTime time() const { return m_selectedTime.isValid() ? m_selectedTime : m_time; }
    QTime selectedTime() const { return m_selectedTime; }
    int minuteIncrement() const { return m_minuteIncrement; }
    ClockIdentifier clockIdentifier() const { return m_clockIdentifier; }
    QLocale locale() const { return QWidget::locale(); }
    bool isDropDownOpen() const { return m_dropDownOpen; }
    bool isOpen() const { return isDropDownOpen(); }

    QString fieldDisplayText(TimeField field) const;
    QString placeholderText(TimeField field) const;
    QString confirmButtonAccessibleName() const { return m_confirmButtonAccessibleName; }
    QString cancelButtonAccessibleName() const { return m_cancelButtonAccessibleName; }
    Qt::Alignment fieldTextAlignment(TimeField field) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    /** @brief Sets the default/pending time without changing selectedTime; ignores invalid times.
     * zh_CN: 设置未选中时的默认显示时间，不会修改 selectedTime；忽略无效时间。 */
    void setTime(const QTime& time);
    /** @brief Commits a selected time; an invalid time clears the selection.
     * zh_CN: 提交选中时间；无效时间清空选择。 */
    void setSelectedTime(const QTime& time);
    void clearSelectedTime();
    void setMinuteIncrement(int increment);
    void setClockIdentifier(ClockIdentifier identifier);
    void setLocale(const QLocale& locale);
    /**
     * @brief Sets custom placeholder text for one time field.
     * zh_CN: 为指定时间字段设置自定义占位文本。
     */
    void setPlaceholderText(TimeField field, const QString& text);
    void setConfirmButtonAccessibleName(const QString& name);
    void setCancelButtonAccessibleName(const QString& name);
    void setFieldTextAlignment(TimeField field, Qt::Alignment alignment);
    void openPicker();
    void closePicker();

signals:
    void timeChanged(const QTime& time);
    void selectedTimeChanged(const QTime& time);
    void minuteIncrementChanged(int increment);
    void clockIdentifierChanged(TimePicker::ClockIdentifier identifier);
    void localeChanged(const QLocale& locale);
    void placeholderTextChanged(TimePicker::TimeField field, const QString& text);
    void confirmButtonAccessibleNameChanged(const QString& name);
    void cancelButtonAccessibleNameChanged(const QString& name);
    void dropDownOpenChanged(bool open);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class TimePickerFlyout;
    friend class DateTimeRangePicker;

    struct FieldSegment {
        TimeField field;
        QRect rect;
    };

    QVector<TimeField> visibleFields() const;
    bool isFieldVisible(TimeField field) const;
    QVector<FieldSegment> fieldSegments() const;
    QRect fieldSurfaceRect() const;
    int preferredFieldWidth(TimeField field) const;

    QString formatField(TimeField field, const QTime& time) const;
    QTime normalizeTime(const QTime& time) const;
    QTime shiftedTime(const QTime& time, TimeField field, int offset) const;
    QVector<int> minuteValues(const QTime& reference = QTime()) const;
    int snappedMinute(int minute, const QTime& reference = QTime()) const;

    void setDropDownOpen(bool open);
    void applyPendingTime(const QTime& time);
    void handleFlyoutClosed();

    QPointer<TimePickerFlyout> m_flyout;

    QTime m_time;
    QTime m_selectedTime;
    int m_minuteIncrement = 1;
    // The range endpoint also admits the final minute of a whole calendar day.
    // zh_CN: 范围结束端点额外允许整天的最后一分钟。
    bool m_allowDayEnd = false;
    ClockIdentifier m_clockIdentifier = ClockIdentifier::TwelveHourClock;
    QLocale m_observedLocale;
    QString m_hourPlaceholderText;
    QString m_minutePlaceholderText;
    QString m_periodPlaceholderText;
    QString m_confirmButtonAccessibleName;
    QString m_cancelButtonAccessibleName;
    Qt::Alignment m_hourTextAlignment = Qt::AlignLeft;
    Qt::Alignment m_minuteTextAlignment = Qt::AlignHCenter;
    Qt::Alignment m_periodTextAlignment = Qt::AlignHCenter;

    bool m_dropDownOpen = false;
};

} // namespace fluent::date_time

Q_DECLARE_METATYPE(fluent::date_time::TimePicker::TimeField)
Q_DECLARE_METATYPE(fluent::date_time::TimePicker::ClockIdentifier)

#endif // TIMEPICKER_H
