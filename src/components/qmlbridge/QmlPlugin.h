#ifndef FLUENTQT_QMLBRIDGE_QMLPLUGIN_H
#define FLUENTQT_QMLBRIDGE_QMLPLUGIN_H

#include <memory>
#include "components/foundation/plugins/WidgetPlugin.h"

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Plugin that controls loading and unloading of QmlView scenes.
 * Deactivating unloads QML scenes while keeping widgets and external models intact; must be called on the GUI thread.
 * zh_CN: 控制 QmlView 加载与卸载的插件。停用时卸载 QML 场景但保留控件和外部模型，须在 GUI 线程调用。
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
     * @brief Attaches an existing QmlView (caller retains ownership); fails if attached elsewhere.
     * zh_CN: 关联已有的 QmlView（仍由应用管理，不会随插件销毁）；若视图已关联其他插件则返回 false。
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
