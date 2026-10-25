#pragma once

#include <QAbstractItemView>
#include <QHeaderView>
#include <QItemSelectionModel>
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

} // namespace compatibility::detail
