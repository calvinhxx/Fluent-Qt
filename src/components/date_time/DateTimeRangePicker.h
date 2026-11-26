#ifndef FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKER_H
#define FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKER_H

#include <QDateTime>
#include <QLocale>
#include <QPointer>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QBoxLayout;

namespace fluent::basicinput {
class Button;
}
namespace fluent::layout {
class Field;
}

namespace fluent::date_time {

class CalendarDatePicker;
class TimePicker;
class CalendarView;
class DateTimeRangePickerPopup;

/**
 * @brief Compact date/time range picker with a calendar flyout and independently clearable endpoints.
 * zh_CN: 紧凑型日期时间范围选择器，带范围日历浮层，起止端点可独立留空。
 *
 * Invalid QDateTime values leave an endpoint unbounded. Values use minute
 * precision and keep their Qt::TimeSpec; date-only mode stores midnight for
 * both endpoints (inclusive/exclusive filtering is up to the caller). Reversed
 * endpoints stay visible and set rangeValid to false. Built-in captions support
 * Simplified/Traditional Chinese and English (preferring Qt translations),
 * while custom captions are preserved.
 * zh_CN: 无效的 QDateTime 表示不限制该端；值精确到分钟并保留 Qt 时间规格。
 * 仅日期模式下两端均保存 00:00，查询边界是否包含由应用决定；起止倒置时保留输入，
 * isRangeValid() 返回 false。默认文案内置简繁中文并优先使用 Qt 翻译（其余回退英文），
 * 手动设置的文案不会被覆盖。
 */
class DateTimeRangePicker : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QDateTime startDateTime
        READ startDateTime
        WRITE setStartDateTime
        RESET clearStart
        NOTIFY startDateTimeChanged)
    Q_PROPERTY(QDateTime endDateTime READ endDateTime WRITE setEndDateTime RESET clearEnd NOTIFY endDateTimeChanged)
    Q_PROPERTY(bool timeEnabled READ isTimeEnabled WRITE setTimeEnabled NOTIFY timeEnabledChanged)
    Q_PROPERTY(bool rangeValid READ isRangeValid NOTIFY rangeValidityChanged)
    Q_PROPERTY(QString startLabelText READ startLabelText WRITE setStartLabelText NOTIFY startLabelTextChanged)
    Q_PROPERTY(QString endLabelText READ endLabelText WRITE setEndLabelText NOTIFY endLabelTextChanged)
    Q_PROPERTY(QString invalidRangeText READ invalidRangeText WRITE setInvalidRangeText NOTIFY invalidRangeTextChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(bool pickerOpen READ isPickerOpen WRITE setPickerOpen NOTIFY pickerOpenChanged)
    Q_PROPERTY(bool presetsVisible READ isPresetsVisible WRITE setPresetsVisible NOTIFY presetsVisibleChanged)
    Q_PROPERTY(bool autoApplyPresetsEnabled
        READ isAutoApplyPresetsEnabled
        WRITE setAutoApplyPresetsEnabled
        NOTIFY autoApplyPresetsEnabledChanged)

public:
    /**
     * @brief Built-in rolling calendar ranges including the reference day.
     * zh_CN: 包含参考日的内置滚动日历范围。
     */
    enum class Preset { Today, Yesterday, PastWeek, PastMonth, PastSixMonths, PastYear };
    Q_ENUM(Preset)

    explicit DateTimeRangePicker(QWidget* parent = nullptr);
    ~DateTimeRangePicker() override;

    QDateTime startDateTime() const { return m_startDateTime; }
    QDateTime endDateTime() const { return m_endDateTime; }
    bool isTimeEnabled() const { return m_timeEnabled; }
    bool isRangeValid() const;
    QString startLabelText() const;
    QString endLabelText() const;
    QString invalidRangeText() const { return m_invalidRangeText; }
    QLocale locale() const { return QWidget::locale(); }
    QString placeholderText() const { return m_placeholderText; }
    QString displayText() const;
    bool isPickerOpen() const;
    bool isOpen() const { return isPickerOpen(); }
    bool isPresetsVisible() const { return m_presetsVisible; }
    bool isAutoApplyPresetsEnabled() const { return m_autoApplyPresetsEnabled; }
    basicinput::Button* entryButton() const { return m_entryButton; }
    /**
     * @brief Returns the internal range calendar for navigation and week settings.
     * zh_CN: 返回内部的范围日历视图，用于月份切换和周起始日配置。
     */
    CalendarView* calendarView() const;

    /**
     * @brief Returns the internal date and time pickers for formatting and accessibility setup.
     * zh_CN: 返回内部的子日期/时间选择器，用于自定义格式或无障碍属性。
     *
     * Date inputs open compact anchored calendars, while time inputs edit in
     * place. These child widgets remain owned by DateTimeRangePicker; their date
     * bounds and minute increments also constrain stored endpoints (the end
     * input additionally accepts 23:59 for whole-day ranges).
     * zh_CN: 日期输入框弹出紧凑锚定日历，时间输入框支持原地编辑；返回的子控件仍归
     * 本组件管理，其日期边界和分钟步长同样约束端点值（结束时间额外接受 23:59 以表示整天范围）。
     */
    CalendarDatePicker* startDatePicker() const { return m_startDatePicker; }
    CalendarDatePicker* endDatePicker() const { return m_endDatePicker; }
    TimePicker* startTimePicker() const { return m_startTimePicker; }
    TimePicker* endTimePicker() const { return m_endTimePicker; }
    basicinput::Button* startClearButton() const { return m_startClearButton; }
    basicinput::Button* endClearButton() const { return m_endClearButton; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int width) const override;

public slots:
    void setStartDateTime(const QDateTime& value);
    void setEndDateTime(const QDateTime& value);
    /**
     * @brief Updates both endpoints before emitting their change signals.
     * zh_CN: 在发出端点变化信号前同时更新两个端点。
     */
    void setRange(const QDateTime& start, const QDateTime& end);
    void clearStart();
    void clearEnd();
    void clear();
    /**
     * @brief Shows time inputs or normalizes both endpoints to midnight.
     * zh_CN: 显示时间输入，或将两个端点归一化到午夜。
     */
    void setTimeEnabled(bool enabled);
    void setStartLabelText(const QString& text);
    void setEndLabelText(const QString& text);
    void setInvalidRangeText(const QString& text);
    void setLocale(const QLocale& locale);
    void setPlaceholderText(const QString& text);
    void setPickerOpen(bool open);
    /**
     * @brief Opens the same-window range calendar, endpoint editors, and presets.
     * zh_CN: 打开同窗口范围日历、端点编辑器和快捷预设。
     */
    void openPicker();
    void closePicker();
    void setPresetsVisible(bool visible);
    void setAutoApplyPresetsEnabled(bool enabled);
    /**
     * @brief Applies a built-in preset relative to local today or a supplied reference.
     * zh_CN: 按本地今天或指定参考时刻应用内置预设。
     *
     * Today/Yesterday cover complete calendar days, and PastWeek covers seven days.
     * Month and year presets start the day after subtracting the corresponding calendar
     * interval. When time is enabled, endpoints use 00:00 and 23:59.
     * zh_CN: 今天/昨天覆盖整日，过去一周包含七天；月/年预设从按日历向前推算后的次日开始。
     * 启用时间时端点分别为 00:00 和 23:59，查询边界是否包含由业务决定。
     */
    void applyPreset(Preset preset);
    void applyPreset(Preset preset, const QDateTime& reference);

signals:
    void startDateTimeChanged(const QDateTime& value);
    void endDateTimeChanged(const QDateTime& value);
    void rangeChanged(const QDateTime& start, const QDateTime& end);
    void timeEnabledChanged(bool enabled);
    void rangeValidityChanged(bool valid);
    void startLabelTextChanged(const QString& text);
    void endLabelTextChanged(const QString& text);
    void invalidRangeTextChanged(const QString& text);
    void localeChanged(const QLocale& locale);
    void placeholderTextChanged(const QString& text);
    void pickerOpenChanged(bool open);
    void presetsVisibleChanged(bool visible);
    void autoApplyPresetsEnabledChanged(bool enabled);
    /**
     * @brief Emitted when a preset is clicked; disable autoApplyPresetsEnabled to handle presets externally.
     * zh_CN: 点击快捷预设时发出；关闭 autoApplyPresetsEnabled 后可在外部自定义计算范围。
     */
    void presetRequested(Preset preset);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void onThemeUpdated() override;

private:
    friend class DateTimeRangePickerPopup;
    QDateTime normalizedDateTime(const QDateTime& value, bool start) const;
    void updateDate(const QDate& date, bool start);
    void updateTime(const QTime& time, bool start);
    void synchronizeInputs();
    void refreshPresentationText();
    void updateLayoutDirection();
    void closePickers(QWidget* except = nullptr);
    int horizontalWidth() const;
    void selectCalendarDate(const QDate& date);
    void activateEndpoint(bool start);
    void requestPreset(Preset preset);
    void refreshEntry();

    QDateTime m_startDateTime;
    QDateTime m_endDateTime;
    bool m_timeEnabled = true;
    QString m_invalidRangeText;
    QLocale m_observedLocale;
    QBoxLayout* m_layout = nullptr;
    layout::Field* m_summaryField = nullptr;
    basicinput::Button* m_entryButton = nullptr;
    QPointer<DateTimeRangePickerPopup> m_popup;
    QString m_placeholderText;
    bool m_presetsVisible = true;
    bool m_autoApplyPresetsEnabled = true;
    bool m_editingStart = true;
    bool m_completingRange = false;
    layout::Field* m_startField = nullptr;
    layout::Field* m_endField = nullptr;
    CalendarDatePicker* m_startDatePicker = nullptr;
    CalendarDatePicker* m_endDatePicker = nullptr;
    TimePicker* m_startTimePicker = nullptr;
    TimePicker* m_endTimePicker = nullptr;
    basicinput::Button* m_startClearButton = nullptr;
    basicinput::Button* m_endClearButton = nullptr;
};

} // namespace fluent::date_time

#endif // FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKER_H
