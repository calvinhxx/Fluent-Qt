#pragma once

#include <QApplication>
#include <QMenuBar>
#include <QGuiApplication>
#include <QWidget>

namespace compatibility::detail {

inline void clearWidgetTreeFocus(QWidget* root)
{
    // Clear focus while derived widgets still have intact vtables so platform input
    // contexts (e.g. WASM QWasmInputContext) do not dispatch events into half-destroyed children.
    // zh_CN: 在派生控件虚表仍完整时提前清除焦点，防止平台输入上下文在子控件半析构阶段派发事件。
    if (!root || !qApp)
        return;
    if (QWidget* focused = QApplication::focusWidget(); focused && (focused == root || root->isAncestorOf(focused))) {
        focused->clearFocus();
    }
}

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

inline void hideMenuBarExtensionButton(QMenuBar* menuBar)
{
    if (!menuBar)
        return;
    // FluentMenuBar computes its own compact item rects and paints all actions directly;
    // keep QMenuBar's internal QStyle-measured "»" extension button hidden so it never
    // overlaps custom-painted trailing items when resized to FluentMenuBar::sizeHint().
    // zh_CN: FluentMenuBar 使用自定义紧凑布局与绘制，隐藏 QMenuBar 内置扩展按钮以避免覆盖末尾菜单项。
    if (auto* extension = menuBar->findChild<QWidget*>(QStringLiteral("qt_menubar_ext_button"),
            Qt::FindDirectChildrenOnly)) {
        if (!extension->isHidden())
            extension->hide();
    }
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
