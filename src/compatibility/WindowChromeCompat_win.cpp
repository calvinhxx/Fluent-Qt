#include "WindowChromeCompat.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <QWindow>

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
    // DWM still needs caption/thick-frame styles even when Qt paints the visible chrome.
    // zh_CN: 即使可见 chrome 由 Qt 绘制，DWM 仍需要 caption/thick-frame 样式来保留系统行为。
    desiredStyle |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

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

bool handleCustomChromeMinMaxInfo(MSG* msg, FluentNativeEventResult* result) {
    if (!msg || !result || msg->message != WM_GETMINMAXINFO)
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

    *result = 0;
    return true;
}

} // namespace

namespace detail {

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

    if (handleCustomChromeMinMaxInfo(msg, result))
        return true;

    if (handleCustomChromeClientRect(msg, result))
        return true;

    if (handleDwmFrameMessage(msg, result))
        return true;

    if (msg->message != WM_NCHITTEST)
        return false;

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

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (!options.useCustomWindowChrome)
        return;

    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return;

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
