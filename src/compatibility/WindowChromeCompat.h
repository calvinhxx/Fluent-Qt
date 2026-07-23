#ifndef WINDOWCHROMECOMPAT_H
#define WINDOWCHROMECOMPAT_H

// Architecture: this class is a thin per-OS router. Public methods forward to
// detail functions implemented in WindowChromeCompat_{win,mac,linux,fallback}.cpp.
// Visual title-bar/content colors are painted by Window/TitleBar/NavigationView.
// zh_CN: 本类是按平台分发的轻量路由。public 方法转发到各平台
// WindowChromeCompat_{win,mac,linux,fallback}.cpp 中的 detail 实现。
// 标题栏和内容颜色由 Window/TitleBar/NavigationView 绘制。

#include <QByteArray>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#include "components/windowing/WindowBackdrop.h"

namespace compatibility {

/**
 * @brief Native-event result type used by QWidget::nativeEvent() across Qt versions.
 * zh_CN: 跨 Qt 版本用于 QWidget::nativeEvent() 的原生事件结果类型。
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using FluentNativeEventResult = qintptr;
#else
using FluentNativeEventResult = long;
#endif

using BackdropEffect = fluent::windowing::BackdropEffect;
using BackdropApplyResult = fluent::windowing::BackdropApplyResult;
using BackdropCapabilities = fluent::windowing::BackdropCapabilities;

/**
 * @brief Declarative options for platform window-chrome integration.
 * zh_CN: 平台窗口 chrome 集成使用的声明式配置。
 */
struct WindowChromeOptions {
    /**
     * @brief Local title-bar area that can behave as a draggable caption.
     * zh_CN: 可作为可拖拽 caption 区域的本地标题栏矩形。
     */
    QRect titleBarRect;

    /**
     * @brief Local child-control rectangles that must stay normal client areas.
     * zh_CN: 必须保留为普通 client 区域的子控件本地矩形。
     */
    QVector<QRect> dragExclusionRects;

    /**
     * @brief Resize hit-test border width in device-independent pixels.
     * zh_CN: resize 命中测试边框宽度，单位为设备无关像素。
     */
    int resizeBorderWidth = 8;

    /**
     * @brief Enables Fluent-managed title-bar/non-client integration.
     * zh_CN: 启用由 Fluent 管理的标题栏/非客户区集成。
     *
     * This does not mean "draw everything ourselves" on every platform. The
     * platform implementation should use native window-management APIs first and
     * expose client-side fallback only when needed.
     * zh_CN: 这不表示所有平台都完全自绘。平台实现应优先使用系统窗口管理能力，
     * 仅在需要时暴露客户端回退方案。
     */
    bool useCustomWindowChrome = false;

    /**
     * @brief Keeps native macOS traffic-light buttons when available.
     * zh_CN: 可用时保留 macOS 原生 traffic-light 窗口按钮。
     */
    bool preferNativeMacControls = true;

    /**
     * @brief Locks chrome move/resize when false, for modal overlays.
     * zh_CN: 为 false 时锁定 chrome 移动/缩放，用于模态覆盖层。
     */
    bool chromeInteractive = true;
};

/**
 * @brief Cross-platform adapter for window chrome behavior.
 * zh_CN: 窗口 chrome 行为的跨平台适配器。
 */
class WindowChromeCompat {
public:
    /**
     * @brief Runtime platform family used for chrome policy decisions.
     * zh_CN: 用于 chrome 策略决策的运行时平台族。
     */
    enum class Platform {
        Windows,
        MacOS,
        Linux,
        Other
    };

    /**
     * @brief Logical hit-test result before mapping to native platform codes.
     * zh_CN: 映射到平台原生 code 前的逻辑命中测试结果。
     */
    enum class HitTest {
        Client,
        Caption,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    explicit WindowChromeCompat(QWidget* window);

    /**
     * @brief Replaces the current chrome options and synchronizes platform geometry.
     * zh_CN: 替换当前 chrome 配置，并同步平台侧几何。
     */
    void configure(const WindowChromeOptions& options);
    const WindowChromeOptions& options() const { return m_options; }

    /**
     * @brief Applies platform window flags required by the current chrome policy.
     * zh_CN: 应用当前 chrome 策略需要的平台窗口 flag。
     */
    void applyPlatformWindowFlags();

    /**
     * @brief Whether this platform supports a translucent system backdrop.
     * zh_CN: 当前平台是否支持半透明系统背景。
     */
    bool systemBackdropSupported() const;

    /**
     * @brief Returns per-effect platform capabilities for the current session.
     * zh_CN: 返回当前会话逐效果的平台能力。
     */
    BackdropCapabilities backdropCapabilities() const;
    /**
     * @brief Applies the requested system backdrop effect; returns success.
     * zh_CN: 施加请求的系统背景效果；返回是否成功。
     *
     * Kept for source compatibility. New code that needs the actual backend,
     * fidelity, surface mode, or failure reason should use the detailed form.
     * zh_CN: 为源码兼容保留；需要实际后端、保真度、表面模式或失败原因的新代码应使用详细接口。
     */
    bool applySystemBackdrop(BackdropEffect effect,
                             bool dark,
                             bool forceRecomposite = false);

    /**
     * @brief Applies a backdrop and returns the structured actual result.
     * zh_CN: 施加背景并返回结构化的实际结果。
     */
    BackdropApplyResult applySystemBackdropDetailed(BackdropEffect effect,
                                                    bool dark,
                                                    bool forceRecomposite = false);

    /**
     * @brief Handles forwarded native events and returns true when consumed.
     * zh_CN: 处理转发来的原生事件；事件被消费时返回 true。
     */
    bool handleNativeEvent(const QByteArray& eventType,
                           void* message,
                           FluentNativeEventResult* result);

    /**
     * @brief Starts a platform system-move operation.
     * zh_CN: 启动平台系统移动操作。
     */
    bool beginSystemMove(const QPoint& globalPos);

    /**
     * @brief Starts a platform system-resize operation for the given edges.
     * zh_CN: 针对指定边缘启动平台系统缩放操作。
     */
    bool beginSystemResize(Qt::Edges edges, const QPoint& globalPos);

    /**
     * @brief Performs the platform title-bar double-click action.
     * zh_CN: 执行平台标题栏双击动作。
     */
    bool performTitleBarDoubleClick();

    /**
     * @brief Shows the platform system menu at the given global position.
     * zh_CN: 在指定全局坐标处显示平台系统菜单。
     */
    bool showSystemMenu(const QPoint& globalPos);

    /**
     * @brief Requests foreground activation through the platform window manager.
     * zh_CN: 通过平台窗口管理器请求将窗口激活到前台。
     */
    bool requestForegroundActivation();

    bool usesCustomWindowChrome() const;
    bool prefersNativeMacControls() const;
    bool manualMoveResizeFallbackAllowed() const;
    int nativeTitleBarLeadingInset() const;
    int nativeTitleBarTrailingInset() const;
    int clientSideFrameMargin() const;
    QWidget* window() const { return m_window; }

    /**
     * @brief Classifies a local point against configured title/resize regions.
     * zh_CN: 根据配置的标题栏和缩放区域对本地点进行命中分类。
     */
    HitTest hitTestLocal(const QPoint& localPos) const;

    static Platform currentPlatform();
    static bool platformPrefersCustomWindowChrome();
    static bool expandedClientAreaHintsAvailable();
    static bool windowHasExpandedClientAreaHint(const QWidget* window);
    static bool windowHasNoTitleBarBackgroundHint(const QWidget* window);
    static HitTest classifyHitTest(const WindowChromeOptions& options,
                                   const QSize& windowSize,
                                   const QPoint& localPos);

private:
    QWidget* m_window = nullptr;
    WindowChromeOptions m_options;
};

} // namespace compatibility

#endif // WINDOWCHROMECOMPAT_H
