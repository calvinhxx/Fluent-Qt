#include "compatibility/InputEventCompat.h"
#include "CalendarView.h"

#include "private/CalendarViewAccessibility_p.h"

#include <cmath>

#include <QApplication>
#include <QHideEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QVariantMap>
#include <QWheelEvent>

#include "components/foundation/private/MotionPolicy_p.h"
#include "design/Typography.h"

namespace fluent::date_time {

namespace {

constexpr int kCalendarCardWidth = 320;
constexpr int kCalendarHeaderHeight = 52;
constexpr int kWeekHeaderHeight = 36;
constexpr int kDayCellHeight = 44;
constexpr int kCalendarCardHeight = kCalendarHeaderHeight + kWeekHeaderHeight + kDayCellHeight * 6;
constexpr int kNavButtonSize = 32;
constexpr qreal kDateIndicatorDiameter = 34.0;
constexpr qreal kContentIndicatorDiameter = 52.0;
constexpr int kContentColumns = 3;
constexpr int kContentRows = 4;

QDate firstOfMonth(const QDate& date)
{
    if (!date.isValid())
        return QDate();
    return QDate(date.year(), date.month(), 1);
}

QDate todayMonth()
{
    return firstOfMonth(QDate::currentDate());
}

QColor withOpacity(QColor color, qreal opacity)
{
    color.setAlphaF(color.alphaF() * qBound(0.0, opacity, 1.0));
    return color;
}

Qt::DayOfWeek normalizeDayOfWeek(Qt::DayOfWeek day, Qt::DayOfWeek fallback)
{
    if (day < Qt::Monday || day > Qt::Sunday)
        return fallback;
    return day;
}

int dayOffset(Qt::DayOfWeek day, Qt::DayOfWeek firstDay)
{
    return (static_cast<int>(day) - static_cast<int>(firstDay) + 7) % 7;
}

QDate gridStartForMonth(const QDate& month, Qt::DayOfWeek firstDay)
{
    const QDate first = firstOfMonth(month);
    const int offset = dayOffset(static_cast<Qt::DayOfWeek>(first.dayOfWeek()), firstDay);
    return first.addDays(-offset);
}

QString weekdayLabel(const QLocale& locale, Qt::DayOfWeek day)
{
    QString label = locale.standaloneDayName(day, QLocale::ShortFormat);
    if (label.size() > 2)
        label = label.left(2);
    return label;
}

int contentLevelDepth(CalendarView::CalendarContentLevel level)
{
    switch (level) {
    case CalendarView::CalendarContentLevel::Day:
        return 0;
    case CalendarView::CalendarContentLevel::Month:
        return 1;
    case CalendarView::CalendarContentLevel::Year:
        return 2;
    }
    return 0;
}

} // namespace

CalendarView::CalendarView(QWidget* parent) : QWidget(parent)
{
    detail::ensureCalendarViewAccessibilityFactory();
    m_observedLocale = QWidget::locale();
    m_firstDayOfWeek = m_observedLocale.firstDayOfWeek();
    setObjectName(QStringLiteral("CalendarView"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(sizeHint());
    m_firstDayOfWeek = normalizeDayOfWeek(m_firstDayOfWeek, m_observedLocale.firstDayOfWeek());
    m_visibleMonth = todayMonth();
    m_focusedDate = QDate::currentDate();
    m_monthTransitionAnimation = new QVariantAnimation(this);
    m_monthTransitionAnimation->setDuration(themeAnimation().normal);
    m_monthTransitionAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_monthTransitionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_monthTransitionProgress = value.toReal();
        refreshProperties();
        update();
    });
    connect(m_monthTransitionAnimation, &QVariantAnimation::finished, this, [this]() {
        finishMonthTransition();
        refreshProperties();
        update();
        refreshHoverFromCursor();
    });
    m_scrollAnimation = new QVariantAnimation(this);
    m_scrollAnimation->setObjectName(QStringLiteral("calendarScrollAnimation"));
    m_scrollAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_scrollAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setScrollPosition(value.toReal());
    });
    m_scrollSettleTimer = new QTimer(this);
    m_scrollSettleTimer->setSingleShot(true);
    m_scrollSettleTimer->setInterval(themeAnimation().fast);
    connect(m_scrollSettleTimer, &QTimer::timeout, this, &CalendarView::finishScrollUpdates);
    connect(m_scrollAnimation, &QVariantAnimation::finished, this, [this]() {
        finishScrollUpdates();
        refreshHoverFromCursor();
    });
    m_contentTransitionAnimation = new QVariantAnimation(this);
    m_contentTransitionAnimation->setDuration(themeAnimation().normal);
    m_contentTransitionAnimation->setEasingCurve(themeAnimation().decelerate);
    connect(m_contentTransitionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_contentTransitionProgress = value.toReal();
        refreshProperties();
        update();
    });
    connect(m_contentTransitionAnimation, &QVariantAnimation::finished, this, [this]() {
        m_contentTransitionProgress = 1.0;
        m_contentTransitionDirection = 0;
        m_previousContentVisibleMonth = QDate();
        m_previousContentLevel = m_contentLevel;
        refreshProperties();
        update();
    });
    refreshProperties();
}

CalendarView::~CalendarView()
{
    m_scrollAnimation->stop();
    if (m_monthTransitionAnimation)
        m_monthTransitionAnimation->stop();
    if (m_contentTransitionAnimation)
        m_contentTransitionAnimation->stop();
}

QRect CalendarView::previousButtonRect() const
{
    return m_compact ? QRect(width() - 64, 6, 28, 28) : QRect(width() - 82, 10, kNavButtonSize, kNavButtonSize);
}

QRect CalendarView::nextButtonRect() const
{
    return m_compact ? QRect(width() - 32, 6, 28, 28) : QRect(width() - 44, 10, kNavButtonSize, kNavButtonSize);
}

QRect CalendarView::titleButtonRect() const
{
    const int maxRight = previousButtonRect().left() - (m_compact ? 4 : 12);
    const int titleLeft = m_compact ? 0 : 16;
    const int buttonWidth = qMax(72, maxRight - titleLeft);
    return QRect(titleLeft, m_compact ? 6 : 10, buttonWidth, m_compact ? 28 : kNavButtonSize);
}

QRect CalendarView::gridRect() const
{
    return QRect(0, headerHeight() + weekHeaderHeight(), width(), dayCellHeight() * 6);
}

QRect CalendarView::contentRect() const
{
    return QRect(0, headerHeight(), width(), height() - headerHeight());
}

QRect CalendarView::cellRect(int row, int column) const
{
    const QRect grid = gridRect();
    const int cellW = grid.width() / 7;
    const int extra = grid.width() - cellW * 7;
    const int x = grid.left() + column * cellW + qMin(column, extra);
    const int w = cellW + (column < extra ? 1 : 0);
    const int offset = m_contentLevel == CalendarContentLevel::Day ? qRound(scrollRowOffset()) : 0;
    return QRect(x, grid.top() + row * dayCellHeight() - offset, w, dayCellHeight());
}

QRect CalendarView::contentCellRect(int row, int column) const
{
    const QRect area = contentRect();
    const int cellW = area.width() / kContentColumns;
    const int extraW = area.width() - cellW * kContentColumns;
    const int x = area.left() + column * cellW + qMin(column, extraW);
    const int w = cellW + (column < extraW ? 1 : 0);
    const qreal rowHeight = qreal(area.height()) / kContentRows;
    const qreal offset = m_contentLevel == CalendarContentLevel::Day ? 0.0 : scrollRowOffset();
    const int y = area.top() + qRound(row * rowHeight - offset);
    const int bottom = area.top() + qRound((row + 1) * rowHeight - offset);
    return QRect(x, y, w, bottom - y);
}

QRect CalendarView::dateCellRect(const QDate& date) const
{
    if (!date.isValid())
        return QRect();

    const QDate start = gridStartDate();
    const int offset = start.daysTo(date);
    if (offset < 0 || offset >= (m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42))
        return QRect();
    return cellRect(offset / 7, offset % 7).intersected(gridRect());
}

QDate CalendarView::gridStartDate() const
{
    if (m_scrollOrigin.isValid() && m_contentLevel == CalendarContentLevel::Day)
        return scrollDateAtRow(qint64(std::floor(m_scrollPosition)));
    return gridStartForMonth(monthForDayPage(dayPageKey(m_visibleMonth)), m_firstDayOfWeek);
}

QDate CalendarView::dateAt(const QPoint& pos) const
{
    QPoint hitPos;
    if (!adjustedTransitionHitPosition(pos, gridRect(), &hitPos))
        return QDate();

    const QRect grid = gridRect();
    const int row = (hitPos.y() - grid.top() + qRound(scrollRowOffset())) / dayCellHeight();
    for (int column = 0; column < 7; ++column) {
        if (cellRect(row, column).contains(hitPos))
            return gridStartDate().addDays(row * 7 + column);
    }
    return QDate();
}

bool CalendarView::isDateSelectable(const QDate& date) const
{
    if (!date.isValid())
        return false;
    if (m_minDate.isValid() && date < m_minDate)
        return false;
    if (m_maxDate.isValid() && date > m_maxDate)
        return false;
    return true;
}

bool CalendarView::isMonthSelectable(int year, int month) const
{
    const QDate first(year, month, 1);
    if (!first.isValid())
        return false;
    const QDate last = first.addDays(first.daysInMonth() - 1);
    if (m_minDate.isValid() && last < m_minDate)
        return false;
    if (m_maxDate.isValid() && first > m_maxDate)
        return false;
    return true;
}

bool CalendarView::isYearSelectable(int year) const
{
    const QDate first(year, 1, 1);
    const QDate last(year, 12, 31);
    if (!first.isValid() || !last.isValid())
        return false;
    if (m_minDate.isValid() && last < m_minDate)
        return false;
    if (m_maxDate.isValid() && first > m_maxDate)
        return false;
    return true;
}

QSize CalendarView::sizeHint() const
{
    return m_compact ? QSize(252, 256) : QSize(kCalendarCardWidth, kCalendarCardHeight);
}

QSize CalendarView::minimumSizeHint() const
{
    return sizeHint();
}

void CalendarView::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    resetScroll();
    m_compact = compact;
    setFixedSize(sizeHint());
    refreshProperties();
    updateGeometry();
    update();
}

int CalendarView::headerHeight() const
{
    return m_compact ? 40 : kCalendarHeaderHeight;
}
int CalendarView::weekHeaderHeight() const
{
    return m_compact ? 24 : kWeekHeaderHeight;
}
int CalendarView::dayCellHeight() const
{
    return m_compact ? 32 : kDayCellHeight;
}

void CalendarView::setSelectedDate(const QDate& date)
{
    const QDate normalized = normalizeDateForRange(date);
    if (m_selectedDate == normalized)
        return;

    m_selectedDate = normalized;
    if (m_selectedDate.isValid())
        m_focusedDate = m_selectedDate;
    m_focusIndicatorVisible = false;
    if (!m_focusedDate.isValid() || !dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit selectedDateChanged(m_selectedDate);
    detail::notifyCalendarViewAccessibilitySelection(this);
}

void CalendarView::setHighlightedDateRange(const QDate& start, const QDate& end)
{
    if (m_rangeHighlightEnabled && m_highlightedStartDate == start && m_highlightedEndDate == end)
        return;
    m_rangeHighlightEnabled = true;
    m_highlightedStartDate = start;
    m_highlightedEndDate = end;
    refreshProperties();
    update();
    detail::notifyCalendarViewAccessibilitySelection(this);
}

bool CalendarView::isHighlightedDate(const QDate& date) const
{
    if (!m_rangeHighlightEnabled)
        return date == m_selectedDate;
    if (date == m_highlightedStartDate || date == m_highlightedEndDate)
        return date.isValid();
    return m_highlightedStartDate.isValid() && m_highlightedEndDate.isValid() && date > m_highlightedStartDate
        && date < m_highlightedEndDate;
}

void CalendarView::setVisibleMonth(const QDate& month)
{
    const QDate bounded = boundedMonthForRange(month);
    if (m_visibleMonth == bounded)
        return;

    const QDate previousMonth = m_visibleMonth;
    resetScroll();
    finishMonthTransition();
    m_visibleMonth = bounded;
    m_focusIndicatorVisible = false;
    startMonthTransition(previousMonth, bounded);
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit visibleMonthChanged(m_visibleMonth);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::setMinDate(const QDate& date)
{
    setDateRange(date, m_maxDate);
}

void CalendarView::setMaxDate(const QDate& date)
{
    setDateRange(m_minDate, date);
}

void CalendarView::setDateRange(const QDate& minDate, const QDate& maxDate)
{
    QDate nextMin = minDate;
    QDate nextMax = maxDate;
    if (nextMin.isValid() && nextMax.isValid() && nextMax < nextMin)
        nextMax = nextMin;

    const bool minChanged = m_minDate != nextMin;
    const bool maxChanged = m_maxDate != nextMax;
    if (!minChanged && !maxChanged)
        return;

    resetScroll();
    m_minDate = nextMin;
    m_maxDate = nextMax;
    enforceSelectedDateInRange();
    setVisibleMonth(m_visibleMonth);
    if (!isDateSelectable(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    if (minChanged)
        emit minDateChanged(m_minDate);
    if (maxChanged)
        emit maxDateChanged(m_maxDate);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::setLocale(const QLocale& locale)
{
    if (QWidget::locale() == locale)
        return;
    QWidget::setLocale(locale);
}

void CalendarView::setFirstDayOfWeek(Qt::DayOfWeek day)
{
    const Qt::DayOfWeek normalized = normalizeDayOfWeek(day, locale().firstDayOfWeek());
    m_firstDayFollowsLocale = false;
    if (m_firstDayOfWeek == normalized)
        return;

    resetScroll();
    m_firstDayOfWeek = normalized;
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit firstDayOfWeekChanged(m_firstDayOfWeek);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::resetFirstDayOfWeek()
{
    m_firstDayFollowsLocale = true;
    const Qt::DayOfWeek automaticDay = locale().firstDayOfWeek();
    if (m_firstDayOfWeek == automaticDay)
        return;
    resetScroll();
    m_firstDayOfWeek = automaticDay;
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit firstDayOfWeekChanged(m_firstDayOfWeek);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::setContentLevel(CalendarContentLevel level)
{
    if (m_contentLevel == level)
        return;

    const CalendarContentLevel previousLevel = m_contentLevel;
    m_contentLevel = level;
    resetScroll();
    startContentTransition(previousLevel, level);
    clearContentInteractionState();
    refreshProperties();
    update();
    emit contentLevelChanged(m_contentLevel);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::setFrameVisible(bool visible)
{
    if (m_frameVisible == visible)
        return;

    m_frameVisible = visible;
    refreshProperties();
    update();
    emit frameVisibleChanged(m_frameVisible);
}

void CalendarView::paintEvent(QPaintEvent*)
{
    if (m_scrollPropertiesDirty)
        refreshProperties();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();

    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath cardPath;
    cardPath.addRoundedRect(cardRect, themeRadius().control, themeRadius().control);
    painter.fillPath(cardPath, colors.bgLayer);

    painter.save();
    painter.setClipPath(cardPath);
    paintHeader(painter);
    paintContent(painter);
    painter.restore();

    if (m_frameVisible) {
        painter.setPen(QPen(colors.strokeDefault, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(cardPath);
    }
}

void CalendarView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refreshProperties();
}

void CalendarView::hideEvent(QHideEvent* event)
{
    m_scrollAnimation->stop();
    m_scrollTarget = m_scrollPosition;
    finishScrollUpdates();
    QWidget::hideEvent(event);
}

void CalendarView::mouseMoveEvent(QMouseEvent* event)
{
    if (updateHoverStateAt(fluentMousePos(event)))
        update();
    QWidget::mouseMoveEvent(event);
}

void CalendarView::leaveEvent(QEvent* event)
{
    clearHoverState();
    update();
    QWidget::leaveEvent(event);
}

void CalendarView::mousePressEvent(QMouseEvent* event)
{
    const QPoint pos = fluentMousePos(event);
    if (event->button() == Qt::LeftButton) {
        m_scrollAnimation->stop();
        m_scrollTarget = m_scrollPosition;
        if (previousButtonRect().contains(pos)) {
            m_pressedButton = previousButtonRect();
            update();
            event->accept();
            return;
        }
        if (nextButtonRect().contains(pos)) {
            m_pressedButton = nextButtonRect();
            update();
            event->accept();
            return;
        }

        if (titleButtonRect().contains(pos)) {
            m_titlePressed = true;
            update();
            event->accept();
            return;
        }

        if (m_contentLevel == CalendarContentLevel::Day) {
            const QDate pressedDate = dateAt(pos);
            if (isDateSelectable(pressedDate)) {
                const bool alreadyFocused = hasFocus();
                m_pressedDate = pressedDate;
                m_focusedDate = pressedDate;
                m_focusIndicatorVisible = false;
                refreshProperties();
                setFocus(Qt::MouseFocusReason);
                update();
                if (alreadyFocused)
                    detail::notifyCalendarViewAccessibilityFocus(this);
                event->accept();
                return;
            }
        } else if (m_contentLevel == CalendarContentLevel::Month) {
            const QDate pressedMonth = monthAt(pos);
            if (pressedMonth.isValid()) {
                m_pressedMonth = pressedMonth;
                update();
                event->accept();
                return;
            }
        } else if (m_contentLevel == CalendarContentLevel::Year) {
            const int pressedYear = yearAt(pos);
            if (pressedYear > 0) {
                m_pressedYear = pressedYear;
                update();
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void CalendarView::mouseReleaseEvent(QMouseEvent* event)
{
    const QPoint pos = fluentMousePos(event);
    if (event->button() == Qt::LeftButton) {
        if (m_pressedButton.isValid()) {
            const QRect button = m_pressedButton;
            m_pressedButton = QRect();
            if (button == previousButtonRect() && button.contains(pos)) {
                navigatePage(-1);
                event->accept();
                return;
            }
            if (button == nextButtonRect() && button.contains(pos)) {
                navigatePage(1);
                event->accept();
                return;
            }
            update();
        }

        if (m_titlePressed) {
            m_titlePressed = false;
            if (titleButtonRect().contains(pos)) {
                switchToParentContentLevel();
                event->accept();
                return;
            }
            update();
        }

        if (m_pressedDate.isValid()) {
            const QDate date = m_pressedDate;
            m_pressedDate = QDate();
            if (date == dateAt(pos) && isDateSelectable(date)) {
                activateDate(date);
                event->accept();
                return;
            }
            update();
        }

        if (m_pressedMonth.isValid()) {
            const QDate month = m_pressedMonth;
            m_pressedMonth = QDate();
            if (month == monthAt(pos)) {
                setVisibleMonth(month);
                setContentLevel(CalendarContentLevel::Day);
                event->accept();
                return;
            }
            update();
        }

        if (m_pressedYear > 0) {
            const int year = m_pressedYear;
            m_pressedYear = 0;
            if (year == yearAt(pos)) {
                setVisibleMonth(QDate(year, m_visibleMonth.month(), 1));
                setContentLevel(CalendarContentLevel::Month);
                event->accept();
                return;
            }
            update();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void CalendarView::wheelEvent(QWheelEvent* event)
{
    const QPoint pos = fluentWheelPosition(event).toPoint();
    if (!isEnabled() || !contentRect().contains(pos)) {
        QWidget::wheelEvent(event);
        return;
    }

    // Pixel packets already carry the platform's gesture and momentum curve.
    // Interpolate only discrete wheel steps, without discarding their tails.
    // zh_CN: 像素事件自带系统手势与惯性曲线；仅对离散滚轮插值，不吞掉后续输入。
    const bool direct = !event->pixelDelta().isNull() || event->phase() != Qt::NoScrollPhase;
    if (event->phase() == Qt::ScrollBegin) {
        m_scrollAnimation->stop();
        m_scrollTarget = m_scrollPosition;
    }
    const qreal pixels = !event->pixelDelta().isNull()
        ? event->pixelDelta().y()
        : event->angleDelta().y() / 120.0 * qMin(3, QApplication::wheelScrollLines()) * scrollRowHeight();
    if (qFuzzyIsNull(pixels)) {
        if (event->phase() == Qt::ScrollEnd)
            finishScrollUpdates();
        event->accept();
        return;
    }
    if (!m_scrollOrigin.isValid()) {
        finishMonthTransition();
        m_contentTransitionAnimation->stop();
        m_contentTransitionProgress = 1.0;
        m_contentTransitionDirection = 0;
        m_previousContentVisibleMonth = QDate();
        m_scrollOrigin = m_contentLevel == CalendarContentLevel::Day ? gridStartDate()
            : m_contentLevel == CalendarContentLevel::Month
            ? QDate(m_visibleMonth.year(), 1, 1)
            : QDate(decadeStartYear(m_visibleMonth), 1, 1);
    }
    clearContentInteractionState();
    m_focusIndicatorVisible = false;
    m_scrollAnimation->stop();
    const qreal delta = -pixels / scrollRowHeight();
    // Reversing direction starts at the current presentation, not at a queued target.
    // zh_CN: 反向输入从当前画面位置开始，不先走完尚未到达的目标。
    const qreal base = direct || (m_scrollTarget - m_scrollPosition) * delta < 0.0 ? m_scrollPosition : m_scrollTarget;
    m_scrollTarget = boundedScrollPosition(base + delta);
    if (direct || !isVisible()) {
        setScrollPosition(m_scrollTarget);
        if (event->phase() == Qt::ScrollEnd)
            finishScrollUpdates();
    } else {
        m_scrollAnimation->setStartValue(m_scrollPosition);
        m_scrollAnimation->setEndValue(m_scrollTarget);
        ::fluent::detail::startMotionTransition(m_scrollAnimation, themeAnimation().fast);
    }
    event->accept();
}

void CalendarView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        moveFocusByDays(-1);
        event->accept();
        return;
    case Qt::Key_Right:
        moveFocusByDays(1);
        event->accept();
        return;
    case Qt::Key_Up:
        moveFocusByDays(-7);
        event->accept();
        return;
    case Qt::Key_Down:
        moveFocusByDays(7);
        event->accept();
        return;
    case Qt::Key_PageUp:
        navigatePage(-1, true);
        event->accept();
        return;
    case Qt::Key_PageDown:
        navigatePage(1, true);
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        activateDate(m_focusedDate);
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void CalendarView::focusInEvent(QFocusEvent* event)
{
    if (!m_focusedDate.isValid())
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    detail::notifyCalendarViewAccessibilityFocus(this);
    QWidget::focusInEvent(event);
}

void CalendarView::focusOutEvent(QFocusEvent* event)
{
    update();
    QWidget::focusOutEvent(event);
}

void CalendarView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        m_scrollAnimation->stop();
        m_scrollTarget = m_scrollPosition;
    }
    if (event->type() != QEvent::LocaleChange || m_observedLocale == QWidget::locale()) {
        return;
    }

    resetScroll();
    m_observedLocale = QWidget::locale();
    if (m_firstDayFollowsLocale) {
        const Qt::DayOfWeek automaticDay = m_observedLocale.firstDayOfWeek();
        if (m_firstDayOfWeek != automaticDay) {
            resetScroll();
            m_firstDayOfWeek = automaticDay;
            emit firstDayOfWeekChanged(m_firstDayOfWeek);
        }
    }
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    updateGeometry();
    update();
    emit localeChanged(m_observedLocale);
    detail::notifyCalendarViewAccessibilityReset(this);
}

void CalendarView::onThemeUpdated()
{
    update();
}

void CalendarView::paintHeader(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    paintTitleButtonBackground(painter);
    if (contentTransitionActive()) {
        paintTitleForLevel(painter, m_contentLevel, m_visibleMonth, 0.0, 1.0);
    } else {
        paintTitleForLevel(painter, m_contentLevel, transitionTargetVisibleMonth(), 0.0, 1.0);
    }

    paintNavButton(painter, previousButtonRect(), m_compact ? Typography::Icons::ChevronLeft : Typography::Icons::Up);
    paintNavButton(painter, nextButtonRect(), m_compact ? Typography::Icons::ChevronRight : Typography::Icons::Down);

    painter.setPen(colors.strokeDivider);
    painter.drawLine(QPoint(0, headerHeight() - 1), QPoint(width(), headerHeight() - 1));
}

void CalendarView::paintTitleButtonBackground(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    QColor bg = Qt::transparent;
    if (m_titlePressed)
        bg = colors.subtleTertiary;
    else if (m_titleHovered)
        bg = colors.subtleSecondary;

    if (bg.alpha() <= 0)
        return;

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(titleButtonRect(), themeRadius().control, themeRadius().control);
}

void CalendarView::paintTitleForLevel(QPainter& painter,
    CalendarContentLevel level,
    const QDate& visibleMonth,
    qreal yOffset,
    qreal opacity)
{
    if (!visibleMonth.isValid())
        return;

    const auto& colors = themeColorsRef();
    const qreal inset = m_compact ? 8.0 : 12.0;
    const QRectF titleRect = QRectF(titleButtonRect()).adjusted(inset, yOffset, -inset, yOffset);
    painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());
    painter.setPen(withOpacity(colors.textPrimary, opacity));
    painter.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, titleTextForLevel(level, visibleMonth));
}

void CalendarView::paintNavButton(QPainter& painter, const QRect& buttonRect, const QString& glyph)
{
    const auto& colors = themeColorsRef();
    QColor bg = Qt::transparent;
    if (m_pressedButton == buttonRect)
        bg = colors.subtleTertiary;
    else if (m_hoveredButton == buttonRect)
        bg = colors.subtleSecondary;

    if (bg.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bg);
        painter.drawRoundedRect(buttonRect.adjusted(4, 4, -4, -4), themeRadius().control, themeRadius().control);
    }

    painter.setPen(colors.textPrimary);
    Typography::Icons::paintGlyph(painter, QRectF(buttonRect), glyph, 10, Qt::AlignCenter);
}

void CalendarView::paintWeekdays(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
    painter.setPen(colors.textPrimary);
    const int y = headerHeight();
    for (int c = 0; c < 7; ++c) {
        const int day = ((static_cast<int>(m_firstDayOfWeek) - 1 + c) % 7) + 1;
        QRect rect = cellRect(0, c);
        rect.moveTop(y);
        if (m_compact)
            rect.setHeight(weekHeaderHeight());
        painter.drawText(rect, Qt::AlignCenter, weekdayLabel(locale(), static_cast<Qt::DayOfWeek>(day)));
    }
}

void CalendarView::paintContent(QPainter& painter)
{
    if (contentTransitionActive()) {
        const QRect area = contentRect();
        const qreal progress = qBound(0.0, m_contentTransitionProgress, 1.0);
        const bool zoomOut = m_contentTransitionDirection > 0;
        const qreal currentScale = zoomOut ? 1.06 - 0.06 * progress : 0.94 + 0.06 * progress;
        const qreal currentOpacity = 0.92 + 0.08 * progress;
        painter.save();
        painter.setClipRect(area);
        paintContentLevelZoom(painter, m_contentLevel, m_visibleMonth, currentScale, currentOpacity);
        painter.restore();
        return;
    }

    if (monthTransitionActive()) {
        if (m_contentLevel == CalendarContentLevel::Day) {
            paintContentLevel(painter, m_contentLevel, m_visibleMonth, 0.0, 1.0);
            return;
        }

        const QRect area = contentRect();
        const qreal travel = area.height();
        painter.save();
        painter.setClipRect(area);
        paintContentLevel(painter, m_contentLevel, m_previousVisibleMonth,
            -m_monthTransitionDirection * m_monthTransitionProgress * travel, 1.0);
        paintContentLevel(painter, m_contentLevel, transitionTargetVisibleMonth(),
            m_monthTransitionDirection * (1.0 - m_monthTransitionProgress) * travel, 1.0);
        painter.restore();
        return;
    }

    painter.save();
    painter.setClipRect(contentRect());
    paintContentLevel(painter, m_contentLevel, m_visibleMonth, 0.0, 1.0);
    painter.restore();
}

void CalendarView::paintContentLevel(QPainter& painter,
    CalendarContentLevel level,
    const QDate& visibleMonth,
    qreal yOffset,
    qreal opacity)
{
    switch (level) {
    case CalendarContentLevel::Day:
        paintDayContent(painter, visibleMonth, yOffset, opacity);
        break;
    case CalendarContentLevel::Month:
        paintMonthContent(painter, visibleMonth, yOffset, opacity);
        break;
    case CalendarContentLevel::Year:
        paintYearContent(painter, visibleMonth, yOffset, opacity);
        break;
    }
}

void CalendarView::paintContentLevelZoom(QPainter& painter,
    CalendarContentLevel level,
    const QDate& visibleMonth,
    qreal scale,
    qreal opacity)
{
    const QRectF area = contentRect();
    const QPointF center = area.center();

    painter.save();
    painter.translate(center);
    painter.scale(scale, scale);
    painter.translate(-center);
    paintContentLevel(painter, level, visibleMonth, 0.0, opacity);
    painter.restore();
}

void CalendarView::paintDayContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity)
{
    if (monthTransitionActive()) {
        const QRect grid = gridRect();
        const qreal travel = grid.height();
        painter.save();
        painter.setOpacity(opacity);
        painter.translate(0.0, yOffset);
        paintWeekdays(painter);
        painter.setClipRect(grid);
        paintMonthDays(painter, m_previousVisibleMonth,
            -m_monthTransitionDirection * m_monthTransitionProgress * travel, 1.0);
        paintMonthDays(painter, transitionTargetVisibleMonth(),
            m_monthTransitionDirection * (1.0 - m_monthTransitionProgress) * travel, 1.0);
        painter.restore();
        return;
    }

    painter.save();
    painter.setOpacity(opacity);
    painter.translate(0.0, yOffset);
    paintWeekdays(painter);
    painter.setClipRect(gridRect(), Qt::IntersectClip);
    paintMonthDays(painter, visibleMonth, 0.0, 1.0);
    painter.restore();
}

void CalendarView::paintMonthDays(QPainter& painter, const QDate& month, qreal yOffset, qreal opacity)
{
    const auto& colors = themeColorsRef();
    const QDate today = QDate::currentDate();
    const QDate start = m_scrollOrigin.isValid() ? gridStartDate() : gridStartForMonth(month, m_firstDayOfWeek);

    painter.save();
    painter.setOpacity(opacity);
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());

    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42); ++i) {
        const int row = i / 7;
        const int column = i % 7;
        const QDate date = start.addDays(i);
        const QRectF cell = QRectF(cellRect(row, column)).translated(0.0, yOffset).adjusted(2.0, 2.0, -2.0, -2.0);
        const QRectF indicatorRect = dateIndicatorRectForCell(cell);
        const bool inMonth = date.month() == month.month() && date.year() == month.year();
        const bool selectable = isDateSelectable(date);
        const bool selected = date.isValid()
            && (m_rangeHighlightEnabled
                    ? date == m_highlightedStartDate || date == m_highlightedEndDate
                    : date == m_selectedDate);
        const bool inRange = m_rangeHighlightEnabled && m_highlightedStartDate.isValid()
            && m_highlightedEndDate.isValid() && date >= m_highlightedStartDate && date <= m_highlightedEndDate;
        const bool isToday = date == today;
        const bool hovered = date == m_hoveredDate && selectable;
        const bool pressed = date == m_pressedDate && selectable;
        const bool focused = m_focusIndicatorVisible && hasFocus() && date == m_focusedDate && selectable;
        const bool current = isToday && selectable;

        QColor textColor = colors.textPrimary;
        if (!inMonth)
            textColor = colors.textTertiary;
        if (!selectable)
            textColor = colors.textDisabled;

        if (inRange && selectable && m_highlightedStartDate != m_highlightedEndDate) {
            const QRectF fullCell = QRectF(cellRect(row, column)).translated(0.0, yOffset);
            QRectF band(fullCell.left(), indicatorRect.top(), fullCell.width(), indicatorRect.height());
            if (date == m_highlightedStartDate)
                band.setLeft(indicatorRect.center().x());
            if (date == m_highlightedEndDate)
                band.setRight(indicatorRect.center().x());
            QColor fill = colors.accentDefault;
            fill.setAlphaF(0.14);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawRect(band);
        }

        // Fluent treatment. zh_CN: Fluent 样式。
        if (!current && !selected && (hovered || pressed || focused)) {
            QColor bg = pressed ? colors.subtleTertiary : colors.subtleSecondary;
            painter.setPen(Qt::NoPen);
            painter.setBrush(bg);
            painter.drawEllipse(indicatorRect);
        }

        if (selected && m_rangeHighlightEnabled && selectable) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accentDefault);
            painter.drawEllipse(indicatorRect);
            textColor = colors.textOnAccent;
        } else if (current && m_rangeHighlightEnabled) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(colors.accentDefault, 1.5));
            painter.drawEllipse(indicatorRect.adjusted(1.0, 1.0, -1.0, -1.0));
        } else if (current) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accentDefault);
            painter.drawEllipse(indicatorRect);
            textColor = colors.textOnAccent;
        } else if (selected) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(colors.accentDefault, 1.5));
            painter.drawEllipse(indicatorRect.adjusted(1.0, 1.0, -1.0, -1.0));
            textColor = colors.textAccentPrimary;
        }

        painter.setPen(textColor);
        painter.drawText(indicatorRect, Qt::AlignCenter, QString::number(date.day()));
    }
    painter.restore();
}

void CalendarView::paintMonthContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity)
{
    const QDate today = QDate::currentDate();
    painter.save();
    painter.setOpacity(opacity);
    painter.translate(0.0, yOffset);
    painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());

    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? 12 : visibleRowCount() * kContentColumns); ++i) {
        const int row = i / kContentColumns;
        const int column = i % kContentColumns;
        const QRectF cell = QRectF(contentCellRect(row, column)).adjusted(8.0, 8.0, -8.0, -8.0);
        const QRectF indicator = contentIndicatorRectForCell(cell);
        const QDate monthDate = m_scrollOrigin.isValid() ? contentDateForCell(i) : QDate(visibleMonth.year(), i + 1, 1);
        const int month = monthDate.month();
        const bool selectable = isMonthSelectable(monthDate.year(), month);
        const bool hovered = selectable && m_hoveredMonth == monthDate;
        const bool pressed = selectable && m_pressedMonth == monthDate;
        const bool selected = selectable && m_selectedDate.isValid() && m_selectedDate.year() == monthDate.year()
            && m_selectedDate.month() == month;
        const bool current = selectable && today.year() == monthDate.year() && today.month() == month;

        paintContentCellChrome(painter, indicator, current, selected, hovered, pressed);
        painter.setPen(!selectable ? themeColorsRef().textDisabled
                : !current && !selected && monthDate.year() != visibleMonth.year()
                ? themeColorsRef().textTertiary
                : contentCellTextColor(current, selected));
        painter.drawText(indicator, Qt::AlignCenter, locale().standaloneMonthName(month, QLocale::ShortFormat));
    }
    painter.restore();
}

void CalendarView::paintYearContent(QPainter& painter, const QDate& visibleMonth, qreal yOffset, qreal opacity)
{
    const QDate today = QDate::currentDate();
    const int startYear = decadeStartYear(visibleMonth);
    painter.save();
    painter.setOpacity(opacity);
    painter.translate(0.0, yOffset);
    painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());

    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? 12 : visibleRowCount() * kContentColumns); ++i) {
        const int row = i / kContentColumns;
        const int column = i % kContentColumns;
        const QRectF cell = QRectF(contentCellRect(row, column)).adjusted(8.0, 8.0, -8.0, -8.0);
        const QRectF indicator = contentIndicatorRectForCell(cell);
        const int year = m_scrollOrigin.isValid() ? contentDateForCell(i).year() : startYear + i;
        const bool selectable = isYearSelectable(year);
        const bool hovered = selectable && m_hoveredYear == year;
        const bool pressed = selectable && m_pressedYear == year;
        const bool selected = selectable && m_selectedDate.isValid() && m_selectedDate.year() == year;
        const bool current = selectable && today.year() == year;

        paintContentCellChrome(painter, indicator, current, selected, hovered, pressed);
        painter.setPen(!selectable ? themeColorsRef().textDisabled
                : !current && !selected && (year < startYear || year >= startYear + 12)
                ? themeColorsRef().textTertiary
                : contentCellTextColor(current, selected));
        painter.drawText(indicator, Qt::AlignCenter, QString::number(year));
    }
    painter.restore();
}

void CalendarView::paintContentCellChrome(QPainter& painter,
    const QRectF& indicator,
    bool current,
    bool selected,
    bool hovered,
    bool pressed)
{
    const auto& colors = themeColorsRef();

    if (current) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.accentDefault);
        painter.drawEllipse(indicator);
    } else if (hovered || pressed) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? colors.subtleTertiary : colors.subtleSecondary);
        painter.drawEllipse(indicator);
    } else if (selected) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.accentDefault, 1.5));
        painter.drawEllipse(indicator.adjusted(1.0, 1.0, -1.0, -1.0));
    }
}

QColor CalendarView::contentCellTextColor(bool current, bool selected) const
{
    const auto& colors = themeColorsRef();
    if (current)
        return colors.textOnAccent;
    if (selected)
        return colors.textAccentPrimary;
    return colors.textPrimary;
}

QRectF CalendarView::dateIndicatorRectForCell(const QRectF& cell) const
{
    const qreal diameter = qMin(kDateIndicatorDiameter, qMin(cell.width(), cell.height()) - (m_compact ? 4.0 : 10.0));
    const QPointF center = cell.center();
    return QRectF(center.x() - diameter / 2.0, center.y() - diameter / 2.0, diameter, diameter);
}

QRectF CalendarView::contentIndicatorRectForCell(const QRectF& cell) const
{
    const qreal diameter = qMax<qreal>(0.0, qMin(kContentIndicatorDiameter, qMin(cell.width(), cell.height())));
    const QPointF center = cell.center();
    return QRectF(center.x() - diameter / 2.0, center.y() - diameter / 2.0, diameter, diameter);
}

bool CalendarView::dateInGrid(const QDate& date) const
{
    if (!date.isValid())
        return false;
    const QDate start = gridStartDate();
    const QDate end = start.addDays((m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42) - 1);
    return date >= start && date <= end;
}

QString CalendarView::titleTextForLevel(CalendarContentLevel level, const QDate& visibleMonth) const
{
    switch (level) {
    case CalendarContentLevel::Day: {
        const DayPageKey key = dayPageKey(visibleMonth);
        return locale().standaloneMonthName(key.month, QLocale::LongFormat) + QStringLiteral(" %1").arg(key.year);
    }
    case CalendarContentLevel::Month: {
        const MonthPageKey key = monthPageKey(visibleMonth);
        return QString::number(key.year);
    }
    case CalendarContentLevel::Year: {
        const int startYear = yearPageKey(visibleMonth).startYear;
        return QStringLiteral("%1 - %2").arg(startYear).arg(startYear + 11);
    }
    }
    return QString();
}

int CalendarView::decadeStartYear(const QDate& visibleMonth) const
{
    return yearPageKey(visibleMonth).startYear;
}

CalendarView::DayPageKey CalendarView::dayPageKey(const QDate& visibleMonth) const
{
    const QDate month = firstOfMonth(visibleMonth.isValid() ? visibleMonth : QDate::currentDate());
    return {month.year(), month.month()};
}

CalendarView::MonthPageKey CalendarView::monthPageKey(const QDate& visibleMonth) const
{
    const QDate month = firstOfMonth(visibleMonth.isValid() ? visibleMonth : QDate::currentDate());
    return {month.year()};
}

CalendarView::YearPageKey CalendarView::yearPageKey(const QDate& visibleMonth) const
{
    const int year = visibleMonth.isValid() ? visibleMonth.year() : QDate::currentDate().year();
    return {qMax(1, year - year % 12)};
}

QDate CalendarView::monthForDayPage(const DayPageKey& key) const
{
    return QDate(key.year, key.month, 1);
}

QRect CalendarView::navigationButtonAt(const QPoint& pos) const
{
    if (previousButtonRect().contains(pos))
        return previousButtonRect();
    if (nextButtonRect().contains(pos))
        return nextButtonRect();
    return QRect();
}

bool CalendarView::adjustedTransitionHitPosition(const QPoint& pos, const QRect& bounds, QPoint* adjustedPos) const
{
    if (!bounds.contains(pos))
        return false;

    QPoint hitPos = pos;
    if (monthTransitionActive())
        return false;

    if (adjustedPos)
        *adjustedPos = hitPos;
    return true;
}

int CalendarView::contentCellIndexAt(const QPoint& pos) const
{
    QPoint hitPos;
    if (!adjustedTransitionHitPosition(pos, contentRect(), &hitPos))
        return -1;

    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? 12 : visibleRowCount() * kContentColumns); ++i) {
        if (contentCellRect(i / kContentColumns, i % kContentColumns).contains(hitPos))
            return i;
    }
    return -1;
}

QDate CalendarView::monthAt(const QPoint& pos) const
{
    const int index = contentCellIndexAt(pos);
    const QDate date = contentDateForCell(index);
    return isMonthSelectable(date.year(), date.month()) ? date : QDate();
}

int CalendarView::yearAt(const QPoint& pos) const
{
    const int index = contentCellIndexAt(pos);
    const int year = contentDateForCell(index).year();
    return isYearSelectable(year) ? year : 0;
}

QDate CalendarView::focusFallbackDate(bool includeFirstSelectable) const
{
    if (isDateSelectable(m_selectedDate) && dateInGrid(m_selectedDate))
        return m_selectedDate;

    const QDate today = QDate::currentDate();
    if (isDateSelectable(today) && dateInGrid(today))
        return today;

    if (!includeFirstSelectable)
        return QDate();

    const QDate start = gridStartDate();
    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42); ++i) {
        const QDate candidate = start.addDays(i);
        if (isDateSelectable(candidate) && candidate.month() == m_visibleMonth.month())
            return candidate;
    }
    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42); ++i) {
        const QDate candidate = start.addDays(i);
        if (isDateSelectable(candidate))
            return candidate;
    }
    return QDate();
}

QDate CalendarView::normalizeDateForRange(const QDate& date) const
{
    if (!date.isValid())
        return QDate();
    if (m_minDate.isValid() && date < m_minDate)
        return m_minDate;
    if (m_maxDate.isValid() && date > m_maxDate)
        return m_maxDate;
    return date;
}

QDate CalendarView::boundedMonthForRange(const QDate& month) const
{
    QDate visible = firstOfMonth(month.isValid() ? month : QDate::currentDate());
    if (!visible.isValid())
        visible = todayMonth();

    if (m_minDate.isValid()) {
        const QDate minMonth = firstOfMonth(m_minDate);
        if (visible.daysInMonth() > 0 && visible.addDays(visible.daysInMonth() - 1) < minMonth)
            visible = minMonth;
    }
    if (m_maxDate.isValid()) {
        const QDate maxMonth = firstOfMonth(m_maxDate);
        if (visible > maxMonth)
            visible = maxMonth;
    }
    return visible;
}

void CalendarView::enforceSelectedDateInRange()
{
    if (!m_selectedDate.isValid())
        return;

    const QDate normalized = normalizeDateForRange(m_selectedDate);
    if (normalized == m_selectedDate)
        return;

    m_selectedDate = normalized;
    if (m_selectedDate.isValid())
        m_focusedDate = m_selectedDate;
    m_focusIndicatorVisible = false;
    emit selectedDateChanged(m_selectedDate);
    detail::notifyCalendarViewAccessibilitySelection(this);
}

void CalendarView::moveMonth(int months, bool showFocusIndicator)
{
    const QDate previousFocus = m_focusedDate;
    setVisibleMonth(m_visibleMonth.addMonths(months));
    if (previousFocus.isValid())
        m_focusedDate = QDate(m_visibleMonth.year(), m_visibleMonth.month(),
            qMin(previousFocus.day(), m_visibleMonth.daysInMonth()));
    if (!isDateSelectable(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    m_focusIndicatorVisible = showFocusIndicator && m_focusedDate.isValid();
    refreshProperties();
    update();
    if (m_focusIndicatorVisible)
        detail::notifyCalendarViewAccessibilityFocus(this);
}

void CalendarView::moveYear(int years)
{
    setVisibleMonth(m_visibleMonth.addYears(years));
    refreshProperties();
    update();
}

void CalendarView::moveYearPage(int pages)
{
    setVisibleMonth(m_visibleMonth.addYears(pages * 12));
    refreshProperties();
    update();
}

void CalendarView::navigatePage(int pages, bool showFocusIndicator)
{
    if (pages == 0)
        return;

    if (m_contentLevel == CalendarContentLevel::Day)
        moveMonth(pages, showFocusIndicator);
    else if (m_contentLevel == CalendarContentLevel::Month)
        moveYear(pages);
    else
        moveYearPage(pages);
}

void CalendarView::resetScroll()
{
    m_scrollAnimation->stop();
    m_scrollSettleTimer->stop();
    m_scrollPropertiesDirty = false;
    m_scrollAccessibilityDirty = false;
    m_scrollOrigin = QDate();
    m_scrollPosition = 0.0;
    m_scrollTarget = 0.0;
}

qreal CalendarView::scrollRowHeight() const
{
    return m_contentLevel == CalendarContentLevel::Day ? dayCellHeight() : qreal(contentRect().height()) / kContentRows;
}

qreal CalendarView::scrollRowOffset() const
{
    return (m_scrollPosition - std::floor(m_scrollPosition)) * scrollRowHeight();
}

int CalendarView::visibleRowCount() const
{
    const int rows = m_contentLevel == CalendarContentLevel::Day ? 6 : kContentRows;
    return rows + (qRound(scrollRowOffset()) > 0 ? 1 : 0);
}

QDate CalendarView::scrollDateAtRow(qint64 row) const
{
    if (m_contentLevel == CalendarContentLevel::Day)
        return m_scrollOrigin.addDays(row * 7);
    if (m_contentLevel == CalendarContentLevel::Month)
        return m_scrollOrigin.addMonths(int(row * kContentColumns));
    return m_scrollOrigin.addYears(int(row * kContentColumns));
}

QDate CalendarView::contentDateForCell(int index) const
{
    if (index < 0 || index >= visibleRowCount() * kContentColumns)
        return QDate();
    const QDate start = m_scrollOrigin.isValid() ? scrollDateAtRow(qint64(std::floor(m_scrollPosition)))
        : m_contentLevel == CalendarContentLevel::Month
        ? QDate(m_visibleMonth.year(), 1, 1)
        : QDate(decadeStartYear(m_visibleMonth), 1, 1);
    return m_contentLevel == CalendarContentLevel::Month ? start.addMonths(index) : start.addYears(index);
}

qreal CalendarView::boundedScrollPosition(qreal position) const
{
    const auto rowForBoundary = [this](const QDate& date) -> qreal {
        if (m_contentLevel == CalendarContentLevel::Day)
            return m_scrollOrigin.daysTo(gridStartForMonth(date, m_firstDayOfWeek)) / 7.0;
        if (m_contentLevel == CalendarContentLevel::Month)
            return ((qint64(date.year()) - m_scrollOrigin.year()) * 12 + 1 - m_scrollOrigin.month())
                / qreal(kContentColumns);
        return (qint64(decadeStartYear(date)) - m_scrollOrigin.year()) / qreal(kContentColumns);
    };
    // The unbounded public range remains unbounded for selection; scrolling has
    // a finite representable viewport so large wheel packets cannot overflow.
    // zh_CN: 未设置范围时不限制日期选择；滚动限制在可表示的视口，防止大输入溢出。
    const QDate minimum = m_minDate.isValid() ? m_minDate : qMin(QDate(1, 1, 1), m_scrollOrigin);
    const QDate maximum = m_maxDate.isValid() ? m_maxDate : qMax(QDate(9999, 12, 31), m_scrollOrigin);
    return qBound(rowForBoundary(minimum), position, rowForBoundary(maximum));
}

void CalendarView::setScrollPosition(qreal position)
{
    const QDate previousFirst = m_contentLevel == CalendarContentLevel::Day ? gridStartDate() : contentDateForCell(0);
    const int previousRows = visibleRowCount();
    // Quantize presentation once so drawing and hit testing agree at row edges.
    // zh_CN: 统一取整画面位置，使整行边界上的绘制与命中测试保持一致。
    m_scrollPosition = qRound64(position * scrollRowHeight()) / scrollRowHeight();
    const QDate first = scrollDateAtRow(qint64(std::floor(m_scrollPosition)));
    const QDate middle = m_contentLevel == CalendarContentLevel::Day ? first.addDays(20)
        : m_contentLevel == CalendarContentLevel::Month
        ? first.addMonths(5)
        : first.addYears(5);
    const QDate month = boundedMonthForRange(middle);
    const bool monthChanged = month != m_visibleMonth;
    m_visibleMonth = month;
    m_scrollPropertiesDirty = true;
    refreshProperties(false);
    update();
    if (previousFirst != first || previousRows != visibleRowCount())
        m_scrollAccessibilityDirty = true;
    m_scrollSettleTimer->start();
    if (monthChanged)
        emit visibleMonthChanged(m_visibleMonth);
}

void CalendarView::finishScrollUpdates()
{
    m_scrollSettleTimer->stop();
    if (m_scrollPropertiesDirty)
        refreshProperties();
    if (m_scrollAccessibilityDirty) {
        m_scrollAccessibilityDirty = false;
        detail::notifyCalendarViewAccessibilityReset(this);
    }
}

QDate CalendarView::shiftedVisibleMonth(int pages) const
{
    return shiftedVisibleMonthFrom(m_visibleMonth, pages);
}

QDate CalendarView::shiftedVisibleMonthFrom(const QDate& originMonth, int pages) const
{
    if (!originMonth.isValid())
        return boundedMonthForRange(originMonth);
    if (m_contentLevel == CalendarContentLevel::Day) {
        const DayPageKey key = dayPageKey(originMonth.addMonths(pages));
        return boundedMonthForRange(monthForDayPage(key));
    }
    if (m_contentLevel == CalendarContentLevel::Month) {
        const MonthPageKey key = monthPageKey(originMonth.addYears(pages));
        return boundedMonthForRange(QDate(key.year, originMonth.month(), 1));
    }

    const YearPageKey key = yearPageKey(originMonth);
    const int yearOffset = originMonth.year() - key.startYear;
    return boundedMonthForRange(QDate(key.startYear + pages * 12 + yearOffset, originMonth.month(), 1));
}

QDate CalendarView::transitionTargetVisibleMonth() const
{
    return m_transitionVisibleMonth.isValid() ? m_transitionVisibleMonth : m_visibleMonth;
}

void CalendarView::finishMonthTransition()
{
    if (m_monthTransitionAnimation)
        m_monthTransitionAnimation->stop();
    m_previousVisibleMonth = QDate();
    m_transitionVisibleMonth = QDate();
    m_monthTransitionDirection = 0;
    m_monthTransitionProgress = 1.0;
}

void CalendarView::refreshHoverFromCursor()
{
    const QPoint local = mapFromGlobal(QCursor::pos());
    if (rect().contains(local) && updateHoverStateAt(local))
        update();
}

bool CalendarView::updateHoverStateAt(const QPoint& pos)
{
    const QDate hoverDate = m_contentLevel == CalendarContentLevel::Day ? dateAt(pos) : QDate();
    const QRect hoverButton = navigationButtonAt(pos);
    const bool titleHovered = titleButtonRect().contains(pos);
    const QDate hoverMonth = m_contentLevel == CalendarContentLevel::Month ? monthAt(pos) : QDate();
    const int hoverYear = m_contentLevel == CalendarContentLevel::Year ? yearAt(pos) : 0;

    if (m_hoveredDate == hoverDate && m_hoveredButton == hoverButton && m_titleHovered == titleHovered
        && m_hoveredMonth == hoverMonth && m_hoveredYear == hoverYear) {
        return false;
    }

    m_hoveredDate = hoverDate;
    m_hoveredButton = hoverButton;
    m_titleHovered = titleHovered;
    m_hoveredMonth = hoverMonth;
    m_hoveredYear = hoverYear;
    return true;
}

void CalendarView::clearHoverState()
{
    m_hoveredDate = QDate();
    m_hoveredButton = QRect();
    m_titleHovered = false;
    m_hoveredMonth = QDate();
    m_hoveredYear = 0;
}

void CalendarView::clearContentInteractionState()
{
    m_hoveredDate = QDate();
    m_pressedDate = QDate();
    m_hoveredMonth = QDate();
    m_pressedMonth = QDate();
    m_hoveredYear = 0;
    m_pressedYear = 0;
}

void CalendarView::startMonthTransition(const QDate& previousMonth,
    const QDate& currentMonth,
    qreal startProgress,
    bool animate)
{
    // Animate only when this view itself is on screen. Checking the top-level
    // window is not enough: inside a same-window overlay (the date picker
    // flyout) the host window is visible while the flyout is still hidden, so
    // the open-time jump to the selected month would start an entrance
    // animation — and day-cell hits are rejected while it runs.
    // zh_CN: 仅当视图自身在屏幕上时才播放动画。只查顶层窗口不够：在同窗口
    // overlay（日期选择器 flyout）里，宿主窗口可见而 flyout 尚未显示，打开时
    // 跳到选中月份会触发入场动画——动画期间日期单元格的点击会被拒绝。
    if (!previousMonth.isValid() || !currentMonth.isValid() || previousMonth == currentMonth || !isVisible()) {
        finishMonthTransition();
        return;
    }

    if (m_monthTransitionAnimation)
        m_monthTransitionAnimation->stop();
    m_previousVisibleMonth = previousMonth;
    m_transitionVisibleMonth = currentMonth;
    m_monthTransitionDirection = currentMonth > previousMonth ? 1 : -1;
    m_monthTransitionProgress = qBound(0.0, startProgress, 1.0);
    if (!m_monthTransitionAnimation)
        return;
    if (!animate)
        return;
    m_monthTransitionAnimation->setStartValue(m_monthTransitionProgress);
    m_monthTransitionAnimation->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_monthTransitionAnimation, themeAnimation().normal);
}

bool CalendarView::monthTransitionActive() const
{
    return m_previousVisibleMonth.isValid() && m_transitionVisibleMonth.isValid() && m_monthTransitionDirection != 0
        && m_monthTransitionProgress < 1.0;
}

void CalendarView::startContentTransition(CalendarContentLevel previousLevel, CalendarContentLevel currentLevel)
{
    resetScroll();
    finishMonthTransition();

    if (previousLevel == currentLevel || !isVisible()) {
        if (m_contentTransitionAnimation)
            m_contentTransitionAnimation->stop();
        m_previousContentLevel = currentLevel;
        m_previousContentVisibleMonth = QDate();
        m_contentTransitionDirection = 0;
        m_contentTransitionProgress = 1.0;
        return;
    }

    m_previousContentLevel = previousLevel;
    m_previousContentVisibleMonth = m_visibleMonth;
    m_contentTransitionDirection = contentLevelDepth(currentLevel) > contentLevelDepth(previousLevel) ? 1 : -1;
    m_contentTransitionProgress = 0.0;
    if (!m_contentTransitionAnimation)
        return;
    m_contentTransitionAnimation->stop();
    m_contentTransitionAnimation->setStartValue(0.0);
    m_contentTransitionAnimation->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_contentTransitionAnimation, themeAnimation().normal);
}

bool CalendarView::contentTransitionActive() const
{
    return m_previousContentVisibleMonth.isValid() && m_contentTransitionDirection != 0
        && m_contentTransitionProgress < 1.0;
}

void CalendarView::switchToParentContentLevel()
{
    if (m_contentLevel == CalendarContentLevel::Day)
        setContentLevel(CalendarContentLevel::Month);
    else if (m_contentLevel == CalendarContentLevel::Month)
        setContentLevel(CalendarContentLevel::Year);
    else
        setContentLevel(CalendarContentLevel::Day);
}

void CalendarView::moveFocusByDays(int days)
{
    m_scrollAnimation->stop();
    m_scrollTarget = m_scrollPosition;
    const bool continuous = m_scrollOrigin.isValid() && m_contentLevel == CalendarContentLevel::Day;
    QDate base =
        m_focusedDate.isValid() && (!continuous || dateInGrid(m_focusedDate)) ? m_focusedDate : focusFallbackDate();
    if (!base.isValid())
        return;

    const int direction = days >= 0 ? 1 : -1;
    QDate candidate = base.addDays(days);
    for (int attempts = 0; attempts < 80 && candidate.isValid(); ++attempts) {
        if (isDateSelectable(candidate)) {
            m_focusedDate = candidate;
            if (continuous) {
                const qreal row = std::floor(m_scrollOrigin.daysTo(candidate) / 7.0);
                if (row < m_scrollPosition)
                    setScrollPosition(boundedScrollPosition(row));
                else if (row + 1 > m_scrollPosition + 6)
                    setScrollPosition(boundedScrollPosition(row - 5));
                m_scrollTarget = m_scrollPosition;
            } else {
                setVisibleMonth(firstOfMonth(candidate));
            }
            m_focusIndicatorVisible = true;
            refreshProperties();
            update();
            detail::notifyCalendarViewAccessibilityFocus(this);
            return;
        }
        candidate = candidate.addDays(direction);
    }
}

void CalendarView::activateDate(const QDate& date)
{
    if (!isDateSelectable(date))
        return;

    setSelectedDate(date);
    if (!m_scrollOrigin.isValid())
        setVisibleMonth(firstOfMonth(date));
    emit dateActivated(date);
}

void CalendarView::refreshProperties(bool includeCells)
{
    setProperty("scrollPosition", m_scrollPosition);
    setProperty("scrollOffset", scrollRowOffset());
    setProperty("firstVisibleDate",
        m_contentLevel == CalendarContentLevel::Day ? gridStartDate() : contentDateForCell(0));
    setProperty("rangeHighlightEnabled", m_rangeHighlightEnabled);
    setProperty("focusedDate", m_focusedDate);
    setProperty("focusIndicatorVisible", m_focusIndicatorVisible);
    setProperty("titleText", titleTextForLevel(m_contentLevel, m_visibleMonth));
    setProperty("titleButtonRect", titleButtonRect());
    setProperty("previousButtonRect", previousButtonRect());
    setProperty("nextButtonRect", nextButtonRect());
    setProperty("previousButtonGlyph", Typography::Icons::Up);
    setProperty("nextButtonGlyph", Typography::Icons::Down);
    setProperty("gridRect", gridRect());
    setProperty("contentRect", contentRect());
    setProperty("previousVisibleMonth", m_previousVisibleMonth);
    setProperty("transitionVisibleMonth", m_transitionVisibleMonth);
    setProperty("committedVisibleMonth", m_visibleMonth);
    setProperty("monthTransitionProgress", m_monthTransitionProgress);
    setProperty("monthTransitionDirection", m_monthTransitionDirection);
    setProperty("previousContentLevel", QVariant::fromValue(m_previousContentLevel));
    setProperty("previousContentVisibleMonth", m_previousContentVisibleMonth);
    setProperty("contentTransitionProgress", m_contentTransitionProgress);
    setProperty("contentTransitionDirection", m_contentTransitionDirection);

    if (!includeCells)
        return;
    m_scrollPropertiesDirty = false;

    QVariantMap cellRects;
    QVariantMap indicatorRects;
    QVariantMap dateVisualStates;
    QVariantMap contentCellRects;
    QVariantMap contentIndicatorRects;
    const QDate start = gridStartDate();
    const QDate today = QDate::currentDate();
    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? visibleRowCount() * 7 : 42); ++i) {
        const QDate date = start.addDays(i);
        const QRect cell = cellRect(i / 7, i % 7);
        cellRects.insert(date.toString(Qt::ISODate), cell);
        indicatorRects.insert(date.toString(Qt::ISODate),
            dateIndicatorRectForCell(QRectF(cell).adjusted(2.0, 2.0, -2.0, -2.0)));
        QString state = QStringLiteral("default");
        if (!isDateSelectable(date))
            state = QStringLiteral("disabled");
        else if (m_rangeHighlightEnabled && isHighlightedDate(date))
            state = date == m_highlightedStartDate ? QStringLiteral("range-start")
                : date == m_highlightedEndDate
                ? QStringLiteral("range-end")
                : QStringLiteral("range-interior");
        else if (date == today)
            state = QStringLiteral("current");
        else if (date == m_selectedDate)
            state = QStringLiteral("selected");
        else if (m_focusIndicatorVisible && date == m_focusedDate)
            state = QStringLiteral("focused");
        dateVisualStates.insert(date.toString(Qt::ISODate), state);
    }
    for (int i = 0; i < (m_contentLevel == CalendarContentLevel::Day ? 12 : visibleRowCount() * kContentColumns); ++i) {
        const QString key = QString::number(i + 1);
        const QRect cell = contentCellRect(i / kContentColumns, i % kContentColumns);
        contentCellRects.insert(key, cell);
        contentIndicatorRects.insert(key, contentIndicatorRectForCell(QRectF(cell).adjusted(8.0, 8.0, -8.0, -8.0)));
    }
    setProperty("dateCellRects", cellRects);
    setProperty("dateIndicatorRects", indicatorRects);
    setProperty("dateVisualStates", dateVisualStates);
    setProperty("contentCellRects", contentCellRects);
    setProperty("contentIndicatorRects", contentIndicatorRects);
}

} // namespace fluent::date_time
