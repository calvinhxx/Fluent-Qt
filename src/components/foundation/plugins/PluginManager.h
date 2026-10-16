#ifndef FLUENTQT_PLUGINS_PLUGINMANAGER_H
#define FLUENTQT_PLUGINS_PLUGINMANAGER_H

#include <QObject>
#include <memory>

#include "Plugin.h"
#include "PluginContext.h"

class QWidget;

namespace fluent::plugins {

/**
 * @brief Registers optional extensions and manages dependency-ordered lifetimes.
 * zh_CN: 注册可选扩展，并按依赖顺序管理激活生命周期。
 *
 * Registered plugins and the optional host remain caller-owned. All calls must
 * use the manager's thread. Keep registered plugins, host and manager on that
 * thread; do not move them while plugins are registered.
 * Mutations during hooks or manager signals are rejected;
 * queue a later call instead. Destruction deactivates plugins in reverse activation
 * order without destroying borrowed plugins. Do not delete the manager in its
 * hooks or direct signal handlers. This module manages linked plugin objects,
 * without discovering or loading shared libraries.
 * zh_CN: 注册的插件与可选宿主仍由调用方持有；所有调用须在管理器线程执行。
 * 存在已注册插件期间，不得迁移插件、宿主或管理器的线程归属。
 * 回调或管理器信号期间的修改会被拒绝，应排队稍后执行。管理器析构时按激活逆序停用，
 * 不销毁借用插件。不得在回调或直接信号处理中销毁管理器。本模块管理已链接插件对象，
 * 不负责发现或加载动态库。
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* host = nullptr, QObject* parent = nullptr);
    ~PluginManager() override;

    /**
     * @brief Registers a borrowed plugin without activating it or changing its parent.
     * zh_CN: 注册借用插件，不激活，也不改变其父对象。
     *
     * IDs must be nonempty and trimmed; dependencies may be registered later.
     * Duplicate IDs, duplicate dependencies and cross-thread objects are rejected.
     * zh_CN: ID 必须非空且无首尾空白；依赖可稍后注册。重复 ID、重复依赖与跨线程对象会被拒绝。
     */
    bool registerPlugin(Plugin* plugin, QString* error = nullptr);

    /**
     * @brief Stops active dependents and the plugin, then removes its registration.
     * zh_CN: 停用活动依赖方与插件，再移除注册。
     *
     * Does not delete the plugin. Dependent plugins stay registered but inactive.
     * zh_CN: 不销毁插件；依赖方保留注册但处于停用状态。
     */
    bool unregisterPlugin(const QString& id, QString* error = nullptr);

    /**
     * @brief Activates dependencies first and rolls back this attempt on failure.
     * zh_CN: 先激活依赖，失败时回滚本次操作。
     *
     * Missing dependencies and cycles fail before hooks run. Already active
     * plugins remain active after a failed attempt; repeated activation is a no-op.
     * zh_CN: 缺失依赖或循环在执行回调前失败；本次操作前已激活的插件不会被回滚，重复激活无操作。
     */
    bool activate(const QString& id, QString* error = nullptr);

    /**
     * @brief Deactivates all active dependents before the requested plugin.
     * zh_CN: 先停用所有活动依赖方，再停用指定插件。
     *
     * Dependencies and unrelated plugins stay active. Repeated deactivation is a no-op.
     * zh_CN: 依赖与无关插件保持激活；重复停用无操作。
     */
    bool deactivate(const QString& id, QString* error = nullptr);

    /**
     * @brief Deactivates every active plugin in reverse activation order.
     * zh_CN: 按激活逆序停用全部活动插件。
     */
    bool deactivateAll(QString* error = nullptr);

    QStringList pluginIds() const;
    QStringList activePluginIds() const;
    PluginMetadata metadata(const QString& id) const;
    Plugin* plugin(const QString& id) const;
    Plugin* activePlugin(const QString& id) const;
    bool isActive(const QString& id) const;
    const PluginContext& context() const;

    /**
     * @brief Creates a caller-owned widget through an active WidgetPluginInterface.
     * zh_CN: 通过已激活的 WidgetPluginInterface 创建调用方持有的控件。
     *
     * Requires QApplication and its GUI thread. Does not implicitly activate the
     * plugin. Returns null and an optional error on an unsupported key or failure.
     * zh_CN: 需要 QApplication 及其 GUI 线程，不隐式激活插件；不支持的 key 或失败会返回空指针及可选错误。
     */
    QWidget*
    createWidget(const QString& pluginId, const QString& key, QWidget* parent = nullptr, QString* error = nullptr);

signals:
    void pluginRegistered(const QString& id);
    void pluginUnregistered(const QString& id);
    void pluginActivated(const QString& id);
    void pluginDeactivated(const QString& id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::plugins

#endif // FLUENTQT_PLUGINS_PLUGINMANAGER_H
