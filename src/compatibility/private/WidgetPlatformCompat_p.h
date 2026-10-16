#pragma once

#include <QMenuBar>
#include <QGuiApplication>
#include <QWidget>

namespace compatibility::detail {

inline bool widgetWindowRaiseSupported()
{
    return QGuiApplication::platformName() != QLatin1String("offscreen");
}

inline bool popupWindowStackingSupported()
{
    return !qEnvironmentVariableIsSet("SKIP_VISUAL_TEST") && widgetWindowRaiseSupported()
        && QGuiApplication::platformName() != QLatin1String("minimal");
}

inline void disableNativeFocusRing(QWidget* widget)
{
#ifdef Q_OS_MAC
    widget->setAttribute(Qt::WA_MacShowFocusRect, false);
#else
    Q_UNUSED(widget);
#endif
}

inline void useWidgetMenuBar(QMenuBar* menuBar)
{
#ifdef Q_OS_MAC
    menuBar->setNativeMenuBar(false);
#else
    Q_UNUSED(menuBar);
#endif
}

// Cocoa supplies native pinch events; registering the Qt gesture duplicates them.
// zh_CN: Cocoa 提供原生捏合事件，额外注册 Qt 手势会导致重复处理。
constexpr bool usesNativePinchGestures()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

inline void registerPinchGesture(QWidget* widget)
{
    if (!usesNativePinchGestures())
        widget->grabGesture(Qt::PinchGesture);
}

inline void enableEmbeddedSceneTouch(QWidget* widget)
{
#ifndef Q_OS_MACOS
    widget->setAttribute(Qt::WA_AcceptTouchEvents);
#else
    Q_UNUSED(widget);
#endif
}

} // namespace compatibility::detail
