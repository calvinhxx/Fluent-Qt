#include "Popup.h"

#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QTimer>
#include "compatibility/QtCompat.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/overlay/OverlayLightDismiss.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayWindow.h"

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
    m_originalParent = parent;
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
    if (m_scrim) {
        m_scrim->hide();
        delete m_scrim;
        m_scrim = nullptr;
    }
}

// ── Theme. zh_CN: 主题 ───────────────────────────────────────────────────────

void Popup::onThemeUpdated() {
    update();
    refreshFluentDescendants(this);
    if (m_scrim) {
        if (auto* fe = dynamic_cast<FluentElement*>(m_scrim.data()))
            fe->onThemeUpdated();
    }
}

// ── popupProgress ────────────────────────────────────────────────────────────

void Popup::setPopupProgress(double p) {
    if (qFuzzyCompare(m_popupProgress, p)) return;
    m_popupProgress = p;
    if (m_opacityEffect) m_opacityEffect->setOpacity(p);
    emit popupProgressChanged(p);
    update();
}

void Popup::setThemeSource(QWidget* source) {
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
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
    if (m_scrim) {
        scrimChanged = ::fluent::overlay::syncInheritedThemeOverride(
            m_scrim.data(), this);
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
    ::fluent::overlay::raiseOverlayStack(m_scrim, this);
}

// ── open / close ─────────────────────────────────────────────────────────────

void Popup::open() {
    if (m_isOpen && !m_isClosing) return;
    m_anim->stop();
    m_isClosing = false;

    QWidget* top = originalParentTopLevel();
    m_topLevel = top;
    ::fluent::overlay::attachToTopLevel(this, top);
    if (syncThemeOverrideFromSource())
        onThemeUpdated();

    emit aboutToShow();

    ensureScrim();

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
    ::fluent::overlay::raiseOverlayStack(m_scrim, this);
    setFocus(Qt::PopupFocusReason);

    if (qApp)
        qApp->installEventFilter(this);

    if (!m_animationEnabled) {
        setPopupProgress(1.0);
        m_isOpen = true;
        emit isOpenChanged(true);
        emit opened();
        return;
    }

    m_popupProgress = 0.0;
    startEnterAnimation();
}

void Popup::close() {
    if (!m_isOpen && !m_anim->state()) {
        if (!isVisible()) return;
    }
    if (m_isClosing) return;

    m_anim->stop();
    m_isClosing = true;
    emit aboutToHide();

    if (qApp)
        qApp->removeEventFilter(this);

    if (!m_animationEnabled || !m_exitAnimationEnabled) {
        setPopupProgress(0.0);
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
    if (m_isOpen) return;
    m_isOpen = true;
    emit isOpenChanged(true);
    emit opened();
}

void Popup::finalizeClosed() {
    m_isClosing = false;
    hide();
    destroyScrim();
    if (m_isOpen) {
        m_isOpen = false;
        emit isOpenChanged(false);
    }
    emit closed();
}

// ── Scrim ────────────────────────────────────────────────────────────────────

void Popup::ensureScrim() {
    if (!m_modal) return;
    QWidget* top = m_topLevel ? m_topLevel.data() : originalParentTopLevel();
    if (!top) return;

    if (!m_scrim)
        m_scrim = new ::fluent::overlay::OverlayScrim(top, QStringLiteral("PopupScrim"));
    if (auto* scrim = dynamic_cast<::fluent::overlay::OverlayScrim*>(m_scrim.data()))
        scrim->setModalAndDim(true, m_dim);
    ::fluent::overlay::syncInheritedThemeOverride(m_scrim.data(), this);
    m_scrim->setGeometry(::fluent::overlay::overlaySurfaceRect(top));
    m_scrim->show();
    ::fluent::overlay::raiseOverlayStack(m_scrim, this);
}

void Popup::destroyScrim() {
    if (!m_scrim) return;
    m_scrim->hide();
    m_scrim->deleteLater();
    m_scrim = nullptr;
}

// ── Light-dismiss / Escape ──────────────────────────────────────────────────

bool Popup::eventFilter(QObject* watched, QEvent* event) {
    if (event && event->type() == QEvent::Destroy && watched == m_topLevel) {
        if (qApp)
            qApp->removeEventFilter(this);
        m_topLevel = nullptr;
        m_scrim = nullptr;
        return false;
    }

    if (event && event->type() == QEvent::Resize && watched == m_topLevel) {
        if (m_scrim && m_topLevel)
            m_scrim->setGeometry(::fluent::overlay::overlaySurfaceRect(m_topLevel));
        if (isVisible()) {
            move(resolvedPosition());
            ::fluent::overlay::raiseOverlayStack(m_scrim, this);
        }
        return false;
    }

    if (!m_isOpen && !isVisible()) return false;

    QWidget* positionAnchor = trackedPositionAnchor();
    if (event && positionAnchor && event->type() == QEvent::Destroy
        && watched == positionAnchor) {
        close();
        return false;
    }
    if (::fluent::overlay::anchorGeometryMayChange(watched, event, positionAnchor))
        queuePositionSync();

    const bool noAutoClose = m_closePolicy == ClosePolicy(NoAutoClose);
    if (::fluent::overlay::isEscapeKeyPress(event) &&
        ::fluent::overlay::allowsImplicitClose(noAutoClose, m_closePolicy & CloseOnEscape)) {
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
    const auto& colors = themeColors();
    const DesignLanguage lang = themeDesignLanguage();
    painter.setBrush(colors.bgLayer);
    if (lang == DesignMaterial) {
        // Material 3 elevated "surface-container": elevation is conveyed by the shadow alone,
        // so the card has NO visible stroke. zh_CN: Material 3 高架 "surface-container":高度仅由阴影
        // 表达,故卡片无可见描边。
        painter.setPen(Qt::NoPen);
    } else if (lang == DesignCupertino) {
        // macOS popover: a crisp 1px hairline edge using the stronger neutral stroke.
        // zh_CN: macOS popover:用更强的中性描边绘制清晰的 1px 发丝边缘。
        painter.setPen(QPen(colors.strokeStrong, 1));
    } else {
        // DesignFluent (default): unchanged WinUI overlay stroke. zh_CN: 默认 Fluent,WinUI 浮层描边不变。
        painter.setPen(colors.strokeDefault);
    }
    painter.drawRoundedRect(contentRect, r, r);
}

} // namespace fluent::dialogs_flyouts
