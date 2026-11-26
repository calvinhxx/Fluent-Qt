#ifndef FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_H
#define FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_H

#include <QMarginsF>
#include <QPointF>
#include <QWidget>
#include <memory>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

namespace fluent::particles {

class ParticleBackdropPrivate;
class ParticleBackdropRenderAccess;

/**
 * @brief Animated particle backdrop widget rendered on the CPU by default.
 * Supports normal child layouts on top and pauses automatically when hidden or when reduced motion is enabled.
 * zh_CN: 使用 CPU 绘制的动态粒子背景控件，可在上方正常布局子控件；隐藏或开启减弱动画时自动暂停。
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
    /** @brief Built-in particle presets. zh_CN: 内置粒子特效预设。 */
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
     * @brief Switches the particle preset and resets simulation phase without resuming paused motion.
     * zh_CN: 切换粒子特效并重置动画相位，不会自动恢复已暂停的动画；忽略无效值。
     */
    void setEffect(Effect effect);

    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
    /** @brief Whether the particle animation is currently running. zh_CN: 动画当前是否正在运行（受可见性与动效设置影响）。 */
    bool isAnimating() const;

    /**
     * @brief Allows switching to GPU rendering when used with Spatial (default false; standalone widgets always use CPU).
     * zh_CN: 是否允许在接入 Spatial 时切换为 GPU 绘制（默认 false，独立使用时走 CPU 绘制）。
     */
    bool isGpuAccelerationEnabled() const;
    void setGpuAccelerationEnabled(bool enabled);

    qreal speed() const;
    /** @brief Sets simulation speed multiplier in [0, 4]; 0 pauses motion. zh_CN: 设置速度倍率 [0, 4]；设为 0 表示静止。 */
    void setSpeed(qreal speed);
    int particleCount() const;
    /** @brief Sets particle count in [0, 960] (excluding FlowingRibbons ribbons and sparks). zh_CN: 设置粒子数量 [0, 960]（不含 FlowingRibbons 的光带与拖尾亮点）。 */
    void setParticleCount(int count);
    int maximumFrameRate() const;
    /** @brief Sets maximum animation frame rate in [1, 60] fps. zh_CN: 设置动画最高帧率，范围 [1, 60] fps。 */
    void setMaximumFrameRate(int framesPerSecond);
    bool isInteractive() const;
    void setInteractive(bool interactive);
    bool isPauseWhenInactive() const;
    /** @brief Whether to pause animation when the application becomes inactive. zh_CN: 应用切到后台时是否自动暂停动画。 */
    void setPauseWhenInactive(bool pause);

    BackgroundMode backgroundMode() const;
    void setBackgroundMode(BackgroundMode mode);
    QMarginsF fadeMargins() const;
    /** @brief Sets non-negative edge fade widths in logical pixels. zh_CN: 设置非负的边缘渐隐宽度（逻辑像素）。 */
    void setFadeMargins(const QMarginsF& margins);

    /** @brief Emits a decorative ripple at local coordinates when motion is allowed (up to 3 active). zh_CN: 在开启动效时于局部坐标处触发涟漪效果（最多同时显示 3 个）。 */
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
