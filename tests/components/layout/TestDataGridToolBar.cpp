#include <gtest/gtest.h>
#include <QAccessible>
#include <QApplication>
#include <QHeaderView>
#include <QLayout>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QtTest/QTest>
#include "components/layout/DataGridToolBar.h"
#include "components/collections/DataGrid.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/DropDownButton.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"

using fluent::layout::DataGridToolBar;
using fluent::collections::DataGrid;

TEST(DataGridToolBarTest, Contract_ClearNotificationsCanDestroyTheToolbar)
{
    for (bool withSearch : {false, true}) {
        SCOPED_TRACE(withSearch);
        QPointer<DataGridToolBar> bar = new DataGridToolBar;
        bar->addFilter("status", "Status", {"Active"});
        bar->setFilterValues("status", {"Active"});
        if (withSearch)
            bar->setSearchText("query");
        const auto destroy = [bar] {
            EXPECT_TRUE(bar->searchText().isEmpty());
            EXPECT_TRUE(bar->filters().isEmpty());
            EXPECT_FALSE(bar->clearButton()->isEnabled());
            delete bar.data();
        };
        if (withSearch)
            QObject::connect(bar, &DataGridToolBar::searchTextChanged, bar, destroy);
        else
            QObject::connect(bar, &DataGridToolBar::filtersChanged, bar, destroy);
        bar->clearFilters();
        EXPECT_TRUE(bar.isNull());
    }
}

TEST(DataGridToolBarTest, Contract_ClearCallbacksCanReplaceFacetsAndReapplyValues)
{
    DataGridToolBar bar;
    auto* first = bar.addFilter("first", "First", {"A"});
    bar.addFilter("other", "Other", {"B"});
    bar.setFilterValues("first", {"A"});
    bar.setFilterValues("other", {"B"});
    bar.setSearchText("query");
    QSignalSpy changes(&bar, &DataGridToolBar::filtersChanged);
    QObject::connect(first, &fluent::basicinput::MultiSelectComboBox::selectionChanged, &bar, [&] {
        bar.clearFilters();
        bar.removeFilter("other");
        bar.addFilter("replacement", "Replacement", {"C"});
    });
    QObject::connect(&bar, &DataGridToolBar::searchTextChanged, &bar, [&] {
        bar.setFilterValues("replacement", {"C"});
    });
    bar.clearFilters();
    EXPECT_EQ(bar.filters(), QVariantMap({{"replacement", QStringList{"C"}}}));
    EXPECT_EQ(changes.count(), 1);
}

TEST(DataGridToolBarTest, Contract_SearchFacetsClearAndNoop)
{
    DataGridToolBar bar;
    QSignalSpy search(&bar, &DataGridToolBar::searchTextChanged);
    QSignalSpy filters(&bar, &DataGridToolBar::filtersChanged);
    auto* status = bar.addFilter("status", "Status", {"Active", "Review", "Active"});
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->model()->rowCount(), 2);
    EXPECT_EQ(bar.addFilter("status", "Duplicate", {}), nullptr);
    bar.setSearchText("canvas");
    bar.setSearchText("canvas");
    EXPECT_EQ(search.count(), 1);
    bar.setFilterValues("status", {"Review", "Review", "Unknown"});
    EXPECT_EQ(bar.filters().value("status").toStringList(), QStringList{"Review"});
    const int changes = filters.count();
    bar.setFilterValues("status", {"Review"});
    EXPECT_EQ(filters.count(), changes);
    bar.clearFilters();
    EXPECT_TRUE(bar.searchText().isEmpty());
    EXPECT_TRUE(bar.filters().isEmpty());
    EXPECT_EQ(filters.count(), changes + 1);
    bar.clearFilters();
    EXPECT_EQ(filters.count(), changes + 1);
    bar.removeFilter("status");
    EXPECT_NE(bar.addFilter("status", "Status", {"Active"}), nullptr);
}

TEST(DataGridToolBarTest, Contract_GridIsBorrowedAndModelIsUntouched)
{
    QStandardItemModel model(3, 4);
    auto* grid = new DataGrid;
    grid->setModel(&model);
    DataGridToolBar bar;
    QSignalSpy changed(&bar, &DataGridToolBar::dataGridChanged);
    bar.setDataGrid(grid);
    bar.setDataGrid(grid);
    bar.setSearchText("anything");
    EXPECT_EQ(grid->model(), &model);
    EXPECT_EQ(grid->parent(), nullptr);
    EXPECT_EQ(changed.count(), 1);
    delete grid;
    EXPECT_EQ(bar.dataGrid(), nullptr);
    EXPECT_FALSE(bar.columnsButton()->isEnabled());
    EXPECT_EQ(changed.count(), 2);
}

TEST(DataGridToolBarTest, Contract_AccessibilityAndNarrowLayoutUseNativeControls)
{
    DataGridToolBar bar;
    bar.addFilter("status", "Status", {"Active", "Review"});
    bar.addFilter("owner", "Owner", {"Maya", "Noah"});
    bar.resize(320, bar.heightForWidth(320));
    bar.show();
    QApplication::processEvents();
    for (auto* child : bar.findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!child->isVisible() || child->isWindow())
            continue;
        EXPECT_GE(child->geometry().left(), 0);
        EXPECT_LE(child->geometry().right(), bar.width());
    }
    auto* text = QAccessible::queryAccessibleInterface(bar.searchField());
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->role(), QAccessible::EditableText);
    EXPECT_FALSE(text->text(QAccessible::Name).isEmpty());
    bar.setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();
    EXPECT_GE(bar.searchField()->x(), 0);
}

TEST(DataGridToolBarTest, Contract_ControlEdgesAndWrapGapsStayAligned)
{
    DataGridToolBar bar;
    auto* status = bar.addFilter("status", "Status", {"Active", "Review"});
    auto* owner = bar.addFilter("owner", "Owner", {"Maya", "Noah"});
    auto* clear = bar.findChild<fluent::basicinput::Button*>("dataGridClearFilters");
    ASSERT_NE(clear, nullptr);
    const QList<QWidget*> controls{bar.searchField(), status, owner, clear, bar.columnsButton()};
    bar.resize(960, bar.heightForWidth(960));
    bar.show();
    QApplication::processEvents();
    bar.searchField()->setText("unfinished search");
    bar.searchField()->setSelection(2, 6);
    bar.setFilterValues("status", {"Review"});

    for (int width : {960, 320, 180}) {
        bar.setLayoutDirection(Qt::LeftToRight);
        bar.resize(width, bar.heightForWidth(width));
        bar.layout()->activate();
        QApplication::processEvents();
        QList<QRect> positions;
        for (QWidget* control : controls) {
            SCOPED_TRACE(control->objectName().toStdString());
            positions.append(control->geometry());
            EXPECT_EQ(control->height(), Spacing::ControlHeight::Standard);
            EXPECT_TRUE(bar.rect().contains(control->geometry()));
            if (control == status || control == owner) {
                auto* trigger = control->findChild<QWidget*>("MultiSelectComboBox.Trigger");
                ASSERT_NE(trigger, nullptr);
                EXPECT_EQ(trigger->geometry(), control->rect());
            }
        }
        for (int i = 1; i < positions.size(); ++i) {
            const QRect previous = positions.at(i - 1), current = positions.at(i);
            if (current.top() == previous.top())
                EXPECT_EQ(current.left() - previous.right() - 1, Spacing::Gap::Normal);
            else {
                EXPECT_EQ(current.left(), 0);
                EXPECT_EQ(current.top() - previous.bottom() - 1, Spacing::Gap::Normal);
            }
        }
        EXPECT_EQ(positions.last().bottom() + 1, bar.heightForWidth(width));
        bar.setLayoutDirection(Qt::RightToLeft);
        bar.layout()->activate();
        QApplication::processEvents();
        for (int i = 0; i < controls.size(); ++i) {
            const QRect expected(width - positions.at(i).right() - 1, positions.at(i).top(), positions.at(i).width(),
                positions.at(i).height());
            EXPECT_EQ(controls.at(i)->geometry(), expected);
        }
    }
    EXPECT_EQ(bar.searchText(), QString("unfinished search"));
    EXPECT_EQ(bar.searchField()->selectedText(), QString("finish"));
    EXPECT_EQ(bar.filters().value("status").toStringList(), QStringList{"Review"});
}

TEST(DataGridToolBarTest, Contract_LargerTextGrowsAllControlsTogether)
{
    DataGridToolBar bar;
    auto* status = bar.addFilter("status", "Status", {"Active", "Review"});
    bar.resize(800, 64);
    bar.show();
    QApplication::processEvents();
    QFont larger = bar.searchField()->font();
    larger.setPixelSize(28);
    bar.searchField()->setFont(larger);
    bar.resize(800, bar.heightForWidth(800));
    bar.layout()->activate();
    QApplication::processEvents();

    const int height = bar.searchField()->height();
    EXPECT_GT(height, Spacing::ControlHeight::Standard);
    EXPECT_GE(height, bar.searchField()->fontMetrics().height() + 2 * Spacing::Padding::TextFieldVertical);
    for (int i = 0; i < bar.layout()->count(); ++i)
        EXPECT_EQ(bar.layout()->itemAt(i)->widget()->height(), height);
    auto* trigger = status->findChild<QWidget*>("MultiSelectComboBox.Trigger");
    ASSERT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->height(), height);
}

TEST(DataGridToolBarTest, Contract_ColumnActionsRefreshAndExposeTheirResult)
{
    QStandardItemModel model(3, 3);
    model.setHorizontalHeaderLabels({"Project", "Owner", "Status"});
    DataGrid grid;
    grid.setModel(&model);
    DataGridToolBar bar;
    bar.setDataGrid(&grid);
    bar.resize(600, 40);
    bar.show();
    QApplication::processEvents();
    auto* menu = bar.columnsButton()->menu();
    const auto open = [&] {
        QTest::mouseClick(bar.columnsButton(), Qt::LeftButton);
        QApplication::processEvents();
    };
    open();
    ASSERT_TRUE(menu->isVisible());
    QPointer<QMenu> first = menu->findChild<QMenu*>("dataGridColumn_0");
    ASSERT_NE(first, nullptr);
    auto* pin = first->findChild<QAction*>("columnPinned");
    ASSERT_NE(pin, nullptr);
    pin->trigger();
    EXPECT_TRUE(grid.isColumnPinned(0));
    EXPECT_FALSE(menu->isVisible());
    open();
    EXPECT_EQ(first.data(), menu->findChild<QMenu*>("dataGridColumn_0"));
    EXPECT_EQ(menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly).size(), 3);
    auto* status = menu->findChild<QMenu*>("dataGridColumn_2");
    ASSERT_NE(status, nullptr);
    auto* visible = status->findChild<QAction*>("columnVisible");
    ASSERT_NE(visible, nullptr);
    visible->trigger();
    EXPECT_FALSE(grid.isColumnVisible(2));
    EXPECT_EQ(bar.columnsButton()->accessibleDescription(),
        QString("Visible columns: Project, Owner. Pinned columns: Project."));
    bar.setDataGrid(nullptr);
    grid.setColumnVisible(2, true);
    EXPECT_TRUE(bar.columnsButton()->accessibleDescription().isEmpty());
}

TEST(DataGridToolBarTest, Contract_CallerParentedGridCanTearDownWithToolbar)
{
    auto* bar = new DataGridToolBar;
    QPointer<DataGrid> grid = new DataGrid(bar);
    bar->setDataGrid(grid);
    delete bar;
    EXPECT_TRUE(grid.isNull());
}
