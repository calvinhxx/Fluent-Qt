#include "RangeSlider.h"

#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QVariantAnimation>

#include <algorithm>
#include <limits>

#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "components/basicinput/private/RangeSliderAccessibility_p.h"
#include "components/basicinput/private/SliderPainter_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/status_info/ToolTip.h"

namespace fluent::basicinput {

namespace {

qint64 bounded(qint64 low, qint64 value, qint64 high)
{
    return std::max(low, std::min(value, high));
}

int clampToInt(qint64 value)
{
    return static_cast<int>(bounded(std::numeric_limits<int>::min(), value, std::numeric_limits<int>::max()));
}

int absoluteStep(int step)
{
    if (step == std::numeric_limits<int>::min())
        return std::numeric_limits<int>::max();
    return qAbs(step);
}

} // namespace

RangeSlider::RangeSlider(Qt::Orientation orientation, QWidget* parent) : QWidget(parent), m_orientation(orientation)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    ::compatibility::detail::disableNativeFocusRing(this);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (m_orientation == Qt::Vertical)
        policy.transpose();
    setSizePolicy(policy);
    setAttribute(Qt::WA_WState_OwnSizePolicy, false);

    m_widgetHoverAnim = createAnimation(&m_widgetHoverRatio);
    for (ThumbMotion* thumb : {&m_lowerMotion, &m_upperMotion}) {
        thumb->hoverAnim = createAnimation(&thumb->hoverRatio);
        thumb->pressAnim = createAnimation(&thumb->pressRatio);
    }

    detail::ensureRangeSliderAccessibilityFactory();
}

RangeSlider::RangeSlider(QWidget* parent) : RangeSlider(Qt::Horizontal, parent) {}

RangeSlider::~RangeSlider()
{
    delete m_toolTip;
}

QSize RangeSlider::sizeHint() const
{
    const int thickness = std::max(m_handleSize, m_trackHeight) + 2 * m_visualMargin;
    return m_orientation == Qt::Horizontal ? QSize(m_defaultLength, thickness) : QSize(thickness, m_defaultLength);
}

QSize RangeSlider::minimumSizeHint() const
{
    const int thickness = std::max(m_handleSize, m_trackHeight) + 2 * m_visualMargin;
    // Reserve both endpoints plus room for the two thumbs side by side.
    // zh_CN: 预留两端空间，并容纳两个并排的手柄。
    const int length = 2 * padding() + 2 * m_handleSize;
    return m_orientation == Qt::Horizontal ? QSize(length, thickness) : QSize(thickness, length);
}

// Properties. zh_CN: 属性。

void RangeSlider::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    if (!testAttribute(Qt::WA_WState_OwnSizePolicy)) {
        QSizePolicy policy = sizePolicy();
        policy.transpose();
        setSizePolicy(policy);
        setAttribute(Qt::WA_WState_OwnSizePolicy, false);
    }
    updateGeometry();
    update();
    emit orientationChanged(m_orientation);
}

void RangeSlider::setMinimum(int minimum)
{
    setRange(minimum, qMax(minimum, m_maximum));
}

void RangeSlider::setMaximum(int maximum)
{
    setRange(qMin(m_minimum, maximum), maximum);
}

void RangeSlider::setRange(int minimum, int maximum)
{
    maximum = qMax(minimum, maximum);
    if (m_minimum == minimum && m_maximum == maximum)
        return;
    m_minimum = minimum;
    m_maximum = maximum;
    emit rangeChanged(m_minimum, m_maximum);
    setValues(m_lower, m_upper);
    update();
}

void RangeSlider::setLowerValue(int value)
{
    const qint64 ceiling = static_cast<qint64>(m_upper) - effectiveMinimumRange();
    commitValues(static_cast<int>(bounded(m_minimum, value, ceiling)), m_upper);
}

void RangeSlider::setUpperValue(int value)
{
    const qint64 floor = static_cast<qint64>(m_lower) + effectiveMinimumRange();
    commitValues(m_lower, static_cast<int>(bounded(floor, value, m_maximum)));
}

void RangeSlider::setValues(int lower, int upper)
{
    qint64 low = bounded(m_minimum, qMin(lower, upper), m_maximum);
    qint64 high = bounded(m_minimum, qMax(lower, upper), m_maximum);
    const qint64 gap = effectiveMinimumRange();
    if (high - low < gap) {
        // Keep the lower value and move the upper one first; shift both only at the maximum.
        // zh_CN: 优先保留下限并移动上限；仅在到达最大值时同时平移。
        high = qMin<qint64>(m_maximum, low + gap);
        low = qMax<qint64>(m_minimum, high - gap);
    }
    commitValues(static_cast<int>(low), static_cast<int>(high));
}

void RangeSlider::setMinimumRange(int range)
{
    range = qMax(0, range);
    if (m_minimumRange == range)
        return;
    m_minimumRange = range;
    emit minimumRangeChanged(m_minimumRange);
    setValues(m_lower, m_upper);
}

void RangeSlider::setSingleStep(int step)
{
    step = absoluteStep(step);
    if (m_singleStep == step)
        return;
    m_singleStep = step;
    update();
    emit singleStepChanged(m_singleStep);
}

void RangeSlider::setPageStep(int step)
{
    step = absoluteStep(step);
    if (m_pageStep == step)
        return;
    m_pageStep = step;
    update();
    emit pageStepChanged(m_pageStep);
}

void RangeSlider::setTickPosition(QSlider::TickPosition position)
{
    if (m_tickPosition == position)
        return;
    m_tickPosition = position;
    update();
    emit tickPositionChanged(m_tickPosition);
}

void RangeSlider::setTickInterval(int interval)
{
    interval = qMax(0, interval);
    if (m_tickInterval == interval)
        return;
    m_tickInterval = interval;
    update();
    emit tickIntervalChanged(m_tickInterval);
}

void RangeSlider::setTracking(bool enabled)
{
    if (m_tracking == enabled)
        return;
    m_tracking = enabled;
    emit trackingChanged(m_tracking);
}

void RangeSlider::setActiveThumb(Thumb thumb)
{
    if (m_activeThumb == thumb)
        return;
    m_activeThumb = thumb;
    update();
    updateToolTip();
    emit activeThumbChanged(m_activeThumb);
    if (hasFocus())
        detail::notifyRangeSliderFocusChanged(this);
}

QPoint RangeSlider::thumbCenter(Thumb thumb) const
{
    const int pixel = valueToPixel(position(thumb));
    return m_orientation == Qt::Horizontal ? QPoint(pixel, height() / 2) : QPoint(width() / 2, pixel);
}

// Value model. zh_CN: 数值模型。

qint64 RangeSlider::effectiveMinimumRange() const
{
    return qMin<qint64>(m_minimumRange, static_cast<qint64>(m_maximum) - m_minimum);
}

void RangeSlider::commitValues(int lower, int upper)
{
    const bool positionsChanged = m_lowerPosition != lower || m_upperPosition != upper;
    m_lowerPosition = lower;
    m_upperPosition = upper;
    // A keyboard or programmatic move can carry the hovered thumb away from a
    // stationary pointer, so re-check which thumb is under it.
    // zh_CN: 键盘或代码移动可能让悬停的手柄离开静止的指针，因此重新判断指针下的手柄。
    if (positionsChanged && m_hasHoveredThumb && !m_dragging)
        updateHover(mapFromGlobal(QCursor::pos()));
    const bool lowerChanged = m_lower != lower;
    const bool upperChanged = m_upper != upper;
    if (!lowerChanged && !upperChanged) {
        if (positionsChanged)
            update();
        return;
    }

    // Store both values before any signal so slots always read a consistent pair.
    // zh_CN: 发送信号前先存储两个值，确保槽函数读取到一致的数值对。
    m_lower = lower;
    m_upper = upper;
    update();
    updateToolTip();
    if (lowerChanged) {
        detail::notifyRangeSliderValueChanged(this, Thumb::Lower);
        emit lowerValueChanged(m_lower);
    }
    if (upperChanged) {
        detail::notifyRangeSliderValueChanged(this, Thumb::Upper);
        emit upperValueChanged(m_upper);
    }
    emit valuesChanged(m_lower, m_upper);
}

void RangeSlider::moveThumbTo(Thumb thumb, int value)
{
    const qint64 gap = effectiveMinimumRange();
    int lower = m_lowerPosition;
    int upper = m_upperPosition;
    if (thumb == Thumb::Lower)
        lower = static_cast<int>(bounded(m_minimum, value, static_cast<qint64>(upper) - gap));
    else
        upper = static_cast<int>(bounded(static_cast<qint64>(lower) + gap, value, m_maximum));
    if (lower == m_lowerPosition && upper == m_upperPosition)
        return;

    if (m_tracking || !m_dragging) {
        m_lowerPosition = lower;
        m_upperPosition = upper;
        if (m_dragging)
            emit sliderMoved(lower, upper);
        commitValues(lower, upper);
        return;
    }
    m_lowerPosition = lower;
    m_upperPosition = upper;
    update();
    updateToolTip();
    emit sliderMoved(lower, upper);
}

void RangeSlider::stepThumb(Thumb thumb, qint64 delta)
{
    const qint64 current = thumb == Thumb::Lower ? m_lower : m_upper;
    const int target = clampToInt(current + delta);
    if (thumb == Thumb::Lower)
        setLowerValue(target);
    else
        setUpperValue(target);
}

// Geometry. zh_CN: 几何。

int RangeSlider::padding() const
{
    return m_handleSize / 2 + m_visualMargin;
}

int RangeSlider::trackLength() const
{
    const int length = m_orientation == Qt::Horizontal ? width() : height();
    return qMax(0, length - 2 * padding());
}

bool RangeSlider::isUpsideDown() const
{
    // Same mapping as QSlider: horizontal follows the layout direction and
    // vertical places the minimum at the bottom.
    // zh_CN: 与 QSlider 一致：水平方向跟随布局方向，竖向时最小值在底部。
    return m_orientation == Qt::Horizontal ? layoutDirection() == Qt::RightToLeft : true;
}

int RangeSlider::valueToPixel(int value) const
{
    const int pad = padding();
    const int available = trackLength();
    const qint64 range = static_cast<qint64>(m_maximum) - m_minimum;
    const bool upsideDown = isUpsideDown();
    if (range == 0)
        return pad + (upsideDown ? available : 0);

    // Widen before subtracting and multiplying so the full int range maps safely.
    // zh_CN: 减法和乘法前提升位宽，保证完整 int 范围安全映射。
    const int clamped = qBound(m_minimum, value, m_maximum);
    const qint64 offset =
        upsideDown ? static_cast<qint64>(m_maximum) - clamped : static_cast<qint64>(clamped) - m_minimum;
    return pad + static_cast<int>((offset * available + range / 2) / range);
}

int RangeSlider::pixelToValue(int pixel) const
{
    const int available = trackLength();
    if (available <= 0)
        return m_minimum;
    return QStyle::sliderValueFromPosition(m_minimum, m_maximum, pixel - padding(), available, isUpsideDown());
}

int RangeSlider::pointerAxis(const QPoint& point) const
{
    return m_orientation == Qt::Horizontal ? point.x() : point.y();
}

bool RangeSlider::hitsThumb(Thumb thumb, const QPoint& point) const
{
    const QPoint delta = point - thumbCenter(thumb);
    const int radius = m_handleSize / 2 + m_visualMargin;
    return delta.x() * delta.x() + delta.y() * delta.y() <= radius * radius;
}

RangeSlider::Thumb RangeSlider::thumbForPointer(const QPoint& point, bool* grabbed) const
{
    const int axis = pointerAxis(point);
    const int lowerDistance = qAbs(axis - valueToPixel(m_lowerPosition));
    const int upperDistance = qAbs(axis - valueToPixel(m_upperPosition));
    const bool hitLower = hitsThumb(Thumb::Lower, point);
    const bool hitUpper = hitsThumb(Thumb::Upper, point);
    *grabbed = hitLower || hitUpper;
    if (hitLower != hitUpper)
        return hitLower ? Thumb::Lower : Thumb::Upper;

    // The nearer thumb wins. When both are equally near, the side of the
    // pointer decides; between coincident thumbs the active one keeps focus.
    // zh_CN: 更近的手柄优先；距离相同时按指针所在一侧决定，两个手柄重合时保留活动手柄。
    if (lowerDistance != upperDistance)
        return lowerDistance < upperDistance ? Thumb::Lower : Thumb::Upper;
    if (!*grabbed) {
        const int value = pixelToValue(axis);
        if (value > m_upperPosition)
            return Thumb::Upper;
        if (value < m_lowerPosition)
            return Thumb::Lower;
    }
    return m_activeThumb;
}

// Motion. zh_CN: 动效。

QVariantAnimation* RangeSlider::createAnimation(qreal* ratio)
{
    const auto& tokens = themeAnimation();
    auto* animation = new QVariantAnimation(this);
    animation->setDuration(tokens.normal);
    animation->setEasingCurve(tokens.decelerate);
    connect(animation, &QVariantAnimation::valueChanged, this, [this, ratio](const QVariant& value) {
        *ratio = value.toReal();
        update();
    });
    return animation;
}

void RangeSlider::animate(QVariantAnimation* animation, qreal from, qreal to)
{
    animation->stop();
    if (qFuzzyCompare(from + 1.0, to + 1.0))
        return;
    animation->setStartValue(from);
    animation->setEndValue(to);
    ::fluent::detail::startMotionTransition(animation, themeAnimation().normal);
}

void RangeSlider::setHoveredThumb(bool hasThumb, Thumb thumb)
{
    if (m_hasHoveredThumb == hasThumb && (!hasThumb || m_hoveredThumb == thumb))
        return;
    if (m_hasHoveredThumb) {
        ThumbMotion& previous = motion(m_hoveredThumb);
        animate(previous.hoverAnim, previous.hoverRatio, 0.0);
    }
    m_hasHoveredThumb = hasThumb;
    m_hoveredThumb = thumb;
    if (hasThumb) {
        ThumbMotion& next = motion(thumb);
        animate(next.hoverAnim, next.hoverRatio, 1.0);
    }
}

void RangeSlider::updateHover(const QPoint& point)
{
    if (!isEnabled() || !rect().contains(point)) {
        setHoveredThumb(false, m_hoveredThumb);
        return;
    }
    const bool hitLower = hitsThumb(Thumb::Lower, point);
    const bool hitUpper = hitsThumb(Thumb::Upper, point);
    if (!hitLower && !hitUpper) {
        setHoveredThumb(false, m_hoveredThumb);
        return;
    }
    setHoveredThumb(true, hitLower && hitUpper ? m_activeThumb : (hitLower ? Thumb::Lower : Thumb::Upper));
}

// Painting. zh_CN: 绘制。

void RangeSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const bool horizontal = m_orientation == Qt::Horizontal;
    const bool enabled = isEnabled();
    const int pad = padding();
    const int length = trackLength();
    const int cross = horizontal ? height() / 2 : width() / 2;
    const int thickness = m_trackHeight;

    // 1. Track and the selected range. zh_CN: 绘制轨道与选中区间。
    const QRect trackRect = horizontal
        ? QRect(pad, cross - thickness / 2, length, thickness)
        : QRect(cross - thickness / 2, pad, thickness, length);
    const int lowerPixel = valueToPixel(m_lowerPosition);
    const int upperPixel = valueToPixel(m_upperPosition);
    const int start = qMin(lowerPixel, upperPixel);
    const int span = qAbs(upperPixel - lowerPixel);
    const QRectF fillRect = horizontal
        ? QRectF(start, cross - thickness / 2, span, thickness)
        : QRectF(cross - thickness / 2, start, thickness, span);
    detail::paintSliderBar(p, QRectF(trackRect), detail::sliderTrackColor(colors, enabled));
    detail::paintSliderBar(p, fillRect, detail::sliderFillColor(colors, enabled));

    // 2. Ticks while hovered, as in Slider. zh_CN: 与 Slider 一样仅在悬停时绘制刻度。
    if (m_tickPosition != QSlider::NoTicks && m_widgetHoverRatio > 0.1) {
        p.setPen(detail::sliderTickColor(colors));
        const bool before = m_tickPosition == QSlider::TicksAbove || m_tickPosition == QSlider::TicksBothSides;
        const bool after = m_tickPosition == QSlider::TicksBelow || m_tickPosition == QSlider::TicksBothSides;
        const QVector<int> ticks = detail::sliderTickValues(m_minimum, m_maximum, m_tickInterval, m_singleStep,
            m_pageStep);
        for (const int value : ticks) {
            const int pixel = valueToPixel(value);
            if (horizontal) {
                if (before)
                    p.drawLine(pixel, trackRect.top() - 4, pixel, trackRect.top() - 2);
                if (after)
                    p.drawLine(pixel, trackRect.bottom() + 4, pixel, trackRect.bottom() + 6);
            } else {
                if (before)
                    p.drawLine(trackRect.left() - 4, pixel, trackRect.left() - 2, pixel);
                if (after)
                    p.drawLine(trackRect.right() + 4, pixel, trackRect.right() + 6, pixel);
            }
        }
    }

    // 3. Thumbs; the active thumb paints last so it stays on top.
    // zh_CN: 绘制手柄；活动手柄最后绘制以保持在上层。
    const Thumb inactive = m_activeThumb == Thumb::Lower ? Thumb::Upper : Thumb::Lower;
    for (const Thumb thumb : {inactive, m_activeThumb}) {
        const ThumbMotion& state = motion(thumb);
        detail::SliderThumbVisual visual;
        visual.center = thumbCenter(thumb);
        visual.diameter = m_handleSize;
        visual.hoverRatio = state.hoverRatio;
        visual.pressRatio = state.pressRatio;
        visual.keyboardFocused = thumb == m_activeThumb && hasFocus() && m_keyboardFocusVisible;
        visual.enabled = enabled;
        detail::paintSliderThumb(p, colors, visual);
    }
}

// Pointer input. zh_CN: 指针输入。

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_dragging) {
        event->ignore();
        return;
    }
    event->accept();
    m_keyboardFocusVisible = false;

    const QPoint point = fluentMousePos(event);
    bool grabbed = false;
    const Thumb thumb = thumbForPointer(point, &grabbed);
    // Coincident thumbs wait for the first movement to pick a direction.
    // zh_CN: 两个手柄重合时，等待首次移动方向再决定拖动哪一个。
    m_directionPending = grabbed && m_lowerPosition == m_upperPosition;
    m_dragOffset = grabbed ? pointerAxis(point) - valueToPixel(position(thumb)) : 0;
    m_dragging = true;
    setActiveThumb(thumb);
    setHoveredThumb(true, thumb);
    ThumbMotion& pressed = motion(thumb);
    animate(pressed.pressAnim, pressed.pressRatio, 1.0);

    emit sliderPressed();
    showToolTip();
    if (!grabbed)
        moveThumbTo(thumb, pixelToValue(pointerAxis(point)));
    update();
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint point = fluentMousePos(event);
    if (!m_dragging) {
        updateHover(point);
        event->ignore();
        return;
    }
    event->accept();

    const int value = pixelToValue(pointerAxis(point) - m_dragOffset);
    if (m_directionPending) {
        if (value == m_lowerPosition)
            return;
        m_directionPending = false;
        const Thumb thumb = value > m_upperPosition ? Thumb::Upper : Thumb::Lower;
        if (thumb != m_activeThumb) {
            ThumbMotion& previous = motion(m_activeThumb);
            animate(previous.pressAnim, previous.pressRatio, 0.0);
            setActiveThumb(thumb);
            setHoveredThumb(true, thumb);
            ThumbMotion& next = motion(thumb);
            animate(next.pressAnim, next.pressRatio, 1.0);
        }
    }
    moveThumbTo(m_activeThumb, value);
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        event->ignore();
        return;
    }
    event->accept();
    finishDrag();
    const QPoint point = fluentMousePos(event);
    // leaveEvent() keeps the hover state during a drag; settle it on release.
    // zh_CN: 拖动期间 leaveEvent() 保留悬停状态，松开时再统一收尾。
    if (!rect().contains(point))
        animate(m_widgetHoverAnim, m_widgetHoverRatio, 0.0);
    updateHover(point);
}

void RangeSlider::finishDrag()
{
    if (!m_dragging)
        return;
    m_dragging = false;
    m_directionPending = false;
    ThumbMotion& pressed = motion(m_activeThumb);
    animate(pressed.pressAnim, pressed.pressRatio, 0.0);
    hideToolTip();
    // Commit before announcing the release so slots read the final values.
    // zh_CN: 先提交再发送释放信号，保证槽函数读取到最终值。
    commitValues(m_lowerPosition, m_upperPosition);
    update();
    emit sliderReleased();
}

void RangeSlider::enterEvent(FluentEnterEvent* event)
{
    animate(m_widgetHoverAnim, m_widgetHoverRatio, 1.0);
    QWidget::enterEvent(event);
}

void RangeSlider::leaveEvent(QEvent* event)
{
    if (!m_dragging) {
        animate(m_widgetHoverAnim, m_widgetHoverRatio, 0.0);
        setHoveredThumb(false, m_hoveredThumb);
    }
    QWidget::leaveEvent(event);
}

// Keyboard and focus. zh_CN: 键盘与焦点。

void RangeSlider::keyPressEvent(QKeyEvent* event)
{
    // Same key mapping as QAbstractSlider, applied to the active thumb.
    // zh_CN: 与 QAbstractSlider 相同的按键映射，作用于活动手柄。
    const bool rtl = isRightToLeft();
    qint64 delta = 0;
    switch (event->key()) {
    case Qt::Key_Left:
        delta = rtl ? m_singleStep : -static_cast<qint64>(m_singleStep);
        break;
    case Qt::Key_Right:
        delta = rtl ? -static_cast<qint64>(m_singleStep) : m_singleStep;
        break;
    case Qt::Key_Up:
        delta = m_singleStep;
        break;
    case Qt::Key_Down:
        delta = -static_cast<qint64>(m_singleStep);
        break;
    case Qt::Key_PageUp:
        delta = m_pageStep;
        break;
    case Qt::Key_PageDown:
        delta = -static_cast<qint64>(m_pageStep);
        break;
    case Qt::Key_Home:
        delta = std::numeric_limits<qint64>::min() / 2;
        break;
    case Qt::Key_End:
        delta = std::numeric_limits<qint64>::max() / 2;
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
    m_keyboardFocusVisible = true;
    if (!m_dragging)
        stepThumb(m_activeThumb, delta);
    update();
}

void RangeSlider::focusInEvent(QFocusEvent* event)
{
    // setActiveThumb() announces focus itself when it switches thumbs.
    // zh_CN: setActiveThumb() 切换手柄时会自行发布焦点事件。
    const Thumb previous = m_activeThumb;
    switch (event->reason()) {
    case Qt::TabFocusReason:
        m_keyboardFocusVisible = true;
        setActiveThumb(Thumb::Lower);
        break;
    case Qt::BacktabFocusReason:
        m_keyboardFocusVisible = true;
        setActiveThumb(Thumb::Upper);
        break;
    case Qt::ShortcutFocusReason:
        m_keyboardFocusVisible = true;
        break;
    case Qt::MouseFocusReason:
        m_keyboardFocusVisible = false;
        break;
    default:
        break;
    }
    QWidget::focusInEvent(event);
    if (m_activeThumb == previous)
        detail::notifyRangeSliderFocusChanged(this);
    update();
}

void RangeSlider::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

bool RangeSlider::focusNextPrevChild(bool next)
{
    // Each thumb is a tab stop: Tab moves from the lower to the upper thumb
    // before leaving the control, and Shift+Tab moves back.
    // zh_CN: 每个手柄都是一个 Tab 停靠点：Tab 先从下限移到上限再离开控件，Shift+Tab 反向。
    if (hasFocus() && isEnabled() && !m_dragging) {
        const Thumb target = next ? Thumb::Upper : Thumb::Lower;
        if (m_activeThumb != target) {
            m_keyboardFocusVisible = true;
            setActiveThumb(target);
            return true;
        }
    }
    return QWidget::focusNextPrevChild(next);
}

void RangeSlider::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        finishDrag();
        setHoveredThumb(false, m_hoveredThumb);
    }
    if (event->type() == QEvent::LayoutDirectionChange)
        update();
    QWidget::changeEvent(event);
}

void RangeSlider::hideEvent(QHideEvent* event)
{
    finishDrag();
    QWidget::hideEvent(event);
}

// Value tip. zh_CN: 数值提示。

void RangeSlider::showToolTip()
{
    if (!m_toolTip)
        m_toolTip = new fluent::status_info::ToolTip(nullptr); // Top-level window. zh_CN: 顶层窗口。
    m_toolTip->setText(QString::number(position(m_activeThumb)));
    m_toolTip->show();
    m_toolTip->raise();
    updateToolTip();
}

void RangeSlider::hideToolTip()
{
    if (m_toolTip)
        m_toolTip->hide();
}

void RangeSlider::updateToolTip()
{
    if (!m_toolTip || !m_toolTip->isVisible())
        return;
    m_toolTip->setText(QString::number(position(m_activeThumb)));

    const QPoint handle = mapToGlobal(thumbCenter(m_activeThumb));
    const int spacing = themeSpacing().medium;
    const QSize tip = m_toolTip->size();
    // Above the thumb when horizontal, beside it when vertical, as in Slider.
    // zh_CN: 与 Slider 一致：水平时位于手柄上方，竖向时位于手柄侧边。
    const QPoint topLeft = m_orientation == Qt::Horizontal
        ? QPoint(handle.x() - tip.width() / 2, handle.y() - tip.height() - spacing - m_handleSize / 2)
        : QPoint(handle.x() + m_handleSize / 2 + spacing, handle.y() - tip.height() / 2);
    m_toolTip->move(topLeft);
}

} // namespace fluent::basicinput
