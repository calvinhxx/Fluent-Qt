#ifndef FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKER_H
#define FLUENTQT_COMPONENTS_DATE_TIME_DATETIMERANGEPICKER_H

#include <QDateTime>
#include <QLocale>
#include <QPointer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

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
 * @brief Compact range entry with a calendar flyout and independently nullable endpoints.
 * zh_CN: 带范围日历弹层、起止端点可独立留空的紧凑范围输入。
 *
 * Invalid QDateTime values represent unbounded endpoints. Values use minute
 * precision and retain their Qt time specification. Date-only mode stores
 * midnight for both endpoints; applications own inclusive/exclusive query rules.
 * Reversed endpoints remain visible and make isRangeValid() false.
 * Default captions follow the effective widget locale, with built-in Simplified
 * and Traditional Chinese text and an English fallback. Other languages use
 * application Qt translations; application-authored text is preserved.
 * zh_CN: 无效 QDateTime 表示无界端点；值精确到分钟并保留 Qt 时间规格。
 * 仅日期模式下两端均保存午夜，查询边界是否包含由应用决定。
 * 起止顺序反转时保留输入，isRangeValid() 返回 false。
 * 默认文案跟随控件区域设置，内置简体、繁体中文并回退到英文；
 * 其他语言使用应用 Qt 翻译，应用自行设置的文案保持不变。
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
     * @brief Returns the picker-owned range calendar for navigation and week settings.
     * zh_CN: 返回控件拥有的范围日历，供月份导航和每周起始日配置使用。
     */
    CalendarView* calendarView() const;

    /**
     * @brief Returns picker-owned inputs for formatting and accessibility setup.
     * zh_CN: 返回控件拥有的子输入，供格式和无障碍配置使用。
     *
     * Date inputs open compact anchored calendars; time inputs edit in place.
     * zh_CN: 日期输入打开紧凑的锚定日历浮层，时间输入在原位编辑。
     * Returned widgets remain owned by this component. Date bounds and minute
     * increments configured on the inputs also constrain the stored endpoints.
     * The end input additionally accepts 23:59 to preserve whole-day ranges.
     * zh_CN: 返回的子控件仍由本组件拥有；子输入的日期边界和分钟步长也约束端点值。
     * 结束输入额外接受 23:59，以保留整天范围。
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
     * Today/Yesterday use complete calendar dates; PastWeek contains seven dates.
     * Month/year presets start one day after the corresponding calendar subtraction.
     * Time-enabled endpoints use 00:00 and 23:59. Applications own query inclusion.
     * zh_CN: 今天/昨天覆盖整日，过去一周包含七个日期；月/年预设从日历减法后的下一天开始。
     * 启用时间时端点为 00:00 和 23:59，查询是否包含边界由应用决定。
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
     * @brief Requests a shortcut; disable auto application to supply business ranges.
     * zh_CN: 请求快捷范围；关闭自动应用后可由业务代码提供范围。
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
