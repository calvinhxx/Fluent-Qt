#ifndef FLUENTQT_COMPONENTS_LAYOUT_PARTICLEBACKDROP_P_H
#define FLUENTQT_COMPONENTS_LAYOUT_PARTICLEBACKDROP_P_H

#include <QtGlobal>
#include <functional>

class QObject;
class QPainter;

namespace fluent::layout {
class ParticleBackdrop;

// Backend-neutral bridge for the optional Spatial renderer. Not installed.
// The base library still owns the only simulation and its visibility policy.
class ParticleBackdropRenderAccess {
public:
    static bool claim(ParticleBackdrop* backdrop, QObject* owner,
                      std::function<void()> requestFrame);
    static void release(ParticleBackdrop* backdrop, QObject* owner);
    static bool isClaimedBy(const ParticleBackdrop* backdrop, const QObject* owner);
    static quint64 revision(const ParticleBackdrop* backdrop);
    static bool isInViewport(const ParticleBackdrop* backdrop);
    static void paintIsolated(ParticleBackdrop* backdrop, QPainter& painter);
};
} // namespace fluent::layout

#endif
