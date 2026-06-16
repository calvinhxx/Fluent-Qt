#include "WindowChromeCompat.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <QWindow>

// DWM system-backdrop / dark-mode attributes (defined here so we don't depend on a
// recent Windows SDK header). zh_CN: DWM 系统背景/暗色模式属性常量（在此自定义，避免依赖较新的 SDK 头）。
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2  // Mica
#endif

namespace compatibility {
namespace {

// Store the extended-client-area top margin on the HWND so native event
// handling can keep DWM frame geometry synchronized after Qt recreates state.
// zh_CN: 将扩展客户区的顶部 margin 存到 HWND 上，便于 Qt 重建状态后继续同步 DWM frame 几何。
constexpr wchar_t ChromeTopMarginProperty[] = L"FluentWindowChromeTopMargin";

HWND hwndForWindow(QWidget* window) {
    if (!window || !window->windowHandle())
        return nullptr;

    return reinterpret_cast<HWND>(window->windowHandle()->winId());
}

int storedChromeTopMargin(HWND hwnd) {
    if (!hwnd)
        return -1;

    const auto value = reinterpret_cast<INT_PTR>(GetPropW(hwnd, ChromeTopMarginProperty));
    return value > 0 ? static_cast<int>(value - 1) : -1;
}

void storeChromeTopMargin(HWND hwnd, int topMargin) {
    if (!hwnd)
        return;

    SetPropW(hwnd,
             ChromeTopMarginProperty,
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(qMax(0, topMargin) + 1)));
}

void clearChromeTopMargin(HWND hwnd) {
    if (hwnd)
        RemovePropW(hwnd, ChromeTopMarginProperty);
}

template <typename T>
T resolveDwmProc(const char* name) {
    static HMODULE dwmApi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmApi)
        return nullptr;

    return reinterpret_cast<T>(GetProcAddress(dwmApi, name));
}

void ensureDwmChromeStyle(QWidget* window) {
    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return;

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR desiredStyle = style;
    // DWM still needs thick-frame/sysmenu styles even when Qt paints the visible chrome
    // (resize, Win11 rounded corners, taskbar/system menu). zh_CN: 即使可见 chrome 由 Qt 绘制，
    // DWM 仍需要 thick-frame/sysmenu 样式（缩放、Win11 圆角、任务栏/系统菜单）。
    desiredStyle |= WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    // Under the Mica "sheet of glass", a WS_CAPTION would make DWM render its OWN caption
    // buttons (min/max/close) top-aligned and frame-inset in the top-right glass — duplicating
    // and clashing with our centered, edge-flush Fluent buttons. Drop WS_CAPTION there; the
    // glass still carries the backdrop and WS_THICKFRAME keeps resize + rounded corners.
    // zh_CN: Mica「玻璃」下若保留 WS_CAPTION，DWM 会在右上角玻璃区自绘顶部对齐、内缩的标题栏按钮，
    // 与我们居中贴边的 Fluent 按钮重复冲突。此处去掉 WS_CAPTION；玻璃仍承载背景，WS_THICKFRAME 保留缩放与圆角。
    if (window->property("fluentMicaBackdrop").toBool())
        desiredStyle &= ~WS_CAPTION;
    else
        desiredStyle |= WS_CAPTION;

    if (desiredStyle == style)
        return;

    SetWindowLongPtrW(hwnd, GWL_STYLE, desiredStyle);
    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

bool monitorInfoForWindow(HWND hwnd, MONITORINFO* monitorInfo) {
    if (!hwnd || !monitorInfo)
        return false;

    monitorInfo->cbSize = sizeof(MONITORINFO);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    return monitor && GetMonitorInfoW(monitor, monitorInfo);
}

void extendFrameIntoClientArea(HWND hwnd, int topMargin) {
    using DwmExtendFrameIntoClientAreaFn = HRESULT(WINAPI*)(HWND, const MARGINS*);
    auto* extendFrame = resolveDwmProc<DwmExtendFrameIntoClientAreaFn>(
        "DwmExtendFrameIntoClientArea");
    if (!hwnd || !extendFrame)
        return;

    const MARGINS margins = {0, 0, qMax(0, topMargin), 0};
    extendFrame(hwnd, &margins);
}

bool handleDwmFrameMessage(MSG* msg, FluentNativeEventResult* result) {
    using DwmDefWindowProcFn = BOOL(WINAPI*)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
    auto* dwmDefWindowProc = resolveDwmProc<DwmDefWindowProcFn>("DwmDefWindowProc");
    if (!msg || !result || !msg->hwnd || !dwmDefWindowProc)
        return false;

    if (msg->message == WM_NCHITTEST)
        return false;

    LRESULT dwmResult = 0;
    if (!dwmDefWindowProc(msg->hwnd, msg->message, msg->wParam, msg->lParam, &dwmResult))
        return false;

    *result = static_cast<FluentNativeEventResult>(dwmResult);
    return true;
}

int toNativeHitTest(WindowChromeCompat::HitTest hitTest) {
    switch (hitTest) {
    case WindowChromeCompat::HitTest::Caption: return HTCAPTION;
    case WindowChromeCompat::HitTest::Left: return HTLEFT;
    case WindowChromeCompat::HitTest::Right: return HTRIGHT;
    case WindowChromeCompat::HitTest::Top: return HTTOP;
    case WindowChromeCompat::HitTest::Bottom: return HTBOTTOM;
    case WindowChromeCompat::HitTest::TopLeft: return HTTOPLEFT;
    case WindowChromeCompat::HitTest::TopRight: return HTTOPRIGHT;
    case WindowChromeCompat::HitTest::BottomLeft: return HTBOTTOMLEFT;
    case WindowChromeCompat::HitTest::BottomRight: return HTBOTTOMRIGHT;
    case WindowChromeCompat::HitTest::Client:
    default:
        return HTCLIENT;
    }
}

bool handleCustomChromeClientRect(MSG* msg, FluentNativeEventResult* result) {
    if (!msg || !result || msg->message != WM_NCCALCSIZE)
        return false;

    HWND hwnd = msg->hwnd;
    if (!hwnd)
        return false;

    if (msg->wParam != FALSE) {
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
        if (!params)
            return false;

        if (IsZoomed(hwnd)) {
            MONITORINFO monitorInfo;
            if (!monitorInfoForWindow(hwnd, &monitorInfo))
                return false;

            // Use the monitor work area so maximized custom chrome avoids the taskbar.
            // zh_CN: 最大化自定义 chrome 时使用 monitor work area，避免覆盖任务栏。
            params->rgrc[0] = monitorInfo.rcWork;
        }
    }

    *result = 0;
    return true;
}

bool handleCustomChromeMinMaxInfo(QWidget* window, MSG* msg, FluentNativeEventResult* result) {
    if (!window || !msg || !result || msg->message != WM_GETMINMAXINFO)
        return false;

    HWND hwnd = msg->hwnd;
    if (!hwnd)
        return false;

    auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(msg->lParam);
    if (!minMaxInfo)
        return false;

    MONITORINFO monitorInfo;
    if (!monitorInfoForWindow(hwnd, &monitorInfo))
        return false;

    const RECT& work = monitorInfo.rcWork;
    const RECT& monitor = monitorInfo.rcMonitor;
    minMaxInfo->ptMaxPosition.x = work.left - monitor.left;
    minMaxInfo->ptMaxPosition.y = work.top - monitor.top;
    minMaxInfo->ptMaxSize.x = work.right - work.left;
    minMaxInfo->ptMaxSize.y = work.bottom - work.top;

    // We intercept WM_GETMINMAXINFO and return handled, which bypasses Qt's own
    // minimum-size enforcement (Qt applies setMinimumSize through ptMinTrackSize here).
    // Re-apply it, or the frameless window can be dragged down to almost nothing. The
    // NCCALCSIZE handler makes the client fill the whole window, so the logical minimum
    // (scaled to physical pixels) maps directly onto the window track size.
    // zh_CN: 我们拦截了 WM_GETMINMAXINFO 并返回已处理，绕过了 Qt 自身经 ptMinTrackSize 施加的最小尺寸
    // 约束（setMinimumSize 就由它生效）。在此重新施加，否则无边框窗口能被拖到几乎为零。NCCALCSIZE
    // 让客户区铺满整窗，故逻辑最小尺寸（换算到物理像素）可直接作为窗口轨迹尺寸。
    const QSize minimumDip = window->minimumSize();
    if (minimumDip.width() > 0 || minimumDip.height() > 0) {
        const qreal dpr = window->devicePixelRatioF();
        if (minimumDip.width() > 0)
            minMaxInfo->ptMinTrackSize.x = qRound(minimumDip.width() * dpr);
        if (minimumDip.height() > 0)
            minMaxInfo->ptMinTrackSize.y = qRound(minimumDip.height() * dpr);
    }

    *result = 0;
    return true;
}

} // namespace

namespace detail {

bool platformSupportsSystemBackdrop() {
    // Mica via DWMWA_SYSTEMBACKDROP_TYPE needs Windows 11 22H2 (build 22621+). RtlGetVersion
    // reports the true build (GetVersionEx lies without a manifest).
    // zh_CN: DWMWA_SYSTEMBACKDROP_TYPE 的 Mica 需要 Windows 11 22H2（build 22621+）。RtlGetVersion
    // 返回真实 build（无清单时 GetVersionEx 会谎报）。
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto* rtlGetVersion = ntdll
        ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;
    if (!rtlGetVersion)
        return false;
    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) != 0)
        return false;
    return info.dwMajorVersion > 10
        || (info.dwMajorVersion == 10 && info.dwBuildNumber >= 22621);
}

bool applyPlatformSystemBackdrop(QWidget* window, bool dark) {
    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return false;
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto* setAttr = resolveDwmProc<DwmSetWindowAttributeFn>("DwmSetWindowAttribute");
    if (!setAttr)
        return false;

    // Match the DWM-managed bits (frame/caption) to the theme, then request the Mica backdrop.
    // zh_CN: 先让 DWM 管理的部分（frame/caption）匹配主题，再请求 Mica 背景。
    const BOOL darkMode = dark ? TRUE : FALSE;
    setAttr(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    const int backdropType = DWMSBT_MAINWINDOW;
    const HRESULT hr = setAttr(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

    // Nudge DWM to recompute the frame so the backdrop composites right away.
    // zh_CN: 触发 DWM 重新计算 frame。
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // On first show DWM intermittently leaves a flat default glass instead of the real,
    // wallpaper-tinted Mica material, and only an activation round-trip paints the material in.
    // Re-setting the same backdrop value is a no-op, and SWP_FRAMECHANGED alone is unreliable, so
    // replicate exactly what the user's "switch to another app and back" does: send the active
    // window an NCACTIVATE deactivate→activate pair. DwmDefWindowProc (already wired in our native
    // event handler) services these and re-composites the backdrop, without changing real focus.
    // zh_CN: 首屏时 DWM 会间歇性地留下一层扁平默认玻璃，而非真正带壁纸着色的 Mica 材质，只有一次激活往返才会
    // 把材质绘入。重设相同的背景值是空操作，单靠 SWP_FRAMECHANGED 也不可靠，于是精确复现用户"切到别的 app 再切
    // 回来"的动作：给活动窗口发一对 NCACTIVATE 失活→激活消息。DwmDefWindowProc（已接入我们的原生事件处理）会处理
    // 它们并重新合成背景，且不改变真实焦点。
    if (GetActiveWindow() == hwnd) {
        SendMessageW(hwnd, WM_NCACTIVATE, FALSE, 0);
        SendMessageW(hwnd, WM_NCACTIVATE, TRUE, 0);
    }
    return SUCCEEDED(hr);
}

// Forward declaration — full definition follows handlePlatformNativeEvent.
// zh_CN: 前向声明，完整定义在 handlePlatformNativeEvent 之后。
bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos);

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options) {
    if (!window)
        return;

    if (options.useCustomWindowChrome) {
        window->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
        const Qt::WindowFlags desiredFlags =
            (window->windowFlags() | Qt::Window | Qt::ExpandedClientAreaHint |
             Qt::NoTitleBarBackgroundHint | Qt::CustomizeWindowHint)
            & ~(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint |
                Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint |
                Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
        if (!window->isVisible() && window->windowFlags() != desiredFlags)
            window->setWindowFlags(desiredFlags);
        ensureDwmChromeStyle(window);
    }
}

bool handlePlatformNativeEvent(QWidget* window,
                               const WindowChromeOptions& options,
                               const QByteArray& eventType,
                               void* message,
                               FluentNativeEventResult* result) {
    if (!window || !message || !result || !options.useCustomWindowChrome)
        return false;

    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;

    MSG* msg = static_cast<MSG*>(message);
    if (!msg)
        return false;

    if (msg->message == WM_NCDESTROY) {
        clearChromeTopMargin(msg->hwnd);
    }

    // Handle right-click on the caption area: show the system menu on button-up
    // (matching DefWindowProc behavior which shows system menu on WM_NCRBUTTONUP).
    // This must be done explicitly because DwmDefWindowProc may consume the message
    // before DefWindowProc gets a chance to show the menu.
    // zh_CN: 在 caption 区域处理右键松开：显示系统菜单（与 DefWindowProc 在 WM_NCRBUTTONUP
    // zh_CN: 上的行为一致）。由于 DwmDefWindowProc 可能先消费该消息，需在此显式处理。
    if (msg->message == WM_NCRBUTTONUP) {
        const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        const QPoint localPos = window->mapFromGlobal(globalPos);
        const auto hit = WindowChromeCompat::classifyHitTest(options, window->size(), localPos);
        if (hit == WindowChromeCompat::HitTest::Caption) {
            showPlatformSystemMenu(window, globalPos);
            *result = 0;
            return true;
        }
    }

    if (handleCustomChromeMinMaxInfo(window, msg, result))
        return true;

    if (handleCustomChromeClientRect(msg, result))
        return true;

    if (handleDwmFrameMessage(msg, result))
        return true;

    if (msg->message != WM_NCHITTEST)
        return false;

    // Our own Fluent caption buttons live in the client area (added to the title bar's drag
    // exclusions), so classifyHitTest returns HTCLIENT over them and they receive clicks
    // normally — no DWM button hit-testing needed (the native glass buttons are suppressed).
    // zh_CN: 自绘 Fluent 标题栏按钮位于客户区（已加入标题栏拖拽排除区），classifyHitTest 在其上返回
    // HTCLIENT，按钮正常收到点击——无需 DWM 按钮命中测试（原生玻璃按钮已抑制）。
    const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
    const QPoint localPos = window->mapFromGlobal(globalPos);
    // Convert the repository-level hit-test result to the native HT* code expected by Windows.
    // zh_CN: 将项目内逻辑命中结果转换为 Windows 期望的原生 HT* code。
    const auto hitTest = WindowChromeCompat::classifyHitTest(options, window->size(), localPos);

    *result = static_cast<FluentNativeEventResult>(toNativeHitTest(hitTest));
    return true;
}

bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos) {
    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return false;

    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(globalPos.x(), globalPos.y()));
    return true;
}

bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos) {
    if (!window || edges == Qt::Edges())
        return false;

    int nativeEdge = HTCLIENT;
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);

    if (top && left) nativeEdge = HTTOPLEFT;
    else if (top && right) nativeEdge = HTTOPRIGHT;
    else if (bottom && left) nativeEdge = HTBOTTOMLEFT;
    else if (bottom && right) nativeEdge = HTBOTTOMRIGHT;
    else if (left) nativeEdge = HTLEFT;
    else if (right) nativeEdge = HTRIGHT;
    else if (top) nativeEdge = HTTOP;
    else if (bottom) nativeEdge = HTBOTTOM;

    if (nativeEdge == HTCLIENT)
        return false;

    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return false;

    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, nativeEdge, MAKELPARAM(globalPos.x(), globalPos.y()));
    return true;
}

bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options) {
    if (!options.useCustomWindowChrome)
        return false;

    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return false;

    SendMessageW(hwnd, WM_NCLBUTTONDBLCLK, HTCAPTION, 0);
    return true;
}

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos) {
    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return false;

    HMENU systemMenu = GetSystemMenu(hwnd, FALSE);
    if (!systemMenu)
        return false;

    // Update menu item states to reflect current window state.
    // zh_CN: 根据当前窗口状态更新菜单项的启用/禁用状态。
    const bool isMaximized = IsZoomed(hwnd) != FALSE;
    const bool isMinimized = IsIconic(hwnd) != FALSE;
    const bool canResize = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_THICKFRAME) != 0;

    EnableMenuItem(systemMenu, SC_RESTORE,  MF_BYCOMMAND | ((isMaximized || isMinimized) ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_MOVE,     MF_BYCOMMAND | (isMaximized ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(systemMenu, SC_SIZE,     MF_BYCOMMAND | (!isMaximized && canResize ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(systemMenu, SC_MINIMIZE, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(systemMenu, SC_MAXIMIZE, MF_BYCOMMAND | (isMaximized ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(systemMenu, SC_CLOSE,    MF_BYCOMMAND | MF_ENABLED);

    SetForegroundWindow(hwnd);
    const UINT cmd = TrackPopupMenu(systemMenu,
                                    TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
                                    globalPos.x(), globalPos.y(),
                                    0, hwnd, nullptr);
    if (cmd != 0)
        PostMessageW(hwnd, WM_SYSCOMMAND, static_cast<WPARAM>(cmd), 0);

    return true;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (!options.useCustomWindowChrome)
        return;

    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return;

    // With the Mica backdrop the whole client area must be a "sheet of glass" so the DWM
    // backdrop fills the window; otherwise the translucent (transparent) regions render
    // black instead of showing Mica. zh_CN: 启用 Mica 时整个客户区要做成「玻璃」，让 DWM 背景填满
    // 窗口；否则半透明（透明）区域会渲染成黑色而非露出 Mica。
    if (window && window->property("fluentMicaBackdrop").toBool()) {
        using DwmExtendFrameFn = HRESULT(WINAPI*)(HWND, const MARGINS*);
        if (auto* extendFrame = resolveDwmProc<DwmExtendFrameFn>("DwmExtendFrameIntoClientArea")) {
            const MARGINS sheetOfGlass = {-1, -1, -1, -1};
            extendFrame(hwnd, &sheetOfGlass);
        }
        return;
    }

    const int topMargin = qMax(0, options.titleBarRect.height());
    extendFrameIntoClientArea(hwnd, topMargin);

    if (storedChromeTopMargin(hwnd) == topMargin)
        return;

    storeChromeTopMargin(hwnd, topMargin);
    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

int nativeTitleBarLeadingInset(QWidget* window) {
    Q_UNUSED(window);
    return 0;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_WIN
