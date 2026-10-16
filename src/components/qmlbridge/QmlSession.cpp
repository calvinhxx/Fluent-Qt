#include "compatibility/private/QuickCompat_p.h"
#include "QmlSession.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include "QmlGraphicsApi_p.h"
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

#if FLUENT_QUICK_USES_RHI
QSGRendererInterface::GraphicsApi quickGraphicsApi(QmlSession::GraphicsApi api)
{
    switch (api) {
    case QmlSession::GraphicsApi::Automatic:
        return QSGRendererInterface::Unknown;
    case QmlSession::GraphicsApi::OpenGL:
        return QSGRendererInterface::OpenGL;
    case QmlSession::GraphicsApi::Direct3D11:
        return QSGRendererInterface::Direct3D11;
    case QmlSession::GraphicsApi::Metal:
        return QSGRendererInterface::Metal;
    }
    return QSGRendererInterface::Unknown;
}
#endif
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
#if FLUENT_QUICK_USES_RHI
    QQuickWindow::setGraphicsApi(quickGraphicsApi(api));
#else
    Q_UNUSED(api)
#endif
}
bool QmlSession::isGraphicsApiCompatible(GraphicsApi api)
{
    if (!isKnownGraphicsApi(api))
        return false;
    if (api == GraphicsApi::Automatic || detail::usesSoftwareQuick())
        return true;
#if FLUENT_QUICK_USES_RHI
    const auto selected = QQuickWindow::graphicsApi();
    return selected == QSGRendererInterface::Software || selected == quickGraphicsApi(api);
#else
    return api == GraphicsApi::OpenGL;
#endif
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
