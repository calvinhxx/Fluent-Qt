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
        QObject::connect(&policy, &fluent::MotionPolicy::modeChanged, [this](fluent::MotionPolicy::Mode mode) {
            reconcile(mode);
        });
    }

    void start(QVariantAnimation* animation,
        int fullDurationMs,
        bool localAnimationEnabled,
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

        const int duration = fluent::MotionPolicy::instance().resolvedDuration(fullDurationMs, localAnimationEnabled);
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
        // Iterate over a copy because jumping to the end can synchronously emit finished() and mutate m_active.
        // zh_CN: 复制列表后再遍历，避免到达终值同步触发 finished() 回调修改 m_active。
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
                const int reducedRemaining = policy.resolvedDuration(entry.fullDurationMs, entry.localAnimationEnabled);
                animation->setDuration(qMin(entry.fullDurationMs, animation->currentTime() + reducedRemaining));
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

void startFiniteTransition(QVariantAnimation* animation,
    int fullDurationMs,
    bool localAnimationEnabled,
    QAbstractAnimation::DeletionPolicy deletionPolicy)
{
    transitionTracker().start(animation, fullDurationMs, localAnimationEnabled, deletionPolicy);
}

} // namespace fluent::gallery::motion
