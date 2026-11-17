#ifndef FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_P_H
#define FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_P_H

#include <QtGlobal>
#include <functional>

class QObject;
class QPainter;

namespace fluent::particles {
class ParticleBackdrop;

// Internal bridge for optional GPU compositors; simulation and visibility policy stay in ParticleBackdrop.
class ParticleBackdropRenderAccess {
public:
    static bool claim(ParticleBackdrop* backdrop, QObject* owner, std::function<void()> requestFrame);
    static void release(ParticleBackdrop* backdrop, QObject* owner);
    static bool isClaimedBy(const ParticleBackdrop* backdrop, const QObject* owner);
    static quint64 revision(const ParticleBackdrop* backdrop);
    static bool isInViewport(const ParticleBackdrop* backdrop);
    static void paintIsolated(ParticleBackdrop* backdrop, QPainter& painter);

    // Per-owner pause gate that preserves public settings and auto-lifts when owner is destroyed.
    static void suspend(ParticleBackdrop* backdrop, QObject* owner);
    static void resume(ParticleBackdrop* backdrop, const QObject* owner);
    static bool isSuspendedBy(const ParticleBackdrop* backdrop, const QObject* owner);
};
} // namespace fluent::particles

#endif // FLUENTQT_COMPONENTS_PARTICLES_PARTICLEBACKDROP_P_H
