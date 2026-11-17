#ifndef FLUENTQT_PLUGINS_PLUGINCONTEXT_H
#define FLUENTQT_PLUGINS_PLUGINCONTEXT_H

#include <QPointer>
#include <QString>

namespace fluent::plugins {

class Plugin;
class PluginManager;

/**
 * @brief Provides access to the optional application host and active dependency plugins during plugin callbacks.
 * Must be called on the PluginManager thread; host() and activePlugin() return guarded pointers that become null if the target is destroyed.
 * zh_CN: 在插件回调中提供可选应用宿主与已激活依赖插件的访问入口。须在 PluginManager 所在线程调用；返回指针受 QPointer 保护，目标销毁后自动为空。
 */
class PluginContext {
public:
    QObject* host() const;
    Plugin* activePlugin(const QString& id) const;

private:
    friend class PluginManager;
    PluginContext(PluginManager* manager, QObject* host);

    QPointer<PluginManager> m_manager;
    QPointer<QObject> m_host;
};

} // namespace fluent::plugins

#endif // FLUENTQT_PLUGINS_PLUGINCONTEXT_H
