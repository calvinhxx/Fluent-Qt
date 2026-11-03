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
    // Create an unmapped surface before asking Qt to retain a maximize/restore
    // target. On Qt 5.15/6.2 xcb, a surface created while hidden restores to its
    // maximized size unless its native geometry is submitted again. Before Qt
    // 6.7, QWindow::resize does that even for an unchanged size; Qt 6.7+ returns
    // early then, so the call only repeats a size the platform did not apply.
    // zh_CN: 先创建未显示的表面，再保存最大化/还原目标。Qt 5.15/6.2 xcb 上，隐藏时创建的表面若不重新提交原生几何，
    // 还原后仍是最大化尺寸。Qt 6.7 之前 resize 即使尺寸未变也会重新提交；Qt 6.7+ 此时直接返回，
    // 仅在平台未采用该尺寸时重复请求。
    window->winId();
    if (QWindow* handle = window->windowHandle()) {
        handle->setGeometry(geometry);
        handle->resize(geometry.size());
    }
}

} // namespace compatibility::detail
