#include "ParticlesPlugin.h"

#include "ParticleBackdrop_p.h"

#include <QApplication>
#include <QPointer>
#include <QThread>
#include <QVector>

namespace fluent::particles {
namespace {
bool onGuiThread(const QObject* object)
{
    const auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    return app && QThread::currentThread() == app->thread() && object->thread() == app->thread();
}
} // namespace

struct ParticlesPlugin::Private {
    QVector<QPointer<ParticleBackdrop>> backdrops;
    bool active = false;

    // Pausing goes through a private gate so animationEnabled stays the caller's setting.
    // Returns false when a notification destroyed the plugin.
    static bool setPaused(ParticlesPlugin* plugin, bool paused)
    {
        const auto backdrops = plugin->d->backdrops;
        QPointer<ParticlesPlugin> lifetime(plugin);
        for (const auto& backdrop : backdrops) {
            if (!backdrop)
                continue;
            if (paused)
                ParticleBackdropRenderAccess::suspend(backdrop, plugin);
            else
                ParticleBackdropRenderAccess::resume(backdrop, plugin);
            if (!lifetime)
                return false;
        }
        return true;
    }
};

ParticlesPlugin::ParticlesPlugin(QObject* parent) : plugins::WidgetPlugin(parent), d(std::make_unique<Private>()) {}
ParticlesPlugin::~ParticlesPlugin()
{
    // A destroyed plugin no longer pauses anything; each backdrop follows its own settings again.
    for (const auto& backdrop : d->backdrops)
        ParticleBackdropRenderAccess::resume(backdrop, this);
}

QString ParticlesPlugin::pluginId()
{
    return QStringLiteral("fluentqt.particles");
}

plugins::PluginMetadata ParticlesPlugin::metadata() const
{
    return {pluginId(), QStringLiteral("Particles"), QVersionNumber(1, 0), {}};
}

QStringList ParticlesPlugin::widgetKeys() const
{
    return {QStringLiteral("particle-backdrop")};
}

bool ParticlesPlugin::isActive() const
{
    return d->active;
}

bool ParticlesPlugin::onActivate(const plugins::PluginContext&, QString* error)
{
    if (!onGuiThread(this)) {
        if (error)
            *error = QStringLiteral("Particles activation requires QApplication's GUI thread.");
        return false;
    }
    d->active = true;
    return Private::setPaused(this, false);
}

void ParticlesPlugin::onDeactivate() noexcept
{
    if (!d->active)
        return;
    d->active = false;
    Private::setPaused(this, true);
}

bool ParticlesPlugin::attachBackdrop(ParticleBackdrop* backdrop)
{
    if (!d->active || !onGuiThread(this) || !backdrop || backdrop->thread() != thread())
        return false;
    for (QObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == backdrop)
            return false;
    }
    for (auto entry = d->backdrops.begin(); entry != d->backdrops.end();) {
        if (!*entry) {
            entry = d->backdrops.erase(entry);
        } else {
            if (*entry == backdrop)
                return true;
            ++entry;
        }
    }
    d->backdrops.append(backdrop);
    return true;
}

QWidget* ParticlesPlugin::createWidget(const QString& key, QWidget* parent, QString* error)
{
    if (!d->active || !onGuiThread(this) || key != QStringLiteral("particle-backdrop")) {
        if (error)
            *error = QStringLiteral("Particle backdrop factory is unavailable.");
        return nullptr;
    }
    QPointer<ParticlesPlugin> lifetime(this);
    QPointer<ParticleBackdrop> backdrop = new ParticleBackdrop(parent);
    if (!lifetime || !backdrop || !attachBackdrop(backdrop)) {
        delete backdrop.data();
        return nullptr;
    }
    return backdrop;
}
} // namespace fluent::particles
