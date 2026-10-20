#include "components/foundation/overlay/OverlayCoordinator.h"

#include <QEvent>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <cmath>
#include "compatibility/InputEventCompat.h"

#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "design/OverlayTokens_p.h"

namespace fluent::overlay {
namespace {
constexpr const char* inputActiveProperty = "_fluent_overlay_input_active";

QWidget* owningWidget(QObject* object)
{
    for (auto* node = object; node; node = node->parent()) {
        if (auto* widget = qobject_cast<QWidget*>(node))
            return widget;
    }
    return nullptr;
}

bool belongsToWindow(QObject* object, QWidget* top)
{
    if (!top)
        return false;
    if (auto* native = qobject_cast<QWindow*>(object))
        return native == top->window()->windowHandle();
    auto* widget = owningWidget(object);
    return widget && widget->window() == top->window();
}

// Keep the matching release out of the background even when dismissal deletes
// the overlay synchronously. The next press/deactivation ends a lost gesture.
// zh_CN: 即使关闭同步析构浮层，也吞掉对应释放；新的按下或窗口失活结束丢失的手势。
class DismissedPressGuard final : public QObject {
public:
    DismissedPressGuard(QWidget* top, Qt::MouseButton button) : QObject(top), m_top(top), m_button(button)
    {
        qApp->installEventFilter(this);
        QTimer::singleShot(2000, this, &QObject::deleteLater);
    }
    ~DismissedPressGuard() override
    {
        if (qApp)
            qApp->removeEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!belongsToWindow(watched, m_top))
            return false;
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::WindowDeactivate) {
            qApp->removeEventFilter(this);
            deleteLater();
        } else if (event->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent*>(event)->button() == m_button) {
            qApp->removeEventFilter(this);
            deleteLater();
            event->accept();
            return true;
        }
        return false;
    }

private:
    QPointer<QWidget> m_top;
    Qt::MouseButton m_button;
};
} // namespace

// Input is filtered before QWidget dispatch; host geometry is observed after
// component application filters have adapted their content size.
// zh_CN: 输入在 QWidget 分发前拦截；宿主几何在组件应用过滤器调整内容尺寸后同步。
class OverlayInputFilter final : public QObject {
public:
    explicit OverlayInputFilter(OverlayCoordinator* owner) : QObject(owner), m_owner(owner) {}

    void queueFocusContainment()
    {
        // Defer focus callbacks until the public property setter has returned.
        // zh_CN: 等公共属性 setter 返回后再触发焦点回调。
        QTimer::singleShot(0, this, [this] {
            auto* focus = QApplication::focusWidget();
            if (m_owner && m_owner->m_inputActive && m_owner->m_modal && m_owner->m_overlay
                && !m_owner->hasInputOverlayAbove() && belongsToWindow(focus, m_owner->m_topLevel)
                && focus != m_owner->m_overlay && !m_owner->m_overlay->isAncestorOf(focus))
                m_owner->focusInside(false);
        });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event || !m_owner || !m_owner->m_inputActive)
            return false;
        if (watched == m_owner->m_overlay && event->type() == QEvent::Hide) {
            m_owner->setInputActive(false);
            return false;
        }
        return m_owner->filterInput(watched, event);
    }

private:
    QPointer<OverlayCoordinator> m_owner;
};

OverlayCoordinator::OverlayCoordinator(QWidget* overlay, QObject* parent) : QObject(parent), m_overlay(overlay)
{
    markOverlaySurface(overlay);
    m_inputFilter = new OverlayInputFilter(this);
}

OverlayCoordinator::~OverlayCoordinator()
{
    if (qApp)
        qApp->removeEventFilter(m_inputFilter);
    if (m_topLevel)
        m_topLevel->removeEventFilter(this);
    releaseScrim(ScrimDeletion::Immediate);
}

void OverlayCoordinator::attachTo(QWidget* topLevel)
{
    if (!topLevel)
        return;

    if (m_topLevel != topLevel) {
        if (m_topLevel)
            m_topLevel->removeEventFilter(this);
        m_topLevel = topLevel;
        m_topLevel->installEventFilter(this);
    }

    attachToTopLevel(m_overlay.data(), topLevel);
    if (m_scrim)
        attachToTopLevel(m_scrim.data(), topLevel);
    syncScrimGeometry();
}

void OverlayCoordinator::detach()
{
    if (m_topLevel)
        m_topLevel->removeEventFilter(this);
    m_topLevel = nullptr;
}

OverlayScrim* OverlayCoordinator::ensureScrim(const QString& objectName)
{
    if (!m_topLevel)
        return nullptr;

    if (!m_scrim) {
        m_scrim = new OverlayScrim(m_topLevel.data(), objectName);
        connect(m_scrim, &OverlayScrim::pressed, this, &OverlayCoordinator::scrimPressed);
    } else {
        attachToTopLevel(m_scrim.data(), m_topLevel.data());
    }

    syncScrimGeometry();
    return m_scrim.data();
}

void OverlayCoordinator::releaseScrim(ScrimDeletion deletion)
{
    if (!m_scrim)
        return;

    OverlayScrim* scrimWidget = m_scrim.data();
    m_scrim = nullptr;
    scrimWidget->hide();
    if (deletion == ScrimDeletion::Immediate)
        delete scrimWidget;
    else
        scrimWidget->deleteLater();
}

void OverlayCoordinator::syncScrimGeometry()
{
    if (m_scrim && m_topLevel)
        m_scrim->setGeometry(overlaySurfaceRect(m_topLevel.data()));
}

void OverlayCoordinator::raiseStack()
{
    raiseOverlayStack(m_scrim.data(), m_overlay.data());
}

bool OverlayCoordinator::setModal(bool modal)
{
    if (m_modal == modal)
        return false;
    m_modal = modal;
    if (!m_blurRadiusExplicit) {
        m_blurRadius = modal ? overlay_tokens::modalBlurRadius : 0.0;
        ++m_blurRevision;
    }
    if (m_modal && m_inputActive)
        m_inputFilter->queueFocusContainment();
    return true;
}

bool OverlayCoordinator::setDim(bool dim)
{
    if (m_dim == dim)
        return false;
    m_dim = dim;
    return true;
}

bool OverlayCoordinator::setDimOpacity(double opacity)
{
    if (!std::isfinite(opacity))
        return false;
    const double value = opacity < 0.0 ? -1.0 : qBound(0.0, opacity, 1.0);
    if (m_dimOpacity == value)
        return false;
    m_dimOpacity = value;
    return true;
}

bool OverlayCoordinator::setBackdropBlurRadius(double radius)
{
    if (!std::isfinite(radius))
        return false;
    const double value = qBound(0.0, radius, 32.0);
    // Even an explicit zero before enabling modality opts out of its default.
    // zh_CN: 启用模态前显式设置 0，也会关闭默认模糊。
    m_blurRadiusExplicit = true;
    if (m_blurRadius == value)
        return false;
    m_blurRadius = value;
    ++m_blurRevision;
    return true;
}

bool OverlayCoordinator::needsScrim() const
{
    return m_modal || m_dim || m_blurRadius > 0.0;
}

OverlayScrim* OverlayCoordinator::updateScrim(const QString& objectName)
{
    auto* scrim = ensureScrim(objectName);
    if (!scrim)
        return nullptr;
    syncInheritedThemeOverride(scrim, m_overlay);
    scrim->setModalAndDim(m_modal, m_dim);
    scrim->setDimOpacity(m_dimOpacity);
    // Cached overlays may be below content created or raised since their last
    // opening. Capture only after establishing the stack the user will see.
    // zh_CN: 缓存浮层可能低于后来创建或置顶的内容；先确定显示层级，再抓取背景。
    raiseStack();
    scrim->setBackdrop(m_overlay, m_blurRadius);
    return scrim;
}

void OverlayCoordinator::refreshBackdrop()
{
    if (m_scrim)
        m_scrim->refreshBackdrop();
}

void OverlayCoordinator::setInputActive(bool active)
{
    if (active == m_inputActive || !qApp)
        return;
    m_inputActive = active;
    if (m_overlay)
        m_overlay->setProperty(inputActiveProperty, active);
    if (active) {
        qApp->installEventFilter(m_inputFilter);
        if (m_modal)
            m_inputFilter->queueFocusContainment();
    } else
        qApp->removeEventFilter(m_inputFilter);
}

bool OverlayCoordinator::hasInputOverlayAbove() const
{
    if (!m_topLevel)
        return false;
    const auto children = m_topLevel->children();
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
        auto* widget = qobject_cast<QWidget*>(*it);
        if (!widget || !widget->isVisible() || !widget->property(inputActiveProperty).toBool())
            continue;
        return widget != m_overlay;
    }
    return false;
}

bool OverlayCoordinator::containsGlobalPoint(const QPoint& point) const
{
    return m_overlay
        && (m_contains ? m_contains(point) : visibleCardContains(m_overlay->rect(), m_overlay->mapFromGlobal(point)));
}

void OverlayCoordinator::focusInside(bool backwards)
{
    if (!m_overlay)
        return;
    QWidget* candidate = m_overlay;
    do {
        candidate = backwards ? candidate->previousInFocusChain() : candidate->nextInFocusChain();
        if (candidate != m_overlay && m_overlay->isAncestorOf(candidate) && candidate->isVisible()
            && candidate->isEnabled() && (candidate->focusPolicy() & Qt::TabFocus)) {
            candidate->setFocus(backwards ? Qt::BacktabFocusReason : Qt::TabFocusReason);
            return;
        }
    } while (candidate != m_overlay);
    m_overlay->setFocus(Qt::PopupFocusReason);
}

bool OverlayCoordinator::filterInput(QObject* watched, QEvent* event)
{
    if (!m_overlay || !m_overlay->isVisible() || !belongsToWindow(watched, m_topLevel) || hasInputOverlayAbove())
        return false;
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::Shortcut) {
        auto* widget = owningWidget(watched);
        if (m_modal && widget && widget != m_overlay && !m_overlay->isAncestorOf(widget)) {
            if (event->type() == QEvent::FocusIn)
                focusInside(static_cast<QFocusEvent*>(event)->reason() == Qt::BacktabFocusReason);
            event->accept();
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        if (!(m_closePolicy & 2) && !m_modal)
            return false;
        event->accept();
        if (m_closePolicy & 2)
            emit dismissRequested(DismissReason::Escape);
        return true;
    }
    QPoint global;
    bool press = false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        press = true;
        Q_FALLTHROUGH();
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        global = fluentMouseGlobalPos(static_cast<QMouseEvent*>(event));
        break;
    case QEvent::Wheel:
        global = static_cast<QWheelEvent*>(event)->globalPosition().toPoint();
        break;
    case QEvent::ContextMenu:
        global = static_cast<QContextMenuEvent*>(event)->globalPos();
        break;
    default:
        return false;
    }
    if (containsGlobalPoint(global))
        return false;
    const bool consume = m_modal || (press && (m_closePolicy & 1) && m_consumes && m_consumes(global));
    if (consume)
        event->accept();
    if (press && m_modal)
        new DismissedPressGuard(m_topLevel, static_cast<QMouseEvent*>(event)->button());
    if (press && m_scrim && (m_modal || watched == m_scrim)
        && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        const QPointer<OverlayCoordinator> guard(this);
        const QPointer<QObject> receiver(watched);
        emit m_scrim->pressed(global);
        if (!guard || !receiver)
            return consume || !receiver;
        if (!m_inputActive || !m_overlay || !m_overlay->isVisible())
            return consume;
    }
    if (press && m_overlay->isEnabled() && (m_closePolicy & 1)) {
        const QPointer<QObject> receiver(watched);
        emit dismissRequested(DismissReason::OutsidePress);
        // Closing observers may delete the receiver as well as the overlay.
        // zh_CN: 关闭观察者可能同时析构接收对象和浮层。
        return consume || !receiver;
    }
    return m_modal;
}

bool OverlayCoordinator::eventFilter(QObject* watched, QEvent* event)
{
    if (!event)
        return false;
    if (watched != m_topLevel)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::Destroy) {
        m_scrim = nullptr;
        m_topLevel = nullptr;
        emit hostDestroyed();
        return false;
    }

    if (event->type() == QEvent::Resize) {
        syncScrimGeometry();
        raiseStack();
        emit hostGeometryChanged();
    }
    return false;
}

} // namespace fluent::overlay
