#include "Popup.h"

#include <QAbstractAnimation>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include "compatibility/QtCompat.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayLightDismiss.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/dialogs_flyouts/private/TransientSurfaceAccessibility_p.h"
#include "components/dialogs_flyouts/Flyout.h"

namespace fluent::dialogs_flyouts {

namespace {

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;

    const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        if (auto* fluentChild = dynamic_cast<FluentElement*>(child))
            fluentChild->onThemeUpdated();
        refreshFluentDescendants(child);
    }
}

} // namespace

// ── Construction / destruction. zh_CN: 构造 / 析构 ───────────────────────────

Popup::Popup(QWidget* parent) : QWidget(parent) {
    detail::ensureTransientSurfaceAccessibilityFactory();
    m_originalParent = parent;
    m_overlayCoordinator =
        new ::fluent::overlay::OverlayCoordinator(this, this);
    connect(m_overlayCoordinator,
            &::fluent::overlay::OverlayCoordinator::hostGeometryChanged,
            this,
            [this]() {
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
        if (m_isClosing) finalizeClosed();
        else             finalizeOpened();
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

Popup::~Popup() {
    if (qApp)
        qApp->removeEventFilter(this);
}

// ── Theme. zh_CN: 主题 ───────────────────────────────────────────────────────

void Popup::onThemeUpdated() {
    const QColor surfaceColor = themeColorsRef().bgLayer;
    if (property("fluentSurfaceColor").value<QColor>() != surfaceColor)
        setProperty("fluentSurfaceColor", surfaceColor);
    update();
    refreshFluentDescendants(this);
    if (m_overlayCoordinator->scrim()) {
        if (auto* fe =
                dynamic_cast<FluentElement*>(m_overlayCoordinator->scrim()))
            fe->onThemeUpdated();
    }
}

// ── popupProgress ────────────────────────────────────────────────────────────

void Popup::setPopupProgress(double p) {
    if (qFuzzyCompare(m_popupProgress, p)) return;
    m_popupProgress = p;
    if (m_opacityEffect) m_opacityEffect->setOpacity(p);
    update();
    emit popupProgressChanged(p);
}

void Popup::setClosePolicy(ClosePolicy p) {
    if (m_closePolicy == p)
        return;
    m_closePolicy = p;
    QPointer<Popup> guard(this);
    emit closePolicyChanged(m_closePolicy);
    if (guard)
        detail::notifyPopupAccessibilityActionsChanged(guard.data());
}

void Popup::setModal(bool m) {
    if (m_modal == m)
        return;
    m_modal = m;
    updateScrimState();
    QPointer<Popup> guard(this);
    emit modalChanged(m_modal);
    if (guard)
        detail::notifyPopupAccessibilityModalChanged(guard.data());
}

void Popup::setDim(bool d) {
    if (m_dim == d)
        return;
    m_dim = d;
    updateScrimState();
    emit dimChanged(m_dim);
}

void Popup::setAnimationEnabled(bool e) {
    if (m_animationEnabled == e)
        return;
    m_animationEnabled = e;
    emit animationEnabledChanged(m_animationEnabled);
}

void Popup::setExitAnimationEnabled(bool e) {
    if (m_exitAnimationEnabled == e)
        return;
    m_exitAnimationEnabled = e;
}

void Popup::setPendingCloseReason(CloseReason reason) {
    m_pendingCloseReason = reason;
    m_closeReasonExplicit = true;
}

void Popup::resetPendingCloseReason() {
    m_pendingCloseReason = Programmatic;
    m_closeReasonExplicit = false;
}

void Popup::setThemeSource(QWidget* source) {
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

void Popup::setFocusOnOpenEnabled(bool enabled) {
    m_overlayCoordinator->setFocusOnOpenEnabled(enabled);
}

// ── topLevelWidget resolution. zh_CN: topLevelWidget 推断 ────────────────────

QWidget* Popup::originalParentTopLevel() const {
    return ::fluent::overlay::resolveOwningTopLevel(m_originalParent, parentWidget());
}

// ── setPosition ──────────────────────────────────────────────────────────────

void Popup::setPosition(QWidget* relativeTo, const QPoint& localPos) {
    if (!relativeTo) return;
    QWidget* top = relativeTo->window();
    m_targetPos = relativeTo->mapTo(top, localPos);
    m_positionRelativeTo = relativeTo;
    m_positionLocalPos = localPos;
    m_positionSet = true;
}

// ── Position (centered by default; subclasses may override). zh_CN: 位置计算 ──

QPoint Popup::computePosition() const {
    QWidget* top = originalParentTopLevel();
    if (!top) return pos();
    // Default: center inside the topLevelWidget. zh_CN: 默认在 topLevelWidget 中居中。
    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    return QPoint(surface.left() + (surface.width() - width()) / 2,
                  surface.top() + (surface.height() - height()) / 2);
}

QPoint Popup::resolvedPosition() const {
    if (!m_positionSet)
        return computePosition();

    QPoint cardTopLeft = m_targetPos;
    if (m_positionRelativeTo && m_positionRelativeTo->window()) {
        cardTopLeft = m_positionRelativeTo->mapTo(m_positionRelativeTo->window(),
                                                  m_positionLocalPos);
    }
    return ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft);
}

QWidget* Popup::trackedPositionAnchor() const {
    if (m_positionSet)
        return m_positionRelativeTo.data();
    return automaticPositionAnchor();
}

QWidget* Popup::themeOverrideSource() const {
    if (QWidget* anchor = trackedPositionAnchor())
        return anchor;
    if (m_themeSource)
        return m_themeSource.data();
    if (m_originalParent)
        return m_originalParent.data();
    return parentWidget();
}

bool Popup::syncThemeOverrideFromSource() {
    const bool popupChanged = ::fluent::overlay::syncInheritedThemeOverride(
        this, themeOverrideSource());

    bool scrimChanged = false;
    if (m_overlayCoordinator->scrim()) {
        scrimChanged = ::fluent::overlay::syncInheritedThemeOverride(
            m_overlayCoordinator->scrim(), this);
    }
    return popupChanged || scrimChanged;
}

void Popup::queuePositionSync() {
    if (m_positionSyncPending)
        return;
    m_positionSyncPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_positionSyncPending = false;
        syncPositionToAnchor();
    });
}

void Popup::syncPositionToAnchor() {
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
    move(resolvedPosition());
    m_overlayCoordinator->raiseStack();
}

// ── open / close ─────────────────────────────────────────────────────────────

void Popup::open() {
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
    if (layout()) layout()->activate();

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

void Popup::close() {
    closeWithReason(m_closeReasonExplicit ? m_pendingCloseReason : Programmatic);
}

void Popup::closeWithReason(CloseReason reason) {
    beginClose(reason);
}

void Popup::beginClose(CloseReason reason) {
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

void Popup::setIsOpen(bool open) {
    if (open) this->open();
    else      this->close();
}

// ── Animation. zh_CN: 动画 ───────────────────────────────────────────────────

void Popup::startEnterAnimation() {
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal);       // Matches Dialog: 250ms. zh_CN: 与 Dialog 一致。
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(a.entrance); // Matches Dialog. zh_CN: 与 Dialog 一致。
    m_anim->start();
}

void Popup::startExitAnimation() {
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal);       // Matches Dialog: 250ms. zh_CN: 与 Dialog 一致。
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(0.0);
    m_anim->setEasingCurve(a.exit);     // Matches Dialog. zh_CN: 与 Dialog 一致。
    m_anim->start();
}

void Popup::finalizeOpened() {
    if (m_isClosing || !m_isOpen)
        return;
    QPointer<Popup> guard(this);
    emit opened();
    if (guard)
        m_openInProgress = false;
}

void Popup::finalizeClosed() {
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
    const bool shouldRestoreFocus =
        !focused || focused == this || isAncestorOf(focused);
    QPointer<QWidget> focusRestoreTarget = m_focusRestoreTarget;
    m_focusRestoreTarget = nullptr;
    hide();
    destroyScrim();
    if (shouldRestoreFocus && focusRestoreTarget
        && focusRestoreTarget->isVisible()
        && focusRestoreTarget->isEnabled()
        && focusRestoreTarget->focusPolicy() != Qt::NoFocus) {
        focusRestoreTarget->setFocus(Qt::PopupFocusReason);
    }
    QPointer<Popup> guard(this);
    emit closed();
    if (guard)
        resetPendingCloseReason();
}

// ── Scrim ────────────────────────────────────────────────────────────────────

void Popup::updateScrimState() {
    if (!m_isOpen && !isVisible()) {
        destroyScrim();
        return;
    }
    if (!m_modal && !m_dim) {
        destroyScrim();
        return;
    }

    QWidget* top = m_overlayCoordinator->topLevelWidget();
    if (!top)
        top = originalParentTopLevel();
    if (!top)
        return;

    m_overlayCoordinator->attachTo(top);
    auto* scrim =
        m_overlayCoordinator->ensureScrim(QStringLiteral("PopupScrim"));
    if (!scrim)
        return;
    scrim->setModalAndDim(m_modal, m_dim);
    ::fluent::overlay::syncInheritedThemeOverride(scrim, this);
    scrim->show();
    m_overlayCoordinator->raiseStack();
}

void Popup::destroyScrim() {
    m_overlayCoordinator->releaseScrim();
}

// ── Light-dismiss / Escape ──────────────────────────────────────────────────

bool Popup::eventFilter(QObject* watched, QEvent* event) {
    if (!m_isOpen && !isVisible()) return false;

    QWidget* positionAnchor = trackedPositionAnchor();
    if (event && positionAnchor && event->type() == QEvent::Destroy
        && watched == positionAnchor) {
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

    const bool noAutoClose = m_closePolicy == ClosePolicy(NoAutoClose);
    if (::fluent::overlay::isEscapeKeyPress(event) &&
        ::fluent::overlay::allowsImplicitClose(noAutoClose, m_closePolicy & CloseOnEscape)) {
        if (!m_closeReasonExplicit)
            setPendingCloseReason(Escape);
        close();
        event->accept();
        return true;
    }

    if (!::fluent::overlay::allowsImplicitClose(noAutoClose, m_closePolicy & CloseOnPressOutside)) return false;
    if (!event || event->type() != QEvent::MouseButtonPress) return false;

    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint globalPos = fluentMouseGlobalPos(me);
    const QPoint local = mapFromGlobal(globalPos);
    if (::fluent::overlay::visibleCardContains(rect(), local)) return false;

    if (!m_closeReasonExplicit)
        setPendingCloseReason(LightDismiss);
    close();
    // ComboBox-dropdown semantics when requested: swallow the dismissing press so it doesn't also
    // activate the widget beneath — EXCEPT inside a registered passthrough region (e.g. the sibling nav
    // bar), where the press still falls through so adjacent controls stay one-click reachable.
    // zh_CN: 按需采用 ComboBox 下拉语义：吞掉这次关闭点击，避免顺带激活下方控件——但在已登记的「穿透区域」（如同级导航栏）
    // 内除外，那里的点击仍会穿透，使相邻控件保持「一次点击」直达。
    if (m_lightDismissConsumesPress) {
        // Let the press through if it lands inside a passthrough region (the sibling nav bar) so that
        // bar stays one-click reachable while the popup dismisses. Two complementary tests: widgetAt +
        // isAncestorOf follows the real widget hierarchy (robust on a live top-level), and a geometry
        // contains() as a fallback for platforms where widgetAt returns null (e.g. the offscreen test
        // plugin). zh_CN: 若按下点落在穿透区域(同级导航栏)内则放行,使该栏在弹窗关闭时仍可「一次点击」直达。两种互补判定:
        // widgetAt + isAncestorOf 顺真实控件层级判断(在真实顶层上稳健),几何 contains() 作为后备,用于 widgetAt 返回空的
        // 平台(如 offscreen 测试插件)。
        QWidget* hit = QApplication::widgetAt(globalPos);
        for (const QPointer<QWidget>& passthrough : m_lightDismissPassthrough) {
            if (!passthrough)
                continue;
            const bool byHierarchy = hit
                && (hit == passthrough.data() || passthrough->isAncestorOf(hit));
            const bool byGeometry = passthrough->isVisible()
                && passthrough->rect().contains(passthrough->mapFromGlobal(globalPos));
            if (byHierarchy || byGeometry)
                return false;
        }
        event->accept();
        return true;
    }
    return false;
}

void Popup::keyPressEvent(QKeyEvent* event) {
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

void Popup::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Opacity-only animation; QGraphicsOpacityEffect covers background and
    // children together.
    // zh_CN: 仅做 opacity 动画（QGraphicsOpacityEffect 统一作用于背景与子控件）。

    const QRect contentRect = ::fluent::overlay::visibleCardRect(rect());

    // Shared layered card shadow.
    // zh_CN: 共享的分层卡片阴影。
    const int r = themeRadius().overlay;
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, r,
                                          themeShadow(Elevation::High));

    // Background and border. zh_CN: 背景 + 边框。
    const auto& colors = themeColorsRef();
    const DesignLanguage lang = themeDesignLanguage();
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = r;
    if (lang == DesignMaterial) {
        // Material 3 elevated "surface-container": elevation is conveyed by the shadow alone,
        // so the card has NO visible stroke. zh_CN: Material 3 高架 "surface-container":高度仅由阴影
        // 表达,故卡片无可见描边。
        surface.border = Qt::transparent;
    } else if (lang == DesignCupertino) {
        // macOS popover: a crisp 1px hairline edge using the stronger neutral stroke.
        // zh_CN: macOS popover:用更强的中性描边绘制清晰的 1px 发丝边缘。
        surface.border = colors.strokeStrong;
    } else {
        // DesignFluent (default): unchanged WinUI overlay stroke. zh_CN: 默认 Fluent,WinUI 浮层描边不变。
        surface.border = colors.strokeDefault;
    }
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

} // namespace fluent::dialogs_flyouts
