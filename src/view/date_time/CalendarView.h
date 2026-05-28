#ifndef CALENDARVIEW_H
#define CALENDARVIEW_H

#include <QDate>
#include <QLocale>
#include <QRect>
#include <QRectF>
#include <QWidget>

#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"

class QPainter;
class QVariantAnimation;
class QWheelEvent;

namespace view::date_time {

/**
 * @brief Fluent calendar surface for browsing and selecting dates across levels.
 * zh_CN: 用于按日、月、年层级浏览和选择日期的 Fluent 日历视图。
 *
 * CalendarView owns visible range, selected date, min/max bounds, first-day rules,
 * content-level transitions, keyboard focus, and frame painting without depending on picker chrome.
 * zh_CN: CalendarView 独立管理可见范围、选中日期、最小/最大边界、一周起始日、
 * 内容层级过渡、键盘焦点和外框绘制，不绑定 picker 外壳。
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
     * @brief First day used when laying out calendar weeks.
     * zh_CN: 日历周布局使用的一周起始日。
     */
    Q_PROPERTY(Qt::DayOfWeek firstDayOfWeek READ firstDayOfWeek WRITE setFirstDayOfWeek NOTIFY firstDayOfWeekChanged)
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
    enum class CalendarContentLevel {
        Day,
        Month,
        Year
    };
    Q_ENUM(CalendarContentLevel)

    explicit CalendarView(QWidget* parent = nullptr);
    ~CalendarView() override;

    QDate selectedDate() const { return m_selectedDate; }
    QDate visibleMonth() const { return m_visibleMonth; }
    QDate minDate() const { return m_minDate; }
    QDate maxDate() const { return m_maxDate; }
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
    void setFirstDayOfWeek(Qt::DayOfWeek day);
    void setContentLevel(CalendarContentLevel level);
    void setFrameVisible(bool visible);

signals:
    void selectedDateChanged(const QDate& date);
    void visibleMonthChanged(const QDate& month);
    void minDateChanged(const QDate& date);
    void maxDateChanged(const QDate& date);
    void firstDayOfWeekChanged(Qt::DayOfWeek day);
    void contentLevelChanged(CalendarContentLevel level);
    void frameVisibleChanged(bool visible);
    void dateActivated(const QDate& date);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    void onThemeUpdated() override;

private:
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

    enum class WheelGestureKind {
        None,
        PhaseBased,
        NoPhasePixel,
        NoPhaseDiscrete
    };

    void paintHeader(QPainter& painter);
    void paintTitleButtonBackground(QPainter& painter);
    void paintTitleForLevel(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintNavButton(QPainter& painter, const QRect& buttonRect, const QString& glyph);
    void paintWeekdays(QPainter& painter);
    void paintContent(QPainter& painter);
    void paintContentLevel(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintContentLevelZoom(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal scale, qreal opacity);
    void paintDayContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintMonthDays(QPainter& painter, const QDate& month, qreal yOffset, qreal opacity);
    void paintMonthContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintYearContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity);
    void paintContentCellChrome(QPainter& painter, const QRectF& cell, bool current,
                                bool selected, bool hovered, bool pressed);
    QColor contentCellTextColor(bool current, bool selected) const;
    QRectF dateIndicatorRectForCell(const QRectF& cell) const;
    QString titleTextForLevel(CalendarContentLevel level, const QDate& visibleMonth) const;
    int decadeStartYear(const QDate& visibleMonth) const;
    DayPageKey dayPageKey(const QDate& visibleMonth) const;
    MonthPageKey monthPageKey(const QDate& visibleMonth) const;
    YearPageKey yearPageKey(const QDate& visibleMonth) const;
    QDate monthForDayPage(const DayPageKey& key) const;
    QRect navigationButtonAt(const QPoint& pos) const;
    bool adjustedTransitionHitPosition(const QPoint& pos, const QRect& bounds, QPoint* adjustedPos) const;
    int contentCellIndexAt(const QPoint& pos) const;
    int monthAt(const QPoint& pos) const;
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
    void resetWheelGesture();
    QDate shiftedVisibleMonth(int pages) const;
    QDate shiftedVisibleMonthFrom(const QDate& originMonth, int pages) const;
    QDate transitionTargetVisibleMonth() const;
    void finishMonthTransition();
    void refreshHoverFromCursor();
    bool updateHoverStateAt(const QPoint& pos);
    void clearHoverState();
    void clearContentInteractionState();
    void startMonthTransition(const QDate& previousMonth, const QDate& currentMonth,
                              qreal startProgress = 0.0, bool animate = true);
    bool monthTransitionActive() const;
    void startContentTransition(CalendarContentLevel previousLevel, CalendarContentLevel currentLevel);
    bool contentTransitionActive() const;
    void switchToParentContentLevel();
    void moveFocusByDays(int days);
    void activateDate(const QDate& date);
    void refreshProperties();

    QDate m_visibleMonth;
    QDate m_selectedDate;
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
    int m_hoveredMonth = -1;
    int m_pressedMonth = -1;
    int m_hoveredYear = 0;
    int m_pressedYear = 0;
    QDate m_previousVisibleMonth;
    QDate m_transitionVisibleMonth;
    qreal m_monthTransitionProgress = 1.0;
    int m_monthTransitionDirection = 0;
    QVariantAnimation* m_monthTransitionAnimation = nullptr;
    qreal m_wheelAccum = 0.0;
    int m_wheelDir = 0;
    WheelGestureKind m_wheelGestureKind = WheelGestureKind::None;
    bool m_wheelGestureCommitted = false;
    qint64 m_lastWheelTs = 0;
    CalendarContentLevel m_contentLevel = CalendarContentLevel::Day;
    bool m_frameVisible = true;
    CalendarContentLevel m_previousContentLevel = CalendarContentLevel::Day;
    QDate m_previousContentVisibleMonth;
    qreal m_contentTransitionProgress = 1.0;
    int m_contentTransitionDirection = 0;
    QVariantAnimation* m_contentTransitionAnimation = nullptr;
    Qt::DayOfWeek m_firstDayOfWeek = QLocale().firstDayOfWeek();
};

} // namespace view::date_time

#endif // CALENDARVIEW_H
