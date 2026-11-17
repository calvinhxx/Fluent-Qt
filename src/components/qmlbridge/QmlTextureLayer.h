#ifndef FLUENTQT_QMLBRIDGE_QMLTEXTURELAYER_H
#define FLUENTQT_QMLBRIDGE_QMLTEXTURELAYER_H

#include <QObject>
#include <QRectF>
#include <QSize>
#include <QVariantMap>
#include <memory>

class QOpenGLContext;
class QWidget;

namespace fluent::qmlbridge {

/**
 * @brief Composites QmlView subtrees into an OpenGL panel texture without CPU readback.
 * Set the active page with setRootWidget() and call synchronize() before adding ancestor capture
 * effects. Ready views keep their QML engines and scenes, hide their native Quick widgets, and
 * render to GPU textures that compose() layers over the caller's base widget texture. Cached
 * pages preserve scene state until release(); remove capture effects before calling release() to
 * restore native Quick widgets.
 *
 * All calls require the GUI thread; GPU calls require the given QOpenGLContext to be current with
 * no active Qt Quick frame. Caller retains ownership of widgets and the OpenGL context. Overlapping
 * native siblings, widget graphics effects, and non-OpenGL Quick backends are rejected so the host
 * can fall back to direct 2D rendering.
 * zh_CN: 将指定页面下的 QmlView 合成到 OpenGL 面板纹理中（不使用 Qt Quick 3D 或 CPU 回读）。
 * 用 setRootWidget() 指定当前页后，在添加祖先捕获特效前调用 synchronize()：已就绪的视图会保留
 * QML 引擎和场景、隐藏原生 Quick 控件，并渲染到 GPU 纹理，随后通过 compose() 叠加到普通控件的
 * 基础纹理之上。未释放前缓存页保留场景状态，仅当前页参与渲染；调用 release() 恢复原生 Quick 控件前，
 * 调用方须先移除捕获特效。
 *
 * 所有调用须在 GUI 线程执行；GPU 接口要求传入的 QOpenGLContext 为当前上下文且无进行中的 Quick 帧，
 * 本类不接管控件或上下文所有权。若存在遮挡 QmlView 的原生兄弟控件、图形特效或非 OpenGL 后端，
 * 将拒绝接管以便宿主回退到直接 2D 绘制。
 */
class QmlTextureLayer : public QObject {
    Q_OBJECT
public:
    explicit QmlTextureLayer(QObject* parent = nullptr);
    ~QmlTextureLayer() override;
    QWidget* rootWidget() const;
    void setRootWidget(QWidget* root);
    /**
     * @brief Switches current-page QmlViews to texture mode without recreating their QML scenes.
     * zh_CN: 将当前页下的 QmlView 切换为纹理渲染模式，不重建其 QML 场景。
     */
    bool synchronize();
    /**
     * @brief Estimates required GPU bytes for source in rootWidget() coordinates, or max quint64 if unsupported.
     * Device pixel ratio must be in (0, 4].
     * zh_CN: 估算所需的显存字节数上界；不支持时返回 quint64 最大值。source 为 rootWidget() 坐标系下的面板矩形，DPR 须在 (0, 4] 内。
     */
    quint64 requiredBytes(const QRectF& source, qreal devicePixelRatio) const;
    /**
     * @brief Renders dirty visible QML scenes within the total byte limit.
     * zh_CN: 在总显存限额内渲染当前可见且需更新的 QML 场景。
     */
    bool prepareFrame(QOpenGLContext* context, const QRectF& source, qreal devicePixelRatio, quint64 maximumBytes);
    /**
     * @brief Composites QML layers over a caller-owned RGBA OpenGL texture (bottom-left origin); returns 0 on failure.
     * The returned texture ID is managed by this layer and becomes invalid on resize, context loss, or cache release.
     * zh_CN: 将 QML 图层合成到调用方管理的 RGBA OpenGL 纹理（左下角原点）之上；失败返回 0。
     * 返回的纹理 ID 由本层管理，在尺寸变化、上下文销毁或释放缓存后失效。
     */
    unsigned int compose(unsigned int baseTexture, const QSize& pixels, quint64 baseRevision);
    unsigned int textureId() const;
    QSize textureSize() const;
    QVariantMap statistics() const;
    /**
     * @brief Releases GPU framebuffers while keeping QmlView delegation and scene state.
     * zh_CN: 释放 GPU 帧缓冲缓存，保留对视图的接管与 QML 场景状态。
     */
    void clearFrameCaches();
    /**
     * @brief Ends texture delegation and restores native Quick widgets after capture effects are removed.
     * zh_CN: 结束纹理接管并恢复原生 Quick 控件（调用前须先移除外层捕获特效）。
     */
    void release();

signals:
    void frameRequested();
    void renderingFailed(const QString& message);

private:
    struct Private;
    std::shared_ptr<Private> d;
    bool fail(const QString& message);
};
} // namespace fluent::qmlbridge
#endif
