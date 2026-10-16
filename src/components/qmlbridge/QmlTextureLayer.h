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
 * @brief Composes borrowed QmlViews into an application-owned OpenGL panel.
 * zh_CN: 将借用的 QmlView 合成到应用持有的 OpenGL 面板。
 *
 * Set the selected page as rootWidget(), then synchronize() before installing
 * ancestor capture effects. Ready views retain their engines and scene objects,
 * hide their native Quick widgets, and render directly to GPU textures. The
 * caller paints ordinary Widgets first and passes that texture to compose().
 * Cached pages keep their scene state until release(); only the selected page
 * renders. Remove capture effects before release() restores native Quick widgets.
 * zh_CN: 设置当前页为 rootWidget()，安装祖先捕获特效前调用 synchronize()。已就绪视图
 * 保留引擎和场景对象，隐藏原生 Quick 控件并直接绘制 GPU 纹理。调用者先绘制普通 Widgets，
 * 再将其纹理交给 compose()。缓存页保留场景，只有当前页绘制；release() 恢复原生 Quick
 * 控件前，调用者须先移除捕获特效。
 *
 * All calls use the GUI thread. GPU calls require the supplied context current
 * and no active Qt Quick frame. Source and context ownership remain borrowed.
 * Native siblings overlapping a QML surface, additional graphics effects, and
 * non-OpenGL compositors are rejected so the host can use direct 2D composition.
 * No Qt Quick 3D module or CPU framebuffer readback is used.
 * zh_CN: 所有调用在 GUI 线程执行；GPU 调用要求指定上下文当前有效，且无进行中的 Quick 帧。
 * 不接管源控件或上下文。原生兄弟控件覆盖 QML、额外图形特效或非 OpenGL 合成器会被拒绝，
 * 由宿主退回直接 2D 合成。不使用 Qt Quick 3D 模块或 CPU 帧缓冲读回。
 */
class QmlTextureLayer : public QObject {
    Q_OBJECT
public:
    explicit QmlTextureLayer(QObject* parent = nullptr);
    ~QmlTextureLayer() override;
    QWidget* rootWidget() const;
    void setRootWidget(QWidget* root);
    /** @brief Adopts current-page views without recreating their QML scenes.
     * zh_CN: 接管当前页的视图，不重建其 QML 场景。 */
    bool synchronize();
    /** @brief Conservative target bytes for source, or maximum quint64 if unsupported.
     * Source is the panel rectangle in root-widget coordinates; DPR is in (0, 4].
     * zh_CN: 返回目标显存上界；不支持时返回 quint64 最大值。source 为根控件坐标中的面板矩形，
     * DPR 范围为 (0, 4]。 */
    quint64 requiredBytes(const QRectF& source, qreal devicePixelRatio) const;
    /** @brief Renders dirty visible scenes within the aggregate byte limit.
     * zh_CN: 在总显存限额内更新可见且变脏的场景。 */
    bool prepareFrame(QOpenGLContext* context, const QRectF& source, qreal devicePixelRatio, quint64 maximumBytes);
    /** @brief Composes over a borrowed bottom-up RGBA texture; zero means failure.
     * The result is borrowed and expires on resize, context loss or cache release.
     * zh_CN: 在借用的下原点 RGBA 纹理上合成；失败返回零。结果仅被借用，尺寸改变、
     * 上下文丢失或缓存释放后失效。 */
    unsigned int compose(unsigned int baseTexture, const QSize& pixels, quint64 baseRevision);
    unsigned int textureId() const;
    QSize textureSize() const;
    QVariantMap statistics() const;
    /** @brief Releases GPU caches while retaining scene delegation and local state.
     * zh_CN: 释放 GPU 缓存，保留场景接管及本地状态。 */
    void clearFrameCaches();
    /** @brief Restores native Quick widgets after the caller removes capture effects.
     * zh_CN: 调用者移除捕获特效后，恢复原生 Quick 控件。 */
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
