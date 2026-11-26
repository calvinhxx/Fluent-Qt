#ifndef WINDOWCHROMECOMPAT_H
#define WINDOWCHROMECOMPAT_H

// Thin per-OS router forwarding to WindowChromeCompat_{win,mac,linux,fallback}.cpp;
// visual colors are painted by Window, TitleBar, and NavigationView.
// zh_CN: 按操作系统分发的轻量适配层，具体实现位于 WindowChromeCompat_{win,mac,linux,fallback}.cpp；
// 标题栏与内容区背景由 Window/TitleBar/NavigationView 负责绘制。

#include <QByteArray>
#include <QAbstractButton>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

#include "WindowBackdropTypes.h"

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
 * @brief Options for platform window-chrome integration.
 * zh_CN: 平台窗口标题栏与边框集成配置。
 */
struct WindowChromeOptions {
    /**
     * @brief Draggable title-bar area in window-local coordinates.
     * zh_CN: 窗口局部坐标下可拖拽的标题栏区域。
     */
    QRect titleBarRect;

    /**
     * @brief Child-control rectangles inside the title bar that remain normal client area.
     * zh_CN: 标题栏内需要保持可点击（非拖拽）的子控件本地矩形列表。
     */
    QVector<QRect> dragExclusionRects;

    /**
     * @brief Resize hit-test border width in device-independent pixels.
     * zh_CN: 边框拖拽缩放的命中测试宽度（设备无关像素）。
     */
    int resizeBorderWidth = 8;

    /**
     * @brief Enables Fluent custom title-bar integration while preferring native window management where supported.
     * zh_CN: 启用自定义标题栏集成；在支持的平台上仍优先使用系统原生窗口管理，不支持时改由客户端接管。
     */
    bool useCustomWindowChrome = false;

    /**
     * @brief Keeps native macOS traffic-light buttons when available.
     * zh_CN: 可用时保留 macOS 原生窗口控制按钮（红绿灯）。
     */
    bool preferNativeMacControls = true;

    /**
     * @brief Locks window move and resize when false (e.g., while a modal overlay is active).
     * zh_CN: 为 false 时禁止拖拽移动与缩放窗口（如模态遮罩显示期间）。
     */
    bool chromeInteractive = true;

    /**
     * @brief Maximize/restore button used for native caption hit-testing.
     * zh_CN: 用于原生标题栏命中测试的最大化/还原按钮。
     */
    QPointer<QAbstractButton> maximizeButton;
};

/**
 * @brief Cross-platform adapter for window chrome behavior, hit-testing, and system backdrops.
 * zh_CN: 窗口标题栏、命中测试与系统背景的跨平台适配器。
 */
class WindowChromeCompat {
public:
    /**
     * @brief Runtime platform family used for window-chrome policy decisions.
     * zh_CN: 用于选择窗口边框策略的运行平台类型。
     */
    enum class Platform { Windows, MacOS, Linux, Other };

    /**
     * @brief Logical hit-test result mapped to platform-specific hit codes.
     * zh_CN: 映射为各平台原生命中码前的逻辑命中测试结果。
     */
    enum class HitTest { Client, Caption, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };

    explicit WindowChromeCompat(QWidget* window);

    /**
     * @brief Updates the chrome options and synchronizes platform title-bar geometry.
     * zh_CN: 更新窗口边框配置并同步平台侧标题栏几何信息。
     */
    void configure(const WindowChromeOptions& options);
    const WindowChromeOptions& options() const { return m_options; }

    /**
     * @brief Applies platform window flags required by the current chrome configuration.
     * zh_CN: 按当前配置应用所需的平台窗口标志。
     */
    void applyPlatformWindowFlags();

    /**
     * @brief Whether this platform supports a translucent system backdrop.
     * zh_CN: 当前平台是否支持半透明系统背景。
     */
    bool systemBackdropSupported() const;

    /**
     * @brief Returns per-effect platform capabilities for the current session.
     * zh_CN: 返回当前平台环境支持的各项背景能力。
     */
    BackdropCapabilities backdropCapabilities() const;
    /**
     * @brief Applies the requested system backdrop effect (legacy helper; use applySystemBackdropDetailed() for resolved backend details).
     * zh_CN: 应用系统背景效果并返回是否成功（旧接口；如需查看实际后端与降级原因请改用 applySystemBackdropDetailed()）。
     */
    bool applySystemBackdrop(BackdropEffect effect, bool dark, bool forceRecomposite = false);

    /**
     * @brief Applies a backdrop effect and returns the resolved backend, fidelity, and fallback reason.
     * zh_CN: 应用背景效果并返回实际使用的后端、还原程度及降级原因等详情。
     */
    BackdropApplyResult applySystemBackdropDetailed(BackdropEffect effect, bool dark, bool forceRecomposite = false);

    /**
     * @brief Handles forwarded native events and returns true when handled.
     * zh_CN: 处理转发来的原生窗口事件；若已处理则返回 true。
     */
    bool handleNativeEvent(const QByteArray& eventType, void* message, FluentNativeEventResult* result);

    /**
     * @brief Starts a platform system-move operation.
     * zh_CN: 启动系统原生窗口拖拽移动。
     */
    bool beginSystemMove(const QPoint& globalPos);

    /**
     * @brief Starts a platform system-resize operation for the given edges.
     * zh_CN: 针对指定边缘启动系统原生窗口缩放。
     */
    bool beginSystemResize(Qt::Edges edges, const QPoint& globalPos);

    /**
     * @brief Performs the platform title-bar double-click action.
     * zh_CN: 执行平台默认的标题栏双击动作。
     */
    bool performTitleBarDoubleClick();

    /**
     * @brief Shows the platform system menu at the given global position.
     * zh_CN: 在指定全局坐标处弹出系统窗口菜单。
     */
    bool showSystemMenu(const QPoint& globalPos);

    /**
     * @brief Requests foreground activation through the platform window manager.
     * zh_CN: 请求平台窗口管理器将窗口激活到前台。
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
     * @brief Classifies a local point against configured title-bar and resize regions.
     * zh_CN: 根据标题栏与缩放边框配置对本地坐标进行命中分类。
     */
    HitTest hitTestLocal(const QPoint& localPos) const;

    static Platform currentPlatform();
    static bool platformPrefersCustomWindowChrome();
    static bool expandedClientAreaHintsAvailable();
    static bool windowHasExpandedClientAreaHint(const QWidget* window);
    static bool windowHasNoTitleBarBackgroundHint(const QWidget* window);
    static HitTest classifyHitTest(const WindowChromeOptions& options, const QSize& windowSize, const QPoint& localPos);

private:
    QWidget* m_window = nullptr;
    WindowChromeOptions m_options;
};

} // namespace compatibility

#endif // WINDOWCHROMECOMPAT_H
