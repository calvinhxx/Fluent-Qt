#include "QmlPlugin.h"

#include <QApplication>
#include <QPointer>
#include <QThread>
#include <QVector>
#include "QmlView.h"

namespace fluent::qmlbridge {
namespace {
bool onGuiThread(const QObject* object)
{
    const auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    return app && QThread::currentThread() == app->thread() && object->thread() == app->thread();
}
} // namespace
struct QmlPlugin::Private {
    QVector<QPointer<QmlView>> views;
    bool active = false;
};
QmlPlugin::QmlPlugin(QObject* parent) : plugins::WidgetPlugin(parent), d(std::make_shared<Private>()) {}
QmlPlugin::~QmlPlugin()
{
    onDeactivate();
}
QString QmlPlugin::pluginId()
{
    return QStringLiteral("fluentqt.qmlbridge");
}
plugins::PluginMetadata QmlPlugin::metadata() const
{
    return {pluginId(), QStringLiteral("QML bridge"), QVersionNumber(1, 0), {}};
}
QStringList QmlPlugin::widgetKeys() const
{
    return {QStringLiteral("qml-view")};
}
bool QmlPlugin::isActive() const
{
    return d->active;
}
bool QmlPlugin::onActivate(const plugins::PluginContext&, QString* error)
{
    if (!onGuiThread(this)) {
        if (error)
            *error = QStringLiteral("QML bridge activation requires QApplication's GUI thread.");
        return false;
    }
    const auto state = d;
    state->active = true;
    const auto views = state->views;
    QPointer<QmlPlugin> lifetime(this);
    for (const auto& view : views) {
        if (view)
            view->setActive(true);
        if (!lifetime)
            return false;
    }
    return true;
}
void QmlPlugin::onDeactivate() noexcept
{
    const auto state = d;
    if (!state->active)
        return;
    state->active = false;
    const auto views = state->views;
    for (const auto& view : views) {
        if (view)
            view->setActive(false);
    }
}
bool QmlPlugin::attachView(QmlView* view)
{
    if (!d->active || !onGuiThread(this) || !view || view->thread() != thread())
        return false;
    for (QObject* ancestor = this; ancestor; ancestor = ancestor->parent()) {
        if (ancestor == view)
            return false;
    }
    if (view->m_plugin && view->m_plugin != this)
        return false;
    for (auto item = d->views.begin(); item != d->views.end();) {
        if (!*item)
            item = d->views.erase(item);
        else if (*item == view)
            return true;
        else
            ++item;
    }
    d->views.append(view);
    view->m_plugin = this;
    QPointer<QmlView> viewLifetime(view);
    QPointer<QmlPlugin> lifetime(this);
    view->setActive(true);
    return lifetime && viewLifetime && d->active;
}
QWidget* QmlPlugin::createWidget(const QString& key, QWidget* parent, QString* error)
{
    if (!d->active || !onGuiThread(this) || key != QStringLiteral("qml-view")) {
        if (error)
            *error = QStringLiteral("QML view factory is unavailable.");
        return nullptr;
    }
    QPointer<QmlPlugin> lifetime(this);
    QPointer<QmlView> view = new QmlView(parent);
    if (!lifetime || !view || !attachView(view)) {
        delete view.data();
        if (error)
            *error = QStringLiteral("Could not attach the QML view.");
        return nullptr;
    }
    return view;
}
} // namespace fluent::qmlbridge
