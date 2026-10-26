#pragma once

#include <QAbstractItemView>
#include <QAccessible>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QListView>
#include <QPointer>
#include <QTableView>

namespace compatibility::detail {

// A secondary table shares selection and headers without QTableView's automatic
// currentRowChanged -> model.submit connection. The primary view owns submission.
// zh_CN: 辅助表共享选择与表头，但不重复安装 Qt 的换行提交连接，提交由主视图负责。
inline void setPassiveTableSelectionModel(QTableView* view, QItemSelectionModel* selection)
{
    if (!view || !selection)
        return;
    view->verticalHeader()->setSelectionModel(selection);
    view->horizontalHeader()->setSelectionModel(selection);
    view->QAbstractItemView::setSelectionModel(selection);
}

// Match QItemSelection::indexes().value(0) without materializing the full range.
// zh_CN: 保留 Qt 首个可选且启用项的顺序，但不展开整个选择范围。
inline QItemSelection firstAccessibleListSelection(const QItemSelection& selection)
{
    for (const QItemSelectionRange& range : selection) {
        if (!range.isValid())
            continue;
        const QPersistentModelIndex topLeft(range.topLeft());
        const int firstRow = range.top();
        const int lastRow = range.bottom();
        const int firstColumn = range.left();
        const int lastColumn = range.right();
        for (int row = firstRow; row <= lastRow && topLeft.isValid(); ++row) {
            for (int column = firstColumn; column <= lastColumn && topLeft.isValid(); ++column) {
                const QPersistentModelIndex index(QModelIndex(topLeft).sibling(row, column));
                const Qt::ItemFlags flags = index.flags();
                if (index.isValid() && flags.testFlag(Qt::ItemIsSelectable) && flags.testFlag(Qt::ItemIsEnabled))
                    return QItemSelection(index, index);
            }
        }
    }
    return {};
}

// Qt's list implementation expands whole ranges to notify just one item.
// Keep its native mapping, and choose the removal only after Add callbacks.
// zh_CN: 由 Qt 保留原生索引映射；添加回调返回后才寻找移除项，最后重绘完整范围。
template <typename NotifySelection, typename RepaintSelection>
void updateAccessibleListSelection(QListView* view,
    QItemSelection selected,
    QItemSelection deselected,
    NotifySelection&& notify,
    RepaintSelection&& repaint)
{
    const QPointer<QListView> guard(view);
    if (!guard)
        return;
    const QItemSelection addition = firstAccessibleListSelection(selected);
    if (!guard)
        return;
    if (!addition.isEmpty())
        notify(addition, QItemSelection{});
    if (!guard)
        return;
    const QItemSelection removal = firstAccessibleListSelection(deselected);
    if (!guard)
        return;
    if (!removal.isEmpty())
        notify(QItemSelection{}, removal);
    if (guard)
        repaint(selected, deselected);
}

// Preserve the ordinary Qt path when no platform accessibility client is active.
// zh_CN: 无平台无障碍客户端时维持 Qt 原有路径，不增加范围查询。
template <typename NotifySelection, typename RepaintSelection>
void updateListViewSelection(QListView* view,
    const QItemSelection& selected,
    const QItemSelection& deselected,
    NotifySelection&& notify,
    RepaintSelection&& repaint)
{
    if (!view)
        return;
#if QT_CONFIG(accessibility)
    if (QAccessible::isActive()) {
        updateAccessibleListSelection(view, selected, deselected, notify, repaint);
        return;
    }
#endif
    notify(selected, deselected);
}

} // namespace compatibility::detail
