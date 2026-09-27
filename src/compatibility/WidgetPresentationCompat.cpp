#include "WidgetPresentationCompat_p.h"

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointer>
#include <QWidget>

namespace fluent::compat {

WidgetPresentationBridge widgetPresentationBridge(const QWidget* source)
{
    const auto* proxy = source ? source->window()->graphicsProxyWidget() : nullptr;
    if (!proxy || !proxy->scene())
        return {};
    const auto views = proxy->scene()->views();
    QGraphicsView* host = nullptr;
    for (QGraphicsView* view : views) {
        if (!host || (!host->isVisible() && view->isVisible()))
            host = view;
        if (view->isVisible() && view->isActiveWindow()) {
            host = view;
            break;
        }
    }
    if (!host)
        return {};
    return {host->viewport(), proxy->deviceTransform(host->viewportTransform()),
            proxy->isVisible()};
}

QWidget* widgetPresentationWindow(const QWidget* source)
{
    // A widget cannot legitimately be embedded into itself. Bound malformed
    // custom scene graphs without retaining widgets or allocating per frame.
    // zh_CN: 控件不能嵌入自身；限制异常场景环，不持有控件也不逐帧分配缓存。
    for (int depth = 0; source && depth < 32; ++depth) {
        const auto bridge = widgetPresentationBridge(source);
        if (!bridge.viewport)
            return source->window();
        source = bridge.viewport;
    }
    return nullptr;
}

void prepareNativeWidgetPopup(QWidget* popup)
{
    // Otherwise Qt silently embeds a popup owned by a proxy widget back into
    // that scene, although its position is already in native screen coordinates.
    // zh_CN: 否则 Qt 会将代理控件的弹窗再次嵌入场景，错误解释已经映射的屏幕坐标。
    if (!popup)
        return;
    const QPointer<QWidget> guard(popup);
    if (!popup->windowFlags().testFlag(Qt::BypassGraphicsProxyWidget))
        popup->setWindowFlag(Qt::BypassGraphicsProxyWidget);
    // QMenu and setWindowFlags may already have created a child proxy during
    // construction. Setting the bypass flag alone does not remove that proxy.
    // Detach its borrowed widget before deleting the now-empty proxy.
    // zh_CN: 构造时可能已创建子代理；仅加标志不会移除。先解除共享控件再删除空代理。
    if (guard) {
        if (QPointer<QGraphicsProxyWidget> proxy = popup->graphicsProxyWidget()) {
            proxy->setWidget(nullptr);
            delete proxy.data();
        }
    }
}

} // namespace fluent::compat
