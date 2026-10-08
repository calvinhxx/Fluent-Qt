#ifndef FLUENTQT_PLUGINS_PLUGINCONTEXT_H
#define FLUENTQT_PLUGINS_PLUGINCONTEXT_H

#include <QPointer>
#include <QString>

namespace fluent::plugins {

class Plugin;
class PluginManager;

/**
 * @brief Provides guarded access to an application host and active dependencies.
 * zh_CN: 提供对应用宿主与已激活依赖的生命周期受保护访问。
 *
 * The host is an optional caller-owned QObject, such as an application controller
 * or QWidget. Plugins expose typed capabilities through their own Qt interfaces;
 * this context does not own business models or introduce a global service store.
 * Use it on the manager's thread; pointers returned by getters are borrowed.
 * zh_CN: 宿主是可选的调用方持有 QObject，例如应用控制器或 QWidget。插件通过自身的
 * Qt 接口暴露类型化能力；上下文不接管业务模型，也不提供全局服务容器。
 * 应在管理器线程使用上下文；访问器返回的指针均为借用指针。
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
