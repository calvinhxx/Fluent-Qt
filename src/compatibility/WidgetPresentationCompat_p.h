#pragma once

#include <QTransform>

class QWidget;

namespace fluent::compat {

// QGraphicsProxyWidget roots are QWidget windows, but not native overlay hosts.
// Keep that Qt-specific boundary out of component placement and Gallery code.
// zh_CN: 代理根是 QWidget 窗口，却不是原生浮层宿主；将此 Qt 边界封装在兼容层。
struct WidgetPresentationBridge {
    const QWidget* viewport = nullptr;
    QTransform rootToViewport;
    bool visible = false;
};

WidgetPresentationBridge widgetPresentationBridge(const QWidget* source);
QWidget* widgetPresentationWindow(const QWidget* source);
void prepareNativeWidgetPopup(QWidget* popup);

} // namespace fluent::compat
