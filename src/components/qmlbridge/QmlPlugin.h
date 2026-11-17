#ifndef FLUENTQT_QMLBRIDGE_QMLPLUGIN_H
#define FLUENTQT_QMLBRIDGE_QMLPLUGIN_H

#include <memory>
#include "components/foundation/plugins/WidgetPlugin.h"

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Plugin adapter that manages QmlView creation and scene unloading on deactivation.
 * Deactivating unloads attached views without deleting the QmlView widgets or external models;
 * reactivating allows new loads without restoring previous pages. Each view attaches to one plugin
 * at a time, and all calls must run on the GUI thread.
 * zh_CN: 管理 QmlView 创建与停用卸载的插件适配器。
 * 停用时会卸载已关联视图的 QML 场景，但保留 QmlView 控件与外部模型；重新激活后可重新加载，
 * 不会自动恢复旧页面。每个视图同一时间只能关联一个插件，所有调用须在 GUI 线程执行。
 */
class QmlPlugin : public plugins::WidgetPlugin {
    Q_OBJECT
public:
    explicit QmlPlugin(QObject* parent = nullptr);
    ~QmlPlugin() override;
    static QString pluginId();
    plugins::PluginMetadata metadata() const override;
    QStringList widgetKeys() const override;
    QWidget* createWidget(const QString& key, QWidget* parent, QString* error = nullptr) override;
    bool isActive() const;
    /**
     * @brief Attaches an existing QmlView without reparenting or taking ownership; fails if attached elsewhere.
     * zh_CN: 关联已有的 QmlView（不改变父对象或所有权）；若视图已关联其他插件则返回 false。
     */
    bool attachView(QmlView* view);

protected:
    bool onActivate(const plugins::PluginContext& context, QString* error) override;
    void onDeactivate() noexcept override;

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLPLUGIN_H
