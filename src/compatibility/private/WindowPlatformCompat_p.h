#pragma once

#include "compatibility/WindowChromeCompat.h"
#include <QGuiApplication>
#include <QWindow>

namespace compatibility::detail {

enum class NativeChromeRepair { None, WindowFlags, TitleBarInsets };

struct WindowPlatformCapabilities {
    bool keepClientSideFrameHost = false;
    NativeChromeRepair chromeRepair = NativeChromeRepair::None;
    bool recomposeOnEffectChange = true;
    bool materialHostChangesWithWindowState = false;
    bool titleBarLayoutChangesOnActivation = false;
};

inline WindowPlatformCapabilities windowPlatformCapabilities()
{
    WindowPlatformCapabilities result;
    switch (WindowChromeCompat::currentPlatform()) {
    case WindowChromeCompat::Platform::Windows:
        // Reapply flags after Qt rewrites Win32 styles; avoid repeating first-show activation.
        result.chromeRepair = NativeChromeRepair::WindowFlags;
        result.recomposeOnEffectChange = false;
        break;
    case WindowChromeCompat::Platform::MacOS:
        // Cocoa can recreate vibrancy hosts and defer native button layout.
        result.chromeRepair = NativeChromeRepair::TitleBarInsets;
        result.materialHostChangesWithWindowState = true;
        result.titleBarLayoutChangesOnActivation = true;
        break;
    case WindowChromeCompat::Platform::Linux:
        result.keepClientSideFrameHost = true;
        break;
    case WindowChromeCompat::Platform::Other:
        break;
    }
    return result;
}

inline void requestQtForegroundActivation(QWidget* window)
{
    // Older Wayland backends warn on requestActivate; focus needs an XDG activation token.
    // The native backend still receives the caller's activation request.
    // zh_CN: 旧版 Wayland 的 requestActivate 会警告；焦点需 XDG token，调用方仍请求原生激活。
    if (QGuiApplication::platformName().startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive))
        return;
    window->raise();
    window->activateWindow();
    if (QWindow* handle = window->windowHandle())
        handle->requestActivate();
}

inline void prepareWindowNormalGeometry(QWidget* window, const QRect& geometry)
{
    if (!window)
        return;
    window->setGeometry(geometry);
    // Create an unmapped surface and resubmit geometry so Qt 5.15/6.2 xcb restores to normal size instead of staying maximized.
    // zh_CN: 提前创建未显示的表面并重新提交几何，避免 Qt 5.15/6.2 xcb 还原时停留在最大化尺寸（Qt 6.7+ 仅在尺寸未生效时重复请求）。
    window->winId();
    if (QWindow* handle = window->windowHandle()) {
        handle->setGeometry(geometry);
        handle->resize(geometry.size());
    }
}

} // namespace compatibility::detail
