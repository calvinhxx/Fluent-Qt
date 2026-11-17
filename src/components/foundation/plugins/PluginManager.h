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
 * zh_CN: 注册可选扩展，并按依赖顺序管理启停生命周期。
 *
 * Does not take ownership of registered plugins or the optional host, and does not
 * discover or load shared libraries. All calls must run on the manager's thread,
 * and registered objects must remain on that thread. Mutations during hooks or
 * manager signals are rejected. Destruction deactivates plugins in reverse
 * activation order without deleting them; do not delete the manager inside its
 * hooks or direct signal handlers.
 * zh_CN: 管理器不接管插件或可选宿主的所有权，也不负责动态库发现或加载。所有调用须在
 * 管理器线程执行，且已注册对象不得跨线程迁移。回调或信号触发期间的修改请求会被拒绝。
 * 析构时按激活逆序停用全部插件但不销毁对象；请勿在回调或直接信号槽中销毁管理器。
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* host = nullptr, QObject* parent = nullptr);
    ~PluginManager() override;

    /**
     * @brief Registers a plugin without taking ownership, activating it, or changing its parent.
     * zh_CN: 注册插件，不接管所有权、不立即激活，也不改变其父对象。
     *
     * IDs must be nonempty and trimmed; dependencies may be registered later.
     * Duplicate IDs, duplicate dependencies, and cross-thread objects are rejected.
     * zh_CN: ID 必须非空且无首尾空白；依赖项可稍后注册。重复 ID、重复依赖或跨线程对象会被拒绝。
     */
    bool registerPlugin(Plugin* plugin, QString* error = nullptr);

    /**
     * @brief Stops active dependents and the plugin, then removes its registration.
     * zh_CN: 停用活动依赖方与当前插件，随后移除注册。
     *
     * Does not delete the plugin; dependent plugins stay registered as inactive.
     * zh_CN: 不会销毁插件对象；依赖方保留注册但变为未激活状态。
     */
    bool unregisterPlugin(const QString& id, QString* error = nullptr);

    /**
     * @brief Activates dependencies first and rolls back this attempt on failure.
     * zh_CN: 先激活依赖项，失败时回滚本次已激活的插件。
     *
     * Missing dependencies and cycles fail before hooks run. Previously active
     * plugins remain active after a failed attempt; already-active targets return true directly.
     * zh_CN: 缺失依赖或存在循环依赖时会在回调执行前失败；本次之前已激活的插件保持不变，
     * 目标已处于激活状态时直接返回 true。
     */
    bool activate(const QString& id, QString* error = nullptr);

    /**
     * @brief Deactivates all active dependents before the requested plugin.
     * zh_CN: 先停用所有活动依赖方，再停用指定插件。
     *
     * Dependencies and unrelated plugins stay active; inactive targets return true directly.
     * zh_CN: 其依赖项与无关插件保持激活；目标未激活时直接返回 true。
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
     * @brief Creates a widget through an active WidgetPluginInterface; ownership follows @p parent.
     * zh_CN: 通过已激活的 WidgetPluginInterface 创建控件；所有权归属 @p parent（@p parent 为空时归调用方）。
     *
     * Requires QApplication and its GUI thread, and does not implicitly activate
     * the plugin. Returns null and an optional error on an unsupported key or failure.
     * zh_CN: 须在 QApplication 的 GUI 线程调用，且不会隐式激活插件；遇到不支持的 key 或失败时返回空指针。
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
