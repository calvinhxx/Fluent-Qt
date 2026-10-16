#ifndef FLUENTQT_PLUGINS_WIDGETPLUGIN_H
#define FLUENTQT_PLUGINS_WIDGETPLUGIN_H

#include "Plugin.h"
#include "WidgetPluginInterface.h"

namespace fluent::plugins {

/**
 * @brief Abstract widget extension with a Qt interface shared by C++ and Python subclasses.
 * zh_CN: 为 C++ 和 Python 子类提供统一 Qt 接口的抽象控件扩展。
 *
 * Inherits the caller-owned lifecycle of Plugin. A subclass implements metadata,
 * activation, cleanup and the factories below. The interface is declared by this
 * base, so subclasses need no additional Q_INTERFACES declaration. Factory outputs
 * retain the requested parent and remain application-owned after deactivation.
 * zh_CN: 沿用 Plugin 由调用方持有的生命周期。子类实现元数据、激活、清理及下列工厂。
 * 本基类声明 Qt 接口，子类无需再次声明 Q_INTERFACES。工厂产物保留指定父对象，
 * 停用后仍由应用持有。
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
