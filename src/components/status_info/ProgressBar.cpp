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

    // Brand style themes can re-shape the bar structurally. DesignFluent keeps the exact original
    // treatment below; Material 3 and macOS dispatch to their own painters. zh_CN: 品牌样式主题可结构化重塑进度
    // 条。DesignFluent 保留下方原始处理；Material 3 与 macOS 分派到各自的绘制函数。
    const DesignLanguage lang = themeDesignLanguage();
    if (lang == DesignMaterial) {
        paintMaterial(painter, bounds);
        return;
    }
    if (lang == DesignCupertino) {
        paintCupertino(painter, bounds);
        return;
    }

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

void ProgressBar::paintMaterial(QPainter& painter, const QRectF& bounds)
{
    // Material 3 "Linear progress indicator" (expressive): a ~4 dp fully-rounded track tinted from the
    // active color, with the active indicator in accentDefault. The signature M3 cue is determinate-only:
    // a ~4 px gap between the active segment and the remaining track, plus a stop dot pinned at the far
    // (trailing) end. paused→systemCaution, error→systemCritical recolor the active indicator + stop dot.
    // zh_CN: Material 3 线性进度（表达式更新）：~4dp 全圆角轨道，轨道为活动色低透明度着色，活动指示器为
    // accentDefault。M3 标志性提示仅在确定态：活动段与剩余轨道之间留 ~4px 间隙，并在远（尾）端固定一个停止
    // 点。paused→systemCaution、error→systemCritical 重新着色活动指示器与停止点。
    const auto& colors = themeColors();

    // M3 track thickness is ~4 dp, fully rounded, but never thicker than what the widget allows.
    // zh_CN: M3 轨道厚度约 4dp，全圆角，但不超过控件可容纳的高度。
    const qreal thickness = qBound<qreal>(2.0, 4.0, qMax<qreal>(1.0, bounds.height()));
    const qreal radius = thickness / 2.0;
    const qreal centerY = bounds.center().y();
    const QRectF trackBounds(bounds.left(), centerY - radius, bounds.width(), thickness);

    // Active color follows the existing Fluent value semantics (disabled / error / paused / running).
    // zh_CN: 活动色沿用现有 Fluent 值语义（禁用/错误/暂停/运行）。
    const QColor active = indicatorColor();

    // Track tint: a low-alpha wash of the active color, falling back to controlSecondary when the active
    // color is somehow unusable. Guard against the invalid-QColor trap. zh_CN: 轨道着色：活动色低透明度淡彩，
    // 活动色不可用时回退 controlSecondary。防范无效 QColor 陷阱。
    QColor track = Qt::transparent;
    if (active.isValid()) {
        track = active;
        track.setAlpha(effectiveTheme() == Dark ? 56 : 48);
    }
    if (!track.isValid() || track.alpha() == 0) {
        track = colors.controlSecondary;
    }

    painter.setPen(Qt::NoPen);

    // 1. Track. zh_CN: 轨道。
    if (track.isValid() && track.alpha() > 0) {
        painter.setBrush(track);
        painter.drawRoundedRect(trackBounds, radius, radius);
    }

    // M3 dimensions: ~4 px gap before the remaining track and a stop dot of the track thickness.
    // zh_CN: M3 尺寸：剩余轨道前 ~4px 间隙，停止点直径等于轨道厚度。
    const qreal gap = qMin<qreal>(4.0, bounds.width() * 0.25);
    const qreal dotDiameter = thickness;
    const qreal dotRadius = dotDiameter / 2.0;

    // 2. Stop dot pinned at the far (trailing) end of the track. zh_CN: 固定在轨道远（尾）端的停止点。
    if (active.isValid() && active.alpha() > 0) {
        const QPointF dotCenter(trackBounds.right() - dotRadius, centerY);
        painter.setBrush(active);
        painter.drawEllipse(dotCenter, dotRadius, dotRadius);
    }

    if (m_isIndeterminate) {
        // Indeterminate: a travelling rounded segment, reusing the existing animation phase. The stop dot
        // stays drawn (M3 keeps it). zh_CN: 不确定态：行进的圆角段，复用现有动画相位。停止点保持绘制（M3 保留）。
        const qreal segmentWidth = qBound(thickness, bounds.width() * kIndeterminateSegmentRatio, bounds.width());
        qreal segmentLeft = bounds.left() + (bounds.width() - segmentWidth) / 2.0;
        if (isRunningState()) {
            segmentLeft = bounds.left() - segmentWidth + (bounds.width() + segmentWidth) * m_animationPhase;
        }
        if (active.isValid() && active.alpha() > 0) {
            painter.save();
            painter.setClipRect(trackBounds.adjusted(0.0, -1.0, 0.0, 1.0));
            painter.setBrush(active);
            painter.drawRoundedRect(QRectF(segmentLeft, trackBounds.top(), segmentWidth, trackBounds.height()), radius, radius);
            painter.restore();
        }
        return;
    }

    // Determinate: active segment from the left up to the ratio, leaving a ~4 px gap before the remaining
    // track (and before the stop dot). zh_CN: 确定态：活动段从左侧延伸至比例处，在剩余轨道（及停止点）前留 ~4px 间隙。
    const qreal ratio = qBound(0.0, progressRatio(), 1.0);
    if (ratio <= 0.0) return;
    if (!active.isValid() || active.alpha() == 0) return;

    // The active segment must not run under the stop dot; cap it so the gap is preserved near full value.
    // zh_CN: 活动段不得伸入停止点；做上限裁剪，使接近满值时仍保留间隙。
    const qreal maxActiveRight = trackBounds.right() - dotDiameter - gap;
    qreal activeRight = trackBounds.left() + bounds.width() * ratio;
    activeRight = qMin(activeRight, maxActiveRight);
    const qreal activeWidth = activeRight - trackBounds.left();
    if (activeWidth <= 0.0) return;

    painter.setBrush(active);
    painter.drawRoundedRect(QRectF(trackBounds.left(), trackBounds.top(),
                                   qMax(thickness, activeWidth), trackBounds.height()),
                            radius, radius);
}

void ProgressBar::paintCupertino(QPainter& painter, const QRectF& bounds)
{
    // macOS linear progress: a thin (~6 pt) fully-rounded bar. Track = a neutral fill (controlSecondary,
    // falling back to a low-alpha strokeStrong); active = a rounded accentDefault fill. Deliberately quiet
    // — NO gap and NO stop dot (those are M3-only). paused→systemCaution, error→systemCritical.
    // zh_CN: macOS 线性进度：细（~6pt）全圆角条。轨道=中性填充（controlSecondary，回退到低透明度
    // strokeStrong）；活动=圆角 accentDefault 填充。刻意低调——无间隙、无停止点（仅 M3）。
    // paused→systemCaution、error→systemCritical。
    const auto& colors = themeColors();

    const qreal thickness = qBound<qreal>(2.0, 6.0, qMax<qreal>(1.0, bounds.height()));
    const qreal radius = thickness / 2.0;
    const qreal centerY = bounds.center().y();
    const QRectF trackBounds(bounds.left(), centerY - radius, bounds.width(), thickness);

    const QColor active = indicatorColor();

    // Neutral track: controlSecondary, with a low-alpha strokeStrong fallback. Guard the invalid-QColor
    // trap so an unassigned color never paints solid black. zh_CN: 中性轨道：controlSecondary，低透明度
    // strokeStrong 回退。防范无效 QColor 陷阱，避免未赋值色涂成纯黑。
    QColor track = colors.controlSecondary;
    if (!track.isValid() || track.alpha() == 0) {
        track = colors.strokeStrong;
        if (track.isValid()) {
            track.setAlpha(effectiveTheme() == Dark ? 64 : 48);
        }
    }

    painter.setPen(Qt::NoPen);

    // 1. Track. zh_CN: 轨道。
    if (track.isValid() && track.alpha() > 0) {
        painter.setBrush(track);
        painter.drawRoundedRect(trackBounds, radius, radius);
    }

    if (m_isIndeterminate) {
        // Indeterminate: a travelling rounded segment (our barber-pole approximation). zh_CN: 不确定态：行进
        // 的圆角段（理发店灯柱近似）。
        const qreal segmentWidth = qBound(thickness, bounds.width() * kIndeterminateSegmentRatio, bounds.width());
        qreal segmentLeft = bounds.left() + (bounds.width() - segmentWidth) / 2.0;
        if (isRunningState()) {
            segmentLeft = bounds.left() - segmentWidth + (bounds.width() + segmentWidth) * m_animationPhase;
        }
        if (active.isValid() && active.alpha() > 0) {
            painter.save();
            painter.setClipRect(trackBounds.adjusted(0.0, -1.0, 0.0, 1.0));
            painter.setBrush(active);
            painter.drawRoundedRect(QRectF(segmentLeft, trackBounds.top(), segmentWidth, trackBounds.height()), radius, radius);
            painter.restore();
        }
        return;
    }

    // Determinate: a rounded active fill from the left, no gap, no stop dot. zh_CN: 确定态：从左侧起的圆角活动
    // 填充，无间隙、无停止点。
    const qreal ratio = qBound(0.0, progressRatio(), 1.0);
    if (ratio <= 0.0) return;
    if (!active.isValid() || active.alpha() == 0) return;

    const qreal filledWidth = qMin(bounds.width(), qMax(thickness, bounds.width() * ratio));
    painter.setBrush(active);
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
    if (effectiveTheme() == Dark) {
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
