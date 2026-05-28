#include "ScrollView.h"

#include "ScrollBar.h"

#include <algorithm>
#include <cmath>
#include <QEvent>
#include <QGestureEvent>
#include <QPalette>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>

#include "compatibility/QtCompat.h"

namespace view::scrolling {

namespace {

constexpr qint64 NativeZoomPinchSuppressionMs = 350;

class TransparentCornerWidget : public QWidget {
public:
    explicit TransparentCornerWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setObjectName(QStringLiteral("FluentScrollViewTransparentCorner"));
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {}
};

} // namespace

ScrollView::ScrollView(QWidget* parent)
    : QScrollArea(parent) {
    init();
}

ScrollView::~ScrollView() = default;

void ScrollView::init() {
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(false);
    setAutoFillBackground(false);
    setHorizontalScrollBar(new ScrollBar(Qt::Horizontal, this));
    setVerticalScrollBar(new ScrollBar(Qt::Vertical, this));
    m_cornerWidget = new TransparentCornerWidget(this);
    setCornerWidget(m_cornerWidget);
#ifndef Q_OS_MACOS
    grabGesture(Qt::PinchGesture);
#endif
    installEventFilter(this);
#ifndef Q_OS_MACOS
    viewport()->grabGesture(Qt::PinchGesture);
#endif
    viewport()->installEventFilter(this);

    m_horizontalAnimation = new QPropertyAnimation(horizontalScrollBar(), "value", this);
    m_verticalAnimation = new QPropertyAnimation(verticalScrollBar(), "value", this);
    m_zoomAnimation = new QPropertyAnimation(this, "zoomFactor", this);
    connect(m_zoomAnimation, &QPropertyAnimation::finished, this, [this]() {
        m_zoomAnimationAnchor = QPointF(-1.0, -1.0);
    });

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        emit scrollPositionChanged(horizontalOffset(), verticalOffset());
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        emit scrollPositionChanged(horizontalOffset(), verticalOffset());
    });

    applyScrollPolicies();
    updateViewportPalette();
    updateCornerWidget();
}

void ScrollView::setWidget(QWidget* content) {
    stopZoomAnimation();
    if (m_contentWidget)
        m_contentWidget->removeEventFilter(this);
    QScrollArea::setWidget(content);
    m_contentWidget = content;
    m_zoomAwareContent = dynamic_cast<ScrollViewZoomAware*>(content);
    if (content) {
        content->installEventFilter(this);
#ifndef Q_OS_MACOS
        content->grabGesture(Qt::PinchGesture);
#endif
        content->setAttribute(Qt::WA_AcceptTouchEvents);
    }
    captureContentBaseSize();
    applyZoomToContent();
}

void ScrollView::setHorizontalScrollMode(ScrollMode mode) {
    if (m_horizontalScrollMode == mode)
        return;
    m_horizontalScrollMode = mode;
    applyAxisPolicy(Axis::Horizontal);
    emit horizontalScrollModeChanged();
}

void ScrollView::setVerticalScrollMode(ScrollMode mode) {
    if (m_verticalScrollMode == mode)
        return;
    m_verticalScrollMode = mode;
    applyAxisPolicy(Axis::Vertical);
    emit verticalScrollModeChanged();
}

void ScrollView::setHorizontalScrollBarVisibility(ScrollBarVisibility visibility) {
    if (m_horizontalScrollBarVisibility == visibility)
        return;
    m_horizontalScrollBarVisibility = visibility;
    applyAxisPolicy(Axis::Horizontal);
    emit horizontalScrollBarVisibilityChanged();
}

void ScrollView::setVerticalScrollBarVisibility(ScrollBarVisibility visibility) {
    if (m_verticalScrollBarVisibility == visibility)
        return;
    m_verticalScrollBarVisibility = visibility;
    applyAxisPolicy(Axis::Vertical);
    emit verticalScrollBarVisibilityChanged();
}

void ScrollView::setZoomMode(ZoomMode mode) {
    if (m_zoomMode == mode)
        return;
    m_zoomMode = mode;
    emit zoomModeChanged();
}

void ScrollView::setZoomFactor(qreal factor) {
    setZoomFactorAt(factor, effectiveZoomAnchor(m_zoomAnimationAnchor));
}

void ScrollView::setMinZoomFactor(qreal factor) {
    factor = std::max(0.01, factor);
    if (qFuzzyCompare(m_minZoomFactor, factor))
        return;

    m_minZoomFactor = factor;
    if (m_maxZoomFactor < m_minZoomFactor) {
        m_maxZoomFactor = m_minZoomFactor;
        emit maxZoomFactorChanged();
    }
    emit minZoomFactorChanged();
    setZoomFactor(m_zoomFactor);
}

void ScrollView::setMaxZoomFactor(qreal factor) {
    factor = std::max(0.01, factor);
    if (qFuzzyCompare(m_maxZoomFactor, factor))
        return;

    m_maxZoomFactor = factor;
    if (m_minZoomFactor > m_maxZoomFactor) {
        m_minZoomFactor = m_maxZoomFactor;
        emit minZoomFactorChanged();
    }
    emit maxZoomFactorChanged();
    setZoomFactor(m_zoomFactor);
}

int ScrollView::horizontalOffset() const {
    return horizontalScrollBar()->value();
}

int ScrollView::verticalOffset() const {
    return verticalScrollBar()->value();
}

int ScrollView::scrollableWidth() const {
    return horizontalScrollBar()->maximum();
}

int ScrollView::scrollableHeight() const {
    return verticalScrollBar()->maximum();
}

void ScrollView::scrollTo(int x, int y, bool animated) {
    QScrollBar* horizontal = horizontalScrollBar();
    QScrollBar* vertical = verticalScrollBar();

    const int targetX = isAxisEnabled(Axis::Horizontal) ? clampedTarget(horizontal, x) : horizontal->value();
    const int targetY = isAxisEnabled(Axis::Vertical) ? clampedTarget(vertical, y) : vertical->value();

    if (animated) {
        animateScrollBar(horizontal, m_horizontalAnimation, targetX);
        animateScrollBar(vertical, m_verticalAnimation, targetY);
        return;
    }

    stopAnimations();
    horizontal->setValue(targetX);
    vertical->setValue(targetY);
}

void ScrollView::scrollBy(int dx, int dy, bool animated) {
    scrollTo(horizontalOffset() + dx, verticalOffset() + dy, animated);
}

void ScrollView::zoomTo(qreal factor, bool animated) {
    const QPointF anchor = defaultZoomAnchor();
    if (!animated) {
        stopZoomAnimation();
        setZoomFactorAt(factor, anchor);
        return;
    }

    const qreal target = clampedZoomFactor(factor);
    if (qFuzzyCompare(m_zoomFactor, target))
        return;

    const auto anim = themeAnimation();
    m_zoomAnimationAnchor = anchor;
    m_zoomAnimation->stop();
    m_zoomAnimation->setDuration(anim.normal);
    m_zoomAnimation->setEasingCurve(anim.decelerate);
    m_zoomAnimation->setStartValue(m_zoomFactor);
    m_zoomAnimation->setEndValue(target);
    m_zoomAnimation->start();
}

void ScrollView::zoomBy(qreal factorMultiplier, bool animated) {
    if (factorMultiplier <= 0.0)
        return;
    zoomTo(m_zoomFactor * factorMultiplier, animated);
}

void ScrollView::resetZoom(bool animated) {
    zoomTo(1.0, animated);
}

void ScrollView::onThemeUpdated() {
    updateViewportPalette();
    updateCornerWidget();
    update();
    viewport()->update();
    if (m_cornerWidget)
        m_cornerWidget->update();
}

bool ScrollView::event(QEvent* event) {
    if (event->type() == QEvent::NativeGesture && handleNativeGesture(event, this))
        return true;

    if (event->type() == QEvent::Gesture && handlePinchGesture(event, this))
        return true;

    return QScrollArea::event(event);
}

bool ScrollView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == this || watched == viewport() || (m_contentWidget && watched == m_contentWidget.data())) {
        if (event->type() == QEvent::Wheel) {
            auto* wheel = static_cast<QWheelEvent*>(event);
            if (handleZoomWheel(wheel, watched))
                return true;
            if (shouldSuppressScrollWheel()) {
                wheel->accept();
                return true;
            }
        }

        if (event->type() == QEvent::NativeGesture && handleNativeGesture(event, watched))
            return true;

        if (event->type() == QEvent::Gesture && handlePinchGesture(event, watched))
            return true;
    }

    return QScrollArea::eventFilter(watched, event);
}

bool ScrollView::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (handleZoomWheel(wheel, viewport()))
            return true;
        if (shouldSuppressScrollWheel()) {
            wheel->accept();
            return true;
        }
    }

    if (event->type() == QEvent::NativeGesture && handleNativeGesture(event, viewport()))
        return true;

    if (event->type() == QEvent::Gesture && handlePinchGesture(event, viewport()))
        return true;

    return QScrollArea::viewportEvent(event);
}

void ScrollView::wheelEvent(QWheelEvent* event) {
    if (handleZoomWheel(event, this))
        return;

    if (shouldSuppressScrollWheel()) {
        event->accept();
        return;
    }

    QScrollArea::wheelEvent(event);
}

void ScrollView::applyScrollPolicies() {
    applyAxisPolicy(Axis::Horizontal);
    applyAxisPolicy(Axis::Vertical);
}

void ScrollView::applyAxisPolicy(Axis axis) {
    const ScrollBarVisibility visibility = visibilityForAxis(axis);
    const bool enabled = isAxisEnabled(axis);

    Qt::ScrollBarPolicy policy = Qt::ScrollBarAsNeeded;
    if (!enabled || visibility == ScrollBarVisibility::Disabled || visibility == ScrollBarVisibility::Hidden) {
        policy = Qt::ScrollBarAlwaysOff;
    } else if (visibility == ScrollBarVisibility::Visible) {
        policy = Qt::ScrollBarAlwaysOn;
    }

    QScrollBar* scrollBar = axis == Axis::Horizontal ? horizontalScrollBar() : verticalScrollBar();
    scrollBar->setEnabled(enabled);
    if (!enabled)
        scrollBar->setValue(scrollBar->minimum());

    if (axis == Axis::Horizontal) {
        setHorizontalScrollBarPolicy(policy);
    } else {
        setVerticalScrollBarPolicy(policy);
    }
}

void ScrollView::updateViewportPalette() {
    QWidget* area = viewport();
    if (!area)
        return;

    const auto colors = themeColors();
    QPalette pal = area->palette();
    pal.setColor(QPalette::Window, colors.bgCanvas);
    pal.setColor(QPalette::Base, colors.bgCanvas);
    area->setPalette(pal);
    area->setAutoFillBackground(true);
}

void ScrollView::updateCornerWidget() {
    if (!m_cornerWidget)
        return;

    m_cornerWidget->setAutoFillBackground(false);
    m_cornerWidget->setAttribute(Qt::WA_NoSystemBackground);
    m_cornerWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_cornerWidget->update();
}

void ScrollView::stopAnimations() {
    if (m_horizontalAnimation)
        m_horizontalAnimation->stop();
    if (m_verticalAnimation)
        m_verticalAnimation->stop();
}

void ScrollView::stopZoomAnimation() {
    if (m_zoomAnimation)
        m_zoomAnimation->stop();
    m_zoomAnimationAnchor = QPointF(-1.0, -1.0);
}

void ScrollView::animateScrollBar(QScrollBar* scrollBar, QPropertyAnimation* animation, int targetValue) {
    if (!scrollBar || !animation)
        return;

    if (scrollBar->value() == targetValue) {
        animation->stop();
        return;
    }

    const auto anim = themeAnimation();
    animation->stop();
    animation->setTargetObject(scrollBar);
    animation->setPropertyName("value");
    animation->setDuration(anim.normal);
    animation->setEasingCurve(anim.decelerate);
    animation->setStartValue(scrollBar->value());
    animation->setEndValue(targetValue);
    animation->start();
}

int ScrollView::clampedTarget(QScrollBar* scrollBar, int value) const {
    if (!scrollBar)
        return 0;
    return std::clamp(value, scrollBar->minimum(), scrollBar->maximum());
}

bool ScrollView::isAxisEnabled(Axis axis) const {
    return modeForAxis(axis) != ScrollMode::Disabled &&
           visibilityForAxis(axis) != ScrollBarVisibility::Disabled;
}

ScrollView::ScrollMode ScrollView::modeForAxis(Axis axis) const {
    return axis == Axis::Horizontal ? m_horizontalScrollMode : m_verticalScrollMode;
}

ScrollView::ScrollBarVisibility ScrollView::visibilityForAxis(Axis axis) const {
    return axis == Axis::Horizontal ? m_horizontalScrollBarVisibility : m_verticalScrollBarVisibility;
}

void ScrollView::captureContentBaseSize() {
    if (!m_contentWidget) {
        m_unscaledContentSize = QSizeF();
        return;
    }

    if (m_zoomAwareContent) {
        const QSizeF size = m_zoomAwareContent->scrollViewUnscaledSize();
        if (size.isValid() && !size.isEmpty()) {
            m_unscaledContentSize = QSizeF(std::max(1.0, size.width()),
                                           std::max(1.0, size.height()));
            return;
        }
    }

    QSize size = m_contentWidget->size();
    if (size.isEmpty())
        size = m_contentWidget->sizeHint();
    if (size.isEmpty())
        size = m_contentWidget->minimumSizeHint();

    m_unscaledContentSize = QSizeF(std::max(1, size.width()),
                                   std::max(1, size.height()));
}

void ScrollView::applyZoomToContent() {
    if (!m_contentWidget || m_unscaledContentSize.isEmpty())
        return;

    const QSize scaledSize(std::max(1, qRound(m_unscaledContentSize.width() * m_zoomFactor)),
                           std::max(1, qRound(m_unscaledContentSize.height() * m_zoomFactor)));
    if (m_zoomAwareContent)
        m_zoomAwareContent->setScrollViewZoomFactor(m_zoomFactor);
    m_contentWidget->setFixedSize(scaledSize);
    m_contentWidget->update();
}

void ScrollView::setZoomFactorAt(qreal factor, const QPointF& viewportAnchor) {
    const qreal target = clampedZoomFactor(factor);
    if (qFuzzyCompare(m_zoomFactor, target))
        return;

    const qreal previous = std::max(0.01, m_zoomFactor);
    const QPointF anchor = effectiveZoomAnchor(viewportAnchor);
    const QPointF contentAnchor((horizontalOffset() + anchor.x()) / previous,
                                (verticalOffset() + anchor.y()) / previous);

    m_zoomFactor = target;
    applyZoomToContent();

    horizontalScrollBar()->setValue(clampedTarget(horizontalScrollBar(),
                                                 qRound(contentAnchor.x() * target - anchor.x())));
    verticalScrollBar()->setValue(clampedTarget(verticalScrollBar(),
                                               qRound(contentAnchor.y() * target - anchor.y())));
    emit zoomFactorChanged();
}

QPointF ScrollView::defaultZoomAnchor() const {
    const QRect area = viewport() ? viewport()->rect() : rect();
    return QPointF(area.width() / 2.0, area.height() / 2.0);
}

QPointF ScrollView::effectiveZoomAnchor(const QPointF& viewportAnchor) const {
    if (viewportAnchor.x() >= 0.0 && viewportAnchor.y() >= 0.0)
        return viewportAnchor;
    return defaultZoomAnchor();
}

qreal ScrollView::clampedZoomFactor(qreal factor) const {
    if (!std::isfinite(factor))
        return m_zoomFactor;
    return std::clamp(factor, m_minZoomFactor, m_maxZoomFactor);
}

bool ScrollView::handleZoomWheel(QWheelEvent* event, QObject* source) {
    if (!event || m_zoomMode != ZoomMode::Enabled)
        return false;
    if (!(event->modifiers() & Qt::ControlModifier))
        return false;

    qreal wheelSteps = 0.0;
    if (!event->pixelDelta().isNull()) {
        wheelSteps = event->pixelDelta().y() / 120.0;
    } else if (!event->angleDelta().isNull()) {
        wheelSteps = event->angleDelta().y() / 120.0;
    }
    if (qFuzzyIsNull(wheelSteps))
        return false;

    const QPointF anchor = mapEventPositionToViewport(source, fluentWheelPosition(event));
    stopZoomAnimation();
    setZoomFactorAt(m_zoomFactor * std::pow(1.1, wheelSteps), anchor);
    event->accept();
    return true;
}

bool ScrollView::shouldSuppressScrollWheel() const {
    if (m_zoomMode != ZoomMode::Enabled)
        return false;
    if (m_nativeZoomGestureActive && m_nativeZoomGestureHasZoomed)
        return true;
    return m_lastNativeZoomGestureTime.isValid() &&
           m_lastNativeZoomGestureTime.elapsed() < NativeZoomPinchSuppressionMs;
}

bool ScrollView::handleNativeGesture(QEvent* event, QObject* source) {
    if (!event || m_zoomMode != ZoomMode::Enabled)
        return false;

    auto* gesture = static_cast<FluentNativeGestureEvent*>(event);
    switch (gesture->gestureType()) {
    case Qt::BeginNativeGesture:
        stopZoomAnimation();
        m_nativeZoomGestureActive = true;
        m_nativeZoomGestureHasZoomed = false;
        gesture->accept();
        return true;
    case Qt::ZoomNativeGesture: {
        const QPointF anchor = mapEventPositionToViewport(source, fluentNativeGesturePosition(gesture));
        m_lastNativeZoomGestureTime.restart();
        m_nativeZoomGestureActive = true;
        m_nativeZoomGestureHasZoomed = true;
        const qreal scale = std::clamp(1.0 + gesture->value(), 0.25, 4.0);
        setZoomFactorAt(m_zoomFactor * scale, anchor);
        gesture->accept();
        return true;
    }
    case Qt::SmartZoomNativeGesture:
        if (!shouldSuppressScrollWheel()) {
            const qreal target = gesture->value() > 0.0 ? std::min(2.0, m_maxZoomFactor) : 1.0;
            zoomTo(target, true);
        }
        gesture->accept();
        return true;
    case Qt::EndNativeGesture:
        if (m_nativeZoomGestureHasZoomed)
            m_lastNativeZoomGestureTime.restart();
        m_nativeZoomGestureActive = false;
        m_nativeZoomGestureHasZoomed = false;
        gesture->accept();
        return true;
    default:
        return false;
    }
}

bool ScrollView::handlePinchGesture(QEvent* event, QObject* source) {
    if (!event || m_zoomMode != ZoomMode::Enabled)
        return false;

    auto* gestureEvent = static_cast<QGestureEvent*>(event);
    auto* pinch = static_cast<QPinchGesture*>(gestureEvent->gesture(Qt::PinchGesture));
    if (!pinch)
        return false;

#ifdef Q_OS_MACOS
    gestureEvent->accept(pinch);
    return true;
#endif

    if (shouldSuppressScrollWheel()) {
        gestureEvent->accept(pinch);
        return true;
    }

    if (pinch->state() == Qt::GestureStarted)
        stopZoomAnimation();

    if (pinch->state() == Qt::GestureFinished || pinch->state() == Qt::GestureCanceled) {
        gestureEvent->accept(pinch);
        return true;
    }

    if (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) {
        qreal scale = pinch->scaleFactor();
        if (pinch->lastScaleFactor() > 0.0)
            scale /= pinch->lastScaleFactor();
        setZoomFactorAt(m_zoomFactor * scale, mapEventPositionToViewport(source, pinch->centerPoint()));
    }
    gestureEvent->accept(pinch);
    return true;
}

QPointF ScrollView::mapEventPositionToViewport(QObject* source, const QPointF& position) const {
    QWidget* area = viewport();
    if (!area)
        return position;

    auto* sourceWidget = qobject_cast<QWidget*>(source);
    if (!sourceWidget || sourceWidget == area)
        return position;

    return QPointF(area->mapFromGlobal(sourceWidget->mapToGlobal(position.toPoint())));
}

} // namespace view::scrolling
