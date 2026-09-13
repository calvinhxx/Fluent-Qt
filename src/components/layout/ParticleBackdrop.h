#ifndef FLUENTQT_COMPONENTS_LAYOUT_PARTICLEBACKDROP_H
#define FLUENTQT_COMPONENTS_LAYOUT_PARTICLEBACKDROP_H

#include <QMarginsF>
#include <QPointF>
#include <QWidget>
#include <memory>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::layout {

class ParticleBackdropPrivate;

/**
 * @brief CPU-painted particle presets behind caller-composed content.
 * zh_CN: 在调用方组合的内容后方，用 CPU 绘制可切换预设的粒子背景。
 *
 * Uses ordinary QWidget child layouts. Hidden, clipped and reduced-motion
 * surfaces stop their timer; decorative interaction never takes keyboard focus.
 * zh_CN: 使用普通 QWidget 子布局；隐藏、完全裁剪或减少动态效果时停表，装饰交互不获取键盘焦点。
 * Defaults to FlowingRibbons, 240 particles and at most 30 frames per second.
 * zh_CN: 默认使用流动光带、240 个粒子，每秒最多调度 30 帧。
 */
class ParticleBackdrop : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(Effect effect READ effect WRITE setEffect NOTIFY effectChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
                   animationEnabledChanged)
    Q_PROPERTY(bool animating READ isAnimating NOTIFY animatingChanged)
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(
        int particleCount READ particleCount WRITE setParticleCount NOTIFY particleCountChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY
                   maximumFrameRateChanged)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)
    Q_PROPERTY(bool pauseWhenInactive READ isPauseWhenInactive WRITE setPauseWhenInactive NOTIFY
                   pauseWhenInactiveChanged)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY
                   backgroundModeChanged)
    Q_PROPERTY(
        QMarginsF fadeMargins READ fadeMargins WRITE setFadeMargins NOTIFY fadeMarginsChanged)

public:
    /** @brief Built-in CPU particle effects. zh_CN: 内置的 CPU 粒子特效。 */
    enum Effect {
        FlowingRibbons, ///< Orbiting ribbons and sparks. zh_CN: 轨道光带与拖尾亮点。
        FloatingDots, ///< Soft lights drifting upward. zh_CN: 缓慢向上漂浮的柔和光点。
        Starfield ///< Stars moving toward the viewer. zh_CN: 向观察者迎面飞来的星空。
    };
    Q_ENUM(Effect)

    /** @brief Background composition. zh_CN: 背景合成方式。 */
    enum BackgroundMode { Transparent, Solid };
    Q_ENUM(BackgroundMode)

    explicit ParticleBackdrop(QWidget* parent = nullptr);
    ~ParticleBackdrop() override;

    Effect effect() const;
    /**
     * @brief Switches the effect, preserving settings and restarting its phase; invalid values are ignored.
     * zh_CN: 切换特效并重置运动相位，保留其他设置；忽略无效值。
     * Switching clears transient pointer and ripple state and never resumes paused motion.
     * zh_CN: 切换时清除临时指针与涟漪状态，不会恢复已暂停的动效。
     */
    void setEffect(Effect effect);

    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
    /** @brief Effective timer state after policy and visibility. zh_CN: 策略与可见性收敛后的实际计时状态。 */
    bool isAnimating() const;

    qreal speed() const;
    /** @brief Sets speed in [0, 4]; zero freezes motion. zh_CN: 设置 0 到 4 倍速度，零表示静止。 */
    void setSpeed(qreal speed);
    int particleCount() const;
    /**
     * @brief Sets the particle count in [0, 960]; ribbon accents are independent.
     * zh_CN: 设置粒子总数，范围 0 到 960；流动光带的光带、微粒与拖尾不计入此数量。
     */
    void setParticleCount(int count);
    int maximumFrameRate() const;
    /** @brief Caps scheduled frames to [1, 60] per second. zh_CN: 将计划帧率限制为每秒 1 到 60 帧。 */
    void setMaximumFrameRate(int framesPerSecond);
    bool isInteractive() const;
    void setInteractive(bool interactive);
    bool isPauseWhenInactive() const;
    /** @brief Controls pausing on application deactivation. zh_CN: 控制应用失活时是否暂停。 */
    void setPauseWhenInactive(bool pause);

    BackgroundMode backgroundMode() const;
    void setBackgroundMode(BackgroundMode mode);
    QMarginsF fadeMargins() const;
    /** @brief Sets nonnegative edge fade widths in logical pixels. zh_CN: 设置非负的边缘渐隐宽度，单位为逻辑像素。 */
    void setFadeMargins(const QMarginsF& margins);

    /**
     * @brief Emits a decorative ripple in local coordinates when motion is allowed.
     * zh_CN: 动效允许时，在局部坐标处产生装饰涟漪，最多同时保留三圈。
     */
    Q_INVOKABLE void triggerRipple(const QPointF& position);
    void onThemeUpdated() override;

signals:
    void effectChanged(Effect effect);
    void animationEnabledChanged(bool enabled);
    void animatingChanged(bool animating);
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
    std::unique_ptr<ParticleBackdropPrivate> d;
};

} // namespace fluent::layout

#endif // FLUENTQT_COMPONENTS_LAYOUT_PARTICLEBACKDROP_H
