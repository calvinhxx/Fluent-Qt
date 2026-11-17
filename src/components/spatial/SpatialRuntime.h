#ifndef FLUENTQT_SPATIALRUNTIME_H
#define FLUENTQT_SPATIALRUNTIME_H

#include <QString>
#include <QSurface>

namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
/**
 * @brief Static GUI-thread helpers for configuring and probing 3D widget hosts (FluentQt::Spatial).
 * None of these helpers own pages, modify app 3D preferences, or create a rendering canvas.
 * zh_CN: 用于预配置和探测 3D 控件宿主的静态工具类（仅 FluentQt::Spatial 提供，须在 GUI 线程调用）。
 * 这些接口不管理页面生命周期，也不会修改应用 3D 偏好或创建绘制画布。
 */
class SpatialRuntime {
public:
    /** @brief Installs native style adapters before building pages, without creating a GPU context. zh_CN: 在构建应用页面前安装原生样式适配，不创建 GPU 上下文。 */
    static void prepareApplication();
    /** @brief Prepares a hidden Fluent window for OpenGL composition without creating a context; returns false if visible or unsupported, and succeeds immediately if already configured. zh_CN: 为隐藏的 Fluent 窗口预配置 OpenGL 合成属性（不创建上下文）；窗口已显示或平台不支持时返回 false，已配置过则直接返回 true。 */
    static bool prepareWindow(windowing::Window* window);
    /** @brief Prepares a hidden Fluent window for the requested surface type without creating a graphics device; callers must still probe the API before installing a GPU canvas. zh_CN: 为隐藏窗口预配置指定的表面类型（不创建图形设备）；安装 GPU 画布前仍需自行探测该 API 是否可用。 */
    static bool prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType);
    /** @brief Whether the current display backend allows attempting OpenGL (context validation is still required). zh_CN: 当前显示后端是否支持尝试 OpenGL（仍需进一步验证实际上下文）。 */
    static bool supportsOpenGLDisplay();
    /** @brief Returns true if renderer is a non-empty, non-software renderer name. zh_CN: 判断渲染器名称是否非空且不属于已知的软件渲染器。 */
    static bool isHardwareRenderer(const QString& renderer);
    /** @brief Returns GL_RENDERER for the current OpenGL context, or empty if none is current. zh_CN: 返回当前 OpenGL 上下文的渲染器名称；无当前上下文时为空。 */
    static QString currentRendererName();
    /** @brief Runs an offscreen OpenGL probe when needed and returns the failure reason, or empty on success. zh_CN: 在需要时执行离屏 OpenGL 预检查并返回失败原因；通过时返回空字符串。 */
    static QString preflightFailure();
    /** @brief Returns the minimum of GL max texture, renderbuffer, and viewport dimensions in the current context, or 0. zh_CN: 返回当前上下文中纹理、渲染缓冲和视口最大尺寸的最小值；无当前上下文时返回 0。 */
    static int maximumTextureDimension();
    /** @brief Whether native text glyph coverage adjustment is needed at the given display DPR. zh_CN: 判断指定显示缩放比下是否需要原生文字覆盖率适配。 */
    static bool needsNativeGlyphCoverage(qreal nativeDpr);
};
} // namespace fluent::spatial
#endif
