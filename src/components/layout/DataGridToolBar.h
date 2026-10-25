#ifndef FLUENTQT_COMPONENTS_LAYOUT_DATAGRIDTOOLBAR_H
#define FLUENTQT_COMPONENTS_LAYOUT_DATAGRIDTOOLBAR_H

#include <QPointer>
#include <QVariantMap>
#include <QWidget>
#include "components/collections/DataGrid.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

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
 * @brief Compact search, facet and column controls for a borrowed DataGrid.
 * zh_CN: 为借用的 DataGrid 组合紧凑搜索、分面筛选与列管理控件。
 *
 * Search and facet changes are signals; applications own filtering, source
 * models and persistence. This widget never installs or replaces a grid model.
 * zh_CN: 搜索和筛选变化通过信号提供；应用拥有筛选逻辑、源模型和持久化。
 * 工具栏不安装或替换表格模型。
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
    void setDataGrid(fluent::collections::DataGrid* grid);
    QString searchText() const;
    void setSearchText(const QString& text);
    QVariantMap filters() const;

    /**
     * @brief Adds a multi-select facet; duplicate or empty keys are rejected.
     * zh_CN: 添加多选分面；拒绝重复或空键。返回由工具栏拥有的选择控件。
     */
    fluent::basicinput::MultiSelectComboBox*
    addFilter(const QString& key, const QString& label, const QStringList& options);
    /**
     * @brief Selects existing option values, ignoring unknown values and duplicates.
     * zh_CN: 选中已有选项值，忽略未知值和重复值。
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
