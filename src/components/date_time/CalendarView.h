#ifndef CALENDARVIEW_H
#define CALENDARVIEW_H

#include <QDate>
#include <QLocale>
#include <QRect>
#include <QRectF>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QPainter;
class QTimer;
class QVariantAnimation;
class QWheelEvent;

namespace fluent::date_time {

class DateTimeRangePicker;
class CalendarDatePickerPopup;

namespace detail {
class CalendarViewAccessible;
}

/**
 * @brief Fluent calendar widget for browsing and selecting dates by day, month, or year.
 * zh_CN: Fluent 日历控件，支持按日、月、年视图浏览和选择日期。
 *
 * Manages visible month, selected date, date bounds, week start day, level
 * transitions, keyboard navigation, and outer frame painting.
 * zh_CN: 独立管理可见月份、选中日期、可选范围、周起始日、层级过渡动效、键盘导航与外框绘制。
 */
class CalendarView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Selected date; an invalid date represents no selection.
     * zh_CN: 选中日期；无效日期表示未选择。
     */
    Q_PROPERTY(QDate selectedDate READ selectedDate WRITE setSelectedDate NOTIFY selectedDateChanged)
    /**
     * @brief Month currently displayed by CalendarView.
     * zh_CN: CalendarView 当前显示的月份。
     */
    Q_PROPERTY(QDate visibleMonth READ visibleMonth WRITE setVisibleMonth NOTIFY visibleMonthChanged)
    /**
     * @brief Minimum selectable date.
     * zh_CN: 允许选择的最小日期。
     */
    Q_PROPERTY(QDate minDate READ minDate WRITE setMinDate NOTIFY minDateChanged)
    /**
     * @brief Maximum selectable date.
     * zh_CN: 允许选择的最大日期。
     */
    Q_PROPERTY(QDate maxDate READ maxDate WRITE setMaxDate NOTIFY maxDateChanged)
    /**
     * @brief Locale used for calendar names and the automatic first weekday.
     * zh_CN: 用于日历名称和自动每周起始日的区域设置。
     */
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    /**
     * @brief First day used when laying out calendar weeks.
     * zh_CN: 日历周布局使用的一周起始日。
     */
    Q_PROPERTY(Qt::DayOfWeek firstDayOfWeek
        READ firstDayOfWeek
        WRITE setFirstDayOfWeek
        RESET resetFirstDayOfWeek
        NOTIFY firstDayOfWeekChanged)
    /**
     * @brief Calendar content level currently displayed.
     * zh_CN: 当前显示的日历内容层级。
     */
    Q_PROPERTY(CalendarContentLevel contentLevel READ contentLevel WRITE setContentLevel NOTIFY contentLevelChanged)
    /**
     * @brief Whether CalendarView paints its outer frame.
     * zh_CN: CalendarView 是否绘制自身外框。
     */
    Q_PROPERTY(bool frameVisible READ isFrameVisible WRITE setFrameVisible NOTIFY frameVisibleChanged)

public:
    enum class CalendarContentLevel { Day, Month, Year };
    Q_ENUM(CalendarContentLevel)

    explicit CalendarView(QWidget* parent = nullptr);
    ~CalendarView() override;

    QDate selectedDate() const { return m_selectedDate; }
    QDate visibleMonth() const { return m_visibleMonth; }
    QDate minDate() const { return m_minDate; }
    QDate maxDate() const { return m_maxDate; }
    QLocale locale() const { return QWidget::locale(); }
    Qt::DayOfWeek firstDayOfWeek() const { return m_firstDayOfWeek; }
    CalendarContentLevel contentLevel() const { return m_contentLevel; }
    bool isFrameVisible() const { return m_frameVisible; }

    QRect previousButtonRect() const;
    QRect nextButtonRect() const;
    QRect titleButtonRect() const;
    QRect gridRect() const;
    QRect contentRect() const;
    QRect cellRect(int row, int column) const;
    QRect contentCellRect(int row, int column) const;
    QRect dateCellRect(const QDate& date) const;
    QDate gridStartDate() const;
    QDate dateAt(const QPoint& pos) const;
    bool isDateSelectable(const QDate& date) const;
    bool isMonthSelectable(int year, int month) const;
    bool isYearSelectable(int year) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setSelectedDate(const QDate& date);
    void setVisibleMonth(const QDate& month);
    void setMinDate(const QDate& date);
    void setMaxDate(const QDate& date);
    void setDateRange(const QDate& minDate, const QDate& maxDate);
    void setLocale(const QLocale& locale);
    void setFirstDayOfWeek(Qt::DayOfWeek day);
    void resetFirstDayOfWeek();
    void setContentLevel(CalendarContentLevel level);
    void setFrameVisible(bool visible);

signals:
    void selectedDateChanged(const QDate& date);
    void visibleMonthChanged(const QDate& month);
    void minDateChanged(const QDate& date);
    void maxDateChanged(const QDate& date);
    void localeChanged(const QLocale& locale);
    void firstDayOfWeekChanged(Qt::DayOfWeek day);
    void contentLevelChanged(CalendarContentLevel level);
    void frameVisibleChanged(bool visible);
    void dateActivated(const QDate& date);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class detail::CalendarViewAccessible;
    friend class DateTimeRangePicker;
    friend class CalendarDatePickerPopup;

    // Compact density is reserved for the range picker's endpoint flyouts.
    // zh_CN: 紧凑密度供范围选择器的端点浮层使用。
    void setCompact(bool compact);
    int headerHeight() const;
    int weekHeaderHeight() const;
    int dayCellHeight() const;

    // Range highlight is used by DateTimeRangePicker; standalone calendars remain single-date.
    // zh_CN: 范围高亮仅由 DateTimeRangePicker 使用，独立日历仍保持单选行为。
    void setHighlightedDateRange(const QDate& start, const QDate& end);
    bool isHighlightedDate(const QDate& date) const;

    struct DayPageKey {
        int year = 0;
        int month = 0;
    };

    struct MonthPageKey {
        int year = 0;
    };

    struct YearPageKey {
        int startYear = 0;
    };

    void paintHeader(QPainter& painter);
    void paintTitleButtonBackground(QPainter& painter);
    void paintTitleForLevel(QPainter& painter,
        CalendarContentLevel level,
        const QDate& visibleMonth,
        qreal yOffset,
        qreal opacity);
    void paintNavButton(QPainter& painter, const QRect& buttonRect, const QString& glyph);
    void paintWeekdays(QPainter& painter);
    void paintContent(QPainter& painter);
    void paintContentLevel(QPainter& painter,
        CalendarContentLevel level,
        const QDate& visibleMonth,
        qreal yOffset,
        qreal opacity);
    void paintContentLevelZoom(QPainter& painter,
        CalendarContentLevel level,
        const QDate& visibleMonth,
        qreal scale,
        qreal opacity);
    void paintDayContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintMonthDays(QPainter& painter, const QDate& month, qreal yOffset, qreal opacity);
    void paintMonthContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintYearContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintContentCellChrome(QPainter& painter,
        const QRectF& cell,
        bool current,
        bool selected,
        bool hovered,
        bool pressed);
    QColor contentCellTextColor(bool current, bool selected) const;
    QRectF dateIndicatorRectForCell(const QRectF& cell) const;
    QRectF contentIndicatorRectForCell(const QRectF& cell) const;
    QString titleTextForLevel(CalendarContentLevel level, const QDate& visibleMonth) const;
    int decadeStartYear(const QDate& visibleMonth) const;
    DayPageKey dayPageKey(const QDate& visibleMonth) const;
    MonthPageKey monthPageKey(const QDate& visibleMonth) const;
    YearPageKey yearPageKey(const QDate& visibleMonth) const;
    QDate monthForDayPage(const DayPageKey& key) const;
    QRect navigationButtonAt(const QPoint& pos) const;
    bool adjustedTransitionHitPosition(const QPoint& pos, const QRect& bounds, QPoint* adjustedPos) const;
    int contentCellIndexAt(const QPoint& pos) const;
    QDate monthAt(const QPoint& pos) const;
    int yearAt(const QPoint& pos) const;
    bool dateInGrid(const QDate& date) const;
    QDate focusFallbackDate(bool includeFirstSelectable = true) const;
    QDate normalizeDateForRange(const QDate& date) const;
    QDate boundedMonthForRange(const QDate& month) const;
    void enforceSelectedDateInRange();
    void moveMonth(int months, bool showFocusIndicator = false);
    void moveYear(int years);
    void moveYearPage(int pages);
    void navigatePage(int pages, bool showFocusIndicator = false);
    void resetScroll();
    qreal scrollRowHeight() const;
    qreal scrollRowOffset() const;
    int visibleRowCount() const;
    QDate scrollDateAtRow(qint64 row) const;
    QDate contentDateForCell(int index) const;
    qreal boundedScrollPosition(qreal position) const;
    void setScrollPosition(qreal position);
    void finishScrollUpdates();
    QDate shiftedVisibleMonth(int pages) const;
    QDate shiftedVisibleMonthFrom(const QDate& originMonth, int pages) const;
    QDate transitionTargetVisibleMonth() const;
    void finishMonthTransition();
    void refreshHoverFromCursor();
    bool updateHoverStateAt(const QPoint& pos);
    void clearHoverState();
    void clearContentInteractionState();
    void startMonthTransition(const QDate& previousMonth,
        const QDate& currentMonth,
        qreal startProgress = 0.0,
        bool animate = true);
    bool monthTransitionActive() const;
    void startContentTransition(CalendarContentLevel previousLevel, CalendarContentLevel currentLevel);
    bool contentTransitionActive() const;
    void switchToParentContentLevel();
    void moveFocusByDays(int days);
    void activateDate(const QDate& date);
    void refreshProperties(bool includeCells = true);

    QDate m_visibleMonth;
    QDate m_selectedDate;
    bool m_compact = false;
    bool m_rangeHighlightEnabled = false;
    QDate m_highlightedStartDate;
    QDate m_highlightedEndDate;
    QDate m_minDate;
    QDate m_maxDate;
    QDate m_hoveredDate;
    QDate m_pressedDate;
    QDate m_focusedDate;
    bool m_focusIndicatorVisible = false;
    QRect m_hoveredButton;
    QRect m_pressedButton;
    bool m_titleHovered = false;
    bool m_titlePressed = false;
    QDate m_hoveredMonth;
    QDate m_pressedMonth;
    int m_hoveredYear = 0;
    int m_pressedYear = 0;
    QDate m_previousVisibleMonth;
    QDate m_transitionVisibleMonth;
    qreal m_monthTransitionProgress = 1.0;
    int m_monthTransitionDirection = 0;
    QVariantAnimation* m_monthTransitionAnimation = nullptr;
    QDate m_scrollOrigin;
    qreal m_scrollPosition = 0.0;
    qreal m_scrollTarget = 0.0;
    QVariantAnimation* m_scrollAnimation = nullptr;
    QTimer* m_scrollSettleTimer = nullptr;
    bool m_scrollPropertiesDirty = false;
    bool m_scrollAccessibilityDirty = false;
    CalendarContentLevel m_contentLevel = CalendarContentLevel::Day;
    bool m_frameVisible = true;
    CalendarContentLevel m_previousContentLevel = CalendarContentLevel::Day;
    QDate m_previousContentVisibleMonth;
    qreal m_contentTransitionProgress = 1.0;
    int m_contentTransitionDirection = 0;
    QVariantAnimation* m_contentTransitionAnimation = nullptr;
    QLocale m_observedLocale;
    Qt::DayOfWeek m_firstDayOfWeek = Qt::Monday;
    bool m_firstDayFollowsLocale = true;
};

} // namespace fluent::date_time

#endif // CALENDARVIEW_H
