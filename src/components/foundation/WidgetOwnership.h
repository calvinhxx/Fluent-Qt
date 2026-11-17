#ifndef WIDGETOWNERSHIP_H
#define WIDGETOWNERSHIP_H

#include <QObject>

namespace fluent {

Q_NAMESPACE

/**
 * @brief Defines how a container handles a caller-supplied QWidget when removing or destroying it.
 * Borrowed leaves it parentless for the caller to destroy, Reparented restores its previous parent widget, and Owned destroys it with the container.
 * zh_CN: 定义容器移除或销毁外部传入 QWidget 时的处理方式。Borrowed 只放进容器显示，由应用自己负责销毁；Reparented 移除时恢复原父控件；Owned 随容器一起销毁。
 */
enum class WidgetOwnership { Borrowed, Reparented, Owned };
Q_ENUM_NS(WidgetOwnership)

} // namespace fluent

Q_DECLARE_METATYPE(fluent::WidgetOwnership)

#endif // WIDGETOWNERSHIP_H
