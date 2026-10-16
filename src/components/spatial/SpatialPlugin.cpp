#include "SpatialPlugin.h"

#include <QApplication>
#include <QPointer>
#include <QThread>
#include <QVector>
#include "ParticleLayer.h"
#include "SpatialRuntime.h"
#include "SpatialView.h"
#include "components/layout/ParticleBackdrop.h"

namespace fluent::spatial {

namespace {
bool onGuiThread(const QObject* object)
{
    const auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    return app && QThread::currentThread() == app->thread() && object->thread() == app->thread();
}
} // namespace

struct SpatialPlugin::Private {
    struct View {
        QPointer<SpatialView> object;
        SpatialView::RenderMode resumeMode = SpatialView::RenderMode::Auto;
        bool suspended = false;
    };
    QVector<View> views;
    QVector<QPointer<ParticleLayer>> layers;
    bool active = false;
};

SpatialPlugin::SpatialPlugin(QObject* parent) : plugins::WidgetPlugin(parent), d(std::make_unique<Private>()) {}
SpatialPlugin::~SpatialPlugin()
{
    onDeactivate();
}

QString SpatialPlugin::pluginId()
{
    return QStringLiteral("fluentqt.spatial");
}

plugins::PluginMetadata SpatialPlugin::metadata() const
{
    return {pluginId(), QStringLiteral("Spatial"), QVersionNumber(1, 0), {}};
}

QStringList SpatialPlugin::widgetKeys() const
{
    return {QStringLiteral("spatial-view")};
}

bool SpatialPlugin::isActive() const
{
    return d->active;
}

bool SpatialPlugin::onActivate(const plugins::PluginContext&, QString* error)
{
    if (!onGuiThread(this)) {
        if (error)
            *error = QStringLiteral("Spatial activation requires QApplication's GUI thread.");
        return false;
    }
    QPointer<SpatialPlugin> lifetime(this);
    SpatialRuntime::prepareApplication();
    if (!lifetime)
        return false;
    const auto views = d->views;
    for (const auto& record : views) {
        if (record.object && record.suspended) {
            record.object->setRenderMode(record.resumeMode);
            if (!lifetime)
                return false;
        }
    }
    for (auto& record : d->views)
        record.suspended = false;
    d->active = true;
    return true;
}

void SpatialPlugin::onDeactivate() noexcept
{
    if (!d->active)
        return;
    d->active = false;
    for (auto& record : d->views) {
        if (record.object) {
            record.resumeMode = record.object->renderMode();
            record.suspended = true;
        }
    }
    const auto views = d->views;
    const auto layers = d->layers;
    QPointer<SpatialPlugin> lifetime(this);
    for (const auto& record : views) {
        auto view = record.object;
        if (view && !view->isHostTearingDown()) {
            view->setSpatialEnabled(false);
            if (!lifetime)
                return;
            if (view && !view->isHostTearingDown())
                view->setRenderMode(SpatialView::RenderMode::Raster);
            if (!lifetime)
                return;
        }
    }
    for (const auto& layer : layers) {
        if (layer)
            layer->release();
        if (!lifetime)
            return;
    }
}

bool SpatialPlugin::attachView(SpatialView* view)
{
    if (!d->active || !onGuiThread(this) || !view || view->thread() != thread())
        return false;
    for (QObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor == view)
            return false;
    }
    for (auto entry = d->views.begin(); entry != d->views.end();) {
        if (!entry->object) {
            entry = d->views.erase(entry);
        } else {
            if (entry->object == view)
                return true;
            ++entry;
        }
    }
    d->views.append({view, view->renderMode(), false});
    return true;
}

QWidget* SpatialPlugin::createWidget(const QString& key, QWidget* parent, QString* error)
{
    if (!d->active || !onGuiThread(this) || key != QStringLiteral("spatial-view")) {
        if (error)
            *error = QStringLiteral("Spatial view factory is unavailable.");
        return nullptr;
    }
    QPointer<SpatialPlugin> lifetime(this);
    QPointer<SpatialView> view = new SpatialView(parent);
    if (!lifetime || !view || !attachView(view)) {
        delete view.data();
        return nullptr;
    }
    return view.data();
}

ParticleLayer* SpatialPlugin::createParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent)
{
    if (!d->active || !onGuiThread(this) || !backdrop || backdrop->thread() != thread()
        || (parent && parent->thread() != thread()))
        return nullptr;
    QPointer<SpatialPlugin> lifetime(this);
    QPointer<ParticleLayer> layer = new ParticleLayer(backdrop, parent);
    if (!lifetime || !layer) {
        delete layer.data();
        return nullptr;
    }
    for (auto entry = d->layers.begin(); entry != d->layers.end();) {
        if (!*entry)
            entry = d->layers.erase(entry);
        else
            ++entry;
    }
    d->layers.append(layer);
    return layer.data();
}

bool SpatialPlugin::prepareWindow(windowing::Window* window)
{
    return prepareWindow(window, QSurface::OpenGLSurface);
}

bool SpatialPlugin::prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType)
{
    return d->active && onGuiThread(this) && SpatialRuntime::prepareWindow(window, surfaceType);
}

} // namespace fluent::spatial
