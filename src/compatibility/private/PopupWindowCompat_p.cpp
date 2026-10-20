#include "PopupWindowCompat_p.h"

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QWindow>

#include "compatibility/InputEventCompat.h"

namespace compatibility::detail {

PopupWindowHost::PopupWindowHost(QWidget* popup)
    : m_window(popup ? popup->windowHandle() : nullptr),
      m_previousParent(m_window ? m_window->transientParent() : nullptr)
{
    // A root QMenu may select its native QWidget owner during Show, before
    // attach(). Only that known temporary parent is eligible for cancellation.
    // zh_CN: 根菜单可能在 Show 阶段选用原生 QWidget 父级；取消仅还原这个已知的临时父级。
    auto* parent = popup ? popup->nativeParentWidget() : nullptr;
    m_openingParent = parent && parent->windowHandle() ? parent->windowHandle() : m_previousParent.data();
}

void PopupWindowHost::attach(QWidget* popup, QWidget* host)
{
    QWindow* window = popup ? popup->windowHandle() : nullptr;
    if (!window || !host)
        return;
    // QMenu may create its window while opening and choose a native parent.
    // Keep that first parent instead of the empty pre-open snapshot.
    // zh_CN: QMenu 可能在打开时创建窗口并选择原生父级，保留该父级而非打开前的空记录。
    if (m_window != window)
        m_previousParent = window->transientParent();
    m_window = window;
    m_host = host->windowHandle();
    m_attached = true;
    // Publish the restoration state before a synchronous parent-change callback.
    // Do not access this object after the call; the component may be destroyed.
    // zh_CN: 在同步父级变化回调前记录还原状态；调用后不再访问可能已销毁的适配器。
    window->setTransientParent(m_host);
}

void PopupWindowHost::restore()
{
    const auto window = m_window;
    const auto host = m_host;
    const auto previousParent = m_previousParent;
    const auto openingParent = m_openingParent;
    const bool attached = m_attached;
    *this = {};
    // A caller's deliberate reassignment supersedes this invocation's binding.
    // zh_CN: 调用方主动修改父级后，保留其新值。
    if (window && window->transientParent() != previousParent
        && window->transientParent() == (attached ? host : openingParent))
        window->setTransientParent(previousParent);
}

QWidget* widgetForEventReceiver(QObject* receiver)
{
    for (auto* node = receiver; node; node = node->parent()) {
        if (auto* widget = qobject_cast<QWidget*>(node))
            return widget;
    }
    return nullptr;
}

bool eventReceiverBelongsToWindow(QObject* receiver, QWidget* host)
{
    if (!host)
        return false;
    if (auto* native = qobject_cast<QWindow*>(receiver))
        return native == host->window()->windowHandle();
    auto* widget = widgetForEventReceiver(receiver);
    return widget && widget->window() == host->window();
}

bool popupGrabPressBelongsToWindow(QObject* receiver, QEvent* event, QWidget* host)
{
    if (!host || !event || (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick))
        return false;
    auto* native = qobject_cast<QWindow*>(receiver);
    if (!native) {
        auto* widget = widgetForEventReceiver(receiver);
        native = widget ? widget->window()->windowHandle() : nullptr;
    }
    QWindow* owner = host->window()->windowHandle();
    const QPoint global = fluentMouseGlobalPos(static_cast<QMouseEvent*>(event));
    if (!host->rect().contains(host->mapFromGlobal(global)))
        return false;
    // A native popup grab may not replay the outside press to the host. Reject
    // other windows and points inside any popup before accepting that host.
    // zh_CN: 原生抓取可能不重发外侧按下；先排除其他窗口及链内各弹窗的范围，再归属宿主。
    while (native && native != owner) {
        if (native->type() != Qt::Popup || native->frameGeometry().contains(global))
            return false;
        native = native->transientParent();
    }
    return native && native == owner;
}

bool popupContainsEvent(QWidget* popup, QObject* receiver, QEvent* event)
{
    if (!popup || !receiver || !event)
        return false;
    // Popup grabs can target the popup even when the pointer is outside it.
    // zh_CN: 原生抓取可将弹窗外的指针事件送到弹窗，按实际屏幕范围判断。
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
        return popup->frameGeometry().contains(fluentMouseGlobalPos(static_cast<QMouseEvent*>(event)));
    auto* widget = qobject_cast<QWidget*>(receiver);
    return (widget && widget->window() == popup) || receiver == popup->windowHandle();
}

} // namespace compatibility::detail
