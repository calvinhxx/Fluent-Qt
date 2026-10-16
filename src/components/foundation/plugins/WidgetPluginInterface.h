#ifndef FLUENTQT_PLUGINS_WIDGETPLUGININTERFACE_H
#define FLUENTQT_PLUGINS_WIDGETPLUGININTERFACE_H

#include <QtPlugin>
#include <QStringList>

class QWidget;

namespace fluent::plugins {

/**
 * @brief Optional Qt interface for plugins that create named QWidget components.
 * zh_CN: 用于创建具名 QWidget 组件的可选 Qt 插件接口。
 *
 * Implement Q_INTERFACES(fluent::plugins::WidgetPluginInterface) on a Plugin
 * subclass. Every successful factory call returns a new widget with the requested
 * parent; a null parent transfers ownership directly to the caller. The manager
 * does not retain or delete delivered widgets. A plugin requiring live resources
 * must have its surfaces closed before deactivation, or detach them safely in its
 * cleanup hook. Factories run on the GUI thread only.
 * A failing or throwing factory releases partial work it has not returned.
 * zh_CN: 在 Plugin 子类声明 Q_INTERFACES。每次成功调用都返回指定父对象的新控件；
 * 父对象为空时所有权直接交给调用方。管理器不保留或销毁已交付的控件。
 * 若控件依赖插件的活动资源，应在停用前关闭控件，或在清理回调中安全解除关联。
 * 工厂仅在 GUI 线程执行。
 * 工厂失败或抛异常时，应自行清理尚未返回的部分产物。
 */
class WidgetPluginInterface {
public:
    virtual ~WidgetPluginInterface() = default;
    virtual QStringList widgetKeys() const = 0;
    virtual QWidget* createWidget(const QString& key, QWidget* parent, QString* error) = 0;
};

} // namespace fluent::plugins

Q_DECLARE_INTERFACE(fluent::plugins::WidgetPluginInterface, "org.fluentqt.WidgetPluginInterface/1.0")

#endif // FLUENTQT_PLUGINS_WIDGETPLUGININTERFACE_H
