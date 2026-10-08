#ifndef FLUENTQT_PLUGINS_PLUGIN_H
#define FLUENTQT_PLUGINS_PLUGIN_H

#include <QObject>
#include <QStringList>
#include <QVersionNumber>

namespace fluent::plugins {

class PluginContext;
class PluginManager;

/**
 * @brief Describes an extension and the plugin IDs it requires before activation.
 * zh_CN: 描述扩展，以及激活前必须就绪的插件 ID。
 *
 * The manager snapshots this metadata at registration. Version is descriptive;
 * dependencies identify plugins without imposing binary or version compatibility.
 * zh_CN: 管理器在注册时保存元数据快照；版本用于描述，依赖 ID 不承诺二进制或版本兼容。
 */
struct PluginMetadata {
    QString id;
    QString name;
    QVersionNumber version;
    QStringList dependencies;
};

/**
 * @brief Caller-owned extension with manager-controlled activation and cleanup.
 * zh_CN: 由调用方持有、由管理器控制激活与清理的扩展对象。
 *
 * A plugin can be registered with one manager at a time. Hooks run synchronously
 * on that manager's thread; nested manager mutations are rejected. Deactivate
 * before destruction for orderly cleanup. If destroyed unexpectedly, the manager
 * removes it and deactivates surviving dependents; its destructor owns its cleanup.
 * zh_CN: 插件同时只能注册到一个管理器；回调在管理器线程同步执行，嵌套修改会被拒绝。
 * 正常销毁前应先停用；意外销毁会移除注册并停用仍存活的依赖方，自身清理由析构函数负责。
 */
class Plugin : public QObject {
    Q_OBJECT
public:
    explicit Plugin(QObject* parent = nullptr);
    ~Plugin() override;

    virtual PluginMetadata metadata() const = 0;

protected:
    /**
     * @brief Activates this plugin after its dependencies are active.
     * zh_CN: 在依赖就绪后激活插件。
     *
     * Return false with an optional error on failure. The manager calls
     * onDeactivate() after a failed or throwing attempt to release partial work.
     * Context copies remain lifetime-guarded; they do not own the host or manager.
     * zh_CN: 失败时返回 false，可填写错误；失败或抛异常后管理器仍调用 onDeactivate()
     * 清理部分初始化。上下文副本带生命周期保护，不持有宿主或管理器的所有权。
     */
    virtual bool onActivate(const PluginContext& context, QString* error) = 0;

    /**
     * @brief Releases activation resources, including partially initialized work.
     * zh_CN: 释放激活资源，包括部分初始化的资源。
     *
     * Must not throw. Dependencies remain available until their own deactivation.
     * Widgets already returned to the application remain caller-owned.
     * zh_CN: 不得抛异常；依赖在自身停用前仍可访问，已交付应用的控件仍由调用方持有。
     */
    virtual void onDeactivate() noexcept = 0;

private:
    friend class PluginManager;
    PluginManager* m_manager = nullptr;
};

} // namespace fluent::plugins

#endif // FLUENTQT_PLUGINS_PLUGIN_H
