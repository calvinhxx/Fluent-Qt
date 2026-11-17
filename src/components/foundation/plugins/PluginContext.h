#ifndef FLUENTQT_PLUGINS_PLUGINCONTEXT_H
#define FLUENTQT_PLUGINS_PLUGINCONTEXT_H

#include <QPointer>
#include <QString>

namespace fluent::plugins {

class Plugin;
class PluginManager;

/**
 * @brief Provides guarded non-owning access to an application host and active dependencies.
 * zh_CN: 提供对应用宿主和已激活依赖插件的安全非所有权访问。
 *
 * The optional host is an application QObject (such as a controller or QWidget).
 * Plugins expose typed capabilities through their own Qt interfaces; this context
 * does not own business models or act as a service locator. Call getters on the
 * manager's thread; returned pointers are non-owning and guarded by QPointer.
 * zh_CN: 可选宿主为调用方提供的 QObject（如应用控制器或 QWidget）。插件通过自身的 Qt 接口
 * 暴露能力，上下文不接管业务模型也不提供全局服务定位。须在管理器线程中使用；getter 返回的指针
 * 均不转移所有权且受销毁保护。
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
