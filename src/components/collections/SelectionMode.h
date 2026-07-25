#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_SELECTIONMODE_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_SELECTIONMODE_H

#include <QObject>

namespace fluent::collections {

Q_NAMESPACE

/**
 * @brief Shared item-selection behavior for Fluent collection views.
 * zh_CN: Fluent 集合视图共用的条目选择行为。
 */
enum class SelectionMode {
    None,
    Single,
    Multiple,
    Extended,
};
Q_ENUM_NS(SelectionMode)

} // namespace fluent::collections

Q_DECLARE_METATYPE(fluent::collections::SelectionMode)

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_SELECTIONMODE_H
