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
 * @brief Renders QmlView widgets under a root page into OpenGL textures for compositors such as Spatial.
 * Must run on the GUI thread with a current OpenGL context for GPU calls; caller retains widget and context ownership.
 * zh_CN: 将指定页面下的 QmlView 渲染为 OpenGL 纹理并叠加合成到控件层上（供 Spatial 等合成器使用）。
 * 须在 GUI 线程调用，GPU 操作需当前 OpenGL 上下文有效，不接管控件或上下文生命周期。
 */
class QmlTextureLayer : public QObject {
    Q_OBJECT
public:
    explicit QmlTextureLayer(QObject* parent = nullptr);
    ~QmlTextureLayer() override;
    QWidget* rootWidget() const;
    void setRootWidget(QWidget* root);
    /**
     * @brief Switches current-page QmlViews to texture mode before capture effects are added.
     * zh_CN: 将当前页下的 QmlView 切换为纹理渲染模式（须在添加外层捕获特效前调用，不重建 QML 场景）。
     */
    bool synchronize();
    /**
     * @brief Estimates GPU memory needed for source in rootWidget() coordinates, or max quint64 if unsupported.
     * zh_CN: 估算 source 区域（rootWidget() 坐标系）所需的显存上限，不支持时返回 quint64 最大值；缩放倍率须在 (0, 4] 内。
     */
    quint64 requiredBytes(const QRectF& source, qreal devicePixelRatio) const;
    /**
     * @brief Renders dirty visible QML scenes within the total byte limit.
     * zh_CN: 在总显存限额内渲染当前可见且需更新的 QML 场景。
     */
    bool prepareFrame(QOpenGLContext* context, const QRectF& source, qreal devicePixelRatio, quint64 maximumBytes);
    /**
     * @brief Composites QML layers over a caller-owned RGBA OpenGL texture (bottom-left origin); returns 0 on failure.
     * zh_CN: 将 QML 图层叠加合成到外部 RGBA OpenGL 纹理（左下角原点）上，失败返回 0；返回的纹理 ID 仅在尺寸与上下文有效期内可用。
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
