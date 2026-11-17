#ifndef FLUENTQT_COMPONENTS_SPATIAL_PARTICLELAYER_H
#define FLUENTQT_COMPONENTS_SPATIAL_PARTICLELAYER_H

#include <QObject>
#include <QSize>
#include <memory>

namespace fluent::particles {
class ParticleBackdrop;
}

namespace fluent::spatial {

/**
 * @brief Renders a gpuAccelerationEnabled ParticleBackdrop into an OpenGL texture for compositors such as SpatialView.
 * Must be called on the GUI thread with a current OpenGL context; does not take ownership of the backdrop.
 * zh_CN: 将开启 gpuAccelerationEnabled 的 ParticleBackdrop 渲染为 OpenGL 纹理，供 SpatialView 等合成器叠加显示。
 * 须在 GUI 线程且 OpenGL 上下文激活时调用，不接管背景控件的生命周期。
 */
class ParticleLayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit ParticleLayer(particles::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    ~ParticleLayer() override;

    particles::ParticleBackdrop* backdrop() const;
    bool isActive() const;
    /**
     * @brief Estimates GPU bytes needed in the current OpenGL context, or returns 0 if unsupported.
     * zh_CN: 估算当前 OpenGL 上下文所需的显存字节数（含 MSAA 采样开销）；不支持时返回 0。
     */
    static quint64 estimatedBytes(const QSize& logicalSize, qreal devicePixelRatio);
    /**
     * @brief Renders the current frame within maximumBytes; returns false and resumes CPU painting on failure.
     * devicePixelRatio must be in (0, 4]; default limit is 64 MiB.
     * zh_CN: 在显存限额（默认 64 MiB）内渲染当前帧，失败时返回 false 并自动切回 CPU 绘制；缩放倍率须在 (0, 4] 内。
     */
    bool render(qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /**
     * @brief Returns the premultiplied RGBA OpenGL texture (bottom-left origin), or 0 when inactive.
     * Invalidated by release(), resize, or context change.
     * zh_CN: 返回预乘 RGBA OpenGL 纹理（左下角原点），未激活时返回 0；释放、尺寸或上下文变化后失效。
     */
    unsigned int textureId() const;
    QSize textureSize() const;
    quint64 allocatedBytes() const;
    quint64 renderedFrameCount() const;
    /**
     * @brief Frees GPU targets and resumes CPU painting without altering particle state.
     * zh_CN: 释放 GPU 资源并恢复源控件的 CPU 绘制，保留当前粒子状态。
     */
    void release();

signals:
    void frameRequested();
    void activeChanged(bool active);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::spatial

#endif // FLUENTQT_COMPONENTS_SPATIAL_PARTICLELAYER_H
