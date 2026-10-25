#pragma once

#include <QAccessible>
#include <QModelIndex>

namespace compatibility::detail {

inline void retireAccessibleTableChild(QAccessibleInterface* child)
{
#if defined(Q_OS_WASM) && QT_CONFIG(accessibility)
    // Qt WASM retains virtual DOM entries after their interface is retired.
    // zh_CN: Qt WASM 在接口销毁后仍缓存虚拟 DOM，先隐藏其旧表示。
    if (child) {
        QAccessibleEvent event(child, QAccessible::ObjectHide);
        QAccessible::updateAccessibility(&event);
    }
#else
    Q_UNUSED(child);
#endif
}

inline void
refreshAccessibleTableChildren(QAccessibleInterface* table, const QModelIndex& first = {}, const QModelIndex& last = {})
{
#if defined(Q_OS_WASM) && QT_CONFIG(accessibility)
    // Qt WASM does not handle TableModelChanged. ObjectShow updates geometry
    // and names; enumerate the owner's bounded visible children only.
    // zh_CN: Qt WASM 不处理 TableModelChanged；重发可见子项以刷新位置和名称。
    if (!table)
        return;
    const int count = table->childCount();
    for (int i = 0; i < count; ++i) {
        auto* child = table->child(i);
        if (child && child->isValid() && !child->state().invisible) {
            if (first.isValid() && last.isValid()) {
                auto* cell = child->tableCellInterface();
                if (!cell || cell->rowIndex() < first.row() || cell->rowIndex() > last.row()
                    || cell->columnIndex() < first.column() || cell->columnIndex() > last.column())
                    continue;
            }
            QAccessibleEvent event(child, QAccessible::ObjectShow);
            QAccessible::updateAccessibility(&event);
        }
    }
#else
    Q_UNUSED(table);
    Q_UNUSED(first);
    Q_UNUSED(last);
#endif
}

inline void refreshAccessibleTableData(QObject* owner, const QModelIndex& first, const QModelIndex& last)
{
#if defined(Q_OS_WASM) && QT_CONFIG(accessibility)
    // Qt 6.9's browser enable button does not activate QAccessible::isActive(),
    // so QAbstractItemView skips DataChanged notifications. Refresh only the
    // changed visible cells; native backends keep their normal event path.
    // zh_CN: 浏览器启用读屏后 Qt 仍可能跳过表格数据通知，只补发已变更的可见单元格。
    if (owner && !QAccessible::isActive() && first.isValid() && last.isValid())
        refreshAccessibleTableChildren(QAccessible::queryAccessibleInterface(owner), first, last);
#else
    Q_UNUSED(owner);
    Q_UNUSED(first);
    Q_UNUSED(last);
#endif
}

} // namespace compatibility::detail
