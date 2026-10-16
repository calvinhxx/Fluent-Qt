#include "SpatialSession.h"

#include "SpatialPlugin.h"
#include "SpatialView.h"
#include "components/foundation/plugins/PluginManager.h"

namespace fluent::spatial {
struct SpatialSession::Private {
    explicit Private(QObject* host) : manager(host) {}
    SpatialPlugin plugin;
    plugins::PluginManager manager;
};

SpatialSession::SpatialSession(QObject* host, QObject* parent) : QObject(parent), d(std::make_shared<Private>(host))
{
    connect(&d->manager, &plugins::PluginManager::pluginActivated, this, [this](const QString&) {
        emit activeChanged(true);
    });
    connect(&d->manager, &plugins::PluginManager::pluginDeactivated, this, [this](const QString&) {
        emit activeChanged(false);
    });
}
SpatialSession::~SpatialSession()
{
    // In-flight calls retain the manager when a direct signal deletes the facade.
    auto state = std::move(d);
    state.reset();
}

bool SpatialSession::activate(QString* error)
{
    const auto state = d;
    if (!state)
        return false;
    const QString id = SpatialPlugin::pluginId();
    if (!state->manager.plugin(id) && !state->manager.registerPlugin(&state->plugin, error))
        return false;
    return state->manager.activate(id, error);
}
bool SpatialSession::deactivate(QString* error)
{
    const auto state = d;
    return !state || state->manager.deactivateAll(error);
}
bool SpatialSession::isActive() const
{
    return d && d->manager.isActive(SpatialPlugin::pluginId());
}
SpatialView* SpatialSession::createView(QWidget* parent)
{
    const auto state = d;
    return state
        ? qobject_cast<SpatialView*>(
              state->manager.createWidget(SpatialPlugin::pluginId(), QStringLiteral("spatial-view"), parent))
        : nullptr;
}
bool SpatialSession::attachView(SpatialView* view)
{
    const auto state = d;
    if (!state)
        return false;
    for (QObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == view)
            return false;
    }
    return state->manager.isActive(SpatialPlugin::pluginId()) && state->plugin.attachView(view);
}
ParticleLayer* SpatialSession::createParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent)
{
    const auto state = d;
    return state && state->manager.isActive(SpatialPlugin::pluginId())
        ? state->plugin.createParticleLayer(backdrop, parent)
        : nullptr;
}
bool SpatialSession::prepareWindow(windowing::Window* window)
{
    const auto state = d;
    return state && state->plugin.prepareWindow(window);
}
bool SpatialSession::prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType)
{
    const auto state = d;
    return state && state->plugin.prepareWindow(window, surfaceType);
}
} // namespace fluent::spatial
