#ifndef FLUENTQT_PLUGINS_PLUGINMANAGER_H
#define FLUENTQT_PLUGINS_PLUGINMANAGER_H

#include <QObject>
#include <memory>

#include "Plugin.h"
#include "PluginContext.h"

class QWidget;

namespace fluent::plugins {

/**
 * @brief Registers optional plugins and manages dependency-ordered activation and deactivation.
 * Plugins and the optional host remain owned by the caller on the manager's thread; do not mutate registrations or delete the manager inside plugin hooks or direct signal handlers.
 * zh_CN: 注册可选插件并按依赖顺序管理启停。插件与可选宿主仍由应用管理生命周期且须与管理器同线程；请勿在插件回调或直接连接的信号槽中修改注册状态或销毁管理器。
 */
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* host = nullptr, QObject* parent = nullptr);
    ~PluginManager() override;

    /**
     * @brief Registers a plugin without activating it (plugin remains owned by the application).
     * Requires a nonempty trimmed ID on the manager's thread; dependencies may be registered later.
     * zh_CN: 注册插件但不立即激活（插件仍由应用管理生命周期）。插件 ID 须非空且无首尾空白，对象须位于管理器线程；依赖插件可稍后注册。
     */
    bool registerPlugin(Plugin* plugin, QString* error = nullptr);

    /**
     * @brief Stops active dependents and the plugin, then removes its registration without deleting the plugin.
     * zh_CN: 停用活动依赖方及该插件后移除其注册（不会销毁插件对象）。
     */
    bool unregisterPlugin(const QString& id, QString* error = nullptr);

    /**
     * @brief Activates @p id and any inactive dependencies, rolling back newly activated plugins on failure.
     * Missing or cyclic dependencies fail before hooks run; returns true directly if already active.
     * zh_CN: 按顺序激活依赖项与目标插件；失败时自动回滚本次新激活的插件。若缺少依赖或存在循环依赖会直接失败，目标已激活时直接返回 true。
     */
    bool activate(const QString& id, QString* error = nullptr);

    /**
     * @brief Deactivates active dependents first, then deactivates @p id (leaving its dependencies active).
     * zh_CN: 先停用所有活动依赖方，再停用 @p id（其依赖项保持激活状态）。
     */
    bool deactivate(const QString& id, QString* error = nullptr);

    /**
     * @brief Deactivates all active plugins in reverse activation order.
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
     * @brief Creates a widget from an active WidgetPluginInterface on the GUI thread; @p parent takes Qt ownership when non-null.
     * Does not activate the plugin automatically; returns nullptr if the plugin is inactive or the key is unsupported.
     * zh_CN: 在 GUI 线程通过已激活插件的 WidgetPluginInterface 创建控件；传入非空 @p parent 时由其接管 Qt 父子生命周期。不会自动激活插件，插件未激活或 key 不支持时返回空指针。
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
