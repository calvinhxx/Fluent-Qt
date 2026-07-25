#ifndef WIDGETOWNERSHIP_H
#define WIDGETOWNERSHIP_H

#include <QObject>

namespace fluent {

Q_NAMESPACE

/**
 * @brief Defines how a widget host releases caller-supplied content.
 * zh_CN: 定义控件宿主释放调用方所提供 QWidget 时的生命周期语义。
 *
 * QWidget containers must reparent content while it is displayed. Borrowed
 * content is detached to a parentless widget when released, Reparented content
 * is restored to the QWidget parent it had when attached, and Owned content is
 * destroyed by the host.
 * zh_CN: QWidget 容器在显示内容期间必须重设其父对象。Borrowed 内容释放后变为
 * 无父控件，Reparented 内容恢复到接入时的 QWidget 父控件，Owned 内容由宿主销毁。
 */
enum class WidgetOwnership {
    Borrowed,
    Reparented,
    Owned
};
Q_ENUM_NS(WidgetOwnership)

} // namespace fluent

Q_DECLARE_METATYPE(fluent::WidgetOwnership)

#endif // WIDGETOWNERSHIP_H
