#include "compatibility/InputEventCompat.h"
#include "Popup.h"

#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/private/TransientSurfaceAccessibility_p.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/foundation/overlay/OverlayLightDismiss.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include <QAbstractAnimation>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>

namespace fluent::dialogs_flyouts {

// ── Construction / destruction. zh_CN: 构造 / 析构 ───────────────────────────

Popup::Popup(QWidget* parent) : QWidget(parent)
{
    detail::ensureTransientSurfaceAccessibilityFactory();
    m_originalParent = parent;
    m_overlayCoordinator = new ::fluent::overlay::OverlayCoordinator(this, this);
    m_overlayCoordinator->setClosePolicy(int(m_closePolicy));
    connect(m_overlayCoordinator, &::fluent::overlay::OverlayCoordinator::dismissRequested, this,
        [this](::fluent::overlay::OverlayCoordinator::DismissReason reason) {
            if (!m_closeReasonExplicit)
                setPendingCloseReason(
                    reason == ::fluent::overlay::OverlayCoordinator::DismissReason::Escape ? Escape : LightDismiss);
            close();
        });
    m_overlayCoordinator->setOutsidePressConsumption([this](const QPoint& globalPos) {
        if (!m_lightDismissConsumesPress)
            return false;
        QWidget* hit = QApplication::widgetAt(globalPos);
        for (const QPointer<QWidget>& passthrough : m_lightDismissPassthrough) {
            if (!passthrough)
                continue;
            const bool projected = ::fluent::overlay::presentationRoot(passthrough) != nullptr;
            const bool byHierarchy = !projected && hit && (hit == passthrough || passthrough->isAncestorOf(hit));
            const bool byGeometry = passthrough->isVisible()
                && passthrough->rect().contains(
                    ::fluent::overlay::localPointFromPresentedGlobal(passthrough, globalPos));
            if (byHierarchy || byGeometry)
                return false;
        }
        return true;
    });

    connect(m_overlayCoordinator, &::fluent::overlay::OverlayCoordinator::hostGeometryChanged, this, [this]() {
        if ((!m_isOpen && !isVisible()) || m_isClosing)
            return;
        move(resolvedPosition());
        m_overlayCoordinator->raiseStack();
    });
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    const int shadowMargin = ::fluent::overlay::defaultShadowMargin();
    setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    resize(::fluent::overlay::outerSizeForVisibleCard(QSize(320, 160), shadowMargin));

    m_anim = new QPropertyAnimation(this, "popupProgress", this);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing)
            finalizeClosed();
        else
            finalizeOpened();
    });

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    // Must hide explicitly: if the Popup was never hidden when the parent
    // window shows, Qt would surface it automatically (opacity=0 looks
    // invisible but still swallows mouse events, breaking button clicks).
    // zh_CN: 必须显式 hide：父窗口 show() 时未被 hide 的 Popup 会被 Qt 自动
    // 显现（opacity=0 视觉不可见，但仍拦截鼠标事件，导致按钮点击失效）。
    hide();

    onThemeUpdated();
}

Popup::~Popup()
{
    if (qApp)
        qApp->removeEventFilter(this);
}

// ── Theme. zh_CN: 主题 ───────────────────────────────────────────────────────

void Popup::onThemeUpdated()
{
    const QColor surfaceColor = themeColorsRef().bgLayer;
    if (property("fluentSurfaceColor").value<QColor>() != surfaceColor)
        setProperty("fluentSurfaceColor", surfaceColor);
    update();
    ::fluent::overlay::refreshFluentDescendants(this);
    if (m_overlayCoordinator->scrim()) {
        if (auto* fe = dynamic_cast<FluentElement*>(m_overlayCoordinator->scrim()))
            fe->onThemeUpdated();
    }
}

// ── popupProgress ────────────────────────────────────────────────────────────

void Popup::setPopupProgress(double p)
{
    if (qFuzzyCompare(m_popupProgress, p))
        return;
    m_popupProgress = p;
    if (m_opacityEffect)
        m_opacityEffect->setOpacity(p);
    if (auto* scrim = m_overlayCoordinator->scrim())
        scrim->setOpacityProgress(p);
    update();
    emit popupProgressChanged(p);
}

double Popup::dimOpacity() const
{
    return m_overlayCoordinator->dimOpacity();
}

void Popup::setDimOpacity(double opacity)
{
    if (!m_overlayCoordinator->setDimOpacity(opacity))
        return;
    updateScrimState();
    emit dimOpacityChanged(dimOpacity());
}

double Popup::backdropBlurRadius() const
{
    return m_overlayCoordinator->backdropBlurRadius();
}

void Popup::setBackdropBlurRadius(double radius)
{
    if (!m_overlayCoordinator->setBackdropBlurRadius(radius))
        return;
    updateScrimState();
    emit backdropBlurRadiusChanged(backdropBlurRadius());
}

void Popup::refreshBackdrop()
{
    m_overlayCoordinator->refreshBackdrop();
}

void Popup::setClosePolicy(ClosePolicy p)
{
    if (m_closePolicy == p)
        return;
    m_closePolicy = p;
    m_overlayCoordinator->setClosePolicy(int(m_closePolicy));
    QPointer<Popup> guard(this);
    emit closePolicyChanged(m_closePolicy);
    if (guard)
        detail::notifyPopupAccessibilityActionsChanged(guard.data());
}

bool Popup::isModal() const
{
    return m_overlayCoordinator->isModal();
}

void Popup::setModal(bool modal)
{
    const double previousBlur = backdropBlurRadius();
    if (!m_overlayCoordinator->setModal(modal))
        return;
    const double currentBlur = backdropBlurRadius();
    const auto blurRevision = m_overlayCoordinator->backdropBlurRevision();
    QPointer<Popup> guard(this);
    updateScrimState();
    if (!guard)
        return;
    emit modalChanged(isModal());
    // A reentrant handler may replace the automatic value and notify it itself.
    // zh_CN: 重入处理可能覆盖自动值并自行发送通知，避免再发送过期通知。
    if (guard && previousBlur != currentBlur && blurRevision == m_overlayCoordinator->backdropBlurRevision())
        emit backdropBlurRadiusChanged(currentBlur);
    if (guard)
        detail::notifyPopupAccessibilityModalChanged(guard.data());
}

bool Popup::isDim() const
{
    return m_overlayCoordinator->isDim();
}

void Popup::setDim(bool dim)
{
    if (!m_overlayCoordinator->setDim(dim))
        return;
    updateScrimState();
    QPointer<Popup> guard(this);
    emit dimChanged(isDim());
}

void Popup::setAnimationEnabled(bool e)
{
    if (m_animationEnabled == e)
        return;
    m_animationEnabled = e;
    QPointer<Popup> guard(this);
    emit animationEnabledChanged(m_animationEnabled);
    if (!guard || m_animationEnabled || m_anim->state() != QAbstractAnimation::Running)
        return;

    // The local switch has priority over the application policy and settles an
    // active open/close transition through its normal finished path.
    // zh_CN: 局部开关优先于应用策略，并通过正常 finished 路径收敛当前开关过渡。
    m_anim->setCurrentTime(m_anim->duration());
}

void Popup::setExitAnimationEnabled(bool e)
{
    if (m_exitAnimationEnabled == e)
        return;
    m_exitAnimationEnabled = e;
    if (m_exitAnimationEnabled || !m_isClosing || m_anim->state() != QAbstractAnimation::Running) {
        return;
    }

    // The granular exit switch has the same immediate-settle contract as the
    // general animation switch, but only while an exit is active. Reaching the
    // end keeps finalizeClosed() and its focus/scrim/closed cleanup on the
    // animation's normal finished path.
    // zh_CN: 细粒度退出开关仅在退出动画运行时即时收敛，并继续通过正常
    // finished 路径执行 finalizeClosed() 的焦点、遮罩与 closed 清理。
    m_anim->setCurrentTime(m_anim->duration());
}

void Popup::setPendingCloseReason(CloseReason reason)
{
    m_pendingCloseReason = reason;
    m_closeReasonExplicit = true;
}

void Popup::resetPendingCloseReason()
{
    m_pendingCloseReason = Programmatic;
    m_closeReasonExplicit = false;
}

void Popup::setThemeSource(QWidget* source)
{
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

void Popup::setFocusOnOpenEnabled(bool enabled)
{
    m_overlayCoordinator->setFocusOnOpenEnabled(enabled);
}

// ── topLevelWidget resolution. zh_CN: topLevelWidget 推断 ────────────────────

QWidget* Popup::originalParentTopLevel() const
{
    return ::fluent::overlay::resolveOwningTopLevel(m_originalParent, parentWidget());
}

// ── setPosition ──────────────────────────────────────────────────────────────

void Popup::setPosition(QWidget* relativeTo, const QPoint& localPos)
{
    if (!relativeTo)
        return;
    m_targetPos = ::fluent::overlay::presentedPointInTopLevel(relativeTo, localPos);
    m_positionRelativeTo = relativeTo;
    m_positionLocalPos = localPos;
    m_positionSet = true;
    if (m_isOpen)
        queuePositionSync();
}

// ── Position (centered by default; subclasses may override). zh_CN: 位置计算 ──

QPoint Popup::computePosition() const
{
    QWidget* top = originalParentTopLevel();
    if (!top)
        return pos();
    // Default: center inside the topLevelWidget. zh_CN: 默认在 topLevelWidget 中居中。
    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    return QPoint(surface.left() + (surface.width() - width()) / 2, surface.top() + (surface.height() - height()) / 2);
}

QPoint Popup::resolvedPosition() const
{
    if (!m_positionSet)
        return computePosition();

    QPoint cardTopLeft = m_targetPos;
    if (m_positionRelativeTo && m_positionRelativeTo->window()) {
        cardTopLeft = ::fluent::overlay::presentedPointInTopLevel(m_positionRelativeTo, m_positionLocalPos);
        if (::fluent::overlay::hasPresentedTransform(m_positionRelativeTo)) {
            cardTopLeft = ::fluent::overlay::clampCardTopLeft(cardTopLeft, ::fluent::overlay::visibleCardSize(size()),
                ::fluent::overlay::overlaySurfaceRect(originalParentTopLevel()), 0);
        }
    }
    return ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft);
}

QWidget* Popup::trackedPositionAnchor() const
{
    if (m_positionSet)
        return m_positionRelativeTo.data();
    return automaticPositionAnchor();
}

QWidget* Popup::themeOverrideSource() const
{
    if (QWidget* anchor = trackedPositionAnchor())
        return anchor;
    if (m_themeSource)
        return m_themeSource.data();
    if (m_originalParent)
        return m_originalParent.data();
    return parentWidget();
}

bool Popup::syncThemeOverrideFromSource()
{
    const bool popupChanged = ::fluent::overlay::syncInheritedThemeOverride(this, themeOverrideSource());

    bool scrimChanged = false;
    if (m_overlayCoordinator->scrim()) {
        scrimChanged = ::fluent::overlay::syncInheritedThemeOverride(m_overlayCoordinator->scrim(), this);
    }
    return popupChanged || scrimChanged;
}

void Popup::queuePositionSync()
{
    if (m_positionSyncPending)
        return;
    m_positionSyncPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_positionSyncPending = false;
        syncPositionToAnchor();
    });
}

void Popup::syncPositionToAnchor()
{
    if ((!m_isOpen && !isVisible()) || m_isClosing)
        return;
    QWidget* anchor = trackedPositionAnchor();
    if (!anchor)
        return;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    if (!::fluent::overlay::isAnchorVisibleInTopLevel(anchor)) {
        close();
        return;
    }
    if (QWidget* top = originalParentTopLevel(); top && parentWidget() != top) {
        m_overlayCoordinator->attachTo(top);
        show();
        updateScrimState();
    }
    move(resolvedPosition());
    m_overlayCoordinator->raiseStack();
}

// ── open / close ─────────────────────────────────────────────────────────────

void Popup::open()
{
    if (m_openInProgress)
        return;
    if (m_isOpen && !m_isClosing)
        return;

    m_openInProgress = true;
    QPointer<Popup> guard(this);

    m_anim->stop();
    m_isClosing = false;
    resetPendingCloseReason();
    m_focusRestoreTarget = nullptr;
    if (m_overlayCoordinator->focusOnOpenEnabled() && qApp) {
        QWidget* focused = QApplication::focusWidget();
        if (focused && focused != this && !isAncestorOf(focused))
            m_focusRestoreTarget = focused;
    }

    QWidget* top = originalParentTopLevel();
    m_overlayCoordinator->attachTo(top);
    if (syncThemeOverrideFromSource())
        onThemeUpdated();

    auto abortIfCancelled = [&]() -> bool {
        if (!guard)
            return true;
        if (!m_openInProgress || m_isClosing) {
            m_openInProgress = false;
            if (m_isClosing && m_anim->state() != QAbstractAnimation::Running)
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
        detail::notifyPopupAccessibilityOpenChanged(this, true);
    }

    ensurePolished();
    if (layout())
        layout()->activate();

    // Layout drives the size. zh_CN: layout 驱动尺寸。
    if (layout()) {
        QSize hint = layout()->totalSizeHint();
        if (hint.isValid() && !hint.isEmpty())
            resize(hint);
    }

    // Placement: honor setPosition() when provided, else center.
    // zh_CN: 定位——setPosition() 设置过则用目标位置，否则居中。
    move(resolvedPosition());

    show();
    m_overlayCoordinator->raiseStack();
    if (m_overlayCoordinator->focusOnOpenEnabled())
        setFocus(Qt::PopupFocusReason);

    if (qApp)
        qApp->installEventFilter(this);

    updateScrimState();
    m_overlayCoordinator->setInputActive(true);

    if (!m_animationEnabled) {
        setPopupProgress(1.0);
        if (abortIfCancelled())
            return;
        emit opened();
        if (guard)
            m_openInProgress = false;
        return;
    }

    startEnterAnimation();
    if (guard)
        m_openInProgress = false;
}

void Popup::close()
{
    closeWithReason(m_closeReasonExplicit ? m_pendingCloseReason : Programmatic);
}

void Popup::closeWithReason(CloseReason reason)
{
    beginClose(reason);
}

void Popup::beginClose(CloseReason reason)
{
    if (m_isClosing)
        return;
    if (!m_isOpen && !isVisible() && !m_anim->state() && !m_openInProgress)
        return;

    m_anim->stop();
    m_isClosing = true;
    m_pendingCloseReason = reason;
    m_closeReasonExplicit = true;
    QPointer<Popup> guard(this);
    emit closing(reason);
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
        detail::notifyPopupAccessibilityOpenChanged(this, false);
    }

    if (qApp)
        qApp->removeEventFilter(this);

    if (!m_animationEnabled || !m_exitAnimationEnabled || !isVisible()) {
        setPopupProgress(0.0);
        if (!guard)
            return;
        finalizeClosed();
        return;
    }

    startExitAnimation();
}

void Popup::setIsOpen(bool open)
{
    if (open)
        this->open();
    else
        this->close();
}

// ── Animation. zh_CN: 动画 ───────────────────────────────────────────────────

void Popup::startEnterAnimation()
{
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal); // Matches Dialog: 250ms. zh_CN: 与 Dialog 一致。
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(1.0);
    // Opacity has no overshoot: use the monotone Fluent deceleration token.
    // zh_CN: 不透明度无需回弹，使用单调减速的 Fluent 曲线。
    m_anim->setEasingCurve(a.decelerate);
    ::fluent::detail::startMotionTransition(m_anim, a.normal, m_animationEnabled);
}

void Popup::startExitAnimation()
{
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal); // Matches Dialog: 250ms. zh_CN: 与 Dialog 一致。
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(0.0);
    m_anim->setEasingCurve(a.standard);
    ::fluent::detail::startMotionTransition(m_anim, a.normal, m_animationEnabled);
}

void Popup::finalizeOpened()
{
    if (m_isClosing || !m_isOpen)
        return;
    QPointer<Popup> guard(this);
    emit opened();
    if (guard)
        m_openInProgress = false;
}

void Popup::finalizeClosed()
{
    if (m_isOpen) {
        m_isClosing = false;
        return;
    }
    // Leave m_isClosing set while open() is still on the stack so the
    // Opening path can abort instead of continuing after a nested close.
    // zh_CN: open() 仍在栈上时保留 m_isClosing，避免 Opening 里嵌套 close() 后继续完成打开。
    if (!m_openInProgress)
        m_isClosing = false;
    QWidget* focused = qApp ? QApplication::focusWidget() : nullptr;
    const bool shouldRestoreFocus = !focused || focused == this || isAncestorOf(focused);
    QPointer<QWidget> focusRestoreTarget = m_focusRestoreTarget;
    m_focusRestoreTarget = nullptr;
    hide();
    destroyScrim();
    if (shouldRestoreFocus && focusRestoreTarget && focusRestoreTarget->isVisible() && focusRestoreTarget->isEnabled()
        && focusRestoreTarget->focusPolicy() != Qt::NoFocus) {
        focusRestoreTarget->setFocus(Qt::PopupFocusReason);
    }
    QPointer<Popup> guard(this);
    emit closed();
    if (guard)
        resetPendingCloseReason();
}

// ── Scrim ────────────────────────────────────────────────────────────────────

void Popup::updateScrimState()
{
    if (!m_isOpen && !isVisible()) {
        destroyScrim();
        return;
    }
    if (!m_overlayCoordinator->needsScrim()) {
        destroyScrim();
        return;
    }

    QWidget* top = m_overlayCoordinator->topLevelWidget();
    if (!top)
        top = originalParentTopLevel();
    if (!top)
        return;

    m_overlayCoordinator->attachTo(top);
    auto* scrim = m_overlayCoordinator->updateScrim(QStringLiteral("PopupScrim"));
    if (!scrim)
        return;
    scrim->setOpacityProgress(m_popupProgress);
    scrim->show();
    m_overlayCoordinator->raiseStack();
}

void Popup::destroyScrim()
{
    m_overlayCoordinator->releaseScrim();
}

// ── Light-dismiss / Escape ──────────────────────────────────────────────────

bool Popup::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_isOpen && !isVisible())
        return false;

    QWidget* positionAnchor = trackedPositionAnchor();
    if (event && positionAnchor && event->type() == QEvent::Destroy && watched == positionAnchor) {
        // The invocation target is already inside QObject destruction. Do not
        // let finalizeClosed() call setFocus() on it while closing the overlay.
        // zh_CN: 调用目标已进入 QObject 析构；关闭浮层时不能再向其归还焦点。
        m_focusRestoreTarget = nullptr;
        if (auto* flyout = qobject_cast<Flyout*>(this))
            flyout->setAnchor(nullptr);
        if (!m_closeReasonExplicit)
            setPendingCloseReason(TargetDestroyed);
        close();
        return false;
    }
    if (::fluent::overlay::anchorGeometryMayChange(watched, event, positionAnchor))
        queuePositionSync();

    return false;
}

void Popup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && (m_closePolicy & CloseOnEscape)) {
        if (!m_closeReasonExplicit)
            setPendingCloseReason(Escape);
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void Popup::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Opacity-only animation; QGraphicsOpacityEffect covers background and
    // children together.
    // zh_CN: 仅做 opacity 动画（QGraphicsOpacityEffect 统一作用于背景与子控件）。

    const QRect contentRect = ::fluent::overlay::visibleCardRect(rect());

    // Shared layered card shadow.
    // zh_CN: 共享的分层卡片阴影。
    const int r = themeRadius().overlay;
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, r, themeShadow(Elevation::High));

    // Background and border. zh_CN: 背景 + 边框。
    const auto& colors = themeColorsRef();
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = r;
    surface.border = colors.strokeDefault;
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

} // namespace fluent::dialogs_flyouts
