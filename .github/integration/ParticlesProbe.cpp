#include <FluentQt/Particles.h>

#include <QWidget>
#include <cstring>

// Exercise the optional module through its session; the base target has no particle types.
bool verifyParticlesModule()
{
    QWidget host;
    fluent::particles::ParticlesSession session(&host);
    if (!session.activate())
        return false;
    auto* backdrop = session.createBackdrop(&host);
    if (!backdrop || std::strcmp(backdrop->metaObject()->className(), "fluent::particles::ParticleBackdrop") != 0)
        return false;
    if (backdrop->isGpuAccelerationEnabled())
        return false;
    backdrop->setEffect(fluent::particles::ParticleBackdrop::Starfield);
    return session.deactivate() && backdrop->isAnimationEnabled();
}
