#include "DrawerView.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QGraphicsEffect>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "design/Spacing.h"
#include "view/foundation/overlay/OverlayGeometry.h"
#include "view/foundation/overlay/OverlayScrim.h"
#include "view/foundation/overlay/OverlayWindow.h"

namespace view::collections {

namespace {
constexpr qreal kPositionEpsilon = 0.0001;
constexpr int kDefaultCrossAxis = 480;
constexpr int kMinimumDrawerLength = 1;

// Forces the widget to render into an offscreen QPixmap (with full alpha channel)
// before compositing to the parent.  Unlike QGraphicsOpacityEffect at opacity=1.0
// (which Qt short-circuits to drawSource(), skipping the offscreen step), this
// effect always calls sourcePixmap() so that CompositionMode_Source + transparent
// fill in paintEvent correctly produces alpha=0 corner pixels on macOS child widgets.
class DrawerAlphaEffect : public QGraphicsEffect {
public:
    explicit DrawerAlphaEffect(QObject* parent = nullptr) : QGraphicsEffect(parent) {}
    QRectF boundingRectFor(const QRectF& rect) const override { return rect; }
protected:
    void draw(QPainter* painter) override {
        QPoint offset;
        const QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &offset);
        if (!pixmap.isNull())
            painter->drawPixmap(offset, pixmap);
    }
};
} // namespace

DrawerView::DrawerView(QWidget* parent)
    : QWidget(parent),
      m_originalParent(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);

    // DrawerAlphaEffect forces offscreen QPixmap rendering so that
    // WA_TranslucentBackground + CompositionMode_Source in paintEvent
    // produces real alpha=0 at rounded corners, even on macOS child widgets.
    setGraphicsEffect(new DrawerAlphaEffect(this));
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("DrawerView"));
    resize(sizeHint());
    hide();

    m_positionAnimation = new QPropertyAnimation(this, "position", this);
    connect(m_positionAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_transitionTarget == TransitionTarget::Open)
            finalizeOpened();
        else if (m_transitionTarget == TransitionTarget::Closed)
            finalizeClosed();
        m_transitionTarget = TransitionTarget::None;
    });

    ensureApplicationEventFilter();
    installTopLevelEventFilter(resolveTopLevelWidget());
    onThemeUpdated();
}

DrawerView::~DrawerView()
{
    removeApplicationEventFilter();
    removeTopLevelEventFilter();
    stopAnimation();
    destroyScrim();
    if (m_contentWidget)
        m_contentWidget->setParent(nullptr);
}

void DrawerView::onThemeUpdated()
{
    update();
    if (auto* scrimElement = dynamic_cast<FluentElement*>(m_scrim.data()))
        scrimElement->onThemeUpdated();
}

void DrawerView::setIsOpen(bool open)
{
    if (open)
        this->open();
    else
        this->close();
}

void DrawerView::setEdge(DrawerEdge edge)
{
    if (m_edge == edge)
        return;

    m_edge = edge;
    updateOverlayGeometry();
    updateClipMask();
    updateGeometry();
    emit edgeChanged(m_edge);
}

void DrawerView::setPosition(qreal position)
{
    const qreal normalized = normalizedPosition(position);
    if (fuzzyEqual(m_position, normalized))
        return;

    m_position = normalized;
    updateOverlayGeometry();
    emit positionChanged(m_position);
    update();
}

void DrawerView::setDrawerLength(int length)
{
    const int normalized = std::max(kMinimumDrawerLength, length);
    if (m_drawerLength == normalized)
        return;

    m_drawerLength = normalized;
    updateOverlayGeometry();
    updateGeometry();
    emit drawerLengthChanged(m_drawerLength);
}

void DrawerView::setModal(bool modal)
{
    if (m_modal == modal)
        return;

    m_modal = modal;
    updateScrimState();
    emit modalChanged(m_modal);
}

void DrawerView::setDim(bool dim)
{
    if (m_dim == dim)
        return;

    m_dim = dim;
    updateScrimState();
    emit dimChanged(m_dim);
}

void DrawerView::setClosePolicy(ClosePolicy policy)
{
    if (m_closePolicy == policy)
        return;

    m_closePolicy = policy;
    emit closePolicyChanged(m_closePolicy);
}

void DrawerView::setInteractive(bool interactive)
{
    if (m_interactive == interactive)
        return;

    m_interactive = interactive;
    if (!m_interactive)
        cancelDrag();
    emit interactiveChanged(m_interactive);
}

void DrawerView::setDragMargin(int margin)
{
    const int normalized = std::max(0, margin);
    if (m_dragMargin == normalized)
        return;

    m_dragMargin = normalized;
    emit dragMarginChanged(m_dragMargin);
}

void DrawerView::setOuterCornerRadius(int radius)
{
    const int normalized = std::max(0, radius);
    if (m_outerCornerRadius == normalized)
        return;

    m_outerCornerRadius = normalized;
    updateClipMask();
    updateContentGeometry();
    update();
    emit outerCornerRadiusChanged(m_outerCornerRadius);
}

void DrawerView::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;

    m_animationEnabled = enabled;
    emit animationEnabledChanged(m_animationEnabled);
}

void DrawerView::setContentWidget(QWidget* widget)
{
    if (m_contentWidget == widget)
        return;

    if (m_contentWidget) {
        m_contentWidget->hide();
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (m_contentWidget) {
        m_contentWidget->setParent(this);
        m_contentWidget->setAutoFillBackground(false);
        m_contentWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_contentWidget->setAttribute(Qt::WA_TranslucentBackground, true);
        m_contentWidget->clearMask();
        m_contentWidget->setGeometry(m_contentGeometry);
        m_contentWidget->setVisible(isVisible());
    }

    updateContentGeometry();
    emit contentWidgetChanged(m_contentWidget.data());
}

QSize DrawerView::sizeHint() const
{
    const QSize contentHint = m_contentWidget ? m_contentWidget->sizeHint() : QSize();
    if (m_edge == DrawerEdge::Top || m_edge == DrawerEdge::Bottom)
        return QSize(std::max(kDefaultCrossAxis, contentHint.width()), std::max(m_drawerLength, contentHint.height()));
    return QSize(std::max(m_drawerLength, contentHint.width()), std::max(kDefaultCrossAxis, contentHint.height()));
}

QSize DrawerView::minimumSizeHint() const
{
    const QSize contentMinimum = m_contentWidget ? m_contentWidget->minimumSizeHint() : QSize();
    if (m_edge == DrawerEdge::Top || m_edge == DrawerEdge::Bottom)
        return QSize(std::max(120, contentMinimum.width()), std::max(48, std::min(m_drawerLength, 120)));
    return QSize(std::max(48, std::min(m_drawerLength, 120)), std::max(120, contentMinimum.height()));
}

void DrawerView::open()
{
    if (m_isOpen && !m_isClosing && fuzzyEqual(m_position, 1.0))
        return;

    stopAnimation();
    m_isClosing = false;
    beginVisibleTransition();

    if (!m_animationEnabled) {
        setPosition(1.0);
        finalizeOpened();
        return;
    }

    startPositionAnimation(1.0, TransitionTarget::Open);
}

void DrawerView::close()
{
    if (!isVisible() && !m_isOpen)
        return;
    if (m_isClosing)
        return;

    stopAnimation();
    m_isClosing = true;
    cancelDrag();
    emit aboutToHide();

    if (!m_animationEnabled) {
        setPosition(0.0);
        finalizeClosed();
        return;
    }

    startPositionAnimation(0.0, TransitionTarget::Closed);
}

bool DrawerView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Destroy) {
        if (watched == m_topLevel)
            m_topLevel = nullptr;
        return false;
    }

    if (event->type() == QEvent::Resize && watched == m_topLevel) {
        updateOverlayGeometry();
        return false;
    }

    if (!eventBelongsToDrawerTopLevel(watched))
        return false;

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (isVisible() && keyEvent->key() == Qt::Key_Escape && shouldCloseOnEscape()) {
            close();
            keyEvent->accept();
            return true;
        }
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton)
            return false;
        if (m_drag.active)
            return false;

        const QPoint globalPos = fluentMouseGlobalPos(mouseEvent);
        if (isVisible() && shouldCloseOnOutsidePress() && !isPointInsidePanel(globalPos)) {
            close();
            return false;
        }

        if (!isEnabled() || !m_interactive || m_positionAnimation->state() == QAbstractAnimation::Running)
            return false;

        if (isVisible() && isPointInsidePanel(globalPos)) {
            beginDrag(globalPos, false);
            return false;
        }

        if (!isVisible() && isPointInEdgeDragArea(globalPos)) {
            beginDrag(globalPos, true);
            return false;
        }
        return false;
    }
    case QEvent::MouseMove:
        if (m_drag.active)
            updateDrag(fluentMouseGlobalPos(static_cast<QMouseEvent*>(event)));
        return false;
    case QEvent::MouseButtonRelease:
        if (m_drag.active)
            endDrag();
        return false;
    default:
        return false;
    }
}

void DrawerView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect panelRect = rect();
    if (panelRect.isEmpty())
        return;

    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(panelRect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const auto colors = themeColors();
    const QPainterPath fillPath = outerRoundedPanelPath(QRectF(panelRect));
    const QRectF strokeRect = QRectF(panelRect).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath strokePath = outerRoundedPanelPath(strokeRect);

    painter.setBrush(colors.bgLayer);
    painter.setPen(Qt::NoPen);
    painter.drawPath(fillPath);

    painter.setPen(colors.strokeDefault);
    switch (m_edge) {
    case DrawerEdge::Left:
    case DrawerEdge::Right:
    case DrawerEdge::Top:
    case DrawerEdge::Bottom:
        painter.drawPath(strokePath);
        break;
    }
}

void DrawerView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateContentGeometry();
    updateClipMask();
}

void DrawerView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isEnabled() && m_interactive && m_positionAnimation->state() != QAbstractAnimation::Running) {
        beginDrag(fluentMouseGlobalPos(event), false);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DrawerView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_drag.active) {
        updateDrag(fluentMouseGlobalPos(event));
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DrawerView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_drag.active) {
        endDrag();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

QWidget* DrawerView::resolveTopLevelWidget() const
{
    return ::view::overlay::resolveOwningTopLevel(m_originalParent, parentWidget());
}

QWidget* DrawerView::eventTopLevel(QObject* watched) const
{
    return ::view::overlay::eventTopLevel(watched);
}

bool DrawerView::eventBelongsToDrawerTopLevel(QObject* watched) const
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    if (!top)
        return false;
    return eventTopLevel(watched) == top;
}

void DrawerView::ensureApplicationEventFilter()
{
    if (m_applicationFilterInstalled || !qApp)
        return;
    qApp->installEventFilter(this);
    m_applicationFilterInstalled = true;
}

void DrawerView::removeApplicationEventFilter()
{
    if (!m_applicationFilterInstalled || !qApp)
        return;
    qApp->removeEventFilter(this);
    m_applicationFilterInstalled = false;
}

void DrawerView::installTopLevelEventFilter(QWidget* topLevel)
{
    if (!topLevel)
        return;
    if (m_topLevel == topLevel && m_topLevelFilterInstalled)
        return;

    removeTopLevelEventFilter();
    m_topLevel = topLevel;
    m_topLevel->installEventFilter(this);
    m_topLevelFilterInstalled = true;
}

void DrawerView::removeTopLevelEventFilter()
{
    if (m_topLevel && m_topLevelFilterInstalled)
        m_topLevel->removeEventFilter(this);
    m_topLevelFilterInstalled = false;
}

void DrawerView::ensureAttachedToTopLevel()
{
    QWidget* top = resolveTopLevelWidget();
    if (!top)
        return;

    installTopLevelEventFilter(top);

    ::view::overlay::attachToTopLevel(this, top);
}

void DrawerView::beginVisibleTransition()
{
    ensureAttachedToTopLevel();
    updateOverlayGeometry();

    if (!isVisible())
        emit aboutToShow();

    show();
    updateScrimState();
    raiseOverlayStack();
    if (m_contentWidget)
        m_contentWidget->show();
    setFocus(Qt::PopupFocusReason);
}

void DrawerView::finalizeOpened()
{
    m_isClosing = false;
    setPosition(1.0);
    raiseOverlayStack();
    if (!m_isOpen) {
        m_isOpen = true;
        emit isOpenChanged(true);
    }
    emit opened();
}

void DrawerView::finalizeClosed()
{
    m_isClosing = false;
    setPosition(0.0);
    hide();
    destroyScrim();
    if (m_contentWidget)
        m_contentWidget->hide();
    if (m_isOpen) {
        m_isOpen = false;
        emit isOpenChanged(false);
    }
    emit closed();
}

void DrawerView::startPositionAnimation(qreal endPosition, TransitionTarget target)
{
    const auto animation = themeAnimation();
    m_transitionTarget = target;
    m_positionAnimation->setDuration(animation.normal);
    m_positionAnimation->setEasingCurve(target == TransitionTarget::Open ? animation.decelerate : animation.accelerate);
    m_positionAnimation->setStartValue(m_position);
    m_positionAnimation->setEndValue(endPosition);
    m_positionAnimation->start();
}

void DrawerView::stopAnimation()
{
    if (m_positionAnimation)
        m_positionAnimation->stop();
    m_transitionTarget = TransitionTarget::None;
}

int DrawerView::availableAxisLength() const
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    if (!top)
        return m_drawerLength;
    return (m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Right) ? top->height() : top->width();
}

int DrawerView::effectiveDrawerLength() const
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    if (!top)
        return m_drawerLength;
    const int axisLength = (m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Right) ? top->width() : top->height();
    return qBound(kMinimumDrawerLength, m_drawerLength, std::max(kMinimumDrawerLength, axisLength));
}

QRect DrawerView::openPanelRect() const
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    const QRect topRect = top ? top->rect() : QRect(QPoint(0, 0), sizeHint());
    const int length = effectiveDrawerLength();

    switch (m_edge) {
    case DrawerEdge::Left:
        return QRect(0, 0, length, topRect.height());
    case DrawerEdge::Right:
        return QRect(topRect.width() - length, 0, length, topRect.height());
    case DrawerEdge::Top:
        return QRect(0, 0, topRect.width(), length);
    case DrawerEdge::Bottom:
        return QRect(0, topRect.height() - length, topRect.width(), length);
    }
    return QRect();
}

QRect DrawerView::panelRectForPosition(qreal position) const
{
    const QRect openRect = openPanelRect();
    const int length = (m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Right) ? openRect.width() : openRect.height();
    const int visibleLength = qRound(length * normalizedPosition(position));

    switch (m_edge) {
    case DrawerEdge::Left:
        return openRect.translated(visibleLength - length, 0);
    case DrawerEdge::Right:
        return openRect.translated(length - visibleLength, 0);
    case DrawerEdge::Top:
        return openRect.translated(0, visibleLength - length);
    case DrawerEdge::Bottom:
        return openRect.translated(0, length - visibleLength);
    }
    return openRect;
}

QPainterPath DrawerView::outerRoundedPanelPath(const QRectF& rect) const
{
    const qreal radius = qMin<qreal>(m_outerCornerRadius, qMin(rect.width(), rect.height()) / 2.0);
    const bool roundTopLeft = m_edge == DrawerEdge::Right || m_edge == DrawerEdge::Bottom;
    const bool roundTopRight = m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Bottom;
    const bool roundBottomRight = m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Top;
    const bool roundBottomLeft = m_edge == DrawerEdge::Right || m_edge == DrawerEdge::Top;
    return ::view::overlay::roundedCornerRectPath(rect, radius,
                                                  roundTopLeft,
                                                  roundTopRight,
                                                  roundBottomRight,
                                                  roundBottomLeft);
}

void DrawerView::updateClipMask()
{
    if (rect().isEmpty() || m_outerCornerRadius <= 0) {
        clearMask();
        return;
    }
    // QGraphicsOpacityEffect handles transparency via offscreen rendering;
    // no widget mask is required.
    clearMask();
}

void DrawerView::updateOverlayGeometry()
{
    m_panelGeometry = panelRectForPosition(m_position);
    if (parentWidget())
        setGeometry(m_panelGeometry);
    updateScrimGeometry();
    updateScrimOpacity();
    updateContentGeometry();
    raiseOverlayStack();
}

void DrawerView::updateContentGeometry()
{
    QRect nextGeometry = rect();
    if (m_contentGeometry == nextGeometry && (!m_contentWidget || m_contentWidget->geometry() == nextGeometry))
        return;

    m_contentGeometry = nextGeometry;
    if (m_contentWidget && m_contentWidget->geometry() != m_contentGeometry)
        m_contentWidget->setGeometry(m_contentGeometry);
}

void DrawerView::updateScrimGeometry()
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    m_scrimGeometry = top ? top->rect() : QRect();
    if (m_scrim)
        m_scrim->setGeometry(m_scrimGeometry);
}

void DrawerView::updateScrimOpacity()
{
    if (auto* scrim = dynamic_cast<::view::overlay::OverlayScrim*>(m_scrim.data()))
        scrim->setOpacityProgress(m_position);
}

void DrawerView::updateScrimState()
{
    if (!isVisible() && !m_isOpen && !m_drag.active)
        return;

    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    if (!top)
        return;

    if (!m_modal && !m_dim) {
        destroyScrim();
        return;
    }

    if (!m_scrim)
        m_scrim = new ::view::overlay::OverlayScrim(top, QStringLiteral("DrawerViewScrim"));
    if (m_scrim->parentWidget() != top)
        m_scrim->setParent(top);

    if (auto* scrim = dynamic_cast<::view::overlay::OverlayScrim*>(m_scrim.data()))
        scrim->setModalAndDim(m_modal, m_dim);
    updateScrimOpacity();
    m_scrim->setGeometry(top->rect());
    m_scrim->show();
    raiseOverlayStack();
}

void DrawerView::destroyScrim()
{
    if (!m_scrim)
        return;

    m_scrim->hide();
    m_scrim->deleteLater();
    m_scrim = nullptr;
}

void DrawerView::raiseOverlayStack()
{
    ::view::overlay::raiseOverlayStack(m_scrim, this);
}

bool DrawerView::isPointInsidePanel(const QPoint& globalPos) const
{
    if (!parentWidget())
        return false;
    const QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos))
        return false;
    if (m_outerCornerRadius <= 0)
        return true;
    return outerRoundedPanelPath(QRectF(rect())).contains(QPointF(localPos));
}

bool DrawerView::isPointInEdgeDragArea(const QPoint& globalPos) const
{
    if (m_dragMargin <= 0)
        return false;

    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    if (!top)
        return false;

    const QPoint local = top->mapFromGlobal(globalPos);
    const QRect topRect = top->rect();
    if (!topRect.contains(local))
        return false;

    switch (m_edge) {
    case DrawerEdge::Left:
        return local.x() <= m_dragMargin;
    case DrawerEdge::Right:
        return local.x() >= topRect.width() - m_dragMargin;
    case DrawerEdge::Top:
        return local.y() <= m_dragMargin;
    case DrawerEdge::Bottom:
        return local.y() >= topRect.height() - m_dragMargin;
    }
    return false;
}

int DrawerView::dragAxisValue(const QPoint& globalPos) const
{
    QWidget* top = m_topLevel ? m_topLevel.data() : resolveTopLevelWidget();
    const QPoint local = top ? top->mapFromGlobal(globalPos) : globalPos;
    return (m_edge == DrawerEdge::Left || m_edge == DrawerEdge::Right) ? local.x() : local.y();
}

qreal DrawerView::positionForDragDelta(int delta) const
{
    const int length = effectiveDrawerLength();
    if (length <= 0)
        return m_drag.startPosition;

    qreal signedDelta = delta;
    if (m_edge == DrawerEdge::Right || m_edge == DrawerEdge::Bottom)
        signedDelta = -signedDelta;
    return normalizedPosition(m_drag.startPosition + signedDelta / length);
}

void DrawerView::beginDrag(const QPoint& globalPos, bool fromClosed)
{
    if (!m_interactive || !isEnabled())
        return;

    stopAnimation();
    if (fromClosed)
        beginVisibleTransition();

    m_drag.active = true;
    m_drag.startedFromClosed = fromClosed;
    m_drag.startGlobalPos = globalPos;
    m_drag.startPosition = m_position;
}

void DrawerView::updateDrag(const QPoint& globalPos)
{
    if (!m_drag.active)
        return;

    const int delta = dragAxisValue(globalPos) - dragAxisValue(m_drag.startGlobalPos);
    setPosition(positionForDragDelta(delta));
}

void DrawerView::endDrag()
{
    if (!m_drag.active)
        return;

    const bool shouldOpen = m_position >= 0.5;
    m_drag = DragState();
    if (shouldOpen)
        open();
    else
        close();
}

void DrawerView::cancelDrag()
{
    m_drag = DragState();
}

bool DrawerView::shouldCloseOnOutsidePress() const
{
    return isEnabled() && !isEffectivelyNoAutoClose() && (m_closePolicy & CloseOnPressOutside);
}

bool DrawerView::shouldCloseOnEscape() const
{
    return !isEffectivelyNoAutoClose() && (m_closePolicy & CloseOnEscape);
}

bool DrawerView::isEffectivelyNoAutoClose() const
{
    return m_closePolicy == ClosePolicy(NoAutoClose);
}

qreal DrawerView::normalizedPosition(qreal position)
{
    if (std::isnan(position))
        return 0.0;
    return qBound<qreal>(0.0, position, 1.0);
}

bool DrawerView::fuzzyEqual(qreal left, qreal right)
{
    return std::abs(left - right) <= kPositionEpsilon;
}

} // namespace view::collections
