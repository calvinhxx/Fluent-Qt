#include "CalendarView.h"

#include <QDateTime>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVariantAnimation>
#include <QVariantMap>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
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
constexpr int kContentColumns = 3;
constexpr int kContentRows = 4;
constexpr qreal kWheelPageThreshold = 120.0;
constexpr int kWheelClusterGapMs = 120;
constexpr int kNoPhasePixelCommittedTailGapMs = 220;

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

int wheelPageStep(qreal delta)
{
    if (delta > 0.0)
        return -1;
    if (delta < 0.0)
        return 1;
    return 0;
}

int committedTailGapMs(FluentWheelInputKind kind)
{
    return kind == FluentWheelInputKind::NoPhasePixel
               ? kNoPhasePixelCommittedTailGapMs
               : kWheelClusterGapMs;
}

QColor withOpacity(QColor color, qreal opacity)
{
    color.setAlphaF(color.alphaF() * qBound(0.0, opacity, 1.0));
    return color;
}

Qt::DayOfWeek normalizeDayOfWeek(Qt::DayOfWeek day)
{
    if (day < Qt::Monday || day > Qt::Sunday)
        return QLocale().firstDayOfWeek();
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

QString weekdayLabel(Qt::DayOfWeek day)
{
    QString label = QLocale().standaloneDayName(day, QLocale::ShortFormat);
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

CalendarView::CalendarView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CalendarView"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(sizeHint());
    m_firstDayOfWeek = normalizeDayOfWeek(m_firstDayOfWeek);
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
    if (m_monthTransitionAnimation)
        m_monthTransitionAnimation->stop();
    if (m_contentTransitionAnimation)
        m_contentTransitionAnimation->stop();
}

QRect CalendarView::previousButtonRect() const
{
    return QRect(width() - 82, 10, kNavButtonSize, kNavButtonSize);
}

QRect CalendarView::nextButtonRect() const
{
    return QRect(width() - 44, 10, kNavButtonSize, kNavButtonSize);
}

QRect CalendarView::titleButtonRect() const
{
    const int maxRight = previousButtonRect().left() - 12;
    constexpr int titleLeft = 16;
    const int buttonWidth = qMax(72, maxRight - titleLeft);
    return QRect(titleLeft, 10, buttonWidth, kNavButtonSize);
}

QRect CalendarView::gridRect() const
{
    return QRect(0, kCalendarHeaderHeight + kWeekHeaderHeight, width(), kDayCellHeight * 6);
}

QRect CalendarView::contentRect() const
{
    return QRect(0, kCalendarHeaderHeight, width(), height() - kCalendarHeaderHeight);
}

QRect CalendarView::cellRect(int row, int column) const
{
    const QRect grid = gridRect();
    const int cellW = grid.width() / 7;
    const int extra = grid.width() - cellW * 7;
    const int x = grid.left() + column * cellW + qMin(column, extra);
    const int w = cellW + (column < extra ? 1 : 0);
    return QRect(x, grid.top() + row * kDayCellHeight, w, kDayCellHeight);
}

QRect CalendarView::contentCellRect(int row, int column) const
{
    const QRect area = contentRect();
    const int cellW = area.width() / kContentColumns;
    const int extraW = area.width() - cellW * kContentColumns;
    const int x = area.left() + column * cellW + qMin(column, extraW);
    const int w = cellW + (column < extraW ? 1 : 0);
    const int cellH = area.height() / kContentRows;
    const int extraH = area.height() - cellH * kContentRows;
    const int y = area.top() + row * cellH + qMin(row, extraH);
    const int h = cellH + (row < extraH ? 1 : 0);
    return QRect(x, y, w, h);
}

QRect CalendarView::dateCellRect(const QDate& date) const
{
    if (!date.isValid())
        return QRect();

    const QDate start = gridStartDate();
    const int offset = start.daysTo(date);
    if (offset < 0 || offset >= 42)
        return QRect();
    return cellRect(offset / 7, offset % 7);
}

QDate CalendarView::gridStartDate() const
{
    return gridStartForMonth(monthForDayPage(dayPageKey(m_visibleMonth)), m_firstDayOfWeek);
}

QDate CalendarView::dateAt(const QPoint& pos) const
{
    QPoint hitPos;
    if (!adjustedTransitionHitPosition(pos, gridRect(), &hitPos))
        return QDate();

    const QRect grid = gridRect();
    const int row = (hitPos.y() - grid.top()) / kDayCellHeight;
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
    return QSize(kCalendarCardWidth, kCalendarCardHeight);
}

QSize CalendarView::minimumSizeHint() const
{
    return sizeHint();
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
}

void CalendarView::setVisibleMonth(const QDate& month)
{
    const QDate bounded = boundedMonthForRange(month);
    if (m_visibleMonth == bounded)
        return;

    const QDate previousMonth = m_visibleMonth;
    resetWheelGesture();
    finishMonthTransition();
    m_visibleMonth = bounded;
    m_focusIndicatorVisible = false;
    startMonthTransition(previousMonth, bounded);
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit visibleMonthChanged(m_visibleMonth);
}

void CalendarView::setMinDate(const QDate& date)
{
    if (m_minDate == date)
        return;

    m_minDate = date;
    bool maxAdjusted = false;
    if (m_minDate.isValid() && m_maxDate.isValid() && m_maxDate < m_minDate) {
        m_maxDate = m_minDate;
        maxAdjusted = true;
    }

    enforceSelectedDateInRange();
    setVisibleMonth(m_visibleMonth);
    if (!isDateSelectable(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit minDateChanged(m_minDate);
    if (maxAdjusted)
        emit maxDateChanged(m_maxDate);
}

void CalendarView::setMaxDate(const QDate& date)
{
    if (m_maxDate == date)
        return;

    m_maxDate = date;
    bool minAdjusted = false;
    if (m_minDate.isValid() && m_maxDate.isValid() && m_minDate > m_maxDate) {
        m_minDate = m_maxDate;
        minAdjusted = true;
    }

    enforceSelectedDateInRange();
    setVisibleMonth(m_visibleMonth);
    if (!isDateSelectable(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    if (minAdjusted)
        emit minDateChanged(m_minDate);
    emit maxDateChanged(m_maxDate);
}

void CalendarView::setDateRange(const QDate& minDate, const QDate& maxDate)
{
    setMinDate(minDate);
    setMaxDate(maxDate);
}

void CalendarView::setFirstDayOfWeek(Qt::DayOfWeek day)
{
    const Qt::DayOfWeek normalized = normalizeDayOfWeek(day);
    if (m_firstDayOfWeek == normalized)
        return;

    m_firstDayOfWeek = normalized;
    if (!dateInGrid(m_focusedDate))
        m_focusedDate = focusFallbackDate(false);
    refreshProperties();
    update();
    emit firstDayOfWeekChanged(m_firstDayOfWeek);
}

void CalendarView::setContentLevel(CalendarContentLevel level)
{
    if (m_contentLevel == level)
        return;

    const CalendarContentLevel previousLevel = m_contentLevel;
    m_contentLevel = level;
    resetWheelGesture();
    startContentTransition(previousLevel, level);
    clearContentInteractionState();
    refreshProperties();
    update();
    emit contentLevelChanged(m_contentLevel);
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto colors = themeColors();

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
                m_pressedDate = pressedDate;
                m_focusedDate = pressedDate;
                m_focusIndicatorVisible = false;
                refreshProperties();
                setFocus(Qt::MouseFocusReason);
                update();
                event->accept();
                return;
            }
        } else if (m_contentLevel == CalendarContentLevel::Month) {
            const int pressedMonth = monthAt(pos);
            if (pressedMonth > 0) {
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

        if (m_pressedMonth > 0) {
            const int month = m_pressedMonth;
            m_pressedMonth = -1;
            if (month == monthAt(pos)) {
                setVisibleMonth(QDate(m_visibleMonth.year(), month, 1));
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
    if (!contentRect().contains(pos)) {
        QWidget::wheelEvent(event);
        return;
    }

    const FluentWheelInputKind kind = fluentWheelInputKind(event);
    const bool phaseBased = kind == FluentWheelInputKind::PhaseBased;
    const WheelGestureKind gestureKind =
        kind == FluentWheelInputKind::PhaseBased
            ? WheelGestureKind::PhaseBased
            : (kind == FluentWheelInputKind::NoPhasePixel
                   ? WheelGestureKind::NoPhasePixel
                   : WheelGestureKind::NoPhaseDiscrete);

    if (event->phase() == Qt::ScrollBegin) {
        resetWheelGesture();
        m_wheelGestureKind = WheelGestureKind::PhaseBased;
        event->accept();
        return;
    }

    if (event->phase() == Qt::ScrollMomentum) {
        event->accept();
        return;
    }

    if (event->phase() == Qt::ScrollEnd) {
        resetWheelGesture();
        event->accept();
        return;
    }

    const qreal delta = fluentWheelDeltaY(event);
    if (qFuzzyIsNull(delta)) {
        event->accept();
        return;
    }

    const int step = wheelPageStep(delta);
    if (step == 0) {
        event->accept();
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsedSinceLast = m_lastWheelTs == 0 ? 0 : now - m_lastWheelTs;
    const bool transitionActive = contentTransitionActive() || monthTransitionActive();
    const bool preserveCommittedTransitionTail = transitionActive && m_wheelGestureCommitted;
    const bool preserveCommittedNoPhaseTail =
        !phaseBased &&
        m_wheelGestureCommitted &&
        m_wheelGestureKind == gestureKind &&
        m_wheelDir == step &&
        elapsedSinceLast <= committedTailGapMs(kind);
    const bool freshDiscreteNotch = !transitionActive &&
                                    m_wheelGestureCommitted &&
                                    gestureKind == WheelGestureKind::NoPhaseDiscrete &&
                                    qAbs(delta) >= kWheelPageThreshold;
    const bool kindChanged = m_wheelGestureKind != WheelGestureKind::None &&
                             m_wheelGestureKind != gestureKind;
    const bool clusterExpired = !phaseBased && m_lastWheelTs != 0 &&
                                elapsedSinceLast > kWheelClusterGapMs;
    const bool directionChanged = m_wheelDir != 0 && m_wheelDir != step;
    const bool reverseCommittedTransitionTail =
        preserveCommittedTransitionTail && directionChanged;
    if (freshDiscreteNotch ||
        m_lastWheelTs == 0 ||
        (kindChanged && !preserveCommittedTransitionTail) ||
        (clusterExpired && !preserveCommittedTransitionTail && !preserveCommittedNoPhaseTail) ||
        (directionChanged && !m_wheelGestureCommitted)) {
        resetWheelGesture();
    }

    if (reverseCommittedTransitionTail) {
        event->accept();
        return;
    }

    m_lastWheelTs = now;
    m_wheelDir = step;
    m_wheelGestureKind = gestureKind;

    if (transitionActive) {
        m_wheelAccum = 0.0;
        m_wheelGestureCommitted = true;
        event->accept();
        return;
    }

    if (m_wheelGestureCommitted) {
        event->accept();
        return;
    }

    m_wheelAccum += qAbs(delta);
    if (m_wheelAccum < kWheelPageThreshold) {
        event->accept();
        return;
    }

    navigatePage(step);
    m_wheelAccum = 0.0;
    m_wheelDir = step;
    m_wheelGestureKind = gestureKind;
    m_lastWheelTs = now;
    m_wheelGestureCommitted = true;

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
    QWidget::focusInEvent(event);
}

void CalendarView::focusOutEvent(QFocusEvent* event)
{
    update();
    QWidget::focusOutEvent(event);
}

void CalendarView::onThemeUpdated()
{
    update();
}

void CalendarView::paintHeader(QPainter& painter)
{
    const auto colors = themeColors();
    const QRect titleRect = titleButtonRect();
    paintTitleButtonBackground(painter);
    if (contentTransitionActive()) {
        paintTitleForLevel(painter, m_contentLevel, m_visibleMonth, 0.0, 1.0);
    } else {
        paintTitleForLevel(painter, m_contentLevel, transitionTargetVisibleMonth(), 0.0, 1.0);
    }

    paintNavButton(painter, previousButtonRect(), Typography::Icons::Up);
    paintNavButton(painter, nextButtonRect(), Typography::Icons::Down);

    painter.setPen(colors.strokeDivider);
    painter.drawLine(QPoint(0, kCalendarHeaderHeight - 1), QPoint(width(), kCalendarHeaderHeight - 1));
}

void CalendarView::paintTitleButtonBackground(QPainter& painter)
{
    const auto colors = themeColors();
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

void CalendarView::paintTitleForLevel(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal yOffset, qreal opacity)
{
    if (!visibleMonth.isValid())
        return;

    const auto colors = themeColors();
    const QRectF titleRect = QRectF(titleButtonRect()).adjusted(12.0, yOffset, -12.0, yOffset);
    painter.setFont(themeFont(Typography::FontRole::BodyStrong).toQFont());
    painter.setPen(withOpacity(colors.textPrimary, opacity));
    painter.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, titleTextForLevel(level, visibleMonth));
}

void CalendarView::paintNavButton(QPainter& painter, const QRect& buttonRect, const QString& glyph)
{
    const auto colors = themeColors();
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

    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(10);
    QPainterPath iconPath;
    iconPath.addText(0.0, 0.0, iconFont, glyph);
    const QRectF iconBounds = iconPath.boundingRect();
    const QPointF iconCenter = QRectF(buttonRect).center();
    painter.save();
    painter.translate(iconCenter.x() - iconBounds.center().x(),
                      iconCenter.y() - iconBounds.center().y());
    painter.setPen(Qt::NoPen);
    painter.setBrush(colors.textPrimary);
    painter.drawPath(iconPath);
    painter.restore();
}

void CalendarView::paintWeekdays(QPainter& painter)
{
    const auto colors = themeColors();
    painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
    painter.setPen(colors.textPrimary);
    const int y = kCalendarHeaderHeight;
    for (int c = 0; c < 7; ++c) {
        const int day = ((static_cast<int>(m_firstDayOfWeek) - 1 + c) % 7) + 1;
        const QRect rect = cellRect(0, c).translated(0, y - gridRect().top());
        painter.drawText(rect, Qt::AlignCenter, weekdayLabel(static_cast<Qt::DayOfWeek>(day)));
    }
}

void CalendarView::paintContent(QPainter& painter)
{
    if (contentTransitionActive()) {
        const QRect area = contentRect();
        const qreal progress = qBound(0.0, m_contentTransitionProgress, 1.0);
        const bool zoomOut = m_contentTransitionDirection > 0;
        const qreal currentScale = zoomOut
                                       ? 1.06 - 0.06 * progress
                                       : 0.94 + 0.06 * progress;
        const qreal currentOpacity = 0.92 + 0.08 * progress;
        painter.save();
        painter.setClipRect(area);
        paintContentLevelZoom(painter, m_contentLevel, m_visibleMonth,
                              currentScale, currentOpacity);
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
                          -m_monthTransitionDirection * m_monthTransitionProgress * travel,
                          1.0);
        paintContentLevel(painter, m_contentLevel, transitionTargetVisibleMonth(),
                          m_monthTransitionDirection * (1.0 - m_monthTransitionProgress) * travel,
                          1.0);
        painter.restore();
        return;
    }

    paintContentLevel(painter, m_contentLevel, m_visibleMonth, 0.0, 1.0);
}

void CalendarView::paintContentLevel(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal yOffset, qreal opacity)
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

void CalendarView::paintContentLevelZoom(QPainter& painter, CalendarContentLevel level, const QDate& visibleMonth, qreal scale, qreal opacity)
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
                       -m_monthTransitionDirection * m_monthTransitionProgress * travel,
                       1.0);
        paintMonthDays(painter, transitionTargetVisibleMonth(),
                       m_monthTransitionDirection * (1.0 - m_monthTransitionProgress) * travel,
                       1.0);
        painter.restore();
        return;
    }

    painter.save();
    painter.setOpacity(opacity);
    painter.translate(0.0, yOffset);
    paintWeekdays(painter);
    paintMonthDays(painter, visibleMonth, 0.0, 1.0);
    painter.restore();
}

void CalendarView::paintMonthDays(QPainter& painter, const QDate& month, qreal yOffset, qreal opacity)
{
    const auto colors = themeColors();
    const QDate today = QDate::currentDate();
    const QDate start = gridStartForMonth(month, m_firstDayOfWeek);

    // Per-design-language day indicator. DesignFluent is the original WinUI treatment (unchanged);
    // DesignMaterial / DesignCupertino brand the selected / today / hover cues. The "veil" is the
    // theme-aware translucent overlay used by Button: dark surfaces lighten, light surfaces darken, so
    // hover stays visible under both App themes. zh_CN: 按设计语言分支绘制日期指示器。DesignFluent 保持原
    // WinUI 行为不变;DesignMaterial / DesignCupertino 对选中/今天/悬停做品牌化。veil 是与 Button 一致的
    // 主题感知半透明叠加(深色面变亮、浅色面变暗),使悬停在明暗两主题下都可见。
    const DesignLanguage lang = themeDesignLanguage();
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };

    painter.save();
    painter.setOpacity(opacity);
    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());

    for (int i = 0; i < 42; ++i) {
        const int row = i / 7;
        const int column = i % 7;
        const QDate date = start.addDays(i);
        const QRectF cell = QRectF(cellRect(row, column)).translated(0.0, yOffset).adjusted(2.0, 2.0, -2.0, -2.0);
        const QRectF indicatorRect = dateIndicatorRectForCell(cell);
        const bool inMonth = date.month() == month.month() && date.year() == month.year();
        const bool selectable = isDateSelectable(date);
        const bool selected = date.isValid() && date == m_selectedDate;
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

        if (lang == DesignMaterial) {
            // Material 3 date picker: selected day = filled `primary` circle + `on-primary` glyph;
            // today (not selected) = 1 dp `primary` outline ring with normal text; hover/press/focus =
            // a circular state layer (on-surface veil 8/10 %, or accent veil when over the selected
            // fill) inscribed inside the day cell. An oversized halo gets clipped by the backing store
            // into a hard gray band, so the state-layer circle is clamped to fit the indicator.
            // zh_CN: Material 3 日期选择:选中=填充 primary 圆 + on-primary 字;今天(未选中)=1dp primary 描
            // 边环 + 普通文字;悬停/按下/焦点=圆形 state layer(on-surface 薄层 8/10%,覆盖在选中填充上时用
            // accent 薄层),内切于单元格。过大的光晕会被后备存储裁出硬边灰带,故 state-layer 圆收敛于指示器内。
            if (selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.accentDefault);
                painter.drawEllipse(indicatorRect);
                textColor = colors.textOnAccent;
            } else if (current) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(colors.accentDefault, 1.0));
                painter.drawEllipse(indicatorRect.adjusted(0.5, 0.5, -0.5, -0.5));
                // text stays textPrimary (normal). zh_CN: 文字保持 textPrimary(普通)。
            }

            if (hovered || pressed || focused) {
                // State layer: accent veil when it sits over the selected fill (keeps contrast on the
                // saturated circle), otherwise the on-surface veil. zh_CN: state layer:覆盖在选中填充上时用
                // accent 薄层(在饱和圆上保持对比),否则用 on-surface 薄层。
                const int alpha = pressed ? 0x1A : 0x14; // 10% / 8%
                const QColor layer = selected ? withAlpha(colors.accentDefault, alpha) : veil(alpha);
                if (layer.isValid() && layer.alpha() > 0) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(layer);
                    painter.drawEllipse(indicatorRect);
                }
            }
        } else if (lang == DesignCupertino) {
            // macOS graphical date picker: selected day = filled accent circle + white glyph; today =
            // accent outline ring; hover = the quiet theme-aware veil. Small and unobtrusive.
            // zh_CN: macOS 图形日期选择:选中=填充 accent 圆 + 白字;今天=accent 描边环;悬停=安静的主题感知薄层。
            if (selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(colors.accentDefault);
                painter.drawEllipse(indicatorRect);
                textColor = QColor(Qt::white);
            } else {
                if (hovered || pressed || focused) {
                    const QColor bg = veil(pressed ? (darkTheme ? 0x2C : 0x24)
                                                   : (darkTheme ? 0x18 : 0x14));
                    if (bg.isValid() && bg.alpha() > 0) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(bg);
                        painter.drawEllipse(indicatorRect);
                    }
                }
                if (current) {
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(colors.accentDefault, 1.5));
                    painter.drawEllipse(indicatorRect.adjusted(1.0, 1.0, -1.0, -1.0));
                    textColor = colors.textAccentPrimary;
                }
            }
        } else {
            // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
            if (!current && !selected && (hovered || pressed || focused)) {
                QColor bg = pressed ? colors.subtleTertiary : colors.subtleSecondary;
                painter.setPen(Qt::NoPen);
                painter.setBrush(bg);
                painter.drawEllipse(indicatorRect);
            }

            if (current) {
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

    for (int i = 0; i < 12; ++i) {
        const int row = i / kContentColumns;
        const int column = i % kContentColumns;
        const QRectF cell = QRectF(contentCellRect(row, column)).adjusted(8.0, 8.0, -8.0, -8.0);
        const int month = i + 1;
        const bool selectable = isMonthSelectable(visibleMonth.year(), month);
        const bool hovered = selectable && m_hoveredMonth == month;
        const bool pressed = selectable && m_pressedMonth == month;
        const bool selected = selectable && m_selectedDate.isValid() &&
                              m_selectedDate.year() == visibleMonth.year() &&
                              m_selectedDate.month() == month;
        const bool current = selectable && today.year() == visibleMonth.year() && today.month() == month;

        paintContentCellChrome(painter, cell, current, selected, hovered, pressed);
        painter.setPen(selectable ? contentCellTextColor(current, selected) : themeColors().textDisabled);
        painter.drawText(cell, Qt::AlignCenter, QLocale().standaloneMonthName(month, QLocale::ShortFormat));
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

    for (int i = 0; i < 12; ++i) {
        const int row = i / kContentColumns;
        const int column = i % kContentColumns;
        const QRectF cell = QRectF(contentCellRect(row, column)).adjusted(8.0, 8.0, -8.0, -8.0);
        const int year = startYear + i;
        const bool selectable = isYearSelectable(year);
        const bool hovered = selectable && m_hoveredYear == year;
        const bool pressed = selectable && m_pressedYear == year;
        const bool selected = selectable && m_selectedDate.isValid() && m_selectedDate.year() == year;
        const bool current = selectable && today.year() == year;

        paintContentCellChrome(painter, cell, current, selected, hovered, pressed);
        painter.setPen(selectable ? contentCellTextColor(current, selected) : themeColors().textDisabled);
        painter.drawText(cell, Qt::AlignCenter, QString::number(year));
    }
    painter.restore();
}

void CalendarView::paintContentCellChrome(QPainter& painter, const QRectF& cell, bool current,
                                          bool selected, bool hovered, bool pressed)
{
    const auto colors = themeColors();
    const int radius = themeRadius().control;

    if (current) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.accentDefault);
        painter.drawRoundedRect(cell, radius, radius);
    } else if (hovered || pressed) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? colors.subtleTertiary : colors.subtleSecondary);
        painter.drawRoundedRect(cell, radius, radius);
    } else if (selected) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(colors.accentDefault, 1.5));
        painter.drawRoundedRect(cell.adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);
    }
}

QColor CalendarView::contentCellTextColor(bool current, bool selected) const
{
    const auto colors = themeColors();
    if (current)
        return colors.textOnAccent;
    if (selected)
        return colors.textAccentPrimary;
    return colors.textPrimary;
}

QRectF CalendarView::dateIndicatorRectForCell(const QRectF& cell) const
{
    const qreal diameter = qMin(kDateIndicatorDiameter, qMin(cell.width(), cell.height()) - 10.0);
    const QPointF center = cell.center();
    return QRectF(center.x() - diameter / 2.0,
                  center.y() - diameter / 2.0,
                  diameter,
                  diameter);
}

bool CalendarView::dateInGrid(const QDate& date) const
{
    if (!date.isValid())
        return false;
    const QDate start = gridStartDate();
    const QDate end = start.addDays(41);
    return date >= start && date <= end;
}

QString CalendarView::titleTextForLevel(CalendarContentLevel level, const QDate& visibleMonth) const
{
    switch (level) {
    case CalendarContentLevel::Day: {
        const DayPageKey key = dayPageKey(visibleMonth);
        return QLocale().standaloneMonthName(key.month, QLocale::LongFormat)
               + QStringLiteral(" %1").arg(key.year);
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
    return {year - year % 12};
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

    for (int i = 0; i < 12; ++i) {
        if (contentCellRect(i / kContentColumns, i % kContentColumns).contains(hitPos))
            return i;
    }
    return -1;
}

int CalendarView::monthAt(const QPoint& pos) const
{
    const int index = contentCellIndexAt(pos);
    if (index < 0)
        return -1;
    const int month = index + 1;
    return isMonthSelectable(m_visibleMonth.year(), month) ? month : -1;
}

int CalendarView::yearAt(const QPoint& pos) const
{
    const int index = contentCellIndexAt(pos);
    if (index < 0)
        return 0;
    const int startYear = yearPageKey(m_visibleMonth).startYear;
    const int year = startYear + index;
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
    for (int i = 0; i < 42; ++i) {
        const QDate candidate = start.addDays(i);
        if (isDateSelectable(candidate) && candidate.month() == m_visibleMonth.month())
            return candidate;
    }
    for (int i = 0; i < 42; ++i) {
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

void CalendarView::resetWheelGesture()
{
    m_wheelAccum = 0.0;
    m_wheelDir = 0;
    m_wheelGestureKind = WheelGestureKind::None;
    m_wheelGestureCommitted = false;
    m_lastWheelTs = 0;
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
    const int hoverMonth = m_contentLevel == CalendarContentLevel::Month ? monthAt(pos) : -1;
    const int hoverYear = m_contentLevel == CalendarContentLevel::Year ? yearAt(pos) : 0;

    if (m_hoveredDate == hoverDate && m_hoveredButton == hoverButton &&
        m_titleHovered == titleHovered && m_hoveredMonth == hoverMonth && m_hoveredYear == hoverYear) {
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
    m_hoveredMonth = -1;
    m_hoveredYear = 0;
}

void CalendarView::clearContentInteractionState()
{
    m_hoveredDate = QDate();
    m_pressedDate = QDate();
    m_hoveredMonth = -1;
    m_pressedMonth = -1;
    m_hoveredYear = 0;
    m_pressedYear = 0;
}

void CalendarView::startMonthTransition(const QDate& previousMonth, const QDate& currentMonth,
                                        qreal startProgress, bool animate)
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
    m_monthTransitionAnimation->start();
}

bool CalendarView::monthTransitionActive() const
{
    return m_previousVisibleMonth.isValid() &&
           m_transitionVisibleMonth.isValid() &&
           m_monthTransitionDirection != 0 &&
           m_monthTransitionProgress < 1.0;
}

void CalendarView::startContentTransition(CalendarContentLevel previousLevel, CalendarContentLevel currentLevel)
{
    resetWheelGesture();
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
    m_contentTransitionAnimation->start();
}

bool CalendarView::contentTransitionActive() const
{
    return m_previousContentVisibleMonth.isValid() &&
           m_contentTransitionDirection != 0 &&
           m_contentTransitionProgress < 1.0;
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
    QDate base = m_focusedDate.isValid() ? m_focusedDate : focusFallbackDate();
    if (!base.isValid())
        return;

    const int direction = days >= 0 ? 1 : -1;
    QDate candidate = base.addDays(days);
    for (int attempts = 0; attempts < 80 && candidate.isValid(); ++attempts) {
        if (isDateSelectable(candidate)) {
            m_focusedDate = candidate;
            setVisibleMonth(firstOfMonth(candidate));
            m_focusIndicatorVisible = true;
            refreshProperties();
            update();
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
    setVisibleMonth(firstOfMonth(date));
    emit dateActivated(date);
}

void CalendarView::refreshProperties()
{
    setProperty("visibleMonth", m_visibleMonth);
    setProperty("focusedDate", m_focusedDate);
    setProperty("selectedDate", m_selectedDate);
    setProperty("minDate", m_minDate);
    setProperty("maxDate", m_maxDate);
    setProperty("firstDayOfWeek", QVariant::fromValue(m_firstDayOfWeek));
    setProperty("contentLevel", QVariant::fromValue(m_contentLevel));
    setProperty("frameVisible", m_frameVisible);
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

    QVariantMap cellRects;
    QVariantMap indicatorRects;
    QVariantMap dateVisualStates;
    QVariantMap contentCellRects;
    const QDate start = gridStartDate();
    const QDate today = QDate::currentDate();
    for (int i = 0; i < 42; ++i) {
        const QDate date = start.addDays(i);
        const QRect cell = cellRect(i / 7, i % 7);
        cellRects.insert(date.toString(Qt::ISODate), cell);
        indicatorRects.insert(date.toString(Qt::ISODate), dateIndicatorRectForCell(QRectF(cell).adjusted(2.0, 2.0, -2.0, -2.0)));
        QString state = QStringLiteral("default");
        if (!isDateSelectable(date))
            state = QStringLiteral("disabled");
        else if (date == today)
            state = QStringLiteral("current");
        else if (date == m_selectedDate)
            state = QStringLiteral("selected");
        else if (m_focusIndicatorVisible && date == m_focusedDate)
            state = QStringLiteral("focused");
        dateVisualStates.insert(date.toString(Qt::ISODate), state);
    }
    for (int i = 0; i < 12; ++i)
        contentCellRects.insert(QString::number(i + 1), contentCellRect(i / kContentColumns, i % kContentColumns));
    setProperty("dateCellRects", cellRects);
    setProperty("dateIndicatorRects", indicatorRects);
    setProperty("dateVisualStates", dateVisualStates);
    setProperty("contentCellRects", contentCellRects);
}

} // namespace fluent::date_time
