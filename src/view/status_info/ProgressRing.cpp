#include "ProgressRing.h"

#include <QEvent>
#include <QHideEvent>
#include <QPainter>
#include <QPen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimerEvent>
#include <QtGlobal>
#include <cmath>
#include <limits>

namespace view::status_info {

namespace {
constexpr int kSmallDiameter = 16;
constexpr int kMediumDiameter = 32;
constexpr int kLargeDiameter = 64;
constexpr int kAnimationIntervalMs = 16;
constexpr qreal kIndeterminateArcDegrees = 105.0;
constexpr qreal kFullCircleDegrees = 360.0;

bool nearlyEqual(qreal left, qreal right)
{
    return std::abs(left - right) < 0.001;
}
}

ProgressRing::ProgressRing(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    updateThemeColors();
}

ProgressRing::~ProgressRing()
{
    m_animationTimer.stop();
}

void ProgressRing::setIsActive(bool active)
{
    if (m_isActive == active) return;
    m_isActive = active;
    updateAnimationState();
    update();
    emit isActiveChanged(m_isActive);
}

void ProgressRing::setIsIndeterminate(bool indeterminate)
{
    if (m_isIndeterminate == indeterminate) return;
    m_isIndeterminate = indeterminate;
    updateAnimationState();
    update();
    emit isIndeterminateChanged(m_isIndeterminate);
}

void ProgressRing::setMinimum(int minimum)
{
    setRange(minimum, m_maximum);
}

void ProgressRing::setMaximum(int maximum)
{
    setRange(m_minimum, maximum);
}

void ProgressRing::setRange(int minimum, int maximum)
{
    if (maximum <= minimum) {
        if (minimum == std::numeric_limits<int>::max()) {
            minimum = maximum - 1;
        } else {
            maximum = minimum + 1;
        }
    }

    const int oldMinimum = m_minimum;
    const int oldMaximum = m_maximum;
    const int oldValue = m_value;

    m_minimum = minimum;
    m_maximum = maximum;
    m_value = qBound(m_minimum, m_value, m_maximum);

    const bool minimumChangedNow = oldMinimum != m_minimum;
    const bool maximumChangedNow = oldMaximum != m_maximum;
    const bool valueChangedNow = oldValue != m_value;

    if (!minimumChangedNow && !maximumChangedNow && !valueChangedNow) return;

    update();
    if (minimumChangedNow) emit minimumChanged(m_minimum);
    if (maximumChangedNow) emit maximumChanged(m_maximum);
    if (valueChangedNow) emit valueChanged(m_value);
}

void ProgressRing::setValue(int value)
{
    const int clampedValue = qBound(m_minimum, value, m_maximum);
    if (m_value == clampedValue) return;
    m_value = clampedValue;
    update();
    emit valueChanged(m_value);
}

void ProgressRing::setRingSize(ProgressRingSize size)
{
    if (m_ringSize == size) return;
    m_ringSize = size;
    updateGeometry();
    update();
    emit ringSizeChanged(m_ringSize);
}

void ProgressRing::setStrokeWidth(qreal width)
{
    if (width <= 0.0 || nearlyEqual(m_strokeWidth, width)) return;
    m_strokeWidth = width;
    update();
    emit strokeWidthChanged(m_strokeWidth);
}

void ProgressRing::setStatus(ProgressRingStatus status)
{
    if (m_status == status) return;
    m_status = status;
    updateAnimationState();
    update();
    emit statusChanged(m_status);
}

void ProgressRing::setBackgroundVisible(bool visible)
{
    if (m_backgroundVisible == visible) return;
    m_backgroundVisible = visible;
    update();
    emit backgroundVisibleChanged(m_backgroundVisible);
}

double ProgressRing::progressRatio() const
{
    const int range = m_maximum - m_minimum;
    if (range <= 0) return 0.0;
    return static_cast<double>(m_value - m_minimum) / static_cast<double>(range);
}

QSize ProgressRing::sizeHint() const
{
    const int diameter = diameterForSize();
    return QSize(diameter, diameter);
}

QSize ProgressRing::minimumSizeHint() const
{
    return sizeHint();
}

void ProgressRing::onThemeUpdated()
{
    updateThemeColors();
    update();
}

void ProgressRing::paintEvent(QPaintEvent*)
{
    const qreal side = qMin(width(), height());
    if (side <= 0.0) return;

    const qreal effectiveStrokeWidth = qMin(m_strokeWidth, qMax<qreal>(1.0, side - 1.0));
    QRectF arcRect = ringRect(effectiveStrokeWidth);
    if (!arcRect.isValid() || arcRect.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen;
    pen.setWidthF(effectiveStrokeWidth);
    pen.setCapStyle(Qt::RoundCap);

    if (m_backgroundVisible) {
        pen.setColor(m_trackColor);
        painter.setPen(pen);
        painter.drawArc(arcRect, 0, static_cast<int>(kFullCircleDegrees * 16));
    }

    if (!m_isActive) return;

    int spanAngle = 0;
    qreal startDegrees = 90.0;
    if (m_isIndeterminate) {
        startDegrees -= m_animationPhase;
        spanAngle = static_cast<int>(-kIndeterminateArcDegrees * 16);
    } else {
        spanAngle = static_cast<int>(-kFullCircleDegrees * progressRatio() * 16.0);
    }

    if (spanAngle == 0) return;

    pen.setColor(indicatorColor());
    painter.setPen(pen);
    painter.drawArc(arcRect, static_cast<int>(startDegrees * 16.0), spanAngle);
}

void ProgressRing::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_animationTimer.timerId()) {
        QWidget::timerEvent(event);
        return;
    }

    if (!shouldAnimate()) {
        m_animationTimer.stop();
        return;
    }

    const int cycleMs = qMax(1, themeAnimation().normal * 4);
    m_animationPhase = std::fmod(
        m_animationPhase + kFullCircleDegrees * static_cast<qreal>(kAnimationIntervalMs) / cycleMs,
        kFullCircleDegrees);
    update();
}

void ProgressRing::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        updateAnimationState();
        update();
    }
}

void ProgressRing::hideEvent(QHideEvent* event)
{
    m_animationTimer.stop();
    QWidget::hideEvent(event);
}

void ProgressRing::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateAnimationState();
}

int ProgressRing::diameterForSize() const
{
    switch (m_ringSize) {
        case ProgressRingSize::Small: return kSmallDiameter;
        case ProgressRingSize::Large: return kLargeDiameter;
        case ProgressRingSize::Medium:
        default: return kMediumDiameter;
    }
}

QRectF ProgressRing::ringRect(qreal effectiveStrokeWidth) const
{
    const qreal side = qMin(width(), height());
    QRectF bounds((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    const qreal inset = effectiveStrokeWidth / 2.0;
    return bounds.adjusted(inset, inset, -inset, -inset);
}

QColor ProgressRing::indicatorColor() const
{
    if (!isEnabled()) return m_disabledColor;

    switch (m_status) {
        case ProgressRingStatus::Paused: return m_pausedColor;
        case ProgressRingStatus::Error: return m_errorColor;
        case ProgressRingStatus::Running:
        default: return m_runningColor;
    }
}

void ProgressRing::updateThemeColors()
{
    const auto& colors = themeColors();
    m_runningColor = colors.accentDefault;
    m_pausedColor = colors.systemCaution;
    m_errorColor = colors.systemCritical;
    m_disabledColor = colors.accentDisabled;
    m_trackColor = currentTheme() == Dark ? colors.strokeSurface : colors.strokeSecondary;
    if (m_trackColor.alpha() < 64) {
        m_trackColor.setAlpha(64);
    }
}

void ProgressRing::updateAnimationState()
{
    if (shouldAnimate()) {
        if (!m_animationTimer.isActive()) {
            m_animationTimer.start(kAnimationIntervalMs, this);
        }
    } else if (m_animationTimer.isActive()) {
        m_animationTimer.stop();
    }
}

bool ProgressRing::shouldAnimate() const
{
    return m_isActive
        && m_isIndeterminate
        && m_status == ProgressRingStatus::Running
        && isEnabled()
        && isVisible();
}

} // namespace view::status_info