#include "ProgressBar.h"

#include <QEvent>
#include <QHideEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimerEvent>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fluent::status_info {

namespace {
constexpr int kDefaultHeight = 32;
constexpr int kMinimumWidth = 16;
constexpr int kAnimationIntervalMs = 16;
constexpr qreal kRailThickness = 1.0;
constexpr qreal kIndeterminateSegmentRatio = 0.44;
constexpr qreal kNumberEpsilon = 0.001;

bool isFiniteNumber(double value)
{
    return std::isfinite(value);
}

bool nearlyEqual(double left, double right)
{
    return std::abs(left - right) < kNumberEpsilon;
}
}

ProgressBar::ProgressBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("ProgressBar"));
    updateThemeColors();
}

ProgressBar::~ProgressBar()
{
    m_animationTimer.stop();
}

void ProgressBar::setIsIndeterminate(bool indeterminate)
{
    if (m_isIndeterminate == indeterminate) return;
    m_isIndeterminate = indeterminate;
    updateAnimationState();
    update();
    emit isIndeterminateChanged(m_isIndeterminate);
}

void ProgressBar::setMinimum(double minimum)
{
    setRange(minimum, m_maximum);
}

void ProgressBar::setMaximum(double maximum)
{
    setRange(m_minimum, maximum);
}

void ProgressBar::setRange(double minimum, double maximum)
{
    if (!isFiniteNumber(minimum) || !isFiniteNumber(maximum)) return;
    if (maximum <= minimum) {
        maximum = minimum + 1.0;
        if (!isFiniteNumber(maximum) || maximum <= minimum) {
            minimum = maximum - 1.0;
        }
    }

    const double oldMinimum = m_minimum;
    const double oldMaximum = m_maximum;
    const double oldValue = m_value;

    m_minimum = minimum;
    m_maximum = maximum;
    m_value = normalizedValue(m_value);

    const bool minimumChangedNow = !nearlyEqual(oldMinimum, m_minimum);
    const bool maximumChangedNow = !nearlyEqual(oldMaximum, m_maximum);
    const bool valueChangedNow = !nearlyEqual(oldValue, m_value);

    if (!minimumChangedNow && !maximumChangedNow && !valueChangedNow) return;

    update();
    if (minimumChangedNow) emit minimumChanged(m_minimum);
    if (maximumChangedNow) emit maximumChanged(m_maximum);
    if (valueChangedNow) emit valueChanged(m_value);
}

void ProgressBar::setValue(double value)
{
    const double clampedValue = normalizedValue(value);
    if (nearlyEqual(m_value, clampedValue)) return;
    m_value = clampedValue;
    update();
    emit valueChanged(m_value);
}

void ProgressBar::setShowPaused(bool paused)
{
    if (m_showPaused == paused) return;
    m_showPaused = paused;
    updateAnimationState();
    update();
    emit showPausedChanged(m_showPaused);
}

void ProgressBar::setShowError(bool error)
{
    if (m_showError == error) return;
    m_showError = error;
    updateAnimationState();
    update();
    emit showErrorChanged(m_showError);
}

void ProgressBar::setBarWidth(int width)
{
    if (width <= 0 || m_barWidth == width) return;
    m_barWidth = width;
    updateGeometry();
    update();
    emit barWidthChanged(m_barWidth);
}

void ProgressBar::setTrackThickness(qreal thickness)
{
    if (thickness <= 0.0 || !std::isfinite(thickness) || nearlyEqual(m_trackThickness, thickness)) return;
    m_trackThickness = thickness;
    update();
    emit trackThicknessChanged(m_trackThickness);
}

void ProgressBar::setRailVisible(bool visible)
{
    if (m_railVisible == visible) return;
    m_railVisible = visible;
    update();
    emit railVisibleChanged(m_railVisible);
}

double ProgressBar::progressRatio() const
{
    const double range = m_maximum - m_minimum;
    if (range <= 0.0 || !isFiniteNumber(range)) return 0.0;
    return (m_value - m_minimum) / range;
}

QString ProgressBar::progressText() const
{
    return QString::number(qRound(m_value));
}

QSize ProgressBar::sizeHint() const
{
    return QSize(m_barWidth, kDefaultHeight);
}

QSize ProgressBar::minimumSizeHint() const
{
    return QSize(kMinimumWidth, kDefaultHeight);
}

void ProgressBar::onThemeUpdated()
{
    updateThemeColors();
    update();
}

void ProgressBar::paintEvent(QPaintEvent*)
{
    const QRectF bounds = barRect();
    if (!bounds.isValid() || bounds.isEmpty()) return;

    const qreal effectiveTrackThickness = qMin(m_trackThickness, qMax<qreal>(1.0, bounds.height()));
    const qreal centerY = bounds.center().y();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_railVisible) {
        const QRectF railRect(bounds.left(), centerY - kRailThickness / 2.0, bounds.width(), kRailThickness);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_railColor);
        painter.drawRoundedRect(railRect, kRailThickness / 2.0, kRailThickness / 2.0);
    }

    const QColor color = indicatorColor();
    const qreal radius = effectiveTrackThickness / 2.0;
    const QRectF trackBounds(bounds.left(), centerY - radius, bounds.width(), effectiveTrackThickness);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (m_isIndeterminate) {
        const qreal segmentWidth = qBound(effectiveTrackThickness, bounds.width() * kIndeterminateSegmentRatio, bounds.width());
        qreal segmentLeft = bounds.left() + (bounds.width() - segmentWidth) / 2.0;
        if (isRunningState()) {
            segmentLeft = bounds.left() - segmentWidth + (bounds.width() + segmentWidth) * m_animationPhase;
        }

        painter.save();
        painter.setClipRect(trackBounds.adjusted(0.0, -1.0, 0.0, 1.0));
        painter.drawRoundedRect(QRectF(segmentLeft, trackBounds.top(), segmentWidth, trackBounds.height()), radius, radius);
        painter.restore();
        return;
    }

    const qreal ratio = qBound(0.0, progressRatio(), 1.0);
    if (ratio <= 0.0) return;

    const qreal filledWidth = qMin(bounds.width(), qMax(effectiveTrackThickness, bounds.width() * ratio));
    painter.drawRoundedRect(QRectF(trackBounds.left(), trackBounds.top(), filledWidth, trackBounds.height()), radius, radius);
}

void ProgressBar::timerEvent(QTimerEvent* event)
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
        m_animationPhase + static_cast<qreal>(kAnimationIntervalMs) / cycleMs,
        1.0);
    update();
}

void ProgressBar::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        updateAnimationState();
        update();
    }
}

void ProgressBar::hideEvent(QHideEvent* event)
{
    m_animationTimer.stop();
    QWidget::hideEvent(event);
}

void ProgressBar::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateAnimationState();
}

QRectF ProgressBar::barRect() const
{
    const qreal availableWidth = qMax<qreal>(0.0, width());
    const qreal actualWidth = qMin<qreal>(availableWidth, m_barWidth);
    const qreal actualHeight = qMax<qreal>(m_trackThickness, kDefaultHeight);
    const qreal x = (availableWidth - actualWidth) / 2.0;
    const qreal y = (height() - actualHeight) / 2.0;
    return QRectF(x, y, actualWidth, actualHeight);
}

QColor ProgressBar::indicatorColor() const
{
    if (!isEnabled()) return m_disabledColor;
    if (m_showError) return m_errorColor;
    if (m_showPaused) return m_pausedColor;
    return m_runningColor;
}

void ProgressBar::updateThemeColors()
{
    const auto& colors = themeColors();
    m_runningColor = colors.accentDefault;
    m_pausedColor = colors.systemCaution;
    m_errorColor = colors.systemCritical;
    m_disabledColor = colors.accentDisabled;
    m_railColor = colors.strokeStrong;
    if (currentTheme() == Dark) {
        m_railColor.setAlpha(qMax(m_railColor.alpha(), 138));
    } else {
        m_railColor.setAlpha(qMax(m_railColor.alpha(), 112));
    }
}

void ProgressBar::updateAnimationState()
{
    if (shouldAnimate()) {
        if (!m_animationTimer.isActive()) {
            m_animationTimer.start(kAnimationIntervalMs, this);
        }
    } else if (m_animationTimer.isActive()) {
        m_animationTimer.stop();
    }
}

bool ProgressBar::shouldAnimate() const
{
    return m_isIndeterminate
        && !m_showPaused
        && !m_showError
        && isEnabled()
        && isVisible();
}

bool ProgressBar::isRunningState() const
{
    return !m_showPaused && !m_showError && isEnabled();
}

double ProgressBar::normalizedValue(double value) const
{
    if (!isFiniteNumber(value)) return m_minimum;
    return std::min(std::max(value, m_minimum), m_maximum);
}

} // namespace fluent::status_info
