#ifndef FLUENTQT_COMPONENTS_SPATIAL_PARTICLELAYER_H
#define FLUENTQT_COMPONENTS_SPATIAL_PARTICLELAYER_H

#include <QObject>
#include <QSize>
#include <memory>

namespace fluent::layout {
class ParticleBackdrop;
}

namespace fluent::spatial {

/**
 * @brief Render a borrowed ParticleBackdrop into an existing OpenGL compositor.
 * zh_CN: 将借用的 ParticleBackdrop 绘制到现有 OpenGL 合成器中。
 *
 * Available only from FluentQt::Spatial. Construction creates no context or
 * widget and leaves the source on its normal CPU path. A successful render()
 * requires the source's gpuAccelerationEnabled request and temporarily
 * suppresses only its paint, redirecting its existing
 * simulation timer to frameRequested(). The compositor must preserve the
 * source's widget stacking/clipping and draw its texture behind its children.
 * Source ownership, particle state and motion/visibility policy are unchanged.
 * zh_CN: 仅由 FluentQt::Spatial 提供。构造不创建上下文或控件，默认仍使用 CPU。
 * render() 成功后仅接管源控件绘制，原有模拟计时器改发 frameRequested()；合成器需保留
 * 原有层叠和裁剪，将纹理置于子控件之后。不改变源的所有权、粒子状态或动效/可见性策略。
 *
 * Call from the GUI thread with the compositor's context current. The caller
 * selects an appropriate hardware context; this class does not create one or
 * change the widget's window. Hidden sources, invalid contexts and allocation
 * failures release delegation safely. release() restores CPU painting at the
 * same simulation phase; source/context destruction also releases resources.
 * zh_CN: 在 GUI 线程、合成器上下文当前有效时调用。硬件上下文由调用者选择，本类不创建
 * 上下文或改变窗口。隐藏、上下文无效、分配失败时安全退回 CPU；release() 保留模拟相位，
 * 源控件或上下文销毁也会释放资源。
 */
class ParticleLayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit ParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    ~ParticleLayer() override;

    layout::ParticleBackdrop* backdrop() const;
    bool isActive() const;
    /** @brief Conservative current-context target bytes, or zero when unsupported.
     * Includes the actual driver MSAA count; performs at most a tiny context probe.
     * zh_CN: 当前上下文所需目标显存上界；不支持时为零。包含驱动实际 MSAA 数，仅进行微小探测。 */
    static quint64 estimatedBytes(const QSize& logicalSize, qreal devicePixelRatio);
    /** @brief Render/reuse the current frame within the byte limit; false restores CPU.
     * DPR must be finite and in (0, 4]. The default per-layer limit is 64 MiB.
     * zh_CN: 在显存限额内绘制或复用当前帧；失败返回 false 并恢复 CPU。DPR 范围 (0, 4]，默认 64 MiB。 */
    bool render(qreal devicePixelRatio, quint64 maximumBytes = 67108864);
    /** @brief Borrowed premultiplied RGBA, bottom-up OpenGL texture; zero when inactive.
     * Valid only in the current context until release, resize or context replacement.
     * zh_CN: 借用的预乘 RGBA、下原点 GL 纹理；未激活时为零，释放、尺寸或上下文改变后失效。 */
    unsigned int textureId() const;
    QSize textureSize() const;
    quint64 allocatedBytes() const;
    quint64 renderedFrameCount() const;
    /** @brief Release GPU resources and resume the source's unchanged CPU state.
     * zh_CN: 释放 GPU 资源，并从未改变的源状态恢复 CPU 绘制。 */
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
