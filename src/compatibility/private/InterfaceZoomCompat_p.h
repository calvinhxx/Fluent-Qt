#pragma once

// Application-only adapter; never included by the Widgets core or installed SDK.
// zh_CN: 仅供应用的适配器，不由 Widgets 核心或安装后的 SDK 引入。
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QVector>
#include <QWidget>
#include <QWindow>

#include <QtGui/private/qhighdpiscaling_p.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

namespace compatibility::detail {

inline QtMessageHandler& forwardedInterfaceZoomMessageHandler()
{
    static QtMessageHandler handler = nullptr;
    return handler;
}

inline void dropInterfaceZoomFactorWarning(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && message.startsWith(QLatin1String("QHighDpiScaling::setFactor")))
        return;
    forwardedInterfaceZoomMessageHandler()(type, context, message);
}

inline void applyInterfaceZoomFactor(qreal factor)
{
    // Qt warns because setGlobalFactor() does not notify existing windows;
    // the loop below delivers those notifications itself.
    // zh_CN: Qt 的警告源于 setGlobalFactor() 不通知已有窗口；下方循环会自行补发。
    forwardedInterfaceZoomMessageHandler() = qInstallMessageHandler(dropInterfaceZoomFactorWarning);
    QHighDpiScaling::setGlobalFactor(factor);
    qInstallMessageHandler(forwardedInterfaceZoomMessageHandler());
    QVector<QPointer<QWindow>> windows;
    for (QWindow* window : QGuiApplication::topLevelWindows())
        windows.append(window);
    for (const QPointer<QWindow>& window : windows) {
        if (!window || !window->handle())
            continue;
#ifdef FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT
        QWindowSystemInterface::handleWindowDevicePixelRatioChanged<QWindowSystemInterface::SynchronousDelivery>(
            window);
#endif
        if (!window || !window->handle())
            continue;
        QWindowSystemInterface::handleGeometryChange<QWindowSystemInterface::SynchronousDelivery>(window,
            window->handle()->geometry());
        if (window && window->handle())
            window->handle()->propagateSizeHints();
    }
#ifndef FLUENT_QT_INTERFACE_ZOOM_HAS_DPR_EVENT
    // Earlier Qt refreshes backing-store DPR during painting. Notify widget caches
    // even though the physical screen is unchanged; guard deletion during delivery.
    // zh_CN: 旧版 Qt 在绘制时刷新缓冲区 DPR。物理屏幕未变时仍需通知控件缓存，
    // 并防护事件投递期间的控件销毁。
    QVector<QPointer<QWidget>> widgets;
    for (QWidget* widget : QApplication::allWidgets())
        widgets.append(widget);
    for (const QPointer<QWidget>& widget : widgets) {
        if (!widget)
            continue;
        QEvent event(QEvent::ScreenChangeInternal);
        QCoreApplication::sendEvent(widget, &event);
        if (widget)
            widget->update();
    }
#endif
}
} // namespace compatibility::detail
