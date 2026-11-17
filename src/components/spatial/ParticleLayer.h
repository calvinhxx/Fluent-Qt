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
 * @brief Renders a ParticleBackdrop into an OpenGL texture for an external compositor (FluentQt::Spatial).
 * Does not take ownership of the backdrop, create an OpenGL context, or change the widget's window.
 * While inactive, the backdrop paints on CPU as usual. When render() succeeds (requiring
 * gpuAccelerationEnabled and a current hardware/valid context), widget painting is suppressed and
 * timer ticks emit frameRequested() instead; the compositor must preserve stacking/clipping and
 * draw the texture behind child widgets.
 *
 * Call on the GUI thread with the compositor's OpenGL context current. Hiding the source, losing
 * the context, or hitting allocation limits releases GPU resources cleanly and resumes CPU painting
 * at the same simulation phase.
 * zh_CN: 将 ParticleBackdrop 渲染为 OpenGL 纹理供外部合成器使用（仅 FluentQt::Spatial 提供）。
 * 本类不接管粒子控件所有权，也不创建 OpenGL 上下文或更改控件所属窗口。未启用前源控件照常使用 CPU 绘制；
 * render() 成功后（要求开启 gpuAccelerationEnabled 且当前上下文有效），会接管源控件的绘制并将模拟计时器
 * 转为触发 frameRequested()，合成器需保持原有的层叠裁剪关系并将纹理绘制在子控件下方。
 *
 * 所有调用须在 GUI 线程且合成器上下文为当前上下文时执行。控件隐藏、上下文失效、显存超限或对象销毁时
 * 会自动释放 GPU 资源，并以原有模拟相位无缝恢复 CPU 绘制。
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
     * @brief Estimates target GPU bytes in the current OpenGL context, or returns 0 if unsupported.
     * Accounts for the driver's actual MSAA sample count via a minimal context probe.
     * zh_CN: 估算当前 OpenGL 上下文所需的显存字节数（含驱动实际 MSAA 采样数探测）；不支持时返回 0。
     */
    static quint64 estimatedBytes(const QSize& logicalSize, qreal devicePixelRatio);
    /**
     * @brief Renders or reuses the current frame within maximumBytes; returns false and restores CPU on failure.
     * Device pixel ratio must be finite and in (0, 4]. Default limit is 64 MiB.
     * zh_CN: 在显存限额内渲染或复用当前帧；失败时返回 false 并恢复 CPU 绘制。DPR 须在 (0, 4] 内，默认限额 64 MiB。
     */
    bool render(qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /**
     * @brief Returns the premultiplied RGBA OpenGL texture (bottom-left origin), or 0 when inactive.
     * Valid in the current context until release(), resize, or context replacement.
     * zh_CN: 返回内部管理的预乘 RGBA OpenGL 纹理（左下角原点），未激活时返回 0；在 release()、尺寸或上下文变化后失效。
     */
    unsigned int textureId() const;
    QSize textureSize() const;
    quint64 allocatedBytes() const;
    quint64 renderedFrameCount() const;
    /**
     * @brief Releases GPU targets and resumes CPU painting without altering particle state.
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
