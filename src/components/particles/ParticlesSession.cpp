#include "ParticlesSession.h"

#include "ParticlesPlugin.h"
#include "components/foundation/plugins/PluginManager.h"

namespace fluent::particles {
struct ParticlesSession::Private {
    explicit Private(QObject* host) : manager(host) {}
    ParticlesPlugin plugin;
    plugins::PluginManager manager;
};

ParticlesSession::ParticlesSession(QObject* host, QObject* parent) : QObject(parent), d(std::make_shared<Private>(host))
{
    connect(&d->manager, &plugins::PluginManager::pluginActivated, this, [this](const QString&) {
        emit activeChanged(true);
    });
    connect(&d->manager, &plugins::PluginManager::pluginDeactivated, this, [this](const QString&) {
        emit activeChanged(false);
    });
}
ParticlesSession::~ParticlesSession()
{
    // Direct signals may delete this facade during a manager operation.
    auto state = std::move(d);
    state.reset();
}

bool ParticlesSession::activate(QString* error)
{
    const auto state = d;
    if (!state)
        return false;
    const QString id = ParticlesPlugin::pluginId();
    if (!state->manager.plugin(id) && !state->manager.registerPlugin(&state->plugin, error))
        return false;
    return state->manager.activate(id, error);
}
bool ParticlesSession::deactivate(QString* error)
{
    const auto state = d;
    return !state || state->manager.deactivateAll(error);
}
bool ParticlesSession::isActive() const
{
    return d && d->manager.isActive(ParticlesPlugin::pluginId());
}
ParticleBackdrop* ParticlesSession::createBackdrop(QWidget* parent)
{
    const auto state = d;
    return state
        ? qobject_cast<ParticleBackdrop*>(
              state->manager.createWidget(ParticlesPlugin::pluginId(), QStringLiteral("particle-backdrop"), parent))
        : nullptr;
}
bool ParticlesSession::attachBackdrop(ParticleBackdrop* backdrop)
{
    const auto state = d;
    if (!state)
        return false;
    for (QObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == backdrop)
            return false;
    }
    return state->manager.isActive(ParticlesPlugin::pluginId()) && state->plugin.attachBackdrop(backdrop);
}
} // namespace fluent::particles
