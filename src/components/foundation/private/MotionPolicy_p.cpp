#include "components/foundation/private/MotionPolicy_p.h"

#include <QAnimationGroup>
#include <QList>
#include <QPauseAnimation>
#include <QPointer>
#include <QVariantAnimation>

#include "components/foundation/MotionPolicy.h"

namespace fluent::detail {
namespace {

struct AnimationTiming {
    QPointer<QAbstractAnimation> animation;
    int fullDurationMs = 0;
};

void appendAnimationTimings(QAbstractAnimation* animation, QList<AnimationTiming>& timings)
{
    if (!animation)
        return;

    if (auto* valueAnimation = qobject_cast<QVariantAnimation*>(animation)) {
        timings.append({valueAnimation, qMax(0, valueAnimation->duration())});
        return;
    }
    if (auto* pauseAnimation = qobject_cast<QPauseAnimation*>(animation)) {
        timings.append({pauseAnimation, qMax(0, pauseAnimation->duration())});
        return;
    }
    if (auto* group = qobject_cast<QAnimationGroup*>(animation)) {
        for (int index = 0; index < group->animationCount(); ++index)
            appendAnimationTimings(group->animationAt(index), timings);
    }
}

void applyResolvedTimings(const QList<AnimationTiming>& timings,
    bool localAnimationEnabled,
    bool convergeActiveTransition)
{
    MotionPolicy& policy = MotionPolicy::instance();
    for (const AnimationTiming& timing : timings) {
        QAbstractAnimation* animation = timing.animation.data();
        if (!animation)
            continue;

        int resolvedDuration = policy.resolvedDuration(timing.fullDurationMs, localAnimationEnabled);
        if (convergeActiveTransition && policy.mode() == MotionPolicy::Mode::Reduced) {
            resolvedDuration = qMin(timing.fullDurationMs, animation->currentTime() + resolvedDuration);
        }

        if (auto* valueAnimation = qobject_cast<QVariantAnimation*>(animation)) {
            valueAnimation->setDuration(resolvedDuration);
        } else if (auto* pauseAnimation = qobject_cast<QPauseAnimation*>(animation)) {
            pauseAnimation->setDuration(resolvedDuration);
        }
    }
}

class ActiveTransitionTracker {
public:
    ActiveTransitionTracker()
    {
        MotionPolicy& policy = MotionPolicy::instance();
        QObject::connect(&policy, &MotionPolicy::modeChanged, [this](MotionPolicy::Mode mode) {
            handleModeChanged(mode);
        });
    }

    void
    start(QAbstractAnimation* animation, bool localAnimationEnabled, QAbstractAnimation::DeletionPolicy deletionPolicy)
    {
        if (!animation)
            return;

        prune();
        Entry* tracked = nullptr;
        for (Entry& entry : m_active) {
            if (entry.animation == animation) {
                entry.localAnimationEnabled = localAnimationEnabled;
                entry.timings.clear();
                appendAnimationTimings(animation, entry.timings);
                tracked = &entry;
                break;
            }
        }
        if (!tracked) {
            Entry entry;
            entry.animation = animation;
            entry.localAnimationEnabled = localAnimationEnabled;
            appendAnimationTimings(animation, entry.timings);
            m_active.append(entry);
            tracked = &m_active.last();
        }

        applyResolvedTimings(tracked->timings, localAnimationEnabled, false);
        animation->start(deletionPolicy);
    }

private:
    struct Entry {
        QPointer<QAbstractAnimation> animation;
        bool localAnimationEnabled = true;
        QList<AnimationTiming> timings;
    };

    void prune()
    {
        for (int index = m_active.size() - 1; index >= 0; --index) {
            if (!m_active.at(index).animation || m_active.at(index).animation->state() == QAbstractAnimation::Stopped) {
                m_active.removeAt(index);
            }
        }
    }

    void handleModeChanged(MotionPolicy::Mode mode)
    {
        prune();
        // Finishing an animation may emit finished() synchronously and append to m_active, so iterate a snapshot.
        const QList<Entry> active = m_active;
        for (const Entry& entry : active) {
            QAbstractAnimation* animation = entry.animation.data();
            if (!animation || animation->state() != QAbstractAnimation::Running)
                continue;

            if (mode == MotionPolicy::Mode::Disabled || !entry.localAnimationEnabled) {
                const int endTime = animation->duration();
                if (endTime >= 0)
                    animation->setCurrentTime(endTime);
                continue;
            }
            applyResolvedTimings(entry.timings, entry.localAnimationEnabled, mode == MotionPolicy::Mode::Reduced);
        }
    }

    QList<Entry> m_active;
};

ActiveTransitionTracker& transitionTracker()
{
    static ActiveTransitionTracker tracker;
    return tracker;
}

} // namespace

void startMotionTransition(QVariantAnimation* animation,
    int fullDurationMs,
    bool localAnimationEnabled,
    QAbstractAnimation::DeletionPolicy deletionPolicy)
{
    if (!animation)
        return;
    animation->setDuration(qMax(0, fullDurationMs));
    transitionTracker().start(animation, localAnimationEnabled, deletionPolicy);
}

void startMotionTransitionGroup(QAnimationGroup* group,
    bool localAnimationEnabled,
    QAbstractAnimation::DeletionPolicy deletionPolicy)
{
    transitionTracker().start(group, localAnimationEnabled, deletionPolicy);
}

} // namespace fluent::detail
