#ifndef FLUENTQT_COMPONENTS_LAYOUT_DATAGRIDTOOLBAR_H
#define FLUENTQT_COMPONENTS_LAYOUT_DATAGRIDTOOLBAR_H

#include <QPointer>
#include <QVariantMap>
#include <QWidget>
#include "../collections/DataGrid.h"
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QLayout;
namespace fluent::collections {
class DataGrid;
}
namespace fluent::textfields {
class LineEdit;
}
namespace fluent::basicinput {
class Button;
class DropDownButton;
class MultiSelectComboBox;
} // namespace fluent::basicinput
namespace fluent::menus_toolbars {
class FluentMenu;
}

namespace fluent::layout {

/**
 * @brief Toolbar with search input, multi-select filters, and a column visibility menu for a DataGrid.
 * Emits signals when search text or filters change; data filtering stays handled by the application.
 * zh_CN: 为 DataGrid 提供搜索框、多选筛选器和列可见性菜单的工具栏。搜索与筛选变更通过信号通知，实际数据过滤由应用自行处理。
 */
class DataGridToolBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(fluent::collections::DataGrid* dataGrid READ dataGrid WRITE setDataGrid NOTIFY dataGridChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QVariantMap filters READ filters NOTIFY filtersChanged)
public:
    explicit DataGridToolBar(QWidget* parent = nullptr);
    ~DataGridToolBar() override;

    fluent::collections::DataGrid* dataGrid() const;
    /** @brief Binds the target DataGrid (still managed by the caller; not deleted by this toolbar).
     * zh_CN: 关联目标 DataGrid（仍由应用管理，不会随工具栏销毁）。 */
    void setDataGrid(fluent::collections::DataGrid* grid);
    QString searchText() const;
    void setSearchText(const QString& text);
    QVariantMap filters() const;

    /**
     * @brief Adds a multi-select filter dropdown owned by the toolbar; empty or duplicate keys are ignored.
     * zh_CN: 添加一个多选筛选下拉框（由工具栏管理生命周期）；空键或重复键将被忽略。
     */
    fluent::basicinput::MultiSelectComboBox*
    addFilter(const QString& key, const QString& label, const QStringList& options);
    /**
     * @brief Selects matching option values for the given filter key, ignoring unknown or duplicate values.
     * zh_CN: 为指定筛选键选中匹配的选项值，忽略未知或重复值。
     */
    void setFilterValues(const QString& key, const QStringList& values);
    void removeFilter(const QString& key);
    void clearFilters();

    fluent::textfields::LineEdit* searchField() const { return m_search; }
    fluent::basicinput::Button* clearButton() const { return m_clear; }
    fluent::basicinput::DropDownButton* columnsButton() const { return m_columns; }
    void onThemeUpdated() override;

signals:
    void dataGridChanged(fluent::collections::DataGrid* grid);
    void searchTextChanged(const QString& text);
    void filtersChanged();

private:
    void rebuildColumnsMenu();
    void updateClearButton();
    void notifyFiltersChanged();
    QPointer<fluent::collections::DataGrid> m_grid;
    QMetaObject::Connection m_gridDestroyed;
    QMap<QString, QPointer<fluent::basicinput::MultiSelectComboBox>> m_filters;
    fluent::textfields::LineEdit* m_search = nullptr;
    fluent::basicinput::Button* m_clear = nullptr;
    fluent::basicinput::DropDownButton* m_columns = nullptr;
    fluent::menus_toolbars::FluentMenu* m_columnsMenu = nullptr;
    QLayout* m_controls = nullptr;
    bool m_resetting = false;
    QVariantMap m_reportedFilters;
};
} // namespace fluent::layout
#endif
