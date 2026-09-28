#ifndef FLUENTQT_SPATIALRUNTIME_H
#define FLUENTQT_SPATIALRUNTIME_H

#include <QString>
#include <QSurface>

namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
/**
 * @brief Shared opt-in runtime entry for native and application-owned 3D hosts.
 * zh_CN: 原生及应用自有 3D 宿主共用的可选运行时入口。
 * Available only from FluentQt::Spatial. Call on the GUI thread. These helpers
 * do not own pages, change the application's 3D preference or create a canvas.
 * zh_CN: 仅由 FluentQt::Spatial 提供，在 GUI 线程调用；不拥有页面、不修改应用 3D 偏好或创建画布。
 */
class SpatialRuntime {
public:
    /** @brief Install required native-style adapters before building application pages.
     * zh_CN: 在构建应用页面前安装必要的原生样式适配，不创建 GPU 上下文。 */
    static void prepareApplication();
    /** @brief Prepare a hidden Fluent window for later GPU composition without a context.
     * Returns false for visible/unsupported hosts; repeated preparation is a no-op.
     * zh_CN: 为隐藏的 Fluent 窗口准备后续 GPU 合成，不创建上下文；可见或不支持时返回 false，重复调用为空操作。 */
    static bool prepareWindow(windowing::Window* window);
    /** @brief Prepare a hidden window for a caller-selected graphics API; no device is created.
     * The caller must probe that API before installing a GPU canvas.
     * zh_CN: 为调用方指定的图形 API 准备隐藏窗口，不创建设备；安装 GPU 画布前仍须探测该 API。 */
    static bool prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType);
    /** @brief Whether this display can attempt OpenGL; actual context validation is still required.
     * zh_CN: 当前显示是否允许尝试 OpenGL，仍需验证真实上下文。 */
    static bool supportsOpenGLDisplay();
    /** @brief Reject empty and known software renderer names consistently across hosts.
     * zh_CN: 在所有宿主中一致拒绝空名称及已知软件渲染器。 */
    static bool isHardwareRenderer(const QString& renderer);
    /** @brief Current-context renderer name, or empty when no context is current.
     * zh_CN: 当前上下文的渲染器名称；没有当前上下文时为空。 */
    static QString currentRendererName();
    /** @brief Platform preflight failure, or empty when real-surface validation may proceed.
     * zh_CN: 平台预检查失败原因；可继续验证真实表面时为空。 */
    static QString preflightFailure();
    /** @brief Minimum texture/renderbuffer/viewport dimension of the current context, or zero.
     * zh_CN: 当前上下文的纹理、渲染缓冲及视口尺寸下限；无当前上下文时为零。 */
    static int maximumTextureDimension();
    /** @brief Whether native text coverage needs an adapter at the specified display density.
     * zh_CN: 指定显示密度是否需要原生文字覆盖率适配。 */
    static bool needsNativeGlyphCoverage(qreal nativeDpr);
};
} // namespace fluent::spatial
#endif
