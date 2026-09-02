#include "GalleryMotion.h"

#include <QList>
#include <QPointer>
#include <QVariantAnimation>

#include "components/foundation/MotionPolicy.h"

namespace fluent::gallery::motion {
namespace {

struct ActiveTransition {
    QPointer<QVariantAnimation> animation;
    int fullDurationMs = 0;
    bool localAnimationEnabled = true;
};

class ActiveTransitionTracker {
public:
    ActiveTransitionTracker()
    {
        auto& policy = fluent::MotionPolicy::instance();
        QObject::connect(&policy, &fluent::MotionPolicy::modeChanged,
                         [this](fluent::MotionPolicy::Mode mode) { reconcile(mode); });
    }

    void start(QVariantAnimation* animation, int fullDurationMs, bool localAnimationEnabled,
               QAbstractAnimation::DeletionPolicy deletionPolicy)
    {
        if (!animation)
            return;

        prune();
        ActiveTransition* tracked = nullptr;
        for (ActiveTransition& entry : m_active) {
            if (entry.animation == animation) {
                tracked = &entry;
                break;
            }
        }
        if (!tracked) {
            m_active.append({animation, qMax(0, fullDurationMs), localAnimationEnabled});
        } else {
            tracked->fullDurationMs = qMax(0, fullDurationMs);
            tracked->localAnimationEnabled = localAnimationEnabled;
        }

        const int duration = fluent::MotionPolicy::instance().resolvedDuration(
            fullDurationMs, localAnimationEnabled);
        animation->setDuration(duration);
        animation->start(deletionPolicy);
    }

private:
    void prune()
    {
        for (int index = m_active.size() - 1; index >= 0; --index) {
            const auto& animation = m_active.at(index).animation;
            if (!animation || animation->state() == QAbstractAnimation::Stopped)
                m_active.removeAt(index);
        }
    }

    void reconcile(fluent::MotionPolicy::Mode mode)
    {
        prune();
        // Reaching an end value can synchronously emit finished and start a new
        // transition. Traverse a snapshot so those callbacks cannot invalidate
        // this pass. zh_CN: 到达终值会同步发送 finished，回调可能再启动新过渡；
        // 遍历快照以避免当前收敛过程失效。
        const QList<ActiveTransition> active = m_active;
        for (const ActiveTransition& entry : active) {
            QVariantAnimation* animation = entry.animation.data();
            if (!animation || animation->state() != QAbstractAnimation::Running)
                continue;

            auto& policy = fluent::MotionPolicy::instance();
            if (mode == fluent::MotionPolicy::Mode::Disabled || !entry.localAnimationEnabled) {
                animation->setCurrentTime(animation->duration());
                continue;
            }

            if (mode == fluent::MotionPolicy::Mode::Reduced) {
                const int reducedRemaining =
                    policy.resolvedDuration(entry.fullDurationMs, entry.localAnimationEnabled);
                animation->setDuration(
                    qMin(entry.fullDurationMs, animation->currentTime() + reducedRemaining));
            } else {
                animation->setDuration(entry.fullDurationMs);
            }
        }
    }

    QList<ActiveTransition> m_active;
};

ActiveTransitionTracker& transitionTracker()
{
    static ActiveTransitionTracker tracker;
    return tracker;
}

} // namespace

void startFiniteTransition(QVariantAnimation* animation, int fullDurationMs,
                           bool localAnimationEnabled,
                           QAbstractAnimation::DeletionPolicy deletionPolicy)
{
    transitionTracker().start(animation, fullDurationMs, localAnimationEnabled, deletionPolicy);
}

} // namespace fluent::gallery::motion
