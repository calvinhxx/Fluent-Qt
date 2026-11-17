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
 * @brief Base class for extensions whose activation lifecycle is managed by PluginManager.
 * zh_CN: 由 PluginManager 管理启停顺序的扩展基类（插件对象生命周期仍由调用方负责）。
 *
 * A plugin can be registered with one manager at a time; object ownership stays with
 * the caller. Hooks run synchronously on that manager's thread and reject nested manager
 * mutations. Deactivate before destruction for orderly cleanup; if destroyed while
 * registered, the manager removes it and deactivates surviving dependents.
 * zh_CN: 插件同时只能注册到一个管理器，所有权不发生转移。回调在管理器线程同步执行，
 * 期间会拒绝嵌套修改管理器状态。正常销毁前应先停用；若意外销毁，管理器会移除注册并停用仍存活的依赖方。
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
     * Return false with an optional error on failure; onDeactivate() is called after
     * a failed or throwing attempt to release partial initialization. Context holds
     * guarded non-owning pointers to the host and manager.
     * zh_CN: 失败时返回 false 并可填写错误信息；失败或抛异常后管理器仍会调用 onDeactivate()
     * 清理部分初始化资源。传入的 context 仅保存带销毁保护的宿主与管理器非所有权指针。
     */
    virtual bool onActivate(const PluginContext& context, QString* error) = 0;

    /**
     * @brief Releases activation resources, including partially initialized work.
     * zh_CN: 释放激活期间分配的资源（含部分初始化状态）。
     *
     * Must not throw. Dependencies remain available until their own deactivation,
     * and widgets already returned to the application remain owned by the caller.
     * zh_CN: 不得抛异常。依赖插件在自身停用前仍可访问；此前已交付给应用的控件仍归调用方管理。
     */
    virtual void onDeactivate() noexcept = 0;

private:
    friend class PluginManager;
    PluginManager* m_manager = nullptr;
};

} // namespace fluent::plugins

#endif // FLUENTQT_PLUGINS_PLUGIN_H
