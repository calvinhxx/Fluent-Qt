#ifndef FLUENTQT_PLUGINS_WIDGETPLUGIN_H
#define FLUENTQT_PLUGINS_WIDGETPLUGIN_H

#include "Plugin.h"
#include "WidgetPluginInterface.h"

namespace fluent::plugins {

/**
 * @brief Abstract widget extension with a Qt interface shared by C++ and Python subclasses.
 * zh_CN: 为 C++ 和 Python 子类提供统一 Qt 接口的抽象控件扩展。
 *
 * Follows Plugin's caller-managed lifetime. Subclasses implement metadata,
 * activation, cleanup, and the factory methods below (this base already declares
 * Q_INTERFACES). Created widgets keep the requested parent and remain owned by
 * the caller after deactivation.
 * zh_CN: 沿用 Plugin 由调用方管理生命周期的规则。子类实现元数据、启停回调及下列工厂方法
 *（本基类已声明 Q_INTERFACES，子类无需重复声明）。工厂创建的控件保留指定父对象，插件停用后仍归调用方管理。
 */
class WidgetPlugin : public Plugin, public WidgetPluginInterface {
    Q_OBJECT
    Q_INTERFACES(fluent::plugins::WidgetPluginInterface)
public:
    explicit WidgetPlugin(QObject* parent = nullptr);
    ~WidgetPlugin() override;

    /** @brief Lists the widget factory keys available through this extension.
     * zh_CN: 列出此扩展提供的控件工厂标识。 */
    virtual QStringList widgetKeys() const override = 0;
    /** @brief Creates a new widget with the requested parent on the GUI thread.
     * zh_CN: 在 GUI 线程创建指定父对象的新控件。 */
    virtual QWidget* createWidget(const QString& key, QWidget* parent, QString* error = nullptr) override = 0;
};

} // namespace fluent::plugins

#endif // FLUENTQT_PLUGINS_WIDGETPLUGIN_H
