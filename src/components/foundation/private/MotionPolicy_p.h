#ifndef FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H
#define FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H

#include <QAbstractAnimation>

class QAnimationGroup;
class QVariantAnimation;

namespace fluent::detail {

// Starts a finite state transition through the application MotionPolicy.
// Disabled/local-off transitions still apply their final value and emit the
// normal Qt finished signal; Reduced transitions are capped by the policy.
// zh_CN: 通过应用级 MotionPolicy 启动有限状态过渡。Disabled 或局部关闭时
// 仍应用终值并发送 Qt 原生 finished 信号；Reduced 时按策略缩短时长。
void startMotionTransition(
    QVariantAnimation* animation, int fullDurationMs, bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

// Group children must contain their full-motion durations before this call.
// zh_CN: 调用前，动画组子项须已设置完整动效时长。
void startMotionTransitionGroup(
    QAnimationGroup* group, bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

} // namespace fluent::detail

#endif // FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H
