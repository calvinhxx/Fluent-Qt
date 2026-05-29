#ifndef TIMEPICKER_H
#define TIMEPICKER_H

#include <QTime>
#include <QVector>
#include <Qt>

#include "components/basicinput/Button.h"

namespace fluent::date_time {

class TimePickerFlyout;

/**
 * @brief Button-like time picker with nullable selected-time semantics.
 * zh_CN: 支持可空选中时间语义的按钮式时间选择器。
 *
 * TimePicker formats time into a compact button surface and exposes minute step,
 * clock mode, selected value, and dropdown state for time-entry workflows.
 * zh_CN: TimePicker 将时间格式化到紧凑按钮表面，并暴露分钟步长、时钟模式、
 * 选中值和下拉状态，用于时间输入流程。
 */
class TimePicker : public fluent::basicinput::Button {
    Q_OBJECT
    /**
     * @brief Current time value displayed by the picker.
     * zh_CN: picker 当前显示的时间值。
     */
    Q_PROPERTY(QTime time READ time WRITE setTime NOTIFY timeChanged)
    /**
     * @brief Selected time; an invalid time represents no selection.
     * zh_CN: 选中时间；无效时间表示未选择。
     */
    Q_PROPERTY(QTime selectedTime READ selectedTime WRITE setSelectedTime RESET clearSelectedTime NOTIFY selectedTimeChanged)
    /**
     * @brief Minute step used by the time picker.
     * zh_CN: 时间选择器使用的分钟步长。
     */
    Q_PROPERTY(int minuteIncrement READ minuteIncrement WRITE setMinuteIncrement NOTIFY minuteIncrementChanged)
    /**
     * @brief Clock mode used for time display and editing.
     * zh_CN: 时间显示和编辑使用的时钟模式。
     */
    Q_PROPERTY(ClockIdentifier clockIdentifier READ clockIdentifier WRITE setClockIdentifier NOTIFY clockIdentifierChanged)
    /**
     * @brief Whether the picker dropdown is open.
     * zh_CN: picker 下拉面板是否打开。
     */
    Q_PROPERTY(bool dropDownOpen READ isDropDownOpen NOTIFY dropDownOpenChanged)

public:
    enum class TimeField {
        Hour,
        Minute,
        Period
    };
    Q_ENUM(TimeField)

    enum class ClockIdentifier {
        TwelveHourClock,
        TwentyFourHourClock
    };
    Q_ENUM(ClockIdentifier)

    explicit TimePicker(QWidget* parent = nullptr);
    ~TimePicker() override;

    QTime time() const { return m_selectedTime.isValid() ? m_selectedTime : m_time; }
    QTime selectedTime() const { return m_selectedTime; }
    int minuteIncrement() const { return m_minuteIncrement; }
    ClockIdentifier clockIdentifier() const { return m_clockIdentifier; }
    bool isDropDownOpen() const { return m_dropDownOpen; }
    bool isOpen() const { return isDropDownOpen(); }

    QString fieldDisplayText(TimeField field) const;
    QString placeholderText(TimeField field) const;
    Qt::Alignment fieldTextAlignment(TimeField field) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setTime(const QTime& time);
    void setSelectedTime(const QTime& time);
    void clearSelectedTime();
    void setMinuteIncrement(int increment);
    void setClockIdentifier(ClockIdentifier identifier);
    void setFieldTextAlignment(TimeField field, Qt::Alignment alignment);
    void openPicker();
    void closePicker();

signals:
    void timeChanged(const QTime& time);
    void selectedTimeChanged(const QTime& time);
    void minuteIncrementChanged(int increment);
    void clockIdentifierChanged(TimePicker::ClockIdentifier identifier);
    void dropDownOpenChanged(bool open);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class TimePickerFlyout;

    struct FieldSegment {
        TimeField field;
        QRect rect;
    };

    QVector<TimeField> visibleFields() const;
    bool isFieldVisible(TimeField field) const;
    QVector<FieldSegment> fieldSegments() const;
    QRect fieldSurfaceRect() const;

    QString formatField(TimeField field, const QTime& time) const;
    QTime normalizeTime(const QTime& time) const;
    QTime shiftedTime(const QTime& time, TimeField field, int offset) const;
    QVector<int> minuteValues() const;
    int snappedMinute(int minute) const;

    void setDropDownOpen(bool open);
    void applyPendingTime(const QTime& time);
    void handleFlyoutClosed();

    TimePickerFlyout* m_flyout = nullptr;

    QTime m_time;
    QTime m_selectedTime;
    int m_minuteIncrement = 1;
    ClockIdentifier m_clockIdentifier = ClockIdentifier::TwelveHourClock;
    Qt::Alignment m_hourTextAlignment = Qt::AlignLeft;
    Qt::Alignment m_minuteTextAlignment = Qt::AlignHCenter;
    Qt::Alignment m_periodTextAlignment = Qt::AlignHCenter;

    bool m_dropDownOpen = false;
};

} // namespace fluent::date_time

Q_DECLARE_METATYPE(fluent::date_time::TimePicker::TimeField)
Q_DECLARE_METATYPE(fluent::date_time::TimePicker::ClockIdentifier)

#endif // TIMEPICKER_H
