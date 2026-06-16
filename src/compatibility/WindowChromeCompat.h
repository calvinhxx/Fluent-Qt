#ifndef WINDOWCHROMECOMPAT_H
#define WINDOWCHROMECOMPAT_H

#include <QByteArray>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

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
     * @brief Enables custom non-client handling on platforms that support it.
     * zh_CN: 在支持的平台启用自定义非客户区处理。
     */
    bool useCustomWindowChrome = false;

    /**
     * @brief Keeps native macOS traffic-light buttons when available.
     * zh_CN: 可用时保留 macOS 原生 traffic-light 窗口按钮。
     */
    bool preferNativeMacControls = true;
};

/**
 * @brief Cross-platform adapter for custom window chrome behavior.
 * zh_CN: 自定义窗口 chrome 行为的跨平台适配器。
 *
 * WindowChromeCompat keeps platform-specific title-bar flags, native hit testing,
 * system move/resize, and double-click behavior out of reusable widgets. Callers
 * provide geometry through WindowChromeOptions and forward native events when needed.
 * zh_CN: WindowChromeCompat 将平台标题栏 flag、原生命中测试、系统移动/缩放和双击行为
 * zh_CN: 从可复用控件中隔离出来。调用方通过 WindowChromeOptions 提供几何信息，并在需要时转发原生事件。
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
        Other
    };

    /**
     * @brief Logical hit-test result before it is mapped to native platform codes.
     * zh_CN: 映射到平台原生 code 之前的逻辑命中测试结果。
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
     * @brief Whether this platform/OS supports a system backdrop (Windows 11 Mica).
     * zh_CN: 当前平台/系统是否支持系统背景（Windows 11 Mica）。
     *
     * Checked before show so the caller can decide on a translucent window surface.
     * zh_CN: 在显示前检查，使调用方可据此决定是否使用半透明窗口表面。
     */
    bool systemBackdropSupported() const;

    /**
     * @brief Enables the Mica system backdrop for the current theme; returns success.
     * zh_CN: 为当前主题启用 Mica 系统背景；返回是否成功。
     *
     * Requires a live native handle, so call it from showEvent / after winId().
     * zh_CN: 需要有效的原生句柄，故应在 showEvent / winId() 之后调用。
     */
    bool applySystemBackdrop(bool dark);

    /**
     * @brief Handles forwarded native events and returns true when consumed.
     * zh_CN: 处理转发来的原生事件；事件被消费时返回 true。
     */
    bool handleNativeEvent(const QByteArray& eventType,
                           void* message,
                           FluentNativeEventResult* result);

    /**
     * @brief Starts a platform system-move operation from a global cursor position.
     * zh_CN: 从全局光标位置启动平台系统移动操作。
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
     * @brief Shows the platform system menu at the given global screen position.
     * zh_CN: 在指定全局屏幕坐标处显示平台系统菜单。
     */
    bool showSystemMenu(const QPoint& globalPos);

    bool usesCustomWindowChrome() const;
    bool prefersNativeMacControls() const;
    int nativeTitleBarLeadingInset() const;
    int nativeTitleBarTrailingInset() const;
    QWidget* window() const { return m_window; }

    /**
     * @brief Classifies a local point against the configured title and resize regions.
     * zh_CN: 根据配置的标题栏和缩放区域对本地点进行命中分类。
     */
    HitTest hitTestLocal(const QPoint& localPos) const;

    static Platform currentPlatform();
    static bool platformPrefersCustomWindowChrome();
    static HitTest classifyHitTest(const WindowChromeOptions& options,
                                   const QSize& windowSize,
                                   const QPoint& localPos);

private:
    QWidget* m_window = nullptr;
    WindowChromeOptions m_options;
};

} // namespace compatibility

#endif // WINDOWCHROMECOMPAT_H
