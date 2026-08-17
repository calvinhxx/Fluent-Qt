#include <gtest/gtest.h>

#include <QAbstractTableModel>
#include <QAccessible>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QImage>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QScrollBar>
#include <QSet>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVector>
#include <QtTest/QTest>

#include <algorithm>
#include <memory>
#include <type_traits>

#include "components/basicinput/Button.h"
#include "components/collections/DataGrid.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/Label.h"
#include "compatibility/QtCompat.h"
#include "QtTestEnvironment.h"

using fluent::collections::DataGrid;

namespace {

#if QT_CONFIG(accessibility)

struct AccessibleModelEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
    QAccessibleTableModelChangeEvent::ModelChangeType modelChangeType =
        QAccessibleTableModelChangeEvent::ModelReset;
};

QVector<AccessibleModelEventRecord> g_accessibleModelEvents;

void captureAccessibleModelEvent(QAccessibleEvent* event)
{
    if (!event || event->type() != QAccessible::TableModelChanged)
        return;

    auto* modelEvent = static_cast<QAccessibleTableModelChangeEvent*>(event);
    g_accessibleModelEvents.append({
        event->object(),
        event->type(),
        modelEvent->modelChangeType(),
    });
}

struct ScopedAccessibleModelEventCapture {
    ScopedAccessibleModelEventCapture()
    {
        previous = QAccessible::installUpdateHandler(
            captureAccessibleModelEvent);
        g_accessibleModelEvents.clear();
    }

    ~ScopedAccessibleModelEventCapture()
    {
        QAccessible::installUpdateHandler(previous);
        g_accessibleModelEvents.clear();
    }

    QAccessible::UpdateHandler previous = nullptr;
};

#endif

class CountingTableModel final : public QAbstractTableModel {
public:
    CountingTableModel(int rows, int columns, QObject* parent = nullptr)
        : QAbstractTableModel(parent)
        , m_rows(rows)
        , m_columns(columns)
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_rows;
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_columns;
    }

    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows
            || index.column() < 0 || index.column() >= m_columns) {
            return {};
        }

        ++m_dataCalls;
        m_queriedCells.insert(cellKey(index));
        m_minimumObservedRow = std::min(m_minimumObservedRow, index.row());
        m_maximumObservedRow = std::max(m_maximumObservedRow, index.row());

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return QStringLiteral("R%1 C%2")
                .arg(index.row())
                .arg(index.column());
        }
        return {};
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole)
            return {};
        return orientation == Qt::Horizontal
            ? QStringLiteral("Column %1").arg(section)
            : QString::number(section + 1);
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid())
            return Qt::NoItemFlags;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void resetObservations() const
    {
        m_dataCalls = 0;
        m_queriedCells.clear();
        m_minimumObservedRow = m_rows;
        m_maximumObservedRow = -1;
    }

    int dataCallCount() const { return m_dataCalls; }
    int uniqueQueriedCellCount() const { return m_queriedCells.size(); }
    int minimumObservedRow() const { return m_minimumObservedRow; }
    int maximumObservedRow() const { return m_maximumObservedRow; }

private:
    static quint64 cellKey(const QModelIndex& index)
    {
        return (static_cast<quint64>(static_cast<quint32>(index.row())) << 32)
            | static_cast<quint32>(index.column());
    }

    int m_rows = 0;
    int m_columns = 0;
    mutable int m_dataCalls = 0;
    mutable QSet<quint64> m_queriedCells;
    mutable int m_minimumObservedRow = m_rows;
    mutable int m_maximumObservedRow = -1;
};

class CountingTableDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        ++m_paintCount;
        QStyledItemDelegate::paint(painter, option, index);
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (!editor)
            return nullptr;

        ++m_createdEditorCount;
        m_activeEditors.insert(editor);
        QObject::connect(
            editor, &QObject::destroyed,
            const_cast<CountingTableDelegate*>(this),
            [this, editor]() {
                m_activeEditors.remove(editor);
                ++m_destroyedEditorCount;
            });
        return editor;
    }

    void resetPaintCount() const { m_paintCount = 0; }
    int paintCount() const { return m_paintCount; }
    int createdEditorCount() const { return m_createdEditorCount; }
    int destroyedEditorCount() const { return m_destroyedEditorCount; }
    int activeEditorCount() const { return m_activeEditors.size(); }

private:
    mutable int m_paintCount = 0;
    mutable int m_createdEditorCount = 0;
    mutable int m_destroyedEditorCount = 0;
    mutable QSet<QWidget*> m_activeEditors;
};

class SortTrackingTableModel final : public QStandardItemModel {
public:
    using QStandardItemModel::QStandardItemModel;

    void sort(int column,
              Qt::SortOrder order = Qt::AscendingOrder) override
    {
        ++m_sortCallCount;
        m_lastSortColumn = column;
        m_lastSortOrder = order;
        QStandardItemModel::sort(column, order);
    }

    void resetSortTracking()
    {
        m_sortCallCount = 0;
        m_lastSortColumn = -1;
        m_lastSortOrder = Qt::AscendingOrder;
    }

    int sortCallCount() const { return m_sortCallCount; }
    int lastSortColumn() const { return m_lastSortColumn; }
    Qt::SortOrder lastSortOrder() const { return m_lastSortOrder; }

private:
    int m_sortCallCount = 0;
    int m_lastSortColumn = -1;
    Qt::SortOrder m_lastSortOrder = Qt::AscendingOrder;
};

constexpr int kValidationMessageRole = Qt::UserRole + 417;

class EditAuthorityModel final : public QStandardItemModel {
public:
    using QStandardItemModel::QStandardItemModel;

    void rejectValue(const QString& value, const QString& message)
    {
        m_rejectedValue = value;
        m_rejectionMessage = message;
    }

    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override
    {
        if (role == Qt::EditRole) {
            ++m_editAttempts;
            if (value.toString() == m_rejectedValue) {
                QStandardItemModel::setData(
                    index, m_rejectionMessage, kValidationMessageRole);
                return false;
            }
            QStandardItemModel::setData(
                index, QVariant(), kValidationMessageRole);
        }
        return QStandardItemModel::setData(index, value, role);
    }

    int editAttempts() const { return m_editAttempts; }

private:
    QString m_rejectedValue;
    QString m_rejectionMessage;
    int m_editAttempts = 0;
};

class ValidationTrackingDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);
        if (index.data(kValidationMessageRole).toString().isEmpty())
            return;

        ++m_validationPaintCount;
        painter->save();
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(196, 43, 28), 1.5));
        painter->drawRect(QRectF(option.rect).adjusted(1.0, 1.0, -1.0, -1.0));
        painter->restore();
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        QWidget* editor =
            QStyledItemDelegate::createEditor(parent, option, index);
        if (!editor)
            return nullptr;

        ++m_createdEditorCount;
        m_activeEditors.insert(editor);
        QObject::connect(
            editor, &QObject::destroyed,
            const_cast<ValidationTrackingDelegate*>(this),
            [this, editor] {
                m_activeEditors.remove(editor);
                ++m_destroyedEditorCount;
            });
        return editor;
    }

    int validationPaintCount() const { return m_validationPaintCount; }
    int createdEditorCount() const { return m_createdEditorCount; }
    int destroyedEditorCount() const { return m_destroyedEditorCount; }
    int activeEditorCount() const { return m_activeEditors.size(); }

private:
    mutable int m_validationPaintCount = 0;
    mutable int m_createdEditorCount = 0;
    mutable int m_destroyedEditorCount = 0;
    mutable QSet<QWidget*> m_activeEditors;
};

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

void showOffscreen(DataGrid* view, const QSize& size = QSize(800, 480))
{
    view->resize(size);
    view->setAttribute(Qt::WA_DontShowOnScreen, true);
    view->show();
    QTest::qWait(30);
    processEvents();
}

QLineEdit* beginKeyboardEdit(DataGrid* view,
                             const QModelIndex& index)
{
    if (!view || !index.isValid())
        return nullptr;
    view->setCurrentIndex(index);
    view->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(view, Qt::Key_F2);
    processEvents();
    return view->viewport()->findChild<QLineEdit*>();
}

void renderViewport(DataGrid* view)
{
    QImage image(view->viewport()->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    view->viewport()->render(&painter);
}

int visibleCellCount(const DataGrid& view)
{
    if (!view.model() || view.model()->rowCount() == 0
        || view.model()->columnCount() == 0) {
        return 0;
    }

    const int firstRow = view.rowAt(0);
    const int lastRow = view.rowAt(std::max(0, view.viewport()->height() - 1));
    const int firstColumn = view.columnAt(0);
    const int lastColumn = view.columnAt(
        std::max(0, view.viewport()->width() - 1));
    if (firstRow < 0 || firstColumn < 0)
        return 0;

    const int visibleRows = (lastRow >= firstRow ? lastRow : firstRow) - firstRow + 1;
    const int visibleColumns =
        (lastColumn >= firstColumn ? lastColumn : firstColumn) - firstColumn + 1;
    return visibleRows * visibleColumns;
}

void expectViewportBounded(const DataGrid& view,
                           const CountingTableModel& model,
                           const CountingTableDelegate& delegate)
{
    const int visibleCells = visibleCellCount(view);
    ASSERT_GT(visibleCells, 0);
    EXPECT_GT(model.uniqueQueriedCellCount(), 0);
    EXPECT_LE(model.uniqueQueriedCellCount(), visibleCells * 4)
        << "DataGrid must not query cells proportional to total model size";
    EXPECT_LE(model.dataCallCount(), visibleCells * 32)
        << "Role queries must stay bounded around the viewport";
    EXPECT_GT(delegate.paintCount(), 0);
    EXPECT_LE(delegate.paintCount(), visibleCells * 8)
        << "Delegate painting must stay bounded around visible cells";
}

class DataGridVisualWindow final : public QWidget,
                                   public fluent::FluentElement {
public:
    DataGridVisualWindow()
    {
        setAutoFillBackground(true);
        onThemeUpdated();
    }

    void onThemeUpdated() override
    {
        QPalette themedPalette = palette();
        themedPalette.setColor(QPalette::Window, themeColorsRef().bgCanvas);
        setPalette(themedPalette);
        update();
    }
};

void populateVisualModel(QStandardItemModel* model)
{
    if (!model)
        return;

    model->clear();
    model->setColumnCount(6);
    model->setHorizontalHeaderLabels({
        QStringLiteral("Project"),
        QStringLiteral("Owner"),
        QStringLiteral("Status"),
        QStringLiteral("Last activity / 最近一次活动与同步状态"),
        QStringLiteral("Progress"),
        QStringLiteral("Priority"),
    });

    const QStringList owners = {
        QStringLiteral("Alex Morgan"),
        QStringLiteral("林晓雨"),
        QStringLiteral("Jordan Lee"),
        QStringLiteral("Sam Rivera"),
    };
    const QStringList states = {
        QStringLiteral("Ready"),
        QStringLiteral("In review"),
        QStringLiteral("Blocked"),
        QStringLiteral("Complete"),
    };

    for (int row = 0; row < 24; ++row) {
        QList<QStandardItem*> items = {
            new QStandardItem(QStringLiteral("Fluent workspace %1").arg(row + 1)),
            new QStandardItem(owners.at(row % owners.size())),
            new QStandardItem(states.at(row % states.size())),
            new QStandardItem(
                row % 5 == 0
                    ? QStringLiteral("Waiting for a long localized dependency update / 正在等待依赖同步")
                    : QStringLiteral("Today, %1 minutes ago").arg(row + 2)),
            new QStandardItem(QStringLiteral("%1%").arg((row * 13) % 101)),
            new QStandardItem(row % 3 == 0
                                  ? QStringLiteral("High")
                                  : QStringLiteral("Normal")),
        };
        if (row == 4) {
            for (QStandardItem* item : items)
                item->setEnabled(false);
        }
        items.at(4)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        model->appendRow(items);
    }
}

} // namespace

class DataGridTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_previousTheme = fluent::FluentElement::currentTheme();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        fluent::FluentElement::setTheme(m_previousTheme);
    }

private:
    fluent::FluentElement::Theme m_previousTheme =
        fluent::FluentElement::Light;
};

static_assert(std::is_base_of_v<QTableView, DataGrid>);
static_assert(std::is_base_of_v<fluent::FluentElement, DataGrid>);
static_assert(std::is_base_of_v<fluent::QMLPlus, DataGrid>);

TEST_F(DataGridTest, Contract_PublicPropertiesNotifyOnlyOnChanges)
{
    DataGrid view;
    QSignalSpy selectionSpy(&view, &DataGrid::selectionModeChanged);
    QSignalSpy placeholderSpy(&view, &DataGrid::placeholderTextChanged);
    QSignalSpy borderSpy(&view, &DataGrid::borderVisibleChanged);
    QSignalSpy backgroundSpy(&view, &DataGrid::backgroundVisibleChanged);
    QSignalSpy chainingSpy(&view, &DataGrid::scrollChainingEnabledChanged);

    EXPECT_EQ(view.selectionMode(), DataGrid::SelectionMode::Single);
    EXPECT_TRUE(view.placeholderText().isEmpty());
    EXPECT_TRUE(view.isBorderVisible());
    EXPECT_TRUE(view.isBackgroundVisible());
    EXPECT_FALSE(view.isScrollChainingEnabled());

    view.setSelectionMode(DataGrid::SelectionMode::Single);
    view.setPlaceholderText(QString());
    view.setBorderVisible(true);
    view.setBackgroundVisible(true);
    view.setScrollChainingEnabled(false);
    EXPECT_EQ(selectionSpy.count(), 0);
    EXPECT_EQ(placeholderSpy.count(), 0);
    EXPECT_EQ(borderSpy.count(), 0);
    EXPECT_EQ(backgroundSpy.count(), 0);
    EXPECT_EQ(chainingSpy.count(), 0);

    view.setSelectionMode(DataGrid::SelectionMode::Extended);
    view.setPlaceholderText(QStringLiteral("No records"));
    view.setBorderVisible(false);
    view.setBackgroundVisible(false);
    view.setScrollChainingEnabled(true);
    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_EQ(placeholderSpy.count(), 1);
    EXPECT_EQ(borderSpy.count(), 1);
    EXPECT_EQ(backgroundSpy.count(), 1);
    EXPECT_EQ(chainingSpy.count(), 1);
}

TEST_F(DataGridTest, Contract_LargeModelInitialShowQueriesOnlyViewportBoundedCells)
{
    constexpr int kRows = 100000;
    constexpr int kColumns = 20;

    CountingTableModel model(kRows, kColumns);
    CountingTableDelegate delegate;
    DataGrid view;
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    model.resetObservations();
    delegate.resetPaintCount();

    showOffscreen(&view);
    renderViewport(&view);

    expectViewportBounded(view, model, delegate);
    EXPECT_LT(model.uniqueQueriedCellCount(), kRows * kColumns / 1000);
}

TEST_F(DataGridTest, Contract_LargeModelScrollAndResizeRemainViewportBounded)
{
    constexpr int kRows = 100000;
    constexpr int kColumns = 20;

    CountingTableModel model(kRows, kColumns);
    CountingTableDelegate delegate;
    DataGrid view;
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    showOffscreen(&view);

    model.resetObservations();
    delegate.resetPaintCount();
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->maximum());
    processEvents();
    renderViewport(&view);

    expectViewportBounded(view, model, delegate);
    EXPECT_GE(model.maximumObservedRow(), kRows - 2);

    model.resetObservations();
    delegate.resetPaintCount();
    view.resize(1280, 720);
    processEvents();
    renderViewport(&view);

    expectViewportBounded(view, model, delegate);
    EXPECT_GE(model.maximumObservedRow(), kRows - 2);
}

TEST_F(DataGridTest, Contract_CellWidgetsAndEditorsDoNotScaleWithModelSize)
{
    CountingTableModel smallModel(10, 20);
    CountingTableDelegate smallDelegate;
    DataGrid smallView;
    smallView.setModel(&smallModel);
    smallView.setItemDelegate(&smallDelegate);
    showOffscreen(&smallView);

    CountingTableModel largeModel(100000, 20);
    CountingTableDelegate largeDelegate;
    DataGrid largeView;
    largeView.setModel(&largeModel);
    largeView.setItemDelegate(&largeDelegate);
    showOffscreen(&largeView);

    EXPECT_EQ(
        smallView.viewport()->findChildren<QWidget*>(
            QString(), Qt::FindDirectChildrenOnly).size(),
        0);
    EXPECT_EQ(
        largeView.viewport()->findChildren<QWidget*>(
            QString(), Qt::FindDirectChildrenOnly).size(),
        0);
    EXPECT_LE(largeView.findChildren<QWidget*>().size(),
              smallView.findChildren<QWidget*>().size() + 8)
        << "Platform styles may add a constant scrollbar helper set, not item widgets";
    EXPECT_LE(largeView.findChildren<QObject*>().size(),
              smallView.findChildren<QObject*>().size() + 16)
        << "Auxiliary QObject count must remain constant with model size";

    const QModelIndex editable = largeModel.index(4, 3);
    largeView.setCurrentIndex(editable);
    largeView.edit(editable);
    processEvents();

    ASSERT_EQ(largeDelegate.createdEditorCount(), 1);
    ASSERT_EQ(largeDelegate.activeEditorCount(), 1);
    const auto editors = largeView.viewport()->findChildren<QLineEdit*>(
        QString(), Qt::FindDirectChildrenOnly);
    ASSERT_EQ(editors.size(), 1);

    QTest::keyClick(editors.first(), Qt::Key_Escape);
    processEvents();
    EXPECT_EQ(largeDelegate.activeEditorCount(), 0);
    EXPECT_EQ(largeDelegate.destroyedEditorCount(), 1);
}

TEST_F(DataGridTest, Contract_ModelAndDelegateRemainCallerOwned)
{
    QPointer<CountingTableModel> model = new CountingTableModel(100000, 20);
    QPointer<CountingTableDelegate> delegate = new CountingTableDelegate;
    QPointer<QItemSelectionModel> selectionModel =
        new QItemSelectionModel(model);

    auto view = std::make_unique<DataGrid>();
    view->setModel(model);
    view->setItemDelegate(delegate);
    view->setSelectionModel(selectionModel);
    view.reset();

    ASSERT_FALSE(model.isNull());
    ASSERT_FALSE(delegate.isNull());
    ASSERT_FALSE(selectionModel.isNull());
    delete selectionModel;
    delete model;
    delete delegate;
}

TEST_F(DataGridTest, Contract_ModelShapeHeadersAndEmptyStateStayLive)
{
    QStandardItemModel model(0, 3);
    model.setHorizontalHeaderLabels({
        QStringLiteral("Name"),
        QStringLiteral("Owner"),
        QStringLiteral("Last updated — localized header / 最后更新时间"),
    });

    DataGrid view;
    view.setPlaceholderText(QStringLiteral("No rows to display"));
    view.setModel(&model);
    showOffscreen(&view, QSize(520, 240));

    EXPECT_TRUE(view.isShowingPlaceholder());
    EXPECT_EQ(view.horizontalHeader()->count(), 3);
    EXPECT_EQ(view.verticalHeader()->count(), 0);
    EXPECT_EQ(
        model.headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("Last updated — localized header / 最后更新时间"));

    ASSERT_TRUE(model.insertRow(0));
    ASSERT_TRUE(model.setData(model.index(0, 0), QStringLiteral("Alpha")));
    ASSERT_TRUE(model.setData(model.index(0, 1), QStringLiteral("Team A")));
    processEvents();

    EXPECT_FALSE(view.isShowingPlaceholder());
    EXPECT_EQ(view.verticalHeader()->count(), 1);
    EXPECT_EQ(view.model()->index(0, 0).data().toString(),
              QStringLiteral("Alpha"));

    ASSERT_TRUE(model.insertColumn(1));
    ASSERT_TRUE(model.setHeaderData(
        1, Qt::Horizontal, QStringLiteral("Status"), Qt::DisplayRole));
    processEvents();
    EXPECT_EQ(view.horizontalHeader()->count(), 4);
    EXPECT_EQ(model.headerData(1, Qt::Horizontal).toString(),
              QStringLiteral("Status"));

    ASSERT_TRUE(model.removeColumn(1));
    ASSERT_TRUE(model.removeRow(0));
    processEvents();
    EXPECT_EQ(view.horizontalHeader()->count(), 3);
    EXPECT_EQ(view.verticalHeader()->count(), 0);
    EXPECT_TRUE(view.isShowingPlaceholder());

    QStandardItemModel replacement(2, 2);
    replacement.setHorizontalHeaderLabels(
        {QStringLiteral("Key"), QStringLiteral("Value")});
    view.setModel(&replacement);
    processEvents();
    EXPECT_EQ(view.model(), &replacement);
    EXPECT_EQ(view.horizontalHeader()->count(), 2);
    EXPECT_EQ(view.verticalHeader()->count(), 2);
    EXPECT_FALSE(view.isShowingPlaceholder());

    replacement.clear();
    processEvents();
    EXPECT_EQ(view.horizontalHeader()->count(), 0);
    EXPECT_EQ(view.verticalHeader()->count(), 0);
    EXPECT_TRUE(view.isShowingPlaceholder());

    view.setModel(nullptr);
    processEvents();
    EXPECT_TRUE(view.isShowingPlaceholder());
}

TEST_F(DataGridTest, Contract_SelectionModesAndKeyboardStayModelDriven)
{
    QStandardItemModel model(4, 3);
    for (int row = 0; row < model.rowCount(); ++row) {
        for (int column = 0; column < model.columnCount(); ++column) {
            model.setData(model.index(row, column),
                          QStringLiteral("%1,%2").arg(row).arg(column));
        }
    }

    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(480, 240));

    using FluentSelectionMode = fluent::collections::SelectionMode;
    view.setSelectionMode(FluentSelectionMode::None);
    EXPECT_EQ(view.selectionMode(), FluentSelectionMode::None);
    EXPECT_EQ(view.QAbstractItemView::selectionMode(),
              QAbstractItemView::NoSelection);

    view.setSelectionMode(FluentSelectionMode::Single);
    EXPECT_EQ(view.QAbstractItemView::selectionMode(),
              QAbstractItemView::SingleSelection);
    view.setSelectionMode(FluentSelectionMode::Multiple);
    EXPECT_EQ(view.QAbstractItemView::selectionMode(),
              QAbstractItemView::MultiSelection);
    view.setSelectionMode(FluentSelectionMode::Extended);
    EXPECT_EQ(view.QAbstractItemView::selectionMode(),
              QAbstractItemView::ExtendedSelection);

    view.setSelectionBehavior(QAbstractItemView::SelectItems);
    view.setCurrentIndex(model.index(0, 0));
    view.setFocus(Qt::OtherFocusReason);
    QTest::keyClick(&view, Qt::Key_Right);
    EXPECT_EQ(view.currentIndex(), model.index(0, 1));
    QTest::keyClick(&view, Qt::Key_Down);
    EXPECT_EQ(view.currentIndex(), model.index(1, 1));

    view.setSelectionBehavior(QAbstractItemView::SelectRows);
    view.selectionModel()->select(
        model.index(2, 1),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const QModelIndexList selectedRows = view.selectionModel()->selectedRows();
    ASSERT_EQ(selectedRows.size(), 1);
    EXPECT_EQ(selectedRows.first().row(), 2);
    EXPECT_EQ(view.selectionModel()->selectedIndexes().size(),
              model.columnCount());

    view.setLayoutDirection(Qt::RightToLeft);
    EXPECT_EQ(view.layoutDirection(), Qt::RightToLeft);
    EXPECT_EQ(view.horizontalHeader()->count(), model.columnCount());
}

TEST_F(DataGridTest, Contract_ReadOnlyDefaultsAndThemePaletteUseTokens)
{
    QStandardItemModel model(1, 1);
    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(420, 220));

    EXPECT_TRUE(view.isBorderVisible());
    EXPECT_TRUE(view.isBackgroundVisible());
    EXPECT_FALSE(view.alternatingRowColors());
    EXPECT_FALSE(view.showGrid());
    EXPECT_EQ(view.editTriggers(), QAbstractItemView::NoEditTriggers);
    EXPECT_TRUE(view.hasMouseTracking());
    EXPECT_TRUE(view.viewport()->hasMouseTracking());
    EXPECT_EQ(view.focusPolicy(), Qt::StrongFocus);
    EXPECT_EQ(view.horizontalHeader()->sectionResizeMode(0),
              QHeaderView::Interactive);
    EXPECT_TRUE(view.horizontalHeader()->sectionsMovable());
    EXPECT_EQ(view.verticalHeader()->sectionResizeMode(0), QHeaderView::Fixed);
    EXPECT_EQ(view.horizontalHeader()->minimumHeight(), 36);
    EXPECT_EQ(view.horizontalHeader()->maximumHeight(), 36);
    EXPECT_EQ(view.verticalHeader()->defaultSectionSize(), 36);
    EXPECT_FALSE(view.verticalHeader()->isVisible());

    const QColor lightBase = view.themeColorsRef().bgLayer;
    EXPECT_EQ(view.palette().color(QPalette::Base), lightBase);
    EXPECT_EQ(view.palette().color(QPalette::AlternateBase),
              view.themeColorsRef().bgLayer);
    EXPECT_EQ(view.palette().color(QPalette::Highlight),
              view.themeColorsRef().subtleSecondary);
    EXPECT_EQ(view.palette().color(QPalette::HighlightedText),
              view.themeColorsRef().textPrimary);
    EXPECT_EQ(view.palette().color(QPalette::Disabled, QPalette::Text),
              view.themeColorsRef().textDisabled);

    view.setBackgroundVisible(false);
    EXPECT_FALSE(view.isBackgroundVisible());
    EXPECT_EQ(view.palette().color(QPalette::Base).alpha(), 0);
    EXPECT_EQ(view.palette().color(QPalette::AlternateBase).alpha(), 0);

    view.setBackgroundVisible(true);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    processEvents();

    EXPECT_EQ(view.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(view.palette().color(QPalette::Base),
              view.themeColorsRef().bgLayer);
    EXPECT_EQ(view.palette().color(QPalette::AlternateBase),
              view.themeColorsRef().bgLayer);
    EXPECT_EQ(view.palette().color(QPalette::Highlight),
              view.themeColorsRef().subtleSecondary);
    EXPECT_EQ(view.palette().color(QPalette::Disabled, QPalette::Text),
              view.themeColorsRef().textDisabled);
    EXPECT_NE(view.palette().color(QPalette::Base), lightBase);

    view.setEnabled(false);
    processEvents();
    renderViewport(&view);
    EXPECT_FALSE(view.isEnabled());
    EXPECT_EQ(view.palette().color(QPalette::Disabled, QPalette::Text),
              view.themeColorsRef().textDisabled);

    view.setBorderVisible(false);
    EXPECT_FALSE(view.isBorderVisible());
}

TEST_F(DataGridTest, Contract_FluentScrollBarsMirrorAndChainAtBoundaries)
{
    CountingTableModel model(1000, 20);
    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(360, 180));
    view.doItemsLayout();
    processEvents();

    ASSERT_GT(view.verticalScrollBar()->maximum(), 0);
    ASSERT_GT(view.horizontalScrollBar()->maximum(), 0);
    auto* vertical = view.verticalFluentScrollBar();
    auto* horizontal = view.horizontalFluentScrollBar();
    ASSERT_NE(vertical, nullptr);
    ASSERT_NE(horizontal, nullptr);
    EXPECT_EQ(vertical->minimum(), view.verticalScrollBar()->minimum());
    EXPECT_EQ(vertical->maximum(), view.verticalScrollBar()->maximum());
    EXPECT_EQ(vertical->pageStep(), view.verticalScrollBar()->pageStep());
    EXPECT_EQ(horizontal->minimum(), view.horizontalScrollBar()->minimum());
    EXPECT_EQ(horizontal->maximum(), view.horizontalScrollBar()->maximum());
    EXPECT_EQ(horizontal->pageStep(), view.horizontalScrollBar()->pageStep());
    EXPECT_FALSE(vertical->isHidden());
    EXPECT_FALSE(horizontal->isHidden());

    view.verticalScrollBar()->setValue(view.verticalScrollBar()->maximum() / 2);
    processEvents();
    EXPECT_EQ(vertical->value(), view.verticalScrollBar()->value());

    view.setScrollChainingEnabled(true);
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->minimum());
    const QPoint wheelPoint = view.viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(
        chainedWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(view.viewport(), &chainedWheel);
    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(view.verticalScrollBar()->value(),
              view.verticalScrollBar()->minimum());

    FLUENT_MAKE_WHEEL_EVENT(
        containedWheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    containedWheel.setAccepted(false);
    QApplication::sendEvent(view.viewport(), &containedWheel);
    EXPECT_TRUE(containedWheel.isAccepted());
    EXPECT_GT(view.verticalScrollBar()->value(),
              view.verticalScrollBar()->minimum());
    EXPECT_EQ(vertical->value(), view.verticalScrollBar()->value());

    view.verticalScrollBar()->setValue(view.verticalScrollBar()->maximum());
    FLUENT_MAKE_WHEEL_EVENT(
        bottomChainedWheel, wheelPoint.x(), wheelPoint.y(), -120,
        Qt::NoModifier);
    bottomChainedWheel.setAccepted(false);
    QApplication::sendEvent(view.viewport(), &bottomChainedWheel);
    EXPECT_FALSE(bottomChainedWheel.isAccepted());
    EXPECT_EQ(view.verticalScrollBar()->value(),
              view.verticalScrollBar()->maximum());

    CountingTableModel fittedModel(2, 2);
    DataGrid fittedView;
    fittedView.setModel(&fittedModel);
    fittedView.setScrollChainingEnabled(true);
    showOffscreen(&fittedView, QSize(360, 180));
    fittedView.doItemsLayout();
    processEvents();
    ASSERT_EQ(fittedView.verticalScrollBar()->minimum(),
              fittedView.verticalScrollBar()->maximum());
    const QPoint fittedWheelPoint = fittedView.viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(
        fittedChainedWheel, fittedWheelPoint.x(), fittedWheelPoint.y(), -120,
        Qt::NoModifier);
    fittedChainedWheel.setAccepted(false);
    QApplication::sendEvent(fittedView.viewport(), &fittedChainedWheel);
    EXPECT_FALSE(fittedChainedWheel.isAccepted());
}

TEST_F(DataGridTest, Contract_ColumnInteractionUsesHeaderAndModelAuthority)
{
    SortTrackingTableModel model(3, 3);
    model.setHorizontalHeaderLabels({
        QStringLiteral("Project"),
        QStringLiteral("Owner"),
        QStringLiteral("Priority"),
    });
    const QStringList projects = {
        QStringLiteral("Gamma"),
        QStringLiteral("Alpha"),
        QStringLiteral("Beta"),
    };
    for (int row = 0; row < model.rowCount(); ++row) {
        model.setData(model.index(row, 0), projects.at(row));
        model.setData(model.index(row, 1), QStringLiteral("Owner %1").arg(row));
        model.setData(model.index(row, 2), row + 1);
    }

    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(520, 240));

    QHeaderView* header = view.horizontalHeader();
    ASSERT_NE(header, nullptr);
    header->resizeSection(1, 184);
    EXPECT_EQ(header->sectionSize(1), 184);

    header->moveSection(header->visualIndex(2), 0);
    EXPECT_EQ(header->visualIndex(2), 0);
    EXPECT_EQ(header->logicalIndex(0), 2);
    EXPECT_EQ(model.headerData(2, Qt::Horizontal).toString(),
              QStringLiteral("Priority"));

    view.setColumnHidden(1, true);
    EXPECT_TRUE(view.isColumnHidden(1));
    EXPECT_EQ(model.columnCount(), 3);
    view.setColumnHidden(1, false);
    EXPECT_FALSE(view.isColumnHidden(1));

    view.setSortingEnabled(true);
    model.resetSortTracking();
    view.sortByColumn(0, Qt::AscendingOrder);
    processEvents();

    EXPECT_GE(model.sortCallCount(), 1);
    EXPECT_LE(model.sortCallCount(), 2);
    EXPECT_EQ(model.lastSortColumn(), 0);
    EXPECT_EQ(model.lastSortOrder(), Qt::AscendingOrder);
    EXPECT_EQ(header->sortIndicatorSection(), 0);
    EXPECT_EQ(header->sortIndicatorOrder(), Qt::AscendingOrder);
    EXPECT_EQ(model.index(0, 0).data().toString(), QStringLiteral("Alpha"));
}

TEST_F(DataGridTest, Contract_EditingDefaultDelegateCommitsCancelsAndTabs)
{
    QStandardItemModel model(2, 2);
    model.setData(model.index(0, 0), QStringLiteral("Original"));
    model.setData(model.index(0, 1), QStringLiteral("Second"));

    DataGrid view;
    view.setModel(&model);
    view.setEditTriggers(QAbstractItemView::EditKeyPressed);
    view.setTabKeyNavigation(true);
    showOffscreen(&view, QSize(520, 240));

    const QModelIndex first = model.index(0, 0);
    QPointer<QLineEdit> committedEditor = beginKeyboardEdit(&view, first);
    ASSERT_FALSE(committedEditor.isNull());
    EXPECT_NE(committedEditor->focusPolicy(), Qt::NoFocus);
    EXPECT_TRUE(committedEditor->isEnabled());
    EXPECT_TRUE(committedEditor->isVisible());
    EXPECT_TRUE(view.visualRect(first).intersects(committedEditor->geometry()));
    committedEditor->setText(QStringLiteral("Committed"));
    QTest::keyClick(committedEditor, Qt::Key_Return);
    processEvents();

    EXPECT_EQ(model.data(first, Qt::EditRole).toString(),
              QStringLiteral("Committed"));
    EXPECT_TRUE(committedEditor.isNull());

    QPointer<QLineEdit> cancelledEditor = beginKeyboardEdit(&view, first);
    ASSERT_FALSE(cancelledEditor.isNull());
    cancelledEditor->setText(QStringLiteral("Cancelled"));
    QTest::keyClick(cancelledEditor, Qt::Key_Escape);
    processEvents();

    EXPECT_EQ(model.data(first, Qt::EditRole).toString(),
              QStringLiteral("Committed"));
    EXPECT_TRUE(cancelledEditor.isNull());

    QPointer<QLineEdit> tabEditor = beginKeyboardEdit(&view, first);
    ASSERT_FALSE(tabEditor.isNull());
    tabEditor->setText(QStringLiteral("Tab committed"));
    QTest::keyClick(tabEditor, Qt::Key_Tab);
    processEvents();

    EXPECT_EQ(model.data(first, Qt::EditRole).toString(),
              QStringLiteral("Tab committed"));
    EXPECT_EQ(view.currentIndex(), model.index(0, 1));
    EXPECT_TRUE(tabEditor.isNull());

    auto* lockedItem = new QStandardItem(QStringLiteral("Locked"));
    lockedItem->setEditable(false);
    model.setItem(1, 0, lockedItem);
    EXPECT_EQ(beginKeyboardEdit(&view, model.index(1, 0)), nullptr)
        << "Model flags remain authoritative";

    view.setEditTriggers(QAbstractItemView::NoEditTriggers);
    EXPECT_EQ(beginKeyboardEdit(&view, first), nullptr)
        << "F2 must not bypass the read-only default";
}

TEST_F(DataGridTest, Contract_EditingModelRejectionKeepsValuesAndDelegateValidation)
{
    EditAuthorityModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, QStringLiteral("Accepted"), Qt::EditRole);
    model.rejectValue(
        QStringLiteral("reject"),
        QStringLiteral("Value is rejected by the application model"));
    const int baselineAttempts = model.editAttempts();

    ValidationTrackingDelegate delegate;
    DataGrid view;
    view.setItemDelegate(&delegate);
    view.setModel(&model);
    view.setEditTriggers(QAbstractItemView::EditKeyPressed);
    showOffscreen(&view, QSize(420, 220));

    QPointer<QLineEdit> rejectedEditor = beginKeyboardEdit(&view, index);
    ASSERT_FALSE(rejectedEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 1);
    rejectedEditor->setText(QStringLiteral("reject"));
    QTest::keyClick(rejectedEditor, Qt::Key_Return);
    processEvents();

    EXPECT_EQ(model.editAttempts(), baselineAttempts + 1);
    EXPECT_EQ(model.data(index, Qt::DisplayRole).toString(),
              QStringLiteral("Accepted"));
    EXPECT_EQ(model.data(index, Qt::EditRole).toString(),
              QStringLiteral("Accepted"));
    EXPECT_EQ(model.data(index, kValidationMessageRole).toString(),
              QStringLiteral("Value is rejected by the application model"));
    EXPECT_TRUE(rejectedEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 0);
    EXPECT_EQ(delegate.destroyedEditorCount(), 1);

    renderViewport(&view);
    EXPECT_GT(delegate.validationPaintCount(), 0);
    EXPECT_EQ(model.data(index, Qt::DisplayRole).toString(),
              QStringLiteral("Accepted"))
        << "Validation presentation must not rewrite display data";

    QPointer<QLineEdit> acceptedEditor = beginKeyboardEdit(&view, index);
    ASSERT_FALSE(acceptedEditor.isNull());
    acceptedEditor->setText(QStringLiteral("Updated"));
    QTest::keyClick(acceptedEditor, Qt::Key_Return);
    processEvents();

    EXPECT_EQ(model.data(index, Qt::EditRole).toString(),
              QStringLiteral("Updated"));
    EXPECT_TRUE(model.data(index, kValidationMessageRole).toString().isEmpty());
    EXPECT_TRUE(acceptedEditor.isNull());
    EXPECT_EQ(delegate.createdEditorCount(), 2);
    EXPECT_EQ(delegate.destroyedEditorCount(), 2);
}

TEST_F(DataGridTest, Contract_EditingLifecycleClosesTransientEditors)
{
    QStandardItemModel original(2, 2);
    original.setData(original.index(0, 0), QStringLiteral("Original"));
    QStandardItemModel replacement(1, 1);
    replacement.setData(
        replacement.index(0, 0), QStringLiteral("Replacement"));

    CountingTableDelegate delegate;
    auto view = std::make_unique<DataGrid>();
    view->setItemDelegate(&delegate);
    view->setModel(&original);
    view->setEditTriggers(QAbstractItemView::EditKeyPressed);
    showOffscreen(view.get(), QSize(420, 220));

    QPointer<QLineEdit> resetEditor =
        beginKeyboardEdit(view.get(), original.index(0, 0));
    ASSERT_FALSE(resetEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 1);
    original.clear();
    processEvents();
    EXPECT_TRUE(resetEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 0);

    original.setRowCount(1);
    original.setColumnCount(1);
    original.setData(original.index(0, 0), QStringLiteral("Restored"));
    QPointer<QLineEdit> replacementEditor =
        beginKeyboardEdit(view.get(), original.index(0, 0));
    ASSERT_FALSE(replacementEditor.isNull());
    view->setModel(&replacement);
    processEvents();
    EXPECT_TRUE(replacementEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 0);

    QPointer<QLineEdit> destructionEditor =
        beginKeyboardEdit(view.get(), replacement.index(0, 0));
    ASSERT_FALSE(destructionEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 1);
    view.reset();
    processEvents();
    EXPECT_TRUE(destructionEditor.isNull());
    EXPECT_EQ(delegate.activeEditorCount(), 0);
    EXPECT_EQ(delegate.createdEditorCount(),
              delegate.destroyedEditorCount());
}

TEST_F(DataGridTest, Contract_AccessibilityUsesNativeLogicalTableSemantics)
{
    QStandardItemModel model(3, 3);
    model.setHorizontalHeaderLabels({
        QStringLiteral("Project"),
        QStringLiteral("Owner"),
        QStringLiteral("Status"),
    });
    model.setVerticalHeaderLabels({
        QStringLiteral("Row 1"),
        QStringLiteral("Row 2"),
        QStringLiteral("Row 3"),
    });
    for (int row = 0; row < model.rowCount(); ++row) {
        for (int column = 0; column < model.columnCount(); ++column) {
            QStandardItem* item = new QStandardItem(
                QStringLiteral("Cell %1,%2").arg(row).arg(column));
            item->setEditable(false);
            model.setItem(row, column, item);
        }
    }

    DataGrid view;
    view.setAccessibleName(QStringLiteral("Project portfolio"));
    view.setAccessibleDescription(
        QStringLiteral("Application-owned project summary"));
    view.setModel(&model);
    view.setSelectionBehavior(QAbstractItemView::SelectRows);
    showOffscreen(&view, QSize(520, 240));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Table);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Project portfolio"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Application-owned project summary"));
    EXPECT_FALSE(root->state().disabled);

    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 3);
    EXPECT_EQ(table->columnCount(), 3);

    QAccessibleInterface* cell = table->cellAt(1, 2);
    ASSERT_NE(cell, nullptr);
    EXPECT_EQ(cell->role(), QAccessible::Cell);
    EXPECT_EQ(cell->text(QAccessible::Name), QStringLiteral("Cell 1,2"));
    EXPECT_FALSE(model.flags(model.index(1, 2)) & Qt::ItemIsEditable);
    QAccessibleTableCellInterface* cellInfo = cell->tableCellInterface();
    ASSERT_NE(cellInfo, nullptr);
    EXPECT_EQ(cellInfo->rowIndex(), 1);
    EXPECT_EQ(cellInfo->columnIndex(), 2);
    EXPECT_EQ(cellInfo->rowExtent(), 1);
    EXPECT_EQ(cellInfo->columnExtent(), 1);
    EXPECT_EQ(cellInfo->table(), root);

    const QList<QAccessibleInterface*> columnHeaders =
        cellInfo->columnHeaderCells();
    const QList<QAccessibleInterface*> rowHeaders =
        cellInfo->rowHeaderCells();
    ASSERT_EQ(columnHeaders.size(), 1);
    ASSERT_EQ(rowHeaders.size(), 1);
    EXPECT_EQ(columnHeaders.first()->role(), QAccessible::ColumnHeader);
    EXPECT_EQ(columnHeaders.first()->text(QAccessible::Name),
              QStringLiteral("Status"));
    EXPECT_EQ(rowHeaders.first()->role(), QAccessible::RowHeader);
    EXPECT_EQ(rowHeaders.first()->text(QAccessible::Name),
              QStringLiteral("Row 2"));

    view.selectRow(1);
    view.setCurrentIndex(model.index(1, 2));
    processEvents();
    EXPECT_EQ(table->selectedRowCount(), 1);
    EXPECT_EQ(table->selectedCellCount(), model.columnCount());
    EXPECT_TRUE(table->isRowSelected(1));
    EXPECT_TRUE(cellInfo->isSelected());

    QAccessibleInterface* focusCell = root->focusChild();
    ASSERT_NE(focusCell, nullptr);
    ASSERT_NE(focusCell->tableCellInterface(), nullptr);
    EXPECT_EQ(focusCell->tableCellInterface()->rowIndex(), 1);
    EXPECT_EQ(focusCell->tableCellInterface()->columnIndex(), 2);
    EXPECT_TRUE(focusCell->state().focused);

    QAccessibleInterface* actionCell = table->cellAt(2, 0);
    ASSERT_NE(actionCell, nullptr);
    QAccessibleActionInterface* action = actionCell->actionInterface();
    ASSERT_NE(action, nullptr);
    EXPECT_TRUE(action->actionNames().contains(
        QAccessibleActionInterface::toggleAction()));
    action->doAction(QAccessibleActionInterface::toggleAction());
    processEvents();
    EXPECT_TRUE(table->isRowSelected(2));
    EXPECT_FALSE(table->isRowSelected(1));

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    model.setHeaderData(2, Qt::Horizontal, QStringLiteral("State"));
    processEvents();
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Project portfolio"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Application-owned project summary"));
    EXPECT_EQ(table->columnDescription(2), QStringLiteral("State"));

    view.setEnabled(false);
    processEvents();
    EXPECT_TRUE(root->state().disabled);
}

TEST_F(DataGridTest, Contract_AccessibilityReadOnlyStateFollowsViewAndModel)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QStandardItemModel model(1, 2);
    auto* locked = new QStandardItem(QStringLiteral("Locked"));
    locked->setEditable(false);
    auto* editable = new QStandardItem(QStringLiteral("Editable"));
    editable->setEditable(true);
    model.setItem(0, 0, locked);
    model.setItem(0, 1, editable);

    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(420, 220));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    QAccessibleInterface* lockedCell = table->cellAt(0, 0);
    QAccessibleInterface* editableCell = table->cellAt(0, 1);
    ASSERT_NE(lockedCell, nullptr);
    ASSERT_NE(editableCell, nullptr);

    EXPECT_EQ(root->childCount(), 6);
    ASSERT_NE(root->child(0), nullptr);
    EXPECT_EQ(root->child(0)->role(), QAccessible::Pane);
    EXPECT_EQ(root->child(1)->role(), QAccessible::ColumnHeader);
    EXPECT_EQ(root->child(3)->role(), QAccessible::RowHeader);
    EXPECT_EQ(root->indexOfChild(lockedCell), 4);
    EXPECT_EQ(root->indexOfChild(editableCell), 5);

    EXPECT_TRUE(root->state().readOnly);
    EXPECT_FALSE(root->state().editable);
    EXPECT_TRUE(lockedCell->state().readOnly);
    EXPECT_FALSE(lockedCell->state().editable);
    EXPECT_TRUE(editableCell->state().readOnly)
        << "NoEditTriggers keeps an otherwise editable model cell read-only";
    EXPECT_FALSE(editableCell->state().editable);

    view.setEditTriggers(QAbstractItemView::DoubleClicked
                         | QAbstractItemView::EditKeyPressed);
    processEvents();

    EXPECT_FALSE(root->state().readOnly);
    EXPECT_TRUE(root->state().editable);
    EXPECT_TRUE(lockedCell->state().readOnly)
        << "Model flags remain authoritative for an individual cell";
    EXPECT_FALSE(lockedCell->state().editable);
    EXPECT_FALSE(editableCell->state().readOnly);
    EXPECT_TRUE(editableCell->state().editable);

    editable->setEditable(false);
    processEvents();
    EXPECT_TRUE(editableCell->state().readOnly);
    EXPECT_FALSE(editableCell->state().editable);
#endif
}

TEST_F(DataGridTest, Contract_AccessibilityLogicalCacheFollowsViewLifetime)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QAccessible::Id rootId = 0;
    QAccessible::Id cellId = 0;
    {
        QStandardItemModel model(2, 2);
        model.setData(model.index(1, 1), QStringLiteral("Retained cell"));
        auto view = std::make_unique<DataGrid>();
        view->setModel(&model);
        showOffscreen(view.get(), QSize(420, 220));

        QAccessibleInterface* root =
            QAccessible::queryAccessibleInterface(view.get());
        ASSERT_NE(root, nullptr);
        ASSERT_NE(root->tableInterface(), nullptr);
        QAccessibleInterface* cell =
            root->tableInterface()->cellAt(1, 1);
        ASSERT_NE(cell, nullptr);
        rootId = QAccessible::uniqueId(root);
        cellId = QAccessible::uniqueId(cell);
        EXPECT_NE(rootId, 0u);
        EXPECT_NE(cellId, 0u);
    }
    processEvents();

    EXPECT_EQ(QAccessible::accessibleInterface(rootId), nullptr);
    EXPECT_EQ(QAccessible::accessibleInterface(cellId), nullptr);
#endif
}

TEST_F(DataGridTest, Contract_AccessibilityPrefersModelSemanticText)
{
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, QStringLiteral("Visible project"), Qt::DisplayRole);
    model.setData(index, QStringLiteral("Accessible project name"),
                  Qt::AccessibleTextRole);
    model.setData(index, QStringLiteral("Accessible project summary"),
                  Qt::AccessibleDescriptionRole);
    model.setHeaderData(0, Qt::Horizontal,
                        QStringLiteral("Visible project column"));
    model.setHeaderData(0, Qt::Horizontal,
                        QStringLiteral("Accessible project column"),
                        Qt::AccessibleTextRole);

    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(420, 220));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    QAccessibleInterface* cell = table->cellAt(0, 0);
    ASSERT_NE(cell, nullptr);

    EXPECT_EQ(index.data(Qt::DisplayRole).toString(),
              QStringLiteral("Visible project"));
    EXPECT_EQ(cell->text(QAccessible::Name),
              QStringLiteral("Accessible project name"));
    EXPECT_EQ(cell->text(QAccessible::Description),
              QStringLiteral("Accessible project summary"));
    EXPECT_EQ(table->columnDescription(0),
              QStringLiteral("Visible project column"));
    QAccessibleTableCellInterface* cellInfo = cell->tableCellInterface();
    ASSERT_NE(cellInfo, nullptr);
    const QList<QAccessibleInterface*> columnHeaders =
        cellInfo->columnHeaderCells();
    ASSERT_EQ(columnHeaders.size(), 1);
    EXPECT_EQ(columnHeaders.first()->text(QAccessible::Name),
              QStringLiteral("Accessible project column"));

    model.setData(index, QStringLiteral("Renamed for assistive technology"),
                  Qt::AccessibleTextRole);
    model.setData(index, QStringLiteral("Updated semantic summary"),
                  Qt::AccessibleDescriptionRole);
    processEvents();

    EXPECT_EQ(index.data(Qt::DisplayRole).toString(),
              QStringLiteral("Visible project"));
    EXPECT_EQ(cell->text(QAccessible::Name),
              QStringLiteral("Renamed for assistive technology"));
    EXPECT_EQ(cell->text(QAccessible::Description),
              QStringLiteral("Updated semantic summary"));
}

TEST_F(DataGridTest, Contract_AccessibilityModelChangesInvalidateCachedCells)
{
    QStandardItemModel model(3, 2);
    model.setHorizontalHeaderLabels({
        QStringLiteral("Project"),
        QStringLiteral("Owner"),
    });
    for (int row = 0; row < model.rowCount(); ++row) {
        for (int column = 0; column < model.columnCount(); ++column) {
            model.setData(model.index(row, column),
                          QStringLiteral("Original %1,%2")
                              .arg(row)
                              .arg(column));
        }
    }

    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(420, 220));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    QAccessibleInterface* removedCell = table->cellAt(2, 1);
    ASSERT_NE(removedCell, nullptr);
    const QAccessible::Id removedCellId = QAccessible::uniqueId(removedCell);
    const int widgetCount = view.findChildren<QWidget*>().size();

    ASSERT_TRUE(model.removeRow(2));
    processEvents();

    EXPECT_EQ(table->rowCount(), 2);
    QAccessibleInterface* staleRemovedCell =
        QAccessible::accessibleInterface(removedCellId);
    EXPECT_TRUE(!staleRemovedCell || !staleRemovedCell->isValid());

    ASSERT_TRUE(model.insertRow(0));
    model.setData(model.index(0, 0), QStringLiteral("Inserted project"));
    model.setData(model.index(0, 1), QStringLiteral("Inserted owner"));
    processEvents();

    EXPECT_EQ(table->rowCount(), 3);
    QAccessibleInterface* insertedCell = table->cellAt(0, 1);
    ASSERT_NE(insertedCell, nullptr);
    EXPECT_EQ(insertedCell->text(QAccessible::Name),
              QStringLiteral("Inserted owner"));
    const QAccessible::Id originalModelCellId =
        QAccessible::uniqueId(insertedCell);

    QStandardItemModel replacement(2, 1);
    replacement.setHorizontalHeaderLabels({QStringLiteral("Replacement")});
    replacement.setData(replacement.index(0, 0),
                        QStringLiteral("Replacement row 1"));
    replacement.setData(replacement.index(1, 0),
                        QStringLiteral("Replacement row 2"));
    view.setModel(&replacement);
    processEvents();

    EXPECT_EQ(table->rowCount(), 2);
    EXPECT_EQ(table->columnCount(), 1);
    EXPECT_EQ(table->columnDescription(0), QStringLiteral("Replacement"));
    QAccessibleInterface* staleOriginalModelCell =
        QAccessible::accessibleInterface(originalModelCellId);
    EXPECT_TRUE(!staleOriginalModelCell || !staleOriginalModelCell->isValid());
    QAccessibleInterface* replacementCell = table->cellAt(1, 0);
    ASSERT_NE(replacementCell, nullptr);
    EXPECT_EQ(replacementCell->text(QAccessible::Name),
              QStringLiteral("Replacement row 2"));
    EXPECT_EQ(view.findChildren<QWidget*>().size(), widgetCount);
}

TEST_F(DataGridTest, Contract_AccessibilityModelReplacementEmitsOneReset)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QStandardItemModel original(1, 1);
    original.setData(original.index(0, 0), QStringLiteral("Original"));
    QStandardItemModel replacement(1, 1);
    replacement.setData(replacement.index(0, 0), QStringLiteral("Replacement"));

    DataGrid view;
    view.setModel(&original);
    showOffscreen(&view, QSize(420, 220));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    ASSERT_NE(root->tableInterface(), nullptr);
    ASSERT_NE(root->tableInterface()->cellAt(0, 0), nullptr);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibleModelEventCapture capture;
    view.setModel(&replacement);
    processEvents();

    ASSERT_EQ(g_accessibleModelEvents.size(), 1);
    EXPECT_EQ(g_accessibleModelEvents.first().object, &view);
    EXPECT_EQ(g_accessibleModelEvents.first().type,
              QAccessible::TableModelChanged);
    EXPECT_EQ(g_accessibleModelEvents.first().modelChangeType,
              QAccessibleTableModelChangeEvent::ModelReset);

    view.setModel(&replacement);
    processEvents();
    EXPECT_EQ(g_accessibleModelEvents.size(), 1)
        << "Assigning the active model again must stay silent";
#endif
}

TEST_F(DataGridTest, Contract_AccessibilityEmptyStatePreservesCallerText)
{
    QStandardItemModel model(0, 2);
    DataGrid view;
    view.setPlaceholderText(QStringLiteral("No project records"));
    view.setModel(&model);
    showOffscreen(&view, QSize(420, 220));

    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("No project records"));
    EXPECT_EQ(table->rowCount(), 0);
    EXPECT_EQ(table->columnCount(), 2);

    ASSERT_TRUE(model.insertRow(0));
    processEvents();
    EXPECT_EQ(root->text(QAccessible::Description), QString());
    EXPECT_EQ(table->rowCount(), 1);

    view.setAccessibleDescription(QStringLiteral("Caller summary"));
    ASSERT_TRUE(model.removeRow(0));
    view.setPlaceholderText(QStringLiteral("No matching projects"));
    processEvents();
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Caller summary"));
    EXPECT_EQ(table->rowCount(), 0);
}

TEST_F(DataGridTest, Contract_AccessibilityLargeModelStaysLogicalWithoutCellWidgets)
{
    CountingTableModel model(100000, 20);
    DataGrid view;
    view.setModel(&model);
    showOffscreen(&view, QSize(520, 240));

    const int widgetCount = view.findChildren<QWidget*>().size();
    model.resetObservations();
    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    QAccessibleTableInterface* table = root->tableInterface();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 100000);
    EXPECT_EQ(table->columnCount(), 20);
    EXPECT_LT(root->childCount(), 256)
        << "The platform accessibility tree must stay viewport-bounded; "
           "the table interface remains logically complete";
    for (int childIndex = 0; childIndex < root->childCount(); ++childIndex) {
        QAccessibleInterface* child = root->child(childIndex);
        ASSERT_NE(child, nullptr);
        EXPECT_TRUE(child->isValid());
    }

    QAccessibleInterface* lastCell = table->cellAt(99999, 19);
    ASSERT_NE(lastCell, nullptr);
    EXPECT_EQ(lastCell->text(QAccessible::Name),
              QStringLiteral("R99999 C19"));
    ASSERT_NE(lastCell->tableCellInterface(), nullptr);
    EXPECT_EQ(lastCell->tableCellInterface()->rowIndex(), 99999);
    EXPECT_EQ(lastCell->tableCellInterface()->columnIndex(), 19);
    EXPECT_EQ(view.findChildren<QWidget*>().size(), widgetCount);
    EXPECT_LE(model.uniqueQueriedCellCount(), 1);
    EXPECT_LE(model.dataCallCount(), 16);
}

TEST_F(DataGridTest, VisualCheck_ReadOnlyCore)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM")
        && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    const fluent::FluentElement::Theme previousTheme =
        fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    auto* window = new DataGridVisualWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setObjectName(QStringLiteral("DataGridVisualCheck.Window"));
    window->setWindowTitle(QStringLiteral("Project portfolio"));
    window->setMinimumSize(640, 420);
    window->resize(1040, 680);

    auto* layout = new fluent::AnchorLayout(window);
    window->setLayout(layout);
    using Edge = fluent::AnchorLayout::Edge;

    auto* title = new fluent::textfields::Label(
        QStringLiteral("Project portfolio"), window);
    title->setObjectName(QStringLiteral("DataGridVisualCheck.Title"));
    title->setFluentTypography(Typography::FontRole::Title);

    auto* subtitle = new fluent::textfields::Label(
        QStringLiteral("Compare ownership, status, and recent activity across projects."),
        window);
    subtitle->setObjectName(QStringLiteral("DataGridVisualCheck.Subtitle"));
    subtitle->setFluentTypography(Typography::FontRole::Caption);
    subtitle->setTextColorRole(
        fluent::textfields::Label::TextColorRole::Secondary);

    auto* themeButton = new fluent::basicinput::Button(
        QStringLiteral("Theme"), window);
    themeButton->setObjectName(QStringLiteral("DataGridVisualCheck.Theme"));
    themeButton->setFixedSize(84, 32);

    auto* directionButton = new fluent::basicinput::Button(
        QStringLiteral("Direction"), window);
    directionButton->setObjectName(
        QStringLiteral("DataGridVisualCheck.Direction"));
    directionButton->setFixedSize(96, 32);

    auto* dataButton = new fluent::basicinput::Button(
        QStringLiteral("Show empty"), window);
    dataButton->setObjectName(QStringLiteral("DataGridVisualCheck.Data"));
    dataButton->setFixedSize(104, 32);

    auto* lastRowButton = new fluent::basicinput::Button(
        QStringLiteral("Last row"), window);
    lastRowButton->setObjectName(
        QStringLiteral("DataGridVisualCheck.LastRow"));
    lastRowButton->setFixedSize(92, 32);

    auto* model = new QStandardItemModel(window);
    populateVisualModel(model);

    auto* grid = new DataGrid(window);
    grid->setObjectName(QStringLiteral("DataGridVisualCheck.Grid"));
    grid->setModel(model);
    grid->setPlaceholderText(QStringLiteral(
        "No records yet. Use Show data to restore the read-only table.\n"
        "暂无记录，可切回密集数据继续检查。"));
    grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid->setSelectionMode(
        fluent::collections::SelectionMode::Single);
    grid->setScrollChainingEnabled(true);
    grid->horizontalHeader()->resizeSection(0, 176);
    grid->horizontalHeader()->resizeSection(1, 132);
    grid->horizontalHeader()->resizeSection(2, 112);
    grid->horizontalHeader()->resizeSection(3, 300);
    grid->horizontalHeader()->resizeSection(4, 96);
    grid->horizontalHeader()->resizeSection(5, 96);
    grid->horizontalHeader()->setStretchLastSection(true);

    fluent::AnchorLayout::Anchors titleAnchors;
    titleAnchors.left = {window, Edge::Left, 24};
    titleAnchors.top = {window, Edge::Top, 20};
    titleAnchors.right = {window, Edge::Right, -24};
    layout->addAnchoredWidget(title, titleAnchors);

    fluent::AnchorLayout::Anchors subtitleAnchors;
    subtitleAnchors.left = {title, Edge::Left, 0};
    subtitleAnchors.right = {window, Edge::Right, -24};
    subtitleAnchors.top = {title, Edge::Bottom, 4};
    layout->addAnchoredWidget(subtitle, subtitleAnchors);

    fluent::AnchorLayout::Anchors themeAnchors;
    themeAnchors.left = {window, Edge::Left, 24};
    themeAnchors.top = {subtitle, Edge::Bottom, 16};
    layout->addAnchoredWidget(themeButton, themeAnchors);

    fluent::AnchorLayout::Anchors directionAnchors;
    directionAnchors.left = {themeButton, Edge::Right, 8};
    directionAnchors.top = {themeButton, Edge::Top, 0};
    layout->addAnchoredWidget(directionButton, directionAnchors);

    fluent::AnchorLayout::Anchors dataAnchors;
    dataAnchors.left = {directionButton, Edge::Right, 8};
    dataAnchors.top = {themeButton, Edge::Top, 0};
    layout->addAnchoredWidget(dataButton, dataAnchors);

    fluent::AnchorLayout::Anchors lastRowAnchors;
    lastRowAnchors.left = {dataButton, Edge::Right, 8};
    lastRowAnchors.top = {themeButton, Edge::Top, 0};
    layout->addAnchoredWidget(lastRowButton, lastRowAnchors);

    fluent::AnchorLayout::Anchors gridAnchors;
    gridAnchors.left = {window, Edge::Left, 24};
    gridAnchors.right = {window, Edge::Right, -24};
    gridAnchors.top = {themeButton, Edge::Bottom, 16};
    gridAnchors.bottom = {window, Edge::Bottom, -24};
    layout->addAnchoredWidget(grid, gridAnchors);

    QObject::connect(themeButton, &QPushButton::clicked, window, [] {
        fluent::FluentElement::setTheme(
            fluent::FluentElement::currentTheme()
                    == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });
    QObject::connect(directionButton, &QPushButton::clicked, window,
                     [window, directionButton] {
        const bool rtl = window->layoutDirection() != Qt::RightToLeft;
        window->setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        directionButton->setText(rtl ? QStringLiteral("Left to right")
                                     : QStringLiteral("Direction"));
        directionButton->setFixedWidth(rtl ? 112 : 96);
    });
    QObject::connect(dataButton, &QPushButton::clicked, window,
                     [model, dataButton, grid] {
        if (model->rowCount() > 0) {
            model->removeRows(0, model->rowCount());
            dataButton->setText(QStringLiteral("Show data"));
        } else {
            populateVisualModel(model);
            dataButton->setText(QStringLiteral("Show empty"));
            grid->setCurrentIndex(model->index(2, 1));
            grid->selectRow(2);
        }
    });
    QObject::connect(lastRowButton, &QPushButton::clicked, window,
                     [model, grid] {
        if (model->rowCount() == 0)
            return;
        const int lastRow = model->rowCount() - 1;
        grid->scrollToBottom();
        grid->setCurrentIndex(model->index(lastRow, 1));
        grid->selectRow(lastRow);
        grid->setFocus(Qt::OtherFocusReason);
    });
    grid->setCurrentIndex(model->index(2, 1));
    grid->selectRow(2);
    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        const auto snapshot = [](const QString& variant,
                                 tests::support::VisualSnapshotTheme theme) {
            tests::support::VisualSnapshotOptions options;
            options.windowSize = QSize(1040, 680);
            options.variant = variant;
            options.focusObjectName =
                QStringLiteral("DataGridVisualCheck.Grid");
            options.theme = theme;
            return options;
        };
        const QList<tests::support::VisualSnapshotOptions> snapshots = {
            snapshot(QStringLiteral("data-grid-light"),
                     tests::support::VisualSnapshotTheme::Light),
            snapshot(QStringLiteral("data-grid-dark"),
                     tests::support::VisualSnapshotTheme::Dark),
        };
        for (const auto& options : snapshots)
            ASSERT_TRUE(tests::support::captureVisualSnapshot(window, options));
        return;
    }

    QTimer::singleShot(0, grid, [window, grid, model] {
        window->raise();
        window->activateWindow();
        grid->setCurrentIndex(model->index(2, 1));
        grid->selectRow(2);
        grid->setFocus(Qt::OtherFocusReason);
    });
    qApp->exec();

    fluent::FluentElement::setTheme(previousTheme);
}
