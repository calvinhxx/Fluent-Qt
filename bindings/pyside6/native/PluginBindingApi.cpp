#include "PluginBindingApi.h"

#ifdef FLUENT_QT_HAS_PLUGINS
#include <QPointer>
#include <QWidget>
#include <stdexcept>

namespace fluent::pluginbinding {
namespace {
void throwCallbackError(const QString& error)
{
    if (!error.isEmpty())
        throw std::runtime_error(error.toStdString());
}
} // namespace

PluginAdapter::PluginAdapter(QObject* parent) : plugins::Plugin(parent) {}
void PluginAdapter::_setCallbackError(const QString& error) const
{
    m_callbackError = error;
}
QString PluginAdapter::_callbackError() const
{
    return m_callbackError;
}
plugins::PluginMetadata PluginAdapter::metadata() const
{
    m_callbackError.clear();
    QPointer<const PluginAdapter> lifetime(this);
    auto result = _metadataForBinding();
    if (!lifetime)
        throw std::runtime_error("Plugin was destroyed during its metadata callback.");
    throwCallbackError(m_callbackError);
    return result;
}
bool PluginAdapter::onActivate(const plugins::PluginContext& context, QString* error)
{
    m_callbackError.clear();
    QPointer<PluginAdapter> lifetime(this);
    const bool result = _activateForBinding(context);
    if (!lifetime)
        return false;
    if (error)
        *error = m_callbackError;
    return result;
}
void PluginAdapter::onDeactivate() noexcept
{
    _deactivateForBinding();
}

WidgetPluginAdapter::WidgetPluginAdapter(QObject* parent) : plugins::WidgetPlugin(parent) {}
void WidgetPluginAdapter::_setCallbackError(const QString& error) const
{
    m_callbackError = error;
}
QString WidgetPluginAdapter::_callbackError() const
{
    return m_callbackError;
}
plugins::PluginMetadata WidgetPluginAdapter::metadata() const
{
    m_callbackError.clear();
    QPointer<const WidgetPluginAdapter> lifetime(this);
    auto result = _metadataForBinding();
    if (!lifetime)
        throw std::runtime_error("Plugin was destroyed during its metadata callback.");
    throwCallbackError(m_callbackError);
    return result;
}
bool WidgetPluginAdapter::onActivate(const plugins::PluginContext& context, QString* error)
{
    m_callbackError.clear();
    QPointer<WidgetPluginAdapter> lifetime(this);
    const bool result = _activateForBinding(context);
    if (!lifetime)
        return false;
    if (error)
        *error = m_callbackError;
    return result;
}
void WidgetPluginAdapter::onDeactivate() noexcept
{
    _deactivateForBinding();
}
QStringList WidgetPluginAdapter::widgetKeys() const
{
    m_callbackError.clear();
    QPointer<const WidgetPluginAdapter> lifetime(this);
    auto result = _widgetKeysForBinding();
    if (!lifetime)
        throw std::runtime_error("Plugin was destroyed during its widget-key callback.");
    throwCallbackError(m_callbackError);
    return result;
}
QWidget* WidgetPluginAdapter::createWidget(const QString& key, QWidget* parent, QString* error)
{
    m_callbackError.clear();
    QPointer<WidgetPluginAdapter> lifetime(this);
    QWidget* result = _createWidgetForBinding(key, parent);
    if (!lifetime)
        return result;
    if (error)
        *error = m_callbackError;
    return result;
}

PluginManagerAdapter::PluginManagerAdapter(QObject* host, QObject* parent)
    : plugins::PluginManager(host, parent)
{}
bool PluginManagerAdapter::registerPlugin(QObject* object)
{
    QString error;
    auto* candidate = qobject_cast<plugins::Plugin*>(object);
    if (object && !candidate) {
        m_lastError = QStringLiteral("Object does not implement Plugin.");
        return false;
    }
    QPointer<PluginManagerAdapter> lifetime(this);
    const bool result = plugins::PluginManager::registerPlugin(candidate, &error);
    if (lifetime)
        m_lastError = error;
    return result;
}
bool PluginManagerAdapter::unregisterPlugin(const QString& id)
{
    QString error;
    QPointer<PluginManagerAdapter> lifetime(this);
    const bool result = plugins::PluginManager::unregisterPlugin(id, &error);
    if (lifetime)
        m_lastError = error;
    return result;
}
bool PluginManagerAdapter::activate(const QString& id)
{
    QString error;
    QPointer<PluginManagerAdapter> lifetime(this);
    const bool result = plugins::PluginManager::activate(id, &error);
    if (lifetime)
        m_lastError = error;
    return result;
}
bool PluginManagerAdapter::deactivate(const QString& id)
{
    QString error;
    QPointer<PluginManagerAdapter> lifetime(this);
    const bool result = plugins::PluginManager::deactivate(id, &error);
    if (lifetime)
        m_lastError = error;
    return result;
}
bool PluginManagerAdapter::deactivateAll()
{
    QString error;
    QPointer<PluginManagerAdapter> lifetime(this);
    const bool result = plugins::PluginManager::deactivateAll(&error);
    if (lifetime)
        m_lastError = error;
    return result;
}
QStringList PluginManagerAdapter::pluginIds() const
{
    return plugins::PluginManager::pluginIds();
}
QStringList PluginManagerAdapter::activePluginIds() const
{
    return plugins::PluginManager::activePluginIds();
}
plugins::PluginMetadata PluginManagerAdapter::metadata(const QString& id) const
{
    return plugins::PluginManager::metadata(id);
}
QObject* PluginManagerAdapter::plugin(const QString& id) const
{
    return plugins::PluginManager::plugin(id);
}
QObject* PluginManagerAdapter::activePlugin(const QString& id) const
{
    return plugins::PluginManager::activePlugin(id);
}
bool PluginManagerAdapter::isActive(const QString& id) const
{
    return plugins::PluginManager::isActive(id);
}
plugins::PluginContext PluginManagerAdapter::context() const
{
    return plugins::PluginManager::context();
}
QWidget* PluginManagerAdapter::createWidget(const QString& id, const QString& key, QWidget* parent)
{
    QString error;
    QPointer<PluginManagerAdapter> lifetime(this);
    QWidget* result = plugins::PluginManager::createWidget(id, key, parent, &error);
    if (lifetime)
        m_lastError = error;
    return result;
}
QString PluginManagerAdapter::lastError() const
{
    return m_lastError;
}
} // namespace fluent::pluginbinding

QObject* pluginContextActivePluginForBinding(const fluent::plugins::PluginContext& context,
                                             const QString& id)
{
    return context.activePlugin(id);
}
#endif
