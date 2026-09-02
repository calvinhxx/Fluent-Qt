#include "components/foundation/MotionPolicy.h"

#include <QtGlobal>

namespace fluent {

MotionPolicy& MotionPolicy::instance()
{
    static MotionPolicy policy;
    return policy;
}

void MotionPolicy::setMode(Mode mode)
{
    switch (mode) {
    case Mode::Full:
    case Mode::Reduced:
    case Mode::Disabled:
        break;
    default:
        return;
    }

    if (m_mode == mode)
        return;

    m_mode = mode;
    emit modeChanged(m_mode);
}

bool MotionPolicy::shouldAnimate(bool localAnimationEnabled, Kind kind) const
{
    if (!localAnimationEnabled || m_mode == Mode::Disabled)
        return false;
    return m_mode == Mode::Full || kind == Kind::Transition;
}

int MotionPolicy::resolvedDuration(int fullDurationMs, bool localAnimationEnabled) const
{
    if (!shouldAnimate(localAnimationEnabled, Kind::Transition))
        return 0;

    const int boundedDuration = qMax(0, fullDurationMs);
    return m_mode == Mode::Reduced ? qMin(boundedDuration, ReducedTransitionDurationMs)
                                   : boundedDuration;
}

} // namespace fluent
