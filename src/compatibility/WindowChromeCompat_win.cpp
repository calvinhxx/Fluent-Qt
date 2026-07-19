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
#ifndef DWMSBT_AUTO
#define DWMSBT_AUTO 0  // Let DWM decide (no explicit backdrop)
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1  // Explicitly disable the system backdrop
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2  // Mica
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3  // Acrylic
#endif

namespace compatibility {
namespace {

// Store the extended-client-area top margin on the HWND so native event
// handling can keep DWM frame geometry synchronized after Qt recreates state.
// zh_CN: 将扩展客户区的顶部 margin 存到 HWND 上，便于 Qt 重建状态后继续同步 DWM frame 几何。
constexpr wchar_t ChromeTopMarginProperty[] = L"FluentWindowChromeTopMargin";

struct WindowsVersionInfo {
    DWORD major = 0;
    DWORD minor = 0;
    DWORD build = 0;
    bool valid = false;
};

WindowsVersionInfo currentWindowsVersion() {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto* rtlGetVersion = ntdll
        ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;
    if (!rtlGetVersion)
        return {};

    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion(&info) != 0)
        return {};

    return {info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber, true};
}

bool supportsDwmSystemBackdrop(const WindowsVersionInfo& version) {
    return version.valid
        && (version.major > 10 || (version.major == 10 && version.build >= 22621));
}

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

    // Custom chrome owns the whole caption row. Keeping WS_CAPTION lets older DWM paths repaint a
    // native non-client strip at the right/bottom edges, which shows up as black borders or ghosted
    // glass beside overlay scroll bars. WS_THICKFRAME keeps resize behavior.
    // zh_CN: 自绘 chrome 接管整条标题栏。保留 WS_CAPTION 会让旧 DWM 路径在右/底边重绘原生非客户区，
    // 表现为滚动条旁的黑边或玻璃残影。WS_THICKFRAME 继续保留缩放行为。
    desiredStyle &= ~WS_CAPTION;

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

QPoint logicalClientPointForNativeScreenPoint(QWidget* window,
                                               HWND hwnd,
                                               const QPoint& nativeScreenPoint) {
    if (!window || !hwnd)
        return {};

    POINT nativeClientPoint = {nativeScreenPoint.x(), nativeScreenPoint.y()};
    RECT nativeClientRect = {};
    if (!ScreenToClient(hwnd, &nativeClientPoint)
        || !GetClientRect(hwnd, &nativeClientRect)) {
        return window->mapFromGlobal(nativeScreenPoint);
    }

    const int nativeWidth = nativeClientRect.right - nativeClientRect.left;
    const int nativeHeight = nativeClientRect.bottom - nativeClientRect.top;
    const QSize logicalSize = window->size();
    if (nativeWidth <= 0 || nativeHeight <= 0 || logicalSize.isEmpty())
        return window->mapFromGlobal(nativeScreenPoint);

    // WM_NCHITTEST supplies physical screen pixels, while QWidget geometry is
    // expressed in Qt logical pixels. QT_SCALE_FACTOR can make those spaces
    // differ even on a 100%-scaled Windows desktop. Map through the live native
    // client extent instead of feeding physical coordinates to mapFromGlobal().
    // zh_CN: WM_NCHITTEST 使用物理屏幕像素，而 QWidget 几何使用 Qt 逻辑像素；
    // QT_SCALE_FACTOR 即使在系统 100% 缩放下也会让两者不同。应通过当前原生客户区
    // 尺寸换算，不能把物理坐标直接交给 mapFromGlobal()。
    return QPoint(
        qFloor(static_cast<qreal>(nativeClientPoint.x) * logicalSize.width()
               / nativeWidth),
        qFloor(static_cast<qreal>(nativeClientPoint.y) * logicalSize.height()
               / nativeHeight));
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

BackdropCapabilities platformBackdropCapabilities() {
    BackdropCapabilities capabilities;
    const bool supported = supportsDwmSystemBackdrop(currentWindowsVersion());
    capabilities.alphaSurfaceSupported = supported;
    capabilities.nativeMica = supported;
    capabilities.nativeAcrylic = supported;
    capabilities.provider = supported ? QStringLiteral("dwm-system-backdrop")
                                      : QStringLiteral("painted-material");
    return capabilities;
}

bool platformSupportsSystemBackdrop() {
    const BackdropCapabilities capabilities = platformBackdropCapabilities();
    return capabilities.nativeMica || capabilities.nativeAcrylic;
}

BackdropApplyResult applyPlatformSystemBackdrop(QWidget* window,
                                                BackdropEffect effect,
                                                bool dark,
                                                bool forceRecomposite) {
    BackdropApplyResult result;
    HWND hwnd = hwndForWindow(window);
    if (!hwnd) {
        result.reason = QStringLiteral("native-window-unavailable");
        return result;
    }
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto* setAttr = resolveDwmProc<DwmSetWindowAttributeFn>("DwmSetWindowAttribute");
    const WindowsVersionInfo version = currentWindowsVersion();
    const bool dwmSystemBackdrop = supportsDwmSystemBackdrop(version);

    // Match the DWM-managed bits (frame/caption) to the theme, then request the chosen backdrop.
    // zh_CN: 先让 DWM 管理的部分（frame/caption）匹配主题，再请求所选背景。
    if (setAttr) {
        const BOOL darkMode = dark ? TRUE : FALSE;
        setAttr(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    }

    if (effect == BackdropEffect::Solid) {
        if (dwmSystemBackdrop && setAttr) {
            const int backdropType = DWMSBT_NONE;
            setAttr(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
        }
        result.applied = true;
        result.backend = fluent::windowing::BackdropBackend::Solid;
        result.fidelity = fluent::windowing::BackdropFidelity::Solid;
        result.surfaceMode = fluent::windowing::BackdropSurfaceMode::SolidOpaque;
        result.reason = QStringLiteral("solid-requested");
        return result;
    }

    if (!dwmSystemBackdrop || !setAttr) {
        result.reason = QStringLiteral("dwm-system-backdrop-unavailable");
        return result;
    }

    // Windows 10 legacy Acrylic is intentionally not used as a Fluent window backdrop: with Qt's
    // backing store it can leave transparent client regions black, especially in Win10 VMs.
    // Windows 10 therefore stays on the opaque app-painted fallback.
    // Mica and Acrylic share this exact plumbing — only the DWMWA_SYSTEMBACKDROP_TYPE value differs.
    // zh_CN: Mica 与 Acrylic 走完全相同的管线——仅 DWMWA_SYSTEMBACKDROP_TYPE 取值不同。
    int backdropType = DWMSBT_MAINWINDOW;
    switch (effect) {
    case BackdropEffect::Acrylic: backdropType = DWMSBT_TRANSIENTWINDOW; break;
    case BackdropEffect::Solid:   backdropType = DWMSBT_NONE;            break;
    case BackdropEffect::Mica:    backdropType = DWMSBT_MAINWINDOW;      break;
    }
    const HRESULT hr = setAttr(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));
    if (!SUCCEEDED(hr)) {
        result.reason = QStringLiteral("dwm-set-window-attribute-failed-%1")
                            .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
        return result;
    }

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
    // This is ONLY for the first-show race — it briefly flashes the caption active/inactive, so
    // runtime theme/effect changes (which are already composited) skip it to avoid a visible flicker.
    // zh_CN: 首屏时 DWM 会间歇性地留下一层扁平默认玻璃，而非真正带壁纸着色的 Mica 材质，只有一次激活往返才会
    // 把材质绘入。重设相同的背景值是空操作，单靠 SWP_FRAMECHANGED 也不可靠，于是精确复现用户"切到别的 app 再切
    // 回来"的动作：给活动窗口发一对 NCACTIVATE 失活→激活消息。DwmDefWindowProc（已接入）会处理它们并重新合成背景，
    // 且不改变真实焦点。此操作仅用于首屏竞争——它会让标题栏短暂闪一下激活/非激活，故运行时切主题/效果（已合成）跳过它避免可见闪烁。
    if (forceRecomposite && GetActiveWindow() == hwnd) {
        SendMessageW(hwnd, WM_NCACTIVATE, FALSE, 0);
        SendMessageW(hwnd, WM_NCACTIVATE, TRUE, 0);
    }
    result.applied = true;
    result.backend = fluent::windowing::BackdropBackend::DwmSystemBackdrop;
    result.fidelity = fluent::windowing::BackdropFidelity::Native;
    result.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    result.reason = QStringLiteral("dwm-system-backdrop-active");
    return result;
}

// Forward declaration — full definition follows handlePlatformNativeEvent.
// zh_CN: 前向声明，完整定义在 handlePlatformNativeEvent 之后。
bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos);

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options) {
    if (!window)
        return;

    if (options.useCustomWindowChrome) {
        window->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
        Qt::WindowFlags desiredFlags =
            window->windowFlags() | Qt::Window | Qt::CustomizeWindowHint;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        desiredFlags |= Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint;
#endif
        desiredFlags &= ~(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint |
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

    // A native handle/state transition in Qt 5 or Qt 6.2 may restore Qt's
    // default Win32 style after the window was shown. Repair the resize style
    // at the point of use as a final guard; ensureDwmChromeStyle is a no-op when
    // the expected flags are already present.
    // zh_CN: Qt 5 或 Qt 6.2 的原生句柄/状态切换可能在显示后恢复默认 Win32 style；
    // 在命中测试入口做最后一次自愈，正常状态下 ensureDwmChromeStyle 不产生操作。
    if (msg->message == WM_NCHITTEST)
        ensureDwmChromeStyle(window);

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
        const QPoint localPos = logicalClientPointForNativeScreenPoint(
            window, msg->hwnd, globalPos);
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

    // Suppress the non-client frame repaint on focus changes. For a frameless + translucent (Mica)
    // window, DefWindowProc otherwise redraws the default system caption/border on every WM_NCACTIVATE,
    // which flashes a light/white edge when the user fast-switches apps (Alt-Tab away and back).
    // DwmDefWindowProc has already serviced this message above to recomposite the backdrop; we now call
    // DefWindowProc with lParam == -1, which Windows documents as "do not repaint the non-client area to
    // reflect the state change," while still returning the correct activate/deactivate value so the shell
    // and taskbar keep tracking activation. zh_CN: 抑制焦点变化时的非客户区 frame 重绘。无边框 + 半透明(Mica)窗口下,
    // DefWindowProc 会在每个 WM_NCACTIVATE 重绘系统默认标题栏/边框，使用户快速切换 app(Alt-Tab 切走再切回)时闪现浅/白边。
    // 上面 DwmDefWindowProc 已处理过该消息以重新合成背景；此处以 lParam == -1 调用 DefWindowProc——Windows 文档为
    //「不重绘非客户区以反映状态变化」——同时仍返回正确的激活/失活值，使 shell 与任务栏继续跟踪激活状态。
    if (msg->message == WM_NCACTIVATE) {
        *result = static_cast<FluentNativeEventResult>(
            DefWindowProcW(msg->hwnd, WM_NCACTIVATE, msg->wParam, -1));
        return true;
    }

    if (msg->message != WM_NCHITTEST)
        return false;

    // Our own Fluent caption buttons live in the client area (added to the title bar's drag
    // exclusions), so classifyHitTest returns HTCLIENT over them and they receive clicks
    // normally — no DWM button hit-testing needed (the native glass buttons are suppressed).
    // zh_CN: 自绘 Fluent 标题栏按钮位于客户区（已加入标题栏拖拽排除区），classifyHitTest 在其上返回
    // HTCLIENT，按钮正常收到点击——无需 DWM 按钮命中测试（原生玻璃按钮已抑制）。
    const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
    const QPoint localPos = logicalClientPointForNativeScreenPoint(
        window, msg->hwnd, globalPos);
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

bool requestPlatformForegroundActivation(QWidget* window) {
    HWND hwnd = hwndForWindow(window);
    return hwnd && SetForegroundWindow(hwnd) != FALSE;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (!options.useCustomWindowChrome)
        return;

    HWND hwnd = hwndForWindow(window);
    if (!hwnd)
        return;

    // On a translucent window the whole client area must be a "sheet of glass" so the DWM backdrop
    // fills it; otherwise transparent regions render black. Keyed off the window's translucency (fixed
    // for its lifetime), so it stays a sheet of glass across Normal/Mica/Acrylic — Normal just paints
    // its surfaces opaque over it. zh_CN: 半透明窗口下整个客户区要做成「玻璃」，让 DWM 背景填满；否则透明区域会渲染成
    // 黑色。以窗口半透明性（整生命周期固定）为准，故 Normal/Mica/Acrylic 下始终是玻璃——Normal 只是在其上自绘不透明表面。
    if (window && window->testAttribute(Qt::WA_TranslucentBackground)) {
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

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return 0;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_WIN
