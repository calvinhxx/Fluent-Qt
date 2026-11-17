#ifndef FLUENTQT_SPATIALRUNTIME_H
#define FLUENTQT_SPATIALRUNTIME_H

#include <QString>
#include <QSurface>

namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
/**
 * @brief Static GUI-thread helpers for configuring and probing 3D graphics backends in FluentQt::Spatial.
 * zh_CN: 用于预配置窗口与探测 OpenGL 环境的静态工具类（须在 GUI 线程调用）。
 */
class SpatialRuntime {
public:
    /** @brief Installs native style adapters before creating pages, without creating a GPU context. zh_CN: 在创建界面前初始化原生样式适配，不创建 GPU 上下文。 */
    static void prepareApplication();
    /** @brief Prepares a hidden window for OpenGL composition before it is shown; returns false if already visible or unsupported. zh_CN: 在显示窗口前预置 OpenGL 合成属性；窗口已显示或平台不支持时返回 false。 */
    static bool prepareWindow(windowing::Window* window);
    /** @brief Configures the requested surface type on a hidden window before creation; verify backend availability separately. zh_CN: 在创建窗口前预置所需的绘制后端属性，须先自行确认该后端可用。 */
    static bool prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType);
    /** @brief Returns whether the current display backend supports OpenGL. zh_CN: 当前显示后端是否支持尝试 OpenGL。 */
    static bool supportsOpenGLDisplay();
    /** @brief Returns true if renderer is a non-empty, non-software renderer name. zh_CN: 判断渲染器名称是否为硬件加速渲染器。 */
    static bool isHardwareRenderer(const QString& renderer);
    /** @brief Returns GL_RENDERER for the current OpenGL context, or empty if none is current. zh_CN: 返回当前 OpenGL 上下文的渲染器名称，无当前上下文时为空。 */
    static QString currentRendererName();
    /** @brief Probes OpenGL readiness offscreen and returns the failure reason, or empty on success. zh_CN: 执行离屏 OpenGL 预检查并返回失败原因，通过时返回空字符串。 */
    static QString preflightFailure();
    /** @brief Returns the maximum supported texture/viewport size in the current OpenGL context, or 0. zh_CN: 返回当前上下文支持的最大纹理/视口尺寸，无当前上下文时返回 0。 */
    static int maximumTextureDimension();
    /** @brief Whether native text glyph adjustment is needed at the given display scale. zh_CN: 判断指定缩放倍率下是否需要原生字体渲染适配。 */
    static bool needsNativeGlyphCoverage(qreal nativeDpr);
};
} // namespace fluent::spatial
#endif
