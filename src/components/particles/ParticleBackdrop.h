#ifndef FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_H
#define FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_H

#include <QMarginsF>
#include <QPointF>
#include <QWidget>
#include <memory>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::particles {

class ParticleBackdropPrivate;
class ParticleBackdropRenderAccess;

/**
 * @brief Animated particle backdrop widget using CPU rendering by default.
 * Supports normal child widget layouts, pauses automatically when hidden, clipped, or under
 * reduced-motion policies, and never steals keyboard focus during pointer interaction. Defaults to
 * FlowingRibbons with 240 particles at up to 30 fps.
 * zh_CN: 默认使用 CPU 绘制的动态粒子背景控件，可在其上正常放置子控件布局。
 * 控件隐藏、被完全裁剪或系统开启减弱动效时会自动停止计时；鼠标交互不抢占键盘焦点。
 * 默认效果为 FlowingRibbons，240 个粒子，帧率上限 30 fps。
 */
class ParticleBackdrop : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Effect effect READ effect WRITE setEffect NOTIFY effectChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(bool animating READ isAnimating NOTIFY animatingChanged)
    Q_PROPERTY(bool gpuAccelerationEnabled
        READ isGpuAccelerationEnabled
        WRITE setGpuAccelerationEnabled
        NOTIFY gpuAccelerationEnabledChanged)
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(int particleCount READ particleCount WRITE setParticleCount NOTIFY particleCountChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY maximumFrameRateChanged)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)
    Q_PROPERTY(bool pauseWhenInactive
        READ isPauseWhenInactive
        WRITE setPauseWhenInactive
        NOTIFY pauseWhenInactiveChanged)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY backgroundModeChanged)
    Q_PROPERTY(QMarginsF fadeMargins READ fadeMargins WRITE setFadeMargins NOTIFY fadeMarginsChanged)

public:
    /** @brief Built-in CPU particle presets. zh_CN: 内置粒子特效预设。 */
    enum Effect {
        FlowingRibbons, ///< Orbiting ribbons and sparks. zh_CN: 轨道光带与拖尾亮点。
        FloatingDots,   ///< Soft lights drifting upward. zh_CN: 缓慢向上漂浮的柔和光点。
        Starfield       ///< Stars moving toward the viewer. zh_CN: 向观察者迎面飞来的星空。
    };
    Q_ENUM(Effect)

    /** @brief Background fill mode. zh_CN: 背景填充模式。 */
    enum BackgroundMode { Transparent, Solid };
    Q_ENUM(BackgroundMode)

    explicit ParticleBackdrop(QWidget* parent = nullptr);
    ~ParticleBackdrop() override;

    Effect effect() const;
    /**
     * @brief Switches the active preset, resets simulation phase, and clears transient ripples/pointer state.
     * Preserves other settings and never resumes paused motion; invalid values are ignored.
     * zh_CN: 切换粒子特效并重置运动相位，同时清除临时指针与涟漪状态；保留其他配置且不会自动恢复已暂停的动效，忽略无效值。
     */
    void setEffect(Effect effect);

    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
    /** @brief Whether the animation timer is currently running after visibility and motion policies. zh_CN: 综合可见性与动效策略后，计时器当前是否正在运行。 */
    bool isAnimating() const;

    /**
     * @brief Allows an optional Spatial compositor (ParticleLayer) to paint this widget on GPU; default false.
     * Without an active compositor, rendering stays on CPU and no OpenGL context or timer is created.
     * zh_CN: 允许可选的 Spatial 合成器（ParticleLayer）将本控件切换为 GPU 绘制，默认 false。
     * 未接入合成器时仍使用 CPU 绘制，不会创建任何 OpenGL 上下文或额外计时器。
     */
    bool isGpuAccelerationEnabled() const;
    void setGpuAccelerationEnabled(bool enabled);

    qreal speed() const;
    /** @brief Sets simulation speed multiplier in [0, 4]; 0 pauses motion. zh_CN: 设置速度倍率 [0, 4]；设为 0 表示静止。 */
    void setSpeed(qreal speed);
    int particleCount() const;
    /** @brief Sets particle count in [0, 960]; FlowingRibbons ribbons and sparks are counted separately. zh_CN: 设置粒子数量 [0, 960]；FlowingRibbons 的光带与拖尾亮点不计入此数量。 */
    void setParticleCount(int count);
    int maximumFrameRate() const;
    /** @brief Caps animation timer rate to [1, 60] fps. zh_CN: 将动画调度帧率限制在 [1, 60] fps。 */
    void setMaximumFrameRate(int framesPerSecond);
    bool isInteractive() const;
    void setInteractive(bool interactive);
    bool isPauseWhenInactive() const;
    /** @brief Whether to pause animation when the application becomes inactive. zh_CN: 应用切到后台失活时是否自动暂停动画。 */
    void setPauseWhenInactive(bool pause);

    BackgroundMode backgroundMode() const;
    void setBackgroundMode(BackgroundMode mode);
    QMarginsF fadeMargins() const;
    /** @brief Sets non-negative edge fade widths in logical pixels. zh_CN: 设置非负的边缘渐隐宽度（逻辑像素）。 */
    void setFadeMargins(const QMarginsF& margins);

    /** @brief Emits a decorative ripple at local coordinates when motion is allowed (up to 3 active). zh_CN: 在允许动效时于局部坐标处触发装饰涟漪（最多同时保留 3 个）。 */
    Q_INVOKABLE void triggerRipple(const QPointF& position);
    void onThemeUpdated() override;

signals:
    void effectChanged(Effect effect);
    void animationEnabledChanged(bool enabled);
    void animatingChanged(bool animating);
    void gpuAccelerationEnabledChanged(bool enabled);
    void speedChanged(qreal speed);
    void particleCountChanged(int count);
    void maximumFrameRateChanged(int framesPerSecond);
    void interactiveChanged(bool interactive);
    void pauseWhenInactiveChanged(bool pause);
    void backgroundModeChanged(BackgroundMode mode);
    void fadeMarginsChanged(const QMarginsF& margins);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    friend class ParticleBackdropRenderAccess;
    std::unique_ptr<ParticleBackdropPrivate> d;
};

} // namespace fluent::particles

#endif // FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_H
