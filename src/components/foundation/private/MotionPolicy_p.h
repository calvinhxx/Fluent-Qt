#ifndef FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H
#define FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H

#include <QAbstractAnimation>

class QAnimationGroup;
class QVariantAnimation;

namespace fluent::detail {

// Starts a finite transition under MotionPolicy; Disabled/local-off applies the final
// value immediately and still emits Qt's finished() signal, while Reduced caps duration.
void startMotionTransition(QVariantAnimation* animation,
    int fullDurationMs,
    bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

// Group children must hold their full-motion durations before calling this.
void startMotionTransitionGroup(QAnimationGroup* group,
    bool localAnimationEnabled = true,
    QAbstractAnimation::DeletionPolicy deletionPolicy = QAbstractAnimation::KeepWhenStopped);

} // namespace fluent::detail

#endif // FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_MOTIONPOLICY_P_H
