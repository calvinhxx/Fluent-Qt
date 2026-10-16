#include "compatibility/private/quick/QuickCompat_p.h"
#include "QmlSession.h"

#include "QmlPlugin.h"
#include "QmlView.h"
#include "components/foundation/plugins/PluginManager.h"

namespace fluent::qmlbridge {
namespace {
bool isKnownGraphicsApi(QmlSession::GraphicsApi api)
{
    switch (api) {
    case QmlSession::GraphicsApi::Automatic:
    case QmlSession::GraphicsApi::OpenGL:
    case QmlSession::GraphicsApi::Direct3D11:
    case QmlSession::GraphicsApi::Metal:
        return true;
    }
    return false;
}

::compatibility::detail::QuickGraphicsApi quickGraphicsApi(QmlSession::GraphicsApi api)
{
    switch (api) {
    case QmlSession::GraphicsApi::Automatic:
        return ::compatibility::detail::QuickGraphicsApi::Automatic;
    case QmlSession::GraphicsApi::OpenGL:
        return ::compatibility::detail::QuickGraphicsApi::OpenGL;
    case QmlSession::GraphicsApi::Direct3D11:
        return ::compatibility::detail::QuickGraphicsApi::Direct3D11;
    case QmlSession::GraphicsApi::Metal:
        return ::compatibility::detail::QuickGraphicsApi::Metal;
    }
    return ::compatibility::detail::QuickGraphicsApi::Automatic;
}
} // namespace

struct QmlSession::Private {
    explicit Private(QObject* host) : manager(host) {}
    QmlPlugin plugin;
    plugins::PluginManager manager;
};
QmlSession::QmlSession(QObject* host, QObject* parent) : QObject(parent), d(std::make_shared<Private>(host))
{
    connect(&d->manager, &plugins::PluginManager::pluginActivated, this, [this](const QString&) {
        emit activeChanged(true);
    });
    connect(&d->manager, &plugins::PluginManager::pluginDeactivated, this, [this](const QString&) {
        emit activeChanged(false);
    });
}
QmlSession::~QmlSession()
{
    auto state = std::move(d);
    state.reset();
}
void QmlSession::prepareApplication()
{
    prepareApplication(GraphicsApi::OpenGL);
}
void QmlSession::prepareApplication(GraphicsApi api)
{
    if (!isKnownGraphicsApi(api))
        return;
    ::compatibility::detail::prepareQuickGraphicsApi(quickGraphicsApi(api));
}
bool QmlSession::isGraphicsApiCompatible(GraphicsApi api)
{
    if (!isKnownGraphicsApi(api))
        return false;
    return ::compatibility::detail::quickGraphicsApiCompatible(quickGraphicsApi(api));
}
bool QmlSession::activate(QString* error)
{
    const auto state = d;
    if (!state)
        return false;
    const auto id = QmlPlugin::pluginId();
    if (!state->manager.plugin(id) && !state->manager.registerPlugin(&state->plugin, error))
        return false;
    return state->manager.activate(id, error);
}
bool QmlSession::deactivate(QString* error)
{
    const auto state = d;
    return !state || state->manager.deactivateAll(error);
}
bool QmlSession::isActive() const
{
    return d && d->manager.isActive(QmlPlugin::pluginId());
}
QmlView* QmlSession::createView(QWidget* parent)
{
    const auto state = d;
    if (!state)
        return nullptr;
    return qobject_cast<QmlView*>(
        state->manager.createWidget(QmlPlugin::pluginId(), QStringLiteral("qml-view"), parent));
}
bool QmlSession::attachView(QmlView* view)
{
    const auto state = d;
    if (!state)
        return false;
    for (QObject* ancestor = this; ancestor; ancestor = ancestor->parent()) {
        if (ancestor == view)
            return false;
    }
    return state->manager.isActive(QmlPlugin::pluginId()) && state->plugin.attachView(view);
}
} // namespace fluent::qmlbridge
