#include "DataGridAccessibility_p.h"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QHash>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QPointer>

#include <limits>

#include "components/collections/DataGrid.h"
#include "compatibility/QtCompat.h"

namespace fluent::collections::detail {

#if QT_CONFIG(accessibility)

namespace {

class DataGridAccessible;

class DataGridAccessibleCell final : public QAccessibleInterface,
                                     public QAccessibleTableCellInterface,
                                     public QAccessibleActionInterface {
public:
    DataGridAccessibleCell(DataGrid* view,
                           const QModelIndex& index)
        : m_view(view)
        , m_index(index)
    {
    }

    bool isValid() const override;
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override;
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text type) const override;
    void setText(QAccessible::Text type, const QString& text) override;
    QRect rect() const override;
    QAccessible::Role role() const override { return QAccessible::Cell; }
    QAccessible::State state() const override;
    void* interface_cast(QAccessible::InterfaceType type) override;

    bool isSelected() const override;
    QList<QAccessibleInterface*> columnHeaderCells() const override;
    QList<QAccessibleInterface*> rowHeaderCells() const override;
    int columnIndex() const override { return m_index.column(); }
    int rowIndex() const override { return m_index.row(); }
    int columnExtent() const override { return 1; }
    int rowExtent() const override { return 1; }
    QAccessibleInterface* table() const override;

    QStringList actionNames() const override;
    void doAction(const QString& actionName) override;
    QStringList keyBindingsForAction(const QString&) const override
    {
        return {};
    }

    const QPersistentModelIndex& modelIndex() const { return m_index; }

private:
    QPointer<DataGrid> m_view;
    QPersistentModelIndex m_index;
};

class DataGridAccessibleHeader final : public QAccessibleInterface {
public:
    DataGridAccessibleHeader(DataGrid* view, int section,
                             Qt::Orientation orientation)
        : m_view(view)
        , m_section(section)
        , m_orientation(orientation)
    {
    }

    bool isValid() const override;
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override;
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text type) const override;
    void setText(QAccessible::Text, const QString&) override {}
    QRect rect() const override;
    QAccessible::Role role() const override
    {
        return m_orientation == Qt::Horizontal
            ? QAccessible::ColumnHeader
            : QAccessible::RowHeader;
    }
    QAccessible::State state() const override;

    int section() const { return m_section; }
    Qt::Orientation orientation() const { return m_orientation; }

private:
    QHeaderView* header() const;

    QPointer<DataGrid> m_view;
    int m_section = -1;
    Qt::Orientation m_orientation = Qt::Horizontal;
};

class DataGridAccessibleCorner final : public QAccessibleInterface {
public:
    explicit DataGridAccessibleCorner(DataGrid* view)
        : m_view(view)
    {
    }

    bool isValid() const override { return !m_view.isNull(); }
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override;
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text) const override { return {}; }
    void setText(QAccessible::Text, const QString&) override {}
    QRect rect() const override { return {}; }
    QAccessible::Role role() const override { return QAccessible::Pane; }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        result.readOnly = true;
        result.invisible = true;
        return result;
    }

private:
    QPointer<DataGrid> m_view;
};

class DataGridAccessible final : public QAccessibleWidget,
                                 public QAccessibleTableInterface
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
                               , public QAccessibleSelectionInterface
#endif
{
public:
    explicit DataGridAccessible(DataGrid* view)
        : QAccessibleWidget(view, QAccessible::Table)
    {
    }

    ~DataGridAccessible() override { clearChildCache(); }

    QAccessible::State state() const override;
    QAccessibleInterface* childAt(int x, int y) const override;
    QAccessibleInterface* focusChild() const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface* child) const override;
    QAccessibleInterface* child(int logicalIndex) const override;
    void* interface_cast(QAccessible::InterfaceType type) override;

    QAccessibleInterface* caption() const override { return nullptr; }
    QAccessibleInterface* summary() const override { return nullptr; }
    QAccessibleInterface* cellAt(int row, int column) const override;
    int selectedCellCount() const override;
    QList<QAccessibleInterface*> selectedCells() const override;
    QString columnDescription(int column) const override;
    QString rowDescription(int row) const override;
    int selectedColumnCount() const override;
    int selectedRowCount() const override;
    int columnCount() const override;
    int rowCount() const override;
    QList<int> selectedColumns() const override;
    QList<int> selectedRows() const override;
    bool isColumnSelected(int column) const override;
    bool isRowSelected(int row) const override;
    bool selectRow(int row) override;
    bool selectColumn(int column) override;
    bool unselectRow(int row) override;
    bool unselectColumn(int column) override;
    void modelChange(QAccessibleTableModelChangeEvent* event) override;

#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    int selectedItemCount() const override { return selectedCellCount(); }
    QList<QAccessibleInterface*> selectedItems() const override
    {
        return selectedCells();
    }
    bool isSelected(QAccessibleInterface* childItem) const override;
    bool select(QAccessibleInterface* childItem) override;
    bool unselect(QAccessibleInterface* childItem) override;
    bool selectAll() override;
    bool clear() override;
#endif

    QAccessibleInterface* headerCell(int section,
                                     Qt::Orientation orientation) const;

private:
    DataGrid* view() const
    {
        return static_cast<DataGrid*>(widget());
    }
    int logicalIndex(const QModelIndex& index) const;
    QAccessibleInterface* interfaceForLogicalChild(int logicalChild) const;
    QVector<int> visibleLogicalChildren() const;
    void clearChildCache() const;
    bool selectIndex(const QModelIndex& index, bool selected);

    mutable QHash<int, QAccessible::Id> m_childToId;
};

bool viewIsReadOnly(const DataGrid* view)
{
    return !view
        || view->editTriggers() == QAbstractItemView::NoEditTriggers;
}

bool indexIsReadOnly(const DataGrid* view,
                     const QModelIndex& index)
{
    return viewIsReadOnly(view)
        || !index.isValid()
        || !(index.flags() & Qt::ItemIsEditable);
}

QItemSelectionModel::SelectionFlags behaviorFlag(
    const DataGrid* view)
{
    if (!view)
        return QItemSelectionModel::NoUpdate;
    switch (view->selectionBehavior()) {
    case QAbstractItemView::SelectRows:
        return QItemSelectionModel::Rows;
    case QAbstractItemView::SelectColumns:
        return QItemSelectionModel::Columns;
    case QAbstractItemView::SelectItems:
        return QItemSelectionModel::NoUpdate;
    }
    return QItemSelectionModel::NoUpdate;
}

bool DataGridAccessibleCell::isValid() const
{
    return m_view && m_view->model() && m_index.isValid()
        && m_index.model() == m_view->model();
}

QAccessibleInterface* DataGridAccessibleCell::parent() const
{
    return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
}

QString DataGridAccessibleCell::text(QAccessible::Text type) const
{
    if (!isValid())
        return {};

    switch (type) {
    case QAccessible::Name: {
        const QString semantic =
            m_index.data(Qt::AccessibleTextRole).toString();
        return semantic.isEmpty()
            ? m_index.data(Qt::DisplayRole).toString()
            : semantic;
    }
    case QAccessible::Description:
        return m_index.data(Qt::AccessibleDescriptionRole).toString();
    case QAccessible::Value:
        return m_index.data(Qt::EditRole).toString();
    default:
        return {};
    }
}

void DataGridAccessibleCell::setText(QAccessible::Text type,
                                     const QString& text)
{
    if (!isValid() || indexIsReadOnly(m_view, m_index)
        || type != QAccessible::Value) {
        return;
    }
    m_view->model()->setData(m_index, text, Qt::EditRole);
}

QRect DataGridAccessibleCell::rect() const
{
    if (!isValid() || m_view->isRowHidden(m_index.row())
        || m_view->isColumnHidden(m_index.column())) {
        return {};
    }
    QRect result = m_view->visualRect(m_index);
    if (!result.isValid())
        return {};
    result.translate(m_view->viewport()->mapToGlobal(QPoint(0, 0)));
    return result;
}

QAccessible::State DataGridAccessibleCell::state() const
{
    QAccessible::State result;
    if (!isValid()) {
        result.invalid = true;
        result.invisible = true;
        return result;
    }

    const Qt::ItemFlags flags = m_index.flags();
    result.disabled = !m_view->isEnabled()
        || !(flags & Qt::ItemIsEnabled);
    result.readOnly = indexIsReadOnly(m_view, m_index);
    result.editable = !result.disabled && !result.readOnly;
    result.selectable = !result.disabled
        && (flags & Qt::ItemIsSelectable)
        && m_view->QAbstractItemView::selectionMode() != QAbstractItemView::NoSelection;
    result.focusable = result.selectable;
    result.multiSelectable = result.selectable
        && m_view->QAbstractItemView::selectionMode() == QAbstractItemView::MultiSelection;
    result.extSelectable = result.selectable
        && m_view->QAbstractItemView::selectionMode() == QAbstractItemView::ExtendedSelection;

    if (QItemSelectionModel* selection = m_view->selectionModel()) {
        result.selected = selection->isSelected(m_index);
        result.focused = selection->currentIndex() == m_index;
    }

    const QVariant checkState = m_index.data(Qt::CheckStateRole);
    result.checkable = (flags & Qt::ItemIsUserCheckable)
        && checkState.isValid();
    result.checked = result.checkable
        && checkState.toInt() == Qt::Checked;

    const QRect cellRect = rect();
    const QRect viewportRect(
        m_view->viewport()->mapToGlobal(QPoint(0, 0)),
        m_view->viewport()->size());
    result.invisible = cellRect.isEmpty()
        || !m_view->isVisible()
        || !viewportRect.intersects(cellRect);
    result.offscreen = !cellRect.isEmpty()
        && !viewportRect.intersects(cellRect);
    return result;
}

void* DataGridAccessibleCell::interface_cast(
    QAccessible::InterfaceType type)
{
    if (type == QAccessible::TableCellInterface)
        return static_cast<QAccessibleTableCellInterface*>(this);
    if (type == QAccessible::ActionInterface)
        return static_cast<QAccessibleActionInterface*>(this);
    return nullptr;
}

bool DataGridAccessibleCell::isSelected() const
{
    return isValid() && m_view->selectionModel()
        && m_view->selectionModel()->isSelected(m_index);
}

QList<QAccessibleInterface*>
DataGridAccessibleCell::columnHeaderCells() const
{
    auto* root = dynamic_cast<DataGridAccessible*>(table());
    QAccessibleInterface* header = root
        ? root->headerCell(m_index.column(), Qt::Horizontal)
        : nullptr;
    return header ? QList<QAccessibleInterface*>{header}
                  : QList<QAccessibleInterface*>{};
}

QList<QAccessibleInterface*> DataGridAccessibleCell::rowHeaderCells() const
{
    auto* root = dynamic_cast<DataGridAccessible*>(table());
    QAccessibleInterface* header = root
        ? root->headerCell(m_index.row(), Qt::Vertical)
        : nullptr;
    return header ? QList<QAccessibleInterface*>{header}
                  : QList<QAccessibleInterface*>{};
}

QAccessibleInterface* DataGridAccessibleCell::table() const
{
    return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
}

QStringList DataGridAccessibleCell::actionNames() const
{
    const QAccessible::State currentState = state();
    return currentState.selectable
        ? QStringList{QAccessibleActionInterface::toggleAction()}
        : QStringList{};
}

void DataGridAccessibleCell::doAction(const QString& actionName)
{
    if (actionName != QAccessibleActionInterface::toggleAction()
        || !isValid() || !m_view->selectionModel()
        || !state().selectable) {
        return;
    }

    QItemSelectionModel::SelectionFlags command = behaviorFlag(m_view);
    if (m_view->QAbstractItemView::selectionMode() == QAbstractItemView::SingleSelection)
        command |= QItemSelectionModel::ClearAndSelect;
    else
        command |= QItemSelectionModel::Toggle;
    m_view->selectionModel()->setCurrentIndex(
        m_index, QItemSelectionModel::NoUpdate);
    m_view->selectionModel()->select(m_index, command);
}

QHeaderView* DataGridAccessibleHeader::header() const
{
    if (!m_view)
        return nullptr;
    return m_orientation == Qt::Horizontal
        ? m_view->horizontalHeader()
        : m_view->verticalHeader();
}

bool DataGridAccessibleHeader::isValid() const
{
    if (!m_view || !m_view->model() || m_section < 0)
        return false;
    return m_orientation == Qt::Horizontal
        ? m_section < m_view->model()->columnCount(m_view->rootIndex())
        : m_section < m_view->model()->rowCount(m_view->rootIndex());
}

QAccessibleInterface* DataGridAccessibleHeader::parent() const
{
    return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
}

QString DataGridAccessibleHeader::text(QAccessible::Text type) const
{
    if (!isValid())
        return {};
    const int role = type == QAccessible::Name
        ? Qt::AccessibleTextRole
        : type == QAccessible::Description
            ? Qt::AccessibleDescriptionRole
            : -1;
    if (role < 0)
        return {};
    const QString semantic = m_view->model()
        ->headerData(m_section, m_orientation, role).toString();
    if (type != QAccessible::Name || !semantic.isEmpty())
        return semantic;
    return m_view->model()
        ->headerData(m_section, m_orientation, Qt::DisplayRole).toString();
}

QRect DataGridAccessibleHeader::rect() const
{
    QHeaderView* headerView = header();
    if (!isValid() || !headerView || !headerView->isVisible()
        || headerView->isSectionHidden(m_section)) {
        return {};
    }
    const QPoint origin = headerView->mapToGlobal(QPoint(0, 0));
    const int position = headerView->sectionViewportPosition(m_section);
    const int size = headerView->sectionSize(m_section);
    return m_orientation == Qt::Horizontal
        ? QRect(origin.x() + position, origin.y(), size, headerView->height())
        : QRect(origin.x(), origin.y() + position, headerView->width(), size);
}

QAccessible::State DataGridAccessibleHeader::state() const
{
    QAccessible::State result;
    QHeaderView* headerView = header();
    result.readOnly = true;
    result.disabled = !headerView || !headerView->isEnabled();
    const QRect headerRect = headerView
        ? QRect(headerView->mapToGlobal(QPoint(0, 0)), headerView->size())
        : QRect();
    const QRect sectionRect = rect();
    result.invisible = !isValid() || sectionRect.isEmpty()
        || !headerRect.intersects(sectionRect);
    result.offscreen = !sectionRect.isEmpty()
        && !headerRect.intersects(sectionRect);
    return result;
}

QAccessibleInterface* DataGridAccessibleCorner::parent() const
{
    return m_view ? QAccessible::queryAccessibleInterface(m_view) : nullptr;
}

QAccessible::State DataGridAccessible::state() const
{
    QAccessible::State result = QAccessibleWidget::state();
    const DataGrid* grid = view();
    result.readOnly = viewIsReadOnly(grid);
    result.editable = !result.disabled && !result.readOnly;
    if (grid) {
        result.multiSelectable =
            grid->QAbstractItemView::selectionMode() == QAbstractItemView::MultiSelection;
        result.extSelectable =
            grid->QAbstractItemView::selectionMode() == QAbstractItemView::ExtendedSelection;
    }
    return result;
}

QAccessibleInterface* DataGridAccessible::childAt(int x, int y) const
{
    DataGrid* grid = view();
    if (!grid)
        return nullptr;
    const QPoint globalPoint(x, y);

    QHeaderView* horizontal = grid->horizontalHeader();
    if (horizontal && horizontal->isVisible()
        && QRect(horizontal->mapToGlobal(QPoint(0, 0)), horizontal->size())
               .contains(globalPoint)) {
        const int section = horizontal->logicalIndexAt(
            horizontal->mapFromGlobal(globalPoint));
        return headerCell(section, Qt::Horizontal);
    }

    QHeaderView* vertical = grid->verticalHeader();
    if (vertical && vertical->isVisible()
        && QRect(vertical->mapToGlobal(QPoint(0, 0)), vertical->size())
               .contains(globalPoint)) {
        const int section = vertical->logicalIndexAt(
            vertical->mapFromGlobal(globalPoint));
        return headerCell(section, Qt::Vertical);
    }

    const QModelIndex index = grid->indexAt(
        grid->viewport()->mapFromGlobal(globalPoint));
    return index.isValid() ? cellAt(index.row(), index.column()) : nullptr;
}

QAccessibleInterface* DataGridAccessible::focusChild() const
{
    const DataGrid* grid = view();
    const QModelIndex current = grid ? grid->currentIndex() : QModelIndex();
    return current.isValid()
        ? cellAt(current.row(), current.column())
        : nullptr;
}

int DataGridAccessible::childCount() const
{
    return visibleLogicalChildren().size();
}

int DataGridAccessible::logicalIndex(const QModelIndex& index) const
{
    const DataGrid* grid = view();
    if (!grid || !index.isValid() || index.model() != grid->model())
        return -1;
    const qint64 value = static_cast<qint64>(index.row() + 1)
            * static_cast<qint64>(columnCount() + 1)
        + index.column() + 1;
    return value <= std::numeric_limits<int>::max()
        ? static_cast<int>(value)
        : -1;
}

int DataGridAccessible::indexOfChild(
    const QAccessibleInterface* accessibleChild) const
{
    if (!accessibleChild)
        return -1;
    int logicalChild = -1;
    if (auto* cell = dynamic_cast<const DataGridAccessibleCell*>(accessibleChild))
        logicalChild = logicalIndex(cell->modelIndex());
    if (auto* header =
            dynamic_cast<const DataGridAccessibleHeader*>(accessibleChild)) {
        logicalChild = header->orientation() == Qt::Horizontal
            ? header->section() + 1
            : (header->section() + 1) * (columnCount() + 1);
    }
    if (dynamic_cast<const DataGridAccessibleCorner*>(accessibleChild))
        logicalChild = 0;
    return visibleLogicalChildren().indexOf(logicalChild);
}

QVector<int> DataGridAccessible::visibleLogicalChildren() const
{
    QVector<int> children;
    DataGrid* grid = view();
    if (!grid || !grid->model())
        return children;

    const int rows = rowCount();
    const int columns = columnCount();
    const int columnsWithHeader = columns + 1;
    children.append(0);

    QVector<int> visibleColumns;
    QHeaderView* header = grid->horizontalHeader();
    const int viewportWidth = grid->viewport()
        ? grid->viewport()->width() : 0;
    if (header) {
        for (int visual = 0; visual < header->count(); ++visual) {
            const int logical = header->logicalIndex(visual);
            if (logical < 0 || logical >= columns
                || grid->isColumnHidden(logical)) {
                continue;
            }
            const int position = header->sectionViewportPosition(logical);
            const int extent = header->sectionSize(logical);
            if (viewportWidth <= 0
                || (position < viewportWidth && position + extent > 0)) {
                visibleColumns.append(logical);
            }
        }
    }
    if (visibleColumns.isEmpty()) {
        const int fallbackColumns = qMin(columns, 8);
        for (int column = 0; column < fallbackColumns; ++column) {
            if (!grid->isColumnHidden(column))
                visibleColumns.append(column);
        }
    }

    for (int column : visibleColumns)
        children.append(column + 1);

    int firstRow = rows > 0 ? grid->rowAt(0) : -1;
    if (firstRow < 0 && rows > 0)
        firstRow = 0;
    int lastRow = rows > 0 && grid->viewport()
        ? grid->rowAt(qMax(0, grid->viewport()->height() - 1))
        : -1;
    if (lastRow < firstRow && firstRow >= 0) {
        const int rowExtent = qMax(1, grid->verticalHeader()
            ? grid->verticalHeader()->defaultSectionSize() : 1);
        const int visibleEstimate = grid->viewport()
            ? grid->viewport()->height() / rowExtent + 2 : 8;
        lastRow = qMin(rows - 1, firstRow + qMax(1, visibleEstimate));
    }

    for (int row = firstRow; row >= 0 && row <= lastRow && row < rows; ++row) {
        if (grid->isRowHidden(row))
            continue;
        children.append((row + 1) * columnsWithHeader);
        for (int column : visibleColumns) {
            children.append((row + 1) * columnsWithHeader + column + 1);
        }
    }
    return children;
}

QAccessibleInterface* DataGridAccessible::child(int childIndex) const
{
    const QVector<int> children = visibleLogicalChildren();
    if (childIndex < 0 || childIndex >= children.size())
        return nullptr;
    return interfaceForLogicalChild(children.at(childIndex));
}

QAccessibleInterface* DataGridAccessible::interfaceForLogicalChild(
    int logicalChild) const
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || logicalChild < 0)
        return nullptr;

    auto cached = m_childToId.constFind(logicalChild);
    if (cached != m_childToId.constEnd()) {
        if (QAccessibleInterface* interface =
                QAccessible::accessibleInterface(cached.value())) {
            return interface;
        }
        m_childToId.remove(logicalChild);
    }

    const int columnsWithHeader = columnCount() + 1;
    const int row = logicalChild / columnsWithHeader;
    const int column = logicalChild % columnsWithHeader;
    QAccessibleInterface* interface = nullptr;
    if (row == 0 && column == 0) {
        interface = new DataGridAccessibleCorner(grid);
    } else if (row == 0) {
        interface = new DataGridAccessibleHeader(
            grid, column - 1, Qt::Horizontal);
    } else if (column == 0) {
        interface = new DataGridAccessibleHeader(
            grid, row - 1, Qt::Vertical);
    } else {
        const QModelIndex index = grid->model()->index(
            row - 1, column - 1, grid->rootIndex());
        if (!index.isValid())
            return nullptr;
        interface = new DataGridAccessibleCell(grid, index);
    }

    const QAccessible::Id id =
        QAccessible::registerAccessibleInterface(interface);
    m_childToId.insert(logicalChild, id);
    return interface;
}

void* DataGridAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::TableInterface)
        return static_cast<QAccessibleTableInterface*>(this);
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    if (type == QAccessible::SelectionInterface)
        return static_cast<QAccessibleSelectionInterface*>(this);
#endif
    return QAccessibleWidget::interface_cast(type);
}

QAccessibleInterface* DataGridAccessible::cellAt(int row, int column) const
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || row < 0 || column < 0
        || row >= rowCount() || column >= columnCount()) {
        return nullptr;
    }
    return interfaceForLogicalChild(logicalIndex(
        grid->model()->index(row, column, grid->rootIndex())));
}

int DataGridAccessible::selectedCellCount() const
{
    const DataGrid* grid = view();
    return grid && grid->selectionModel()
        ? grid->selectionModel()->selectedIndexes().size()
        : 0;
}

QList<QAccessibleInterface*> DataGridAccessible::selectedCells() const
{
    QList<QAccessibleInterface*> result;
    const DataGrid* grid = view();
    if (!grid || !grid->selectionModel())
        return result;
    const QModelIndexList indexes =
        grid->selectionModel()->selectedIndexes();
    result.reserve(indexes.size());
    for (const QModelIndex& index : indexes) {
        if (QAccessibleInterface* cell = cellAt(index.row(), index.column()))
            result.append(cell);
    }
    return result;
}

QString DataGridAccessible::columnDescription(int column) const
{
    const DataGrid* grid = view();
    return grid && grid->model() && column >= 0 && column < columnCount()
        ? grid->model()->headerData(
              column, Qt::Horizontal, Qt::DisplayRole).toString()
        : QString();
}

QString DataGridAccessible::rowDescription(int row) const
{
    const DataGrid* grid = view();
    return grid && grid->model() && row >= 0 && row < rowCount()
        ? grid->model()->headerData(
              row, Qt::Vertical, Qt::DisplayRole).toString()
        : QString();
}

int DataGridAccessible::selectedColumnCount() const
{
    return selectedColumns().size();
}

int DataGridAccessible::selectedRowCount() const
{
    return selectedRows().size();
}

int DataGridAccessible::columnCount() const
{
    const DataGrid* grid = view();
    return grid && grid->model()
        ? grid->model()->columnCount(grid->rootIndex())
        : 0;
}

int DataGridAccessible::rowCount() const
{
    const DataGrid* grid = view();
    return grid && grid->model()
        ? grid->model()->rowCount(grid->rootIndex())
        : 0;
}

QList<int> DataGridAccessible::selectedColumns() const
{
    QList<int> result;
    const DataGrid* grid = view();
    if (!grid || !grid->selectionModel())
        return result;
    const QModelIndexList indexes =
        grid->selectionModel()->selectedColumns(0);
    result.reserve(indexes.size());
    for (const QModelIndex& index : indexes) {
        if (index.parent() == grid->rootIndex())
            result.append(index.column());
    }
    return result;
}

QList<int> DataGridAccessible::selectedRows() const
{
    QList<int> result;
    const DataGrid* grid = view();
    if (!grid || !grid->selectionModel())
        return result;
    const QModelIndexList indexes =
        grid->selectionModel()->selectedRows(0);
    result.reserve(indexes.size());
    for (const QModelIndex& index : indexes) {
        if (index.parent() == grid->rootIndex())
            result.append(index.row());
    }
    return result;
}

bool DataGridAccessible::isColumnSelected(int column) const
{
    const DataGrid* grid = view();
    return grid && grid->selectionModel() && column >= 0
        && column < columnCount()
        && grid->selectionModel()->isColumnSelected(
            column, grid->rootIndex());
}

bool DataGridAccessible::isRowSelected(int row) const
{
    const DataGrid* grid = view();
    return grid && grid->selectionModel() && row >= 0 && row < rowCount()
        && grid->selectionModel()->isRowSelected(row, grid->rootIndex());
}

bool DataGridAccessible::selectRow(int row)
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || !grid->selectionModel()
        || row < 0 || row >= rowCount()
        || grid->QAbstractItemView::selectionMode() == QAbstractItemView::NoSelection) {
        return false;
    }
    const QModelIndex index = grid->model()->index(
        row, 0, grid->rootIndex());
    QItemSelectionModel::SelectionFlags command =
        QItemSelectionModel::Select | QItemSelectionModel::Rows;
    if (grid->QAbstractItemView::selectionMode() == QAbstractItemView::SingleSelection)
        command |= QItemSelectionModel::Clear;
    grid->selectionModel()->select(index, command);
    return isRowSelected(row);
}

bool DataGridAccessible::selectColumn(int column)
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || !grid->selectionModel()
        || column < 0 || column >= columnCount()
        || grid->QAbstractItemView::selectionMode() == QAbstractItemView::NoSelection) {
        return false;
    }
    const QModelIndex index = grid->model()->index(
        0, column, grid->rootIndex());
    QItemSelectionModel::SelectionFlags command =
        QItemSelectionModel::Select | QItemSelectionModel::Columns;
    if (grid->QAbstractItemView::selectionMode() == QAbstractItemView::SingleSelection)
        command |= QItemSelectionModel::Clear;
    grid->selectionModel()->select(index, command);
    return isColumnSelected(column);
}

bool DataGridAccessible::unselectRow(int row)
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || !grid->selectionModel()
        || row < 0 || row >= rowCount()) {
        return false;
    }
    grid->selectionModel()->select(
        grid->model()->index(row, 0, grid->rootIndex()),
        QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    return !isRowSelected(row);
}

bool DataGridAccessible::unselectColumn(int column)
{
    DataGrid* grid = view();
    if (!grid || !grid->model() || !grid->selectionModel()
        || column < 0 || column >= columnCount()) {
        return false;
    }
    grid->selectionModel()->select(
        grid->model()->index(0, column, grid->rootIndex()),
        QItemSelectionModel::Deselect | QItemSelectionModel::Columns);
    return !isColumnSelected(column);
}

void DataGridAccessible::modelChange(
    QAccessibleTableModelChangeEvent* event)
{
    if (!event
        || event->modelChangeType()
            != QAccessibleTableModelChangeEvent::DataChanged) {
        clearChildCache();
    }
}

#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
bool DataGridAccessible::isSelected(
    QAccessibleInterface* childItem) const
{
    auto* cell = dynamic_cast<DataGridAccessibleCell*>(childItem);
    return cell && cell->isSelected();
}

bool DataGridAccessible::selectIndex(const QModelIndex& index, bool selected)
{
    DataGrid* grid = view();
    if (!grid || !index.isValid() || !grid->selectionModel()
        || grid->QAbstractItemView::selectionMode() == QAbstractItemView::NoSelection) {
        return false;
    }
    QItemSelectionModel::SelectionFlags command = behaviorFlag(grid);
    command |= selected ? QItemSelectionModel::Select
                        : QItemSelectionModel::Deselect;
    if (selected
        && grid->QAbstractItemView::selectionMode() == QAbstractItemView::SingleSelection) {
        command |= QItemSelectionModel::Clear;
    }
    grid->selectionModel()->select(index, command);
    return grid->selectionModel()->isSelected(index) == selected;
}

bool DataGridAccessible::select(QAccessibleInterface* childItem)
{
    auto* cell = dynamic_cast<DataGridAccessibleCell*>(childItem);
    return cell && selectIndex(cell->modelIndex(), true);
}

bool DataGridAccessible::unselect(QAccessibleInterface* childItem)
{
    auto* cell = dynamic_cast<DataGridAccessibleCell*>(childItem);
    return cell && selectIndex(cell->modelIndex(), false);
}

bool DataGridAccessible::selectAll()
{
    DataGrid* grid = view();
    if (!grid || !grid->selectionModel()
        || grid->QAbstractItemView::selectionMode() == QAbstractItemView::NoSelection) {
        return false;
    }
    grid->selectAll();
    return selectedCellCount() > 0 || rowCount() == 0 || columnCount() == 0;
}

bool DataGridAccessible::clear()
{
    DataGrid* grid = view();
    if (!grid || !grid->selectionModel())
        return false;
    grid->selectionModel()->clearSelection();
    return selectedCellCount() == 0;
}
#endif

QAccessibleInterface* DataGridAccessible::headerCell(
    int section, Qt::Orientation orientation) const
{
    if (section < 0
        || (orientation == Qt::Horizontal && section >= columnCount())
        || (orientation == Qt::Vertical && section >= rowCount())) {
        return nullptr;
    }
    const int logicalChild = orientation == Qt::Horizontal
        ? section + 1
        : (section + 1) * (columnCount() + 1);
    return interfaceForLogicalChild(logicalChild);
}

void DataGridAccessible::clearChildCache() const
{
    const QList<QAccessible::Id> ids = m_childToId.values();
    m_childToId.clear();
    for (QAccessible::Id id : ids)
        QAccessible::deleteAccessibleInterface(id);
}

QAccessibleInterface* dataGridAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* view = dynamic_cast<DataGrid*>(object);
    return view ? new DataGridAccessible(view) : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureDataGridAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(dataGridAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::collections::detail
