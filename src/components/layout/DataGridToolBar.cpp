#include "components/foundation/private/ControlMetrics_p.h"
#include "DataGridToolBar.h"

#include <QFontMetrics>
#include <QHeaderView>
#include <QLayout>
#include <QSignalBlocker>
#include <QStringListModel>
#include "components/basicinput/Button.h"
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/collections/DataGrid.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"

namespace fluent::layout {
namespace {
void prepareToolBarControl(QWidget* control)
{
    auto policy = control->sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Preferred);
    control->setSizePolicy(policy);
    control->setMinimumHeight(::Spacing::ControlHeight::Standard);
}

void updateColumnDescription(fluent::basicinput::DropDownButton* button, fluent::collections::DataGrid* grid)
{
    if (!grid || !grid->model()) {
        button->setAccessibleDescription(QString());
        return;
    }
    QStringList visible, pinned;
    auto* header = grid->horizontalHeader();
    for (int v = 0; v < header->count(); ++v) {
        const int c = header->logicalIndex(v);
        const QString name = grid->model()->headerData(c, Qt::Horizontal).toString();
        if (grid->isColumnVisible(c))
            visible.append(name);
        if (grid->isColumnPinned(c))
            pinned.append(name);
    }
    button->setAccessibleDescription(DataGridToolBar::tr("Visible columns: %1. Pinned columns: %2.")
            .arg(visible.isEmpty() ? DataGridToolBar::tr("None") : visible.join(QStringLiteral(", ")),
                pinned.isEmpty() ? DataGridToolBar::tr("None") : pinned.join(QStringLiteral(", "))));
}

// The controls stay alive and keep focus when the toolbar wraps.
// zh_CN: 换行仅改变布局，控件及其焦点保持不变。
class ToolBarLayout final : public QLayout {
public:
    explicit ToolBarLayout(QWidget* parent) : QLayout(parent) { setContentsMargins(0, 0, 0, 0); }
    ~ToolBarLayout() override
    {
        while (auto* item = takeAt(0))
            delete item;
    }
    void addItem(QLayoutItem* item) override
    {
        m_items.append(item);
        invalidate();
    }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override { return i >= 0 && i < count() ? m_items.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), false); }
    QSize minimumSize() const override { return QSize(180, controlHeight()); }
    QSize sizeHint() const override { return QSize(720, heightForWidth(720)); }
    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        arrange(rect, true);
    }

private:
    int controlHeight() const
    {
        int height = ::Spacing::ControlHeight::Standard;
        for (auto* item : m_items) {
            if (item->isEmpty() || !item->widget())
                continue;
            const auto* control = item->widget();
            height = qMax(height, control->minimumHeight());
            height = qMax(height, ::fluent::detail::standardControlHeight(control->fontMetrics()));
            if (const auto* button = qobject_cast<const fluent::basicinput::Button*>(control)) {
                if (button->text().contains(QLatin1Char('\n')))
                    height = qMax(height, button->sizeHint().height());
            }
        }
        // Keep one compact height across wrapped rows, growing only for larger text.
        // zh_CN: 换行后仍保持统一的紧凑高度，仅在较大字体需要时增高。
        return ((height + ::Spacing::BaseUnit - 1) / ::Spacing::BaseUnit) * ::Spacing::BaseUnit;
    }

    int arrange(const QRect& rect, bool apply) const
    {
        const int gap = ::Spacing::Small;
        const int height = controlHeight();
        int trailing = 0;
        for (int i = 1; i < count(); ++i) {
            if (!itemAt(i)->isEmpty())
                trailing += qMin(180, itemAt(i)->sizeHint().width()) + gap;
        }
        int x = rect.x(), y = rect.y(), rowHeight = 0;
        for (int i = 0; i < count(); ++i) {
            auto* item = itemAt(i);
            if (item->isEmpty())
                continue;
            int width = i == 0 ? qMax(180, rect.width() - trailing) : qMin(180, item->sizeHint().width());
            width = qMin(qMax(1, rect.width()), width);
            if (x > rect.x() && x + width > rect.right() + 1) {
                x = rect.x();
                y += rowHeight + gap;
                rowHeight = 0;
            }
            if (apply) {
                QRect geometry(x, y, width, height);
                if (parentWidget()->layoutDirection() == Qt::RightToLeft)
                    geometry.moveLeft(rect.right() - (x - rect.x()) - width + 1);
                item->setGeometry(geometry);
            }
            x += width + gap;
            rowHeight = qMax(rowHeight, height);
        }
        return y + rowHeight - rect.y();
    }
    QList<QLayoutItem*> m_items;
};
} // namespace

DataGridToolBar::DataGridToolBar(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("FluentQtDataGridToolBar"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_controls = new ToolBarLayout(this);
    m_search = new fluent::textfields::LineEdit(this);
    m_search->setObjectName(QStringLiteral("dataGridSearch"));
    m_search->setAccessibleName(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_clear = new fluent::basicinput::Button(tr("Clear"), this);
    m_clear->setObjectName(QStringLiteral("dataGridClearFilters"));
    m_clear->setAccessibleName(tr("Clear filters"));
    m_columns = new fluent::basicinput::DropDownButton(tr("Columns"), this);
    m_columns->setObjectName(QStringLiteral("dataGridColumns"));
    m_columnsMenu = new fluent::menus_toolbars::FluentMenu(QString(), this);
    m_columnsMenu->setObjectName(QStringLiteral("dataGridColumnsMenu"));
    m_columns->setMenu(m_columnsMenu);
    m_columns->setEnabled(false);
    prepareToolBarControl(m_search);
    prepareToolBarControl(m_clear);
    prepareToolBarControl(m_columns);
    m_controls->addWidget(m_search);
    m_controls->addWidget(m_clear);
    m_controls->addWidget(m_columns);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& value) {
        if (m_resetting)
            return;
        updateClearButton();
        emit searchTextChanged(value);
    });
    connect(m_clear, &QAbstractButton::clicked, this, &DataGridToolBar::clearFilters);
    connect(m_columnsMenu, &QMenu::aboutToShow, this, &DataGridToolBar::rebuildColumnsMenu);
    updateClearButton();
}

DataGridToolBar::~DataGridToolBar()
{
    if (m_grid)
        disconnect(m_grid, nullptr, this, nullptr);
}
fluent::collections::DataGrid* DataGridToolBar::dataGrid() const
{
    return m_grid.data();
}

void DataGridToolBar::setDataGrid(fluent::collections::DataGrid* grid)
{
    if (m_grid == grid)
        return;
    if (m_grid)
        disconnect(m_grid, nullptr, this, nullptr);
    disconnect(m_gridDestroyed);
    m_columnsMenu->close();
    m_columnsMenu->setProperty("fluentColumnModel", QVariant());
    m_grid = grid;
    m_columns->setEnabled(grid != nullptr);
    updateColumnDescription(m_columns, grid);
    if (grid) {
        const auto refresh = [this] {
            m_columnsMenu->close();
            updateColumnDescription(m_columns, m_grid);
        };
        connect(grid, &fluent::collections::DataGrid::modelChanged, this, [this, refresh] {
            m_columnsMenu->setProperty("fluentColumnModel", QVariant());
            refresh();
        });
        connect(grid, &fluent::collections::DataGrid::columnLayoutChanged, this, refresh);
        m_gridDestroyed = connect(grid, &QObject::destroyed, this, [this] {
            m_grid = nullptr;
            m_columnsMenu->close();
            m_columns->setEnabled(false);
            updateColumnDescription(m_columns, nullptr);
            emit dataGridChanged(nullptr);
        });
    }
    emit dataGridChanged(grid);
}

QString DataGridToolBar::searchText() const
{
    return m_search->text();
}
void DataGridToolBar::setSearchText(const QString& text)
{
    m_search->setText(text);
}

QVariantMap DataGridToolBar::filters() const
{
    QVariantMap result;
    for (auto it = m_filters.cbegin(); it != m_filters.cend(); ++it) {
        if (!it.value())
            continue;
        QStringList values;
        for (const auto& index : it.value()->selectedIndexes())
            values.append(index.data().toString());
        if (!values.isEmpty())
            result.insert(it.key(), values);
    }
    return result;
}

fluent::basicinput::MultiSelectComboBox*
DataGridToolBar::addFilter(const QString& key, const QString& label, const QStringList& options)
{
    if (key.isEmpty() || m_filters.contains(key))
        return nullptr;
    auto* control = new fluent::basicinput::MultiSelectComboBox(this);
    control->setObjectName(QStringLiteral("dataGridFilter_") + key);
    control->setAccessibleName(label);
    control->setPlaceholderText(label);
    QStringList unique = options;
    unique.removeDuplicates();
    control->setModel(new QStringListModel(unique, control));
    control->setSearchEnabled(true);
    control->setSearchPlaceholderText(tr("Search options"));
    prepareToolBarControl(control);
    m_filters.insert(key, control);
    // Keep clear and column management last without recreating their widgets.
    QLayoutItem* columns = m_controls->takeAt(m_controls->count() - 1);
    QLayoutItem* clear = m_controls->takeAt(m_controls->count() - 1);
    m_controls->addWidget(control);
    m_controls->addItem(clear);
    m_controls->addItem(columns);
    connect(control, &fluent::basicinput::MultiSelectComboBox::selectionChanged, this, [this] {
        if (m_resetting)
            return;
        updateClearButton();
        notifyFiltersChanged();
    });
    return control;
}

void DataGridToolBar::setFilterValues(const QString& key, const QStringList& values)
{
    auto* control = m_filters.value(key).data();
    if (!control || !control->model())
        return;
    QList<int> rows;
    for (int row = 0; row < control->model()->rowCount(); ++row)
        if (values.contains(control->model()->index(row, 0).data().toString()))
            rows.append(row);
    control->setSelectedRows(rows);
}

void DataGridToolBar::removeFilter(const QString& key)
{
    auto* control = m_filters.take(key).data();
    if (!control)
        return;
    m_controls->removeWidget(control);
    control->close();
    delete control;
    updateClearButton();
    if (!m_resetting)
        notifyFiltersChanged();
}

void DataGridToolBar::clearFilters()
{
    if (m_resetting)
        return;
    const QString previousSearch = searchText();
    QPointer<DataGridToolBar> guard(this);
    // Facet callbacks can change the collection while it is being cleared.
    // zh_CN: 筛选项回调可修改集合，用弱引用快照遍历。
    const auto controls = m_filters.values();
    m_resetting = true;
    for (const auto& control : controls) {
        if (control)
            control->clearSelection();
        if (!guard)
            return;
    }
    m_search->clear();
    if (!guard)
        return;
    m_resetting = false;
    updateClearButton();
    // Finish native editor mutation before a public callback can destroy it.
    // zh_CN: 原生编辑器完成修改后再通知，允许公开回调销毁工具栏。
    const QString currentSearch = searchText();
    if (previousSearch != currentSearch)
        emit searchTextChanged(currentSearch);
    if (guard)
        notifyFiltersChanged();
}

void DataGridToolBar::notifyFiltersChanged()
{
    const auto current = filters();
    if (m_reportedFilters != current) {
        m_reportedFilters = current;
        emit filtersChanged();
    }
}

void DataGridToolBar::updateClearButton()
{
    m_clear->setEnabled(!searchText().isEmpty() || !filters().isEmpty());
}
void DataGridToolBar::onThemeUpdated()
{
    if (m_controls)
        m_controls->invalidate();
    updateGeometry();
}

void DataGridToolBar::rebuildColumnsMenu()
{
    if (!m_grid || !m_grid->model()) {
        qDeleteAll(
            m_columnsMenu->findChildren<fluent::menus_toolbars::FluentMenu*>(QString(), Qt::FindDirectChildrenOnly));
        m_columnsMenu->clear();
        m_columnsMenu->setProperty("fluentColumnModel", QVariant());
        return;
    }
    updateColumnDescription(m_columns, m_grid);
    const QPointer<fluent::collections::DataGrid> grid = m_grid;
    auto* header = grid->horizontalHeader();
    if (m_columnsMenu->property("fluentColumnModel").value<QObject*>() != grid->model()
        || m_columnsMenu->actions().size() != header->count()) {
        const auto children = m_columnsMenu->findChildren<fluent::menus_toolbars::FluentMenu*>(QString(),
            Qt::FindDirectChildrenOnly);
        qDeleteAll(children);
        m_columnsMenu->clear();
        m_columnsMenu->setProperty("fluentColumnModel", QVariant::fromValue<QObject*>(grid->model()));
    }
    int visible = 0;
    for (int c = 0; c < header->count(); ++c)
        visible += !grid->isColumnHidden(c);
    for (int v = 0; v < header->count(); ++v) {
        const int c = header->logicalIndex(v);
        const QString name = QStringLiteral("dataGridColumn_%1").arg(c);
        auto* menu = m_columnsMenu->findChild<fluent::menus_toolbars::FluentMenu*>(name, Qt::FindDirectChildrenOnly);
        if (!menu) {
            menu = new fluent::menus_toolbars::FluentMenu(QString(), m_columnsMenu);
            menu->setObjectName(name);
            m_columnsMenu->addMenu(menu)->setObjectName(QStringLiteral("dataGridColumnAction_%1").arg(c));
            auto* show = menu->addAction(tr("Visible"));
            show->setObjectName(QStringLiteral("columnVisible"));
            show->setCheckable(true);
            connect(show, &QAction::triggered, this, [grid, c](bool value) {
                if (grid)
                    grid->setColumnVisible(c, value);
            });
            auto* pin = menu->addAction(tr("Pin to start"));
            pin->setObjectName(QStringLiteral("columnPinned"));
            pin->setCheckable(true);
            connect(pin, &QAction::triggered, this, [grid, c](bool value) {
                if (grid)
                    grid->setColumnPinned(c, value);
            });
            menu->addSeparator();
            for (const int delta : {-1, 1}) {
                auto* move = menu->addAction(delta < 0 ? tr("Move earlier") : tr("Move later"));
                move->setObjectName(
                    delta < 0 ? QStringLiteral("columnMoveEarlier") : QStringLiteral("columnMoveLater"));
                connect(move, &QAction::triggered, this, [grid, c, delta] {
                    if (!grid)
                        return;
                    auto* h = grid->horizontalHeader();
                    const int from = h->visualIndex(c);
                    if (from + delta >= 0 && from + delta < h->count())
                        h->moveSection(from, from + delta);
                });
            }
        }
        // Retain action identity across openings, including accessibility nodes.
        // zh_CN: 重复打开时保留动作和无障碍节点的身份。
        menu->setTitle(grid->model()->headerData(c, Qt::Horizontal).toString());
        if (m_columnsMenu->actions().indexOf(menu->menuAction()) != v) {
            m_columnsMenu->removeAction(menu->menuAction());
            m_columnsMenu->insertMenu(m_columnsMenu->actions().value(v), menu);
        }
        auto* show = menu->findChild<QAction*>(QStringLiteral("columnVisible"));
        show->setChecked(grid->isColumnVisible(c));
        show->setEnabled(!show->isChecked() || visible > 1);
        auto* pin = menu->findChild<QAction*>(QStringLiteral("columnPinned"));
        pin->setChecked(grid->isColumnPinned(c));
        for (const int delta : {-1, 1}) {
            auto* move = menu->findChild<QAction*>(
                delta < 0 ? QStringLiteral("columnMoveEarlier") : QStringLiteral("columnMoveLater"));
            const int neighbor = v + delta;
            move->setEnabled(neighbor >= 0 && neighbor < header->count()
                && grid->isColumnPinned(header->logicalIndex(neighbor)) == grid->isColumnPinned(c));
        }
    }
}
} // namespace fluent::layout
