#ifndef FLUENTQT_QMLBRIDGE_QMLPLUGIN_H
#define FLUENTQT_QMLBRIDGE_QMLPLUGIN_H

#include <memory>
#include "components/foundation/plugins/WidgetPlugin.h"

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Registers QML widget hosts with the optional plugin lifecycle.
 * zh_CN: 将 QML 控件宿主接入可选插件生命周期。
 * Deactivation unloads tracked scenes, preserving caller-owned widgets and
 * models. Reactivation permits explicit loading and never reloads old pages.
 * A view can belong to one plugin at a time. All calls use the GUI thread.
 * zh_CN: 停用卸载受追踪场景，保留调用方控件和模型。重新激活允许显式加载，
 * 不自动重载旧页面。视图同时只能关联一个插件；所有调用均须在 GUI 线程执行。
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
    /** @brief Tracks an existing borrowed view without changing its parent.
     * zh_CN: 追踪借用视图，不改变父对象。 */
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
