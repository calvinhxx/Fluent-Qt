#include "ProgressRing.h"

#include <QEvent>
#include <QHideEvent>
#include <QPainter>
#include <QPen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimerEvent>
#include <QtGlobal>
#include <QtMath>
#include <cmath>
#include <limits>

namespace fluent::status_info {

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

    // Branch on the active design language. DesignFluent keeps the original WinUI arc treatment exactly;
    // Material 3 and macOS render their own circular indicators. zh_CN: 按当前设计语言分支。DesignFluent 保持
    // 原始 WinUI 弧线处理完全不变;Material 3 与 macOS 绘制各自的环形指示器。
    const DesignLanguage lang = themeDesignLanguage();
    if (lang == DesignMaterial) {
        paintMaterial(painter, arcRect, effectiveStrokeWidth);
        return;
    }
    if (lang == DesignCupertino) {
        paintCupertino(painter, arcRect, effectiveStrokeWidth);
        return;
    }

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

void ProgressRing::paintMaterial(QPainter& painter, const QRectF& arcRect, qreal effectiveStrokeWidth)
{
    // Material 3 "Circular progress indicator": a rounded-cap arc in accentDefault over a light
    // secondary-container track ring (low-alpha accent tint, falling back to controlSecondary).
    // Determinate adds a small leading stop dot with a gap from the arc. Paused/error recolor the arc.
    // zh_CN: Material 3「环形进度指示器」:accentDefault 圆头弧线绘制在浅色二级容器轨道环(accent 低透明度,
    // 回退 controlSecondary)之上。确定态在弧线前端加一个带间隙的停止点;暂停/错误重新着色弧线。
    const auto& colors = themeColors();

    QColor activeColor = indicatorColor();
    if (m_status == ProgressRingStatus::Paused) activeColor = colors.systemCaution;
    else if (m_status == ProgressRingStatus::Error) activeColor = colors.systemCritical;

    // Track ring: a faint accent tint; controlSecondary is the fallback when the accent is unusable.
    // zh_CN: 轨道环:淡淡的 accent 着色;accent 不可用时回退 controlSecondary。
    QColor trackColor = colors.accentDefault;
    if (trackColor.isValid()) {
        trackColor.setAlpha(48);
    } else if (colors.controlSecondary.isValid()) {
        trackColor = colors.controlSecondary;
    } else {
        trackColor = Qt::transparent;
    }

    QPen pen;
    pen.setWidthF(effectiveStrokeWidth);
    pen.setCapStyle(Qt::RoundCap);

    if (trackColor.isValid() && trackColor.alpha() > 0) {
        pen.setColor(trackColor);
        painter.setPen(pen);
        painter.drawArc(arcRect, 0, static_cast<int>(kFullCircleDegrees * 16));
    }

    if (!m_isActive) return;
    if (!(activeColor.isValid() && activeColor.alpha() > 0)) return;

    qreal startDegrees = 90.0;
    int spanAngle = 0;
    if (m_isIndeterminate) {
        startDegrees -= m_animationPhase;
        spanAngle = static_cast<int>(-kIndeterminateArcDegrees * 16);
    } else {
        spanAngle = static_cast<int>(-kFullCircleDegrees * progressRatio() * 16.0);
    }

    if (spanAngle == 0) return;

    pen.setColor(activeColor);
    painter.setPen(pen);
    painter.drawArc(arcRect, static_cast<int>(startDegrees * 16.0), spanAngle);

    // Leading stop dot: a small filled accent circle just ahead of the arc's leading end, separated by
    // a small gap. The leading end sits at startDegrees + spanAngle (spanAngle is negative / clockwise).
    // zh_CN: 前端停止点:位于弧线前端略前方的小实心 accent 圆,与弧线之间留小间隙。前端位于
    // startDegrees + spanAngle(spanAngle 为负值,顺时针)。
    const qreal leadingDegrees = startDegrees + static_cast<qreal>(spanAngle) / 16.0;
    const qreal gapDegrees = 14.0; // small visible gap ahead of the arc. zh_CN: 弧线前方的小间隙。
    const qreal dotDegrees = (leadingDegrees - gapDegrees) * M_PI / 180.0;
    const QPointF center = arcRect.center();
    const qreal radius = arcRect.width() / 2.0;
    const QPointF dotPos(center.x() + radius * std::cos(dotDegrees),
                         center.y() - radius * std::sin(dotDegrees));
    const qreal dotRadius = qMax<qreal>(1.0, effectiveStrokeWidth * 0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(activeColor);
    painter.drawEllipse(dotPos, dotRadius, dotRadius);
}

void ProgressRing::paintCupertino(QPainter& painter, const QRectF& arcRect, qreal effectiveStrokeWidth)
{
    // macOS circular progress. INDETERMINATE: the signature spoke spinner — short tapered round-cap
    // lines arranged radially, with opacity fading around the circle (leading spoke ~full alpha,
    // trailing spokes faint). DETERMINATE: a thin accent ring over a faint neutral track. Paused/error
    // recolor the active stroke. zh_CN: macOS 环形进度。不确定态:标志性辐条转轮——短的渐细圆头线段呈放射状
    // 排列,沿圆周不透明度渐隐(前导辐条接近全透明度,尾随辐条很淡)。确定态:淡中性轨道上的细 accent 环。
    // 暂停/错误重新着色活动描边。
    const auto& colors = themeColors();
    const QPointF center = arcRect.center();
    const qreal outerRadius = arcRect.width() / 2.0;

    QColor activeColor = colors.accentDefault;
    if (m_status == ProgressRingStatus::Paused) activeColor = colors.systemCaution;
    else if (m_status == ProgressRingStatus::Error) activeColor = colors.systemCritical;
    if (!isEnabled()) activeColor = m_disabledColor;

    if (m_isIndeterminate) {
        // Spoke spinner. Tint spokes textSecondary at rest (or the status color when paused/error),
        // and ramp alpha from the leading spoke down to the faintest trailing spoke. zh_CN: 辐条转轮。
        // 静息时用 textSecondary 着色(暂停/错误时用状态色),从前导辐条到最淡的尾随辐条做 alpha 渐变。
        QColor spokeColor = colors.textSecondary;
        if (m_status == ProgressRingStatus::Paused) spokeColor = colors.systemCaution;
        else if (m_status == ProgressRingStatus::Error) spokeColor = colors.systemCritical;
        if (!isEnabled()) spokeColor = m_disabledColor;
        if (!spokeColor.isValid()) spokeColor = QColor(128, 128, 128);

        constexpr int kSpokeCount = 12;
        const qreal spokeWidth = qMax<qreal>(1.0, effectiveStrokeWidth * 0.7);
        const qreal innerRadius = outerRadius * 0.55;
        const qreal outerEnd = outerRadius - spokeWidth * 0.5; // keep round caps inside the bounds. zh_CN: 让圆头留在边界内。

        QPen pen;
        pen.setWidthF(spokeWidth);
        pen.setCapStyle(Qt::RoundCap);

        // The leading spoke advances with the animation phase. Even in a static grab (no phase), the
        // fading ramp still reads as a spinner. zh_CN: 前导辐条随动画相位前进。即使静态抓取(无相位),
        // 渐隐也仍呈现为转轮。
        for (int i = 0; i < kSpokeCount; ++i) {
            const qreal fraction = static_cast<qreal>(i) / kSpokeCount;
            const qreal angleDeg = 90.0 - m_animationPhase - fraction * kFullCircleDegrees;
            const qreal angleRad = angleDeg * M_PI / 180.0;
            const qreal cosA = std::cos(angleRad);
            const qreal sinA = std::sin(angleRad);
            // Alpha ramps from ~255 (leading, i==0) down to ~40 (trailing). zh_CN: alpha 从前导 ~255 渐降到尾随 ~40。
            const int alpha = static_cast<int>(255.0 - fraction * 215.0);
            QColor c = spokeColor;
            c.setAlpha(qBound(0, alpha, 255));
            pen.setColor(c);
            painter.setPen(pen);
            const QPointF p1(center.x() + innerRadius * cosA, center.y() - innerRadius * sinA);
            const QPointF p2(center.x() + outerEnd * cosA, center.y() - outerEnd * sinA);
            painter.drawLine(p1, p2);
        }
        return;
    }

    // Determinate: a thin (~3 pt) accent ring over a faint neutral track; cap the stroke so it stays
    // hairline even when callers set a heavier strokeWidth. zh_CN: 确定态:淡中性轨道上的细(~3pt)accent 环;
    // 限制描边粗细,即使调用方设置更粗的 strokeWidth 也保持发丝感。
    const qreal thinStroke = qMin<qreal>(effectiveStrokeWidth, 3.0);
    QPen pen;
    pen.setWidthF(thinStroke);
    pen.setCapStyle(Qt::RoundCap);

    QColor trackColor = (effectiveTheme() == Dark) ? colors.strokeSurface : colors.strokeSecondary;
    if (!trackColor.isValid()) trackColor = colors.controlSecondary;
    if (trackColor.isValid()) {
        if (trackColor.alpha() < 48) trackColor.setAlpha(48);
        pen.setColor(trackColor);
        painter.setPen(pen);
        painter.drawArc(arcRect, 0, static_cast<int>(kFullCircleDegrees * 16));
    }

    if (!m_isActive) return;
    if (!(activeColor.isValid() && activeColor.alpha() > 0)) return;

    const int spanAngle = static_cast<int>(-kFullCircleDegrees * progressRatio() * 16.0);
    if (spanAngle == 0) return;

    pen.setColor(activeColor);
    painter.setPen(pen);
    painter.drawArc(arcRect, static_cast<int>(90.0 * 16.0), spanAngle);
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
    m_trackColor = effectiveTheme() == Dark ? colors.strokeSurface : colors.strokeSecondary;
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

} // namespace fluent::status_info
