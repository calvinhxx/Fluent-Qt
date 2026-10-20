#include "compatibility/InputEventCompat.h"
#include "Dialog.h"

#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/Material.h"

namespace fluent::dialogs_flyouts {

Dialog::Dialog(QWidget* parent) : QDialog(parent), m_originalParent(parent)
{
    m_overlayCoordinator = new ::fluent::overlay::OverlayCoordinator(this, this);
    m_overlayCoordinator->setClosePolicy(int(m_closePolicy));
    connect(m_overlayCoordinator, &::fluent::overlay::OverlayCoordinator::dismissRequested, this, [this] {
        done(QDialog::Rejected);
    });
    m_overlayCoordinator->setHitTest([this](const QPoint& global) {
        return rect()
            .adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize)
            .contains(mapFromGlobal(global));
    });

    connect(m_overlayCoordinator, &::fluent::overlay::OverlayCoordinator::hostGeometryChanged, this, [this]() {
        if (!isVisible())
            return;
        positionInOwner();
        if (auto* smoke = m_overlayCoordinator->scrim()) {
            smoke->setSurfaceRadius(
                qRound(::fluent::overlay::overlaySurfaceRadius(m_overlayCoordinator->topLevelWidget())));
        }
        m_overlayCoordinator->raiseStack();
    });

    // Same-window overlay contract (WinUI ContentDialog / Popup / DrawerView): stay a Qt::Widget
    // child of the owning top-level. Never host Dialog as a native top-level window.
    // zh_CN: 同窗口浮层契约（WinUI ContentDialog / Popup / DrawerView）：保持为 owning top-level 的
    // Qt::Widget 子控件，绝不把 Dialog 做成原生顶层窗口。
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::NoDropShadowWindowHint);
    attachToOwner();

    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_animation = new QPropertyAnimation(this, "animationProgress", this);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing) {
            m_isClosing = false;
            m_isAnimating = false;
            const QSize targetSize = m_targetSize;
            m_targetSize = QSize();
            QPointer<Dialog> guard(this);
            QDialog::done(m_closingResult);
            if (!guard)
                return;
            if (!targetSize.isEmpty())
                guard->resize(targetSize);
            guard->setMinimumSize(guard->m_savedMinSize);
            guard->setMaximumSize(guard->m_savedMaxSize);
            guard->setSurfaceOpacity(1.0);
            emit closed();
        } else {
            resize(m_targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            m_isAnimating = false;
            m_targetSize = QSize();
            emitOpenedIfNeeded();
        }
    });

    onThemeUpdated();
}

Dialog::~Dialog()
{
    if (m_overlayCoordinator->scrim()) {
        m_overlayCoordinator->releaseScrim(::fluent::overlay::OverlayCoordinator::ScrimDeletion::Immediate);
    }
}

QWidget* Dialog::ownerWidget() const
{
    if (QWidget* owner = ::fluent::overlay::resolveOwningTopLevel(m_originalParent, parentWidget())) {
        return owner;
    }
    return m_overlayCoordinator->topLevelWidget();
}

void Dialog::attachToOwner()
{
    QWidget* top = ownerWidget();
    if (top)
        m_overlayCoordinator->attachTo(top);
}

void Dialog::setAnimationProgress(double progress)
{
    m_animationProgress = progress;
    setSurfaceOpacity(progress);
    if (auto* scrim = m_overlayCoordinator->scrim())
        scrim->setOpacityProgress(progress);
    update();
}

void Dialog::setDragEnabled(bool enabled)
{
    if (m_dragEnabled == enabled)
        return;
    m_dragEnabled = enabled;
    emit dragEnabledChanged(m_dragEnabled);
}

void Dialog::setAnimationEnabled(bool enabled)
{
    if (m_animationEnabled == enabled)
        return;
    m_animationEnabled = enabled;
    QPointer<Dialog> guard(this);
    emit animationEnabledChanged(m_animationEnabled);
    if (!guard || m_animationEnabled)
        return;

    // A local opt-out is authoritative even for transitions already in flight.
    // The surface and backdrop share this timeline, including immediate completion.
    // zh_CN: 面板和背景共用时间线，包括立即结束过渡的情况。
    if (m_animation && m_animation->state() == QAbstractAnimation::Running)
        m_animation->setCurrentTime(m_animation->duration());
}

bool Dialog::isModal() const
{
    return m_overlayCoordinator->isModal();
}

void Dialog::setModal(bool modal)
{
    const double previousBlur = backdropBlurRadius();
    const bool smokeWasEnabled = isSmokeEnabled();
    if (!m_overlayCoordinator->setModal(modal))
        return;
    const double currentBlur = backdropBlurRadius();
    const auto blurRevision = m_overlayCoordinator->backdropBlurRevision();
    QPointer<Dialog> guard(this);
    updateScrimState();
    if (!guard)
        return;
    emit modalChanged(isModal());
    // A reentrant handler may replace the automatic value and notify it itself.
    // zh_CN: 重入处理可能覆盖自动值并自行发送通知，避免再发送过期通知。
    if (guard && previousBlur != currentBlur && blurRevision == m_overlayCoordinator->backdropBlurRevision())
        emit backdropBlurRadiusChanged(currentBlur);
    if (guard && smokeWasEnabled != isSmokeEnabled())
        emit smokeEnabledChanged(isSmokeEnabled());
}

bool Dialog::isDim() const
{
    return m_overlayCoordinator->isDim();
}

void Dialog::setDim(bool dim)
{
    const bool smokeWasEnabled = isSmokeEnabled();
    if (!m_overlayCoordinator->setDim(dim))
        return;
    updateScrimState();
    QPointer<Dialog> guard(this);
    emit dimChanged(isDim());
    if (guard && smokeWasEnabled != isSmokeEnabled())
        emit smokeEnabledChanged(isSmokeEnabled());
}

void Dialog::setSmokeEnabled(bool enabled)
{
    if (enabled) {
        setDim(true);
        setModal(true);
    } else {
        setDim(false);
        setModal(false);
    }
}

double Dialog::dimOpacity() const
{
    return m_overlayCoordinator->dimOpacity();
}

void Dialog::setDimOpacity(double opacity)
{
    if (!m_overlayCoordinator->setDimOpacity(opacity))
        return;
    updateScrimState();
    emit dimOpacityChanged(dimOpacity());
}

double Dialog::backdropBlurRadius() const
{
    return m_overlayCoordinator->backdropBlurRadius();
}

void Dialog::setBackdropBlurRadius(double radius)
{
    if (!m_overlayCoordinator->setBackdropBlurRadius(radius))
        return;
    updateScrimState();
    emit backdropBlurRadiusChanged(backdropBlurRadius());
}

void Dialog::refreshBackdrop()
{
    m_overlayCoordinator->refreshBackdrop();
}

void Dialog::setClosePolicy(ClosePolicy policy)
{
    if (m_closePolicy == policy)
        return;
    m_closePolicy = policy;
    m_overlayCoordinator->setClosePolicy(int(m_closePolicy));
    emit closePolicyChanged(m_closePolicy);
}

void Dialog::setIsOpen(bool open)
{
    if (open)
        this->open();
    else
        done(QDialog::Rejected);
}

void Dialog::emitOpenedIfNeeded()
{
    if (m_openedEmitted || !m_isOpen || m_isClosing)
        return;
    m_openedEmitted = true;
    emit opened();
}

void Dialog::setThemeSource(QWidget* source)
{
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

bool Dialog::syncThemeOverrideFromSource()
{
    QWidget* source = m_themeSource ? m_themeSource.data() : ownerWidget();
    return ::fluent::overlay::syncInheritedThemeOverride(this, source);
}

void Dialog::open()
{
    if (m_openInProgress)
        return;
    if (m_isOpen && !m_isClosing)
        return;

    m_openInProgress = true;
    m_isClosing = false;
    m_openedEmitted = false;
    QPointer<Dialog> guard(this);

    auto abortIfCancelled = [&]() -> bool {
        if (!guard)
            return true;
        if (!m_openInProgress || m_isClosing) {
            m_openInProgress = false;
            if (m_isClosing && m_animation->state() != QAbstractAnimation::Running)
                m_isClosing = false;
            return true;
        }
        return false;
    };

    emit opening();
    if (abortIfCancelled())
        return;
    emit aboutToShow();
    if (abortIfCancelled())
        return;

    if (!m_isOpen) {
        m_isOpen = true;
        emit isOpenChanged(true);
        if (abortIfCancelled())
            return;
    }

    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    attachToOwner();
    prepareSurfaceSize();

    if (m_animationEnabled && !isVisible()) {
        m_isAnimating = true;
        setAnimationProgress(0.0);
    } else {
        setAnimationProgress(1.0);
    }

    // Prefer show() over QDialog::open(): the latter's WindowModal path can briefly hide a
    // same-window child and tear down the smoke scrim via hideEvent before the dialog is visible.
    // zh_CN: 优先用 show() 而非 QDialog::open()：后者的 WindowModal 路径会短暂隐藏同窗口子控件，
    // 并通过 hideEvent 在对话框可见前拆掉烟雾遮罩。
    if (windowModality() == Qt::NonModal)
        setWindowModality(Qt::WindowModal);
    QDialog::show();
    if (abortIfCancelled())
        return;

    updateScrimState();
    m_overlayCoordinator->raiseStack();
    setFocus(Qt::ActiveWindowFocusReason);

    if (!m_animationEnabled)
        emitOpenedIfNeeded();
    if (guard)
        m_openInProgress = false;
}

int Dialog::exec()
{
    QPointer<Dialog> guard(this);
    open();
    if (!guard)
        return QDialog::Rejected;
    if (!m_isOpen && !isVisible())
        return result();

    const int result = QDialog::exec();
    if (guard)
        guard->hideSmokeOverlay();
    return result;
}

void Dialog::showEvent(QShowEvent* event)
{
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    attachToOwner();
    positionInOwner();
    QDialog::showEvent(event);
    m_overlayCoordinator->setInputActive(true);
    m_overlayCoordinator->raiseStack();
    updateScrimState();

    if (!m_animationEnabled || !m_isAnimating) {
        setAnimationProgress(1.0);
        m_isAnimating = false;
        if (m_isOpen)
            emitOpenedIfNeeded();
        return;
    }

    m_isClosing = false;
    ensurePolished();
    for (auto* child : findChildren<QWidget*>()) {
        child->ensurePolished();
        if (auto* fluentChild = dynamic_cast<FluentElement*>(child))
            fluentChild->onThemeUpdated();
        if (child->layout())
            child->layout()->activate();
    }
    if (layout())
        layout()->activate();

    m_targetSize = size();
    m_savedMinSize = minimumSize();
    m_savedMaxSize = maximumSize();

    const auto& anim = themeAnimation();
    m_animation->stop();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(anim.decelerate);
    ::fluent::detail::startMotionTransition(m_animation, anim.normal, m_animationEnabled);
}

void Dialog::hideEvent(QHideEvent* event)
{
    m_overlayCoordinator->setInputActive(false);
    hideSmokeOverlay();
    QDialog::hideEvent(event);
}

void Dialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_closePolicy & CloseOnEscape)
            done(QDialog::Rejected);
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool Dialog::eventFilter(QObject* watched, QEvent* event)
{
    return QDialog::eventFilter(watched, event);
}

void Dialog::prepareSurfaceSize()
{
    ensurePolished();
    for (auto* child : findChildren<QWidget*>())
        child->ensurePolished();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }

    const int collapsedHeight = m_shadowSize * 2 + 48;
    if (height() > collapsedHeight)
        return;

    QSize preferred = sizeHint();
    if (!minimumSizeHint().isEmpty())
        preferred = preferred.expandedTo(minimumSizeHint());
    preferred = preferred.expandedTo(minimumSize());
    if (!preferred.isValid() || preferred.isEmpty())
        return;

    QSize next(qMax(width(), preferred.width()), qMax(height(), preferred.height()));
    next = next.expandedTo(minimumSize()).boundedTo(maximumSize());
    if (next != size())
        resize(next);
}

void Dialog::positionInOwner()
{
    QWidget* owner = ownerWidget();
    if (!owner)
        return;

    QRect surface = ::fluent::overlay::overlaySurfaceRect(owner);
    if (surface.isEmpty())
        surface = owner->rect();

    const QRect current(pos(), size());
    const bool defaultPlacement = pos().isNull();
    const bool outsideSurface = !surface.contains(current);
    QPoint next = pos();
    if (isDim() || defaultPlacement || outsideSurface) {
        next = QPoint(surface.left() + (surface.width() - width()) / 2,
            surface.top() + (surface.height() - height()) / 2);
    }

    if (width() <= surface.width())
        next.setX(qBound(surface.left(), next.x(), surface.right() - width() + 1));
    else
        next.setX(surface.left());
    if (height() <= surface.height())
        next.setY(qBound(surface.top(), next.y(), surface.bottom() - height() + 1));
    else
        next.setY(surface.top());

    if (next != pos())
        move(next);
}

void Dialog::setSurfaceOpacity(qreal opacity)
{
    if (m_opacityEffect)
        m_opacityEffect->setOpacity(qBound<qreal>(0.0, opacity, 1.0));
}

void Dialog::done(int result)
{
    if (m_isClosing)
        return;
    if (!m_isOpen && !isVisible() && !m_openInProgress) {
        QDialog::done(result);
        return;
    }

    m_isClosing = true;
    m_closingResult = result;
    QPointer<Dialog> guard(this);
    emit closing();
    if (!guard)
        return;
    emit aboutToHide();
    if (!guard)
        return;
    if (!m_isClosing)
        return;

    if (m_isOpen) {
        m_isOpen = false;
        emit isOpenChanged(false);
        if (!guard)
            return;
        if (m_isOpen || !m_isClosing)
            return;
    }

    if (!m_animationEnabled || !isVisible()) {
        setSurfaceOpacity(0.0);
        QDialog::done(result);
        if (guard) {
            guard->setSurfaceOpacity(1.0);
            if (!m_openInProgress)
                m_isClosing = false;
            emit closed();
        }
        return;
    }

    m_animation->stop();

    if (!m_isAnimating) {
        m_targetSize = size();
        m_savedMinSize = minimumSize();
        m_savedMaxSize = maximumSize();
        setMinimumSize(QSize(0, 0));
        setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        m_isAnimating = true;
        m_animationProgress = 1.0;
    }

    const auto& anim = themeAnimation();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(m_animationProgress);
    m_animation->setEndValue(0.0);
    m_animation->setEasingCurve(anim.standard);
    ::fluent::detail::startMotionTransition(m_animation, anim.normal, m_animationEnabled);
}

void Dialog::mousePressEvent(QMouseEvent* event)
{
    if (m_dragEnabled && event->button() == Qt::LeftButton) {
        m_dragOffset = fluentMousePos(event);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QDialog::mousePressEvent(event);
}

void Dialog::mouseMoveEvent(QMouseEvent* event)
{
    if (cursor().shape() == Qt::ClosedHandCursor && parentWidget()) {
        const QPoint parentPos = parentWidget()->mapFromGlobal(fluentMouseGlobalPos(event));
        move(parentPos - m_dragOffset);
        event->accept();
    }
    QDialog::mouseMoveEvent(event);
}

void Dialog::mouseReleaseEvent(QMouseEvent* event)
{
    if (cursor().shape() == Qt::ClosedHandCursor)
        unsetCursor();
    QDialog::mouseReleaseEvent(event);
}

void Dialog::updateScrimState()
{
    if (!m_isOpen && !isVisible()) {
        hideSmokeOverlay();
        return;
    }
    if (!m_overlayCoordinator->needsScrim()) {
        hideSmokeOverlay();
        return;
    }
    showSmokeOverlay();
}

void Dialog::showSmokeOverlay()
{
    QWidget* owner = ownerWidget();
    if (!owner || !owner->isVisible())
        return;

    const int surfaceRadius = qRound(::fluent::overlay::overlaySurfaceRadius(owner));
    m_overlayCoordinator->attachTo(owner);
    auto* smoke = m_overlayCoordinator->updateScrim(QStringLiteral("DialogSmokeScrim"));
    if (!smoke)
        return;
    const auto& smokeToken = themeSmoke();
    QColor color = smokeToken.baseColor;
    color.setAlphaF(smokeToken.opacity);
    smoke->setColor(color);
    smoke->setProgress(m_animationProgress);
    smoke->setSurfaceRadius(surfaceRadius);
    smoke->show();
    m_overlayCoordinator->raiseStack();
}

void Dialog::hideSmokeOverlay()
{
    auto* overlay = m_overlayCoordinator->scrim();
    if (!overlay)
        return;

    QWidget* owner = overlay->parentWidget();
    const QRect dirtyRect = overlay->geometry();

    overlay->setModalAndDim(false, false);
    m_overlayCoordinator->releaseScrim(::fluent::overlay::OverlayCoordinator::ScrimDeletion::Immediate);

    if (owner)
        owner->update(dirtyRect);
}

void Dialog::onThemeUpdated()
{
    update();
    ::fluent::overlay::refreshFluentDescendants(this);
    if (auto* smokeOverlay = m_overlayCoordinator->scrim()) {
        const auto& smoke = themeSmoke();
        QColor color = smoke.baseColor;
        color.setAlphaF(smoke.opacity);
        smokeOverlay->setColor(color);
    }
}

void Dialog::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);
    drawShadow(painter, contentRect);

    const auto& colors = themeColorsRef();

    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = themeRadius().overlay;
    surface.border = colors.strokeDefault;
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

void Dialog::drawShadow(QPainter& painter, const QRect& contentRect)
{
    // Medium elevation: High stacked on smoke reads as a heavy halo.
    // zh_CN: 用 Medium；High 叠在烟雾上会形成过重暗晕。
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, themeRadius().overlay, themeShadow(Elevation::Medium));
}

} // namespace fluent::dialogs_flyouts
