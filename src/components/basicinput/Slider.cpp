#include "compatibility/InputEventCompat.h"
#include "Slider.h"
#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "components/basicinput/private/SliderPainter_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include <QMouseEvent>
#include <QFocusEvent>
#include <QKeyEvent>

#include <QPainter>
#include <QPropertyAnimation>
#include <QStyle>
#include <QStyleOptionSlider>
#include <algorithm>

#include "design/Typography.h"

namespace fluent::basicinput {

using namespace fluent::textfields;

Slider::Slider(Qt::Orientation orientation, QWidget* parent) : QSlider(orientation, parent)
{
    setAttribute(Qt::WA_Hover);
    ::compatibility::detail::disableNativeFocusRing(this);

    const auto& anim = themeAnimation();

    m_hoverAnim = new QPropertyAnimation(this, "hoverRatio", this);
    m_hoverAnim->setDuration(anim.normal);
    m_hoverAnim->setEasingCurve(anim.decelerate);

    m_pressAnim = new QPropertyAnimation(this, "pressRatio", this);
    m_pressAnim->setDuration(anim.normal);
    m_pressAnim->setEasingCurve(anim.decelerate);
}

Slider::Slider(QWidget* parent) : Slider(Qt::Horizontal, parent) {}

Slider::~Slider()
{
    if (m_toolTip) {
        delete m_toolTip;
    }
}

QSize Slider::sizeHint() const
{
    int length = m_defaultLength;
    int thickness = std::max(m_handleSize, m_trackHeight) + 2 * m_visualMargin;

    if (orientation() == Qt::Horizontal) {
        return QSize(length, thickness);
    } else {
        return QSize(thickness, length);
    }
}

QSize Slider::minimumSizeHint() const
{
    const int thickness = std::max(m_handleSize, m_trackHeight) + 2 * m_visualMargin;
    const int padding = m_handleSize / 2 + m_visualMargin;
    // Reserve endpoint margins plus at least one handle width of travel.
    const int length = 2 * padding + qMax(1, m_handleSize);
    const QSize minimum = orientation() == Qt::Horizontal ? QSize(length, thickness) : QSize(thickness, length);
    return QSlider::minimumSizeHint().expandedTo(minimum);
}

void Slider::focusInEvent(QFocusEvent* event)
{
    if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason
        || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    else if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    QSlider::focusInEvent(event);
    update();
}

void Slider::focusOutEvent(QFocusEvent* event)
{
    QSlider::focusOutEvent(event);
    update();
}

void Slider::keyPressEvent(QKeyEvent* event)
{
    m_keyboardFocusVisible = true;
    QSlider::keyPressEvent(event);
    update();
}

void Slider::paintEvent(QPaintEvent*)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    if (opt.orientation == Qt::Horizontal) {
        drawHorizontal(p, opt);
    } else {
        drawVertical(p, opt);
    }
}

void Slider::enterEvent(FluentEnterEvent* event)
{
    m_hoverAnim->stop();
    m_hoverAnim->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_hoverAnim, themeAnimation().normal);
    QSlider::enterEvent(event);
}

void Slider::leaveEvent(QEvent* event)
{
    if (!m_isPressed) {
        m_hoverAnim->stop();
        m_hoverAnim->setEndValue(0.0);
        ::fluent::detail::startMotionTransition(m_hoverAnim, themeAnimation().normal);
    }
    QSlider::leaveEvent(event);
}

void Slider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    event->accept();
    m_keyboardFocusVisible = false;
    m_isPressed = true;
    setSliderDown(true);

    m_pressAnim->stop();
    m_pressAnim->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_pressAnim, themeAnimation().normal);

    showToolTip();

    int val = pixelPosToRangeValue(
        orientation() == Qt::Horizontal ? fluentMousePos(event).x() : fluentMousePos(event).y());
    setSliderPosition(val);
    updateToolTipPos();

    triggerAction(SliderMove);
    update();
}

void Slider::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isPressed) {
        event->ignore();
        return;
    }
    event->accept();
    int val = pixelPosToRangeValue(
        orientation() == Qt::Horizontal ? fluentMousePos(event).x() : fluentMousePos(event).y());
    setSliderPosition(val);
    updateToolTipPos();

    triggerAction(SliderMove);
    update();
}

void Slider::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    event->accept();
    m_isPressed = false;
    // QAbstractSlider emits sliderReleased() when sliderDown transitions to false.
    setSliderDown(false);

    m_pressAnim->stop();
    m_pressAnim->setEndValue(0.0);
    ::fluent::detail::startMotionTransition(m_pressAnim, themeAnimation().normal);

    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        m_hoverAnim->stop();
        m_hoverAnim->setEndValue(0.0);
        ::fluent::detail::startMotionTransition(m_hoverAnim, themeAnimation().normal);
    }

    hideToolTip();

    triggerAction(SliderMove);
    update();
}

void Slider::showToolTip()
{
    if (!m_toolTip) {
        m_toolTip = new fluent::status_info::ToolTip(nullptr);
    }
    m_toolTip->setText(QString::number(value()));
    updateToolTipPos();
    m_toolTip->show();
    m_toolTip->raise();
}

void Slider::hideToolTip()
{
    if (m_toolTip) {
        m_toolTip->hide();
    }
}

void Slider::updateToolTipPos()
{
    if (!m_toolTip || !m_toolTip->isVisible())
        return;

    m_toolTip->setText(QString::number(value()));

    QPoint handlePos;
    int pos = valueToPixelPos(value());
    const int cy = height() / 2;
    const int cx = width() / 2;

    if (orientation() == Qt::Horizontal) {
        handlePos = QPoint(pos, cy);
    } else {
        handlePos = QPoint(cx, pos);
    }

    QPoint globalHandle = mapToGlobal(handlePos);

    int tipW = m_toolTip->width();
    int tipH = m_toolTip->height();
    int spacing = themeSpacing().medium;

    QPoint tipPos;
    if (orientation() == Qt::Horizontal) {
        tipPos = QPoint(globalHandle.x() - tipW / 2, globalHandle.y() - tipH - spacing - m_handleSize / 2);
    } else {
        tipPos = QPoint(globalHandle.x() + m_handleSize / 2 + spacing, globalHandle.y() - tipH / 2);
    }

    m_toolTip->move(tipPos);
}

int Slider::valueToPixelPos(int val) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const int padding = m_handleSize / 2 + m_visualMargin;
    const int length = orientation() == Qt::Horizontal ? width() : height();
    const int available = qMax(0, length - 2 * padding);
    const qint64 range = static_cast<qint64>(maximum()) - minimum();
    if (range == 0)
        return padding + (opt.upsideDown ? available : 0);

    // Widen to 64-bit before arithmetic to handle full int ranges and tracks wider than 4096 px.
    const int boundedValue = qBound(minimum(), val, maximum());
    const qint64 offset =
        opt.upsideDown ? static_cast<qint64>(maximum()) - boundedValue : static_cast<qint64>(boundedValue) - minimum();
    return padding + static_cast<int>((offset * available + range / 2) / range);
}

int Slider::pixelPosToRangeValue(int pos) const
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const int padding = m_handleSize / 2 + m_visualMargin;
    const int length = orientation() == Qt::Horizontal ? width() : height();
    const int available = qMax(0, length - 2 * padding);
    if (available <= 0)
        return minimum();
    return QStyle::sliderValueFromPosition(minimum(), maximum(), pos - padding, available, opt.upsideDown);
}

void Slider::drawHorizontal(QPainter& p, const QStyleOptionSlider& opt)
{
    const auto& colors = themeColorsRef();

    const int cy = height() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;

    const int trackThickness = m_trackHeight;

    QRect trackRect(padding, cy - trackThickness / 2, width() - 2 * padding, trackThickness);

    int handleX = valueToPixelPos(opt.sliderPosition);
    QPointF center(handleX, cy);

    const int minimumX = valueToPixelPos(opt.minimum);
    QRectF filledRect(qMin(minimumX, handleX), cy - trackThickness / 2, qAbs(handleX - minimumX), trackThickness);

    detail::paintSliderBar(p, QRectF(trackRect), detail::sliderTrackColor(colors, isEnabled()));
    detail::paintSliderBar(p, filledRect, detail::sliderFillColor(colors, isEnabled()));

    if (opt.tickPosition != QSlider::NoTicks && m_hoverRatio > 0.1) {
        auto pixels = detail::sliderTickValues(opt.minimum, opt.maximum, opt.tickInterval, opt.singleStep,
            opt.pageStep);
        for (int& pixel : pixels)
            pixel = valueToPixelPos(pixel);
        detail::paintSliderTicks(p, trackRect, opt.orientation, opt.tickPosition, pixels,
            detail::sliderTickColor(colors));
    }

    detail::SliderThumbVisual thumb;
    thumb.center = center;
    thumb.diameter = m_handleSize;
    thumb.hoverRatio = m_hoverRatio;
    thumb.pressRatio = m_pressRatio;
    thumb.keyboardFocused = hasFocus() && m_keyboardFocusVisible;
    thumb.enabled = isEnabled();
    detail::paintSliderThumb(p, colors, thumb);
}

void Slider::drawVertical(QPainter& p, const QStyleOptionSlider& opt)
{
    const auto& colors = themeColorsRef();

    const int cx = width() / 2;
    const int padding = m_handleSize / 2 + m_visualMargin;

    const int trackThickness = m_trackHeight;

    QRect trackRect(cx - trackThickness / 2, padding, trackThickness, height() - 2 * padding);

    int handleY = valueToPixelPos(opt.sliderPosition);
    QPointF center(cx, handleY);

    const int minimumY = valueToPixelPos(opt.minimum);
    QRectF filledRect(cx - trackThickness / 2, qMin(minimumY, handleY), trackThickness, qAbs(handleY - minimumY));

    detail::paintSliderBar(p, QRectF(trackRect), detail::sliderTrackColor(colors, isEnabled()));
    detail::paintSliderBar(p, filledRect, detail::sliderFillColor(colors, isEnabled()));

    if (opt.tickPosition != QSlider::NoTicks && m_hoverRatio > 0.1) {
        auto pixels = detail::sliderTickValues(opt.minimum, opt.maximum, opt.tickInterval, opt.singleStep,
            opt.pageStep);
        for (int& pixel : pixels)
            pixel = valueToPixelPos(pixel);
        detail::paintSliderTicks(p, trackRect, opt.orientation, opt.tickPosition, pixels,
            detail::sliderTickColor(colors));
    }

    detail::SliderThumbVisual thumb;
    thumb.center = center;
    thumb.diameter = m_handleSize;
    thumb.hoverRatio = m_hoverRatio;
    thumb.pressRatio = m_pressRatio;
    thumb.keyboardFocused = hasFocus() && m_keyboardFocusVisible;
    thumb.enabled = isEnabled();
    detail::paintSliderThumb(p, colors, thumb);
}
} // namespace fluent::basicinput
