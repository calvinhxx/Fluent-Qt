#ifndef FLUENTQT_COMPONENTS_FOUNDATION_MOTIONPOLICY_H
#define FLUENTQT_COMPONENTS_FOUNDATION_MOTIONPOLICY_H

#include <QObject>

namespace fluent {

/**
 * @brief Application-wide policy for Fluent transition and continuous motion.
 * zh_CN: Fluent 过渡动画和持续动画的应用级策略。
 *
 * Local component switches remain authoritative: a component whose
 * animationEnabled property is false never animates regardless of this policy.
 * Reduced mode keeps short state transitions but suppresses continuous motion;
 * Disabled mode suppresses all motion.
 * zh_CN: 控件自身的 animationEnabled 开关仍具有更高优先级：关闭后不受全局策略
 * 影响。Reduced 保留缩短后的状态过渡并停止持续动画，Disabled 停止全部动画。
 */
class MotionPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    /**
     * @brief Global motion preference.
     * zh_CN: 全局动效偏好。
     */
    enum class Mode { Full = 0, Reduced = 1, Disabled = 2 };
    Q_ENUM(Mode)

    /**
     * @brief Animation category used when deciding whether motion should run.
     * zh_CN: 决定动画是否运行的动画类型（过渡或持续动画）。
     */
    enum class Kind { Transition = 0, Continuous = 1 };
    Q_ENUM(Kind)

    static MotionPolicy& instance();

    Mode mode() const { return m_mode; }

    /**
     * @brief Changes the global motion mode and notifies active components once.
     * zh_CN: 更改全局动效模式，并仅通知活动控件一次。
     */
    void setMode(Mode mode);

    /**
     * @brief Returns whether the requested motion may run.
     * zh_CN: 返回请求的动效是否可以运行。
     */
    bool shouldAnimate(bool localAnimationEnabled = true, Kind kind = Kind::Transition) const;

    /**
     * @brief Resolves the effective transition duration (in ms) under the active policy.
     * zh_CN: 根据当前动效策略计算实际过渡时长（毫秒）；Full 模式原样返回，Reduced 缩短，Disabled 返回 0。
     */
    int resolvedDuration(int fullDurationMs, bool localAnimationEnabled = true) const;

signals:
    void modeChanged(Mode mode);

private:
    MotionPolicy() = default;

    static constexpr int ReducedTransitionDurationMs = 50;

    Mode m_mode = Mode::Full;
};

} // namespace fluent

#endif // FLUENTQT_COMPONENTS_FOUNDATION_MOTIONPOLICY_H
