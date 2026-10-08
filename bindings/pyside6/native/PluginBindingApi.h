#ifndef FLUENTQT_PYSIDE6_PLUGINBINDINGAPI_H
#define FLUENTQT_PYSIDE6_PLUGINBINDINGAPI_H

#ifdef FLUENT_QT_HAS_PLUGINS
#include <components/foundation/plugins/Plugin.h>
#include <components/foundation/plugins/PluginContext.h>
#include <components/foundation/plugins/PluginManager.h>
#include <components/foundation/plugins/WidgetPlugin.h>

namespace fluent::pluginbinding {

// Keep Python exception handling and QString output adaptation at the binding
// boundary. Registration, graph validation and rollback stay in PluginManager.
class PluginAdapter : public plugins::Plugin {
public:
    explicit PluginAdapter(QObject* parent = nullptr);
    plugins::PluginMetadata metadata() const final;
    void _setCallbackError(const QString& error) const;
    QString _callbackError() const;

protected:
    virtual plugins::PluginMetadata _metadataForBinding() const = 0;
    virtual bool _activateForBinding(const plugins::PluginContext& context) = 0;
    virtual void _deactivateForBinding() = 0;
    bool onActivate(const plugins::PluginContext& context, QString* error) final;
    void onDeactivate() noexcept final;

private:
    mutable QString m_callbackError;
};

class WidgetPluginAdapter : public plugins::WidgetPlugin {
public:
    explicit WidgetPluginAdapter(QObject* parent = nullptr);
    plugins::PluginMetadata metadata() const final;
    QStringList widgetKeys() const final;
    QWidget* createWidget(const QString& key, QWidget* parent, QString* error) final;
    void _setCallbackError(const QString& error) const;
    QString _callbackError() const;

protected:
    virtual plugins::PluginMetadata _metadataForBinding() const = 0;
    virtual bool _activateForBinding(const plugins::PluginContext& context) = 0;
    virtual void _deactivateForBinding() = 0;
    virtual QStringList _widgetKeysForBinding() const = 0;
    virtual QWidget* _createWidgetForBinding(const QString& key, QWidget* parent) = 0;
    bool onActivate(const plugins::PluginContext& context, QString* error) final;
    void onDeactivate() noexcept final;

private:
    mutable QString m_callbackError;
};

class PluginManagerAdapter : public plugins::PluginManager {
public:
    explicit PluginManagerAdapter(QObject* host = nullptr, QObject* parent = nullptr);
    bool registerPlugin(QObject* plugin);
    bool unregisterPlugin(const QString& id);
    bool activate(const QString& id);
    bool deactivate(const QString& id);
    bool deactivateAll();
    QStringList pluginIds() const;
    QStringList activePluginIds() const;
    plugins::PluginMetadata metadata(const QString& id) const;
    QObject* plugin(const QString& id) const;
    QObject* activePlugin(const QString& id) const;
    bool isActive(const QString& id) const;
    plugins::PluginContext context() const;
    QWidget* createWidget(const QString& pluginId, const QString& key, QWidget* parent = nullptr);
    QString lastError() const;

private:
    QString m_lastError;
};

} // namespace fluent::pluginbinding
QObject* pluginContextActivePluginForBinding(const fluent::plugins::PluginContext& context,
                                             const QString& id);
#endif
#endif // FLUENTQT_PYSIDE6_PLUGINBINDINGAPI_H
