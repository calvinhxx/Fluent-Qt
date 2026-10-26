#include <gtest/gtest.h>
#include "QtTestEnvironment.h"
#include <QAccessible>
#include <QApplication>
#include <QInputMethodEvent>
#include <QPointer>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include "components/basicinput/TreeSelect.h"
#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/collections/TreeView.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/TagInput.h"
#include "components/textfields/Label.h"
#include "components/foundation/FontIcon.h"

using fluent::basicinput::TreeSelect;
using fluent::collections::TreeView;
using fluent::textfields::AutoSuggestBox;
using fluent::textfields::TagInput;

namespace {
QStandardItem* item(const QString& key, const QString& text)
{
    auto* result = new QStandardItem(text);
    result->setData(key, Qt::UserRole);
    return result;
}
void populate(QStandardItemModel& model)
{
    auto* engineering = item("engineering", QStringLiteral("产品研发"));
    engineering->setSelectable(false);
    engineering->appendRow(item("client", QStringLiteral("客户端")));
    engineering->appendRow(item("design", QStringLiteral("设计")));
    auto* brand = item("brand", QStringLiteral("品牌与市场"));
    brand->appendRow(item("brand-design", QStringLiteral("设计")));
    auto* disabled = item("archived", QStringLiteral("归档"));
    disabled->setEnabled(false);
    model.appendRow(engineering);
    model.appendRow(brand);
    model.appendRow(disabled);
}
AutoSuggestBox* input(TreeSelect& box)
{
    return box.findChild<AutoSuggestBox*>(box.selectionMode() == TreeSelect::SelectionMode::Single
            ? QStringLiteral("TreeSelectSingleInput")
            : QStringLiteral("TreeSelectMultipleInput"));
}
TreeView* tree(TreeSelect& box)
{
    return box.window()->findChild<TreeView*>(QStringLiteral("TreeSelectTree"));
}
::testing::AssertionResult show(QWidget& window, TreeSelect& box)
{
    window.resize(520, 580);
    return tests::support::showAndFocusWidget(&box);
}
} // namespace

TEST(TreeSelectTest, EmptyCopyIsCallerOwnedBeforeAndAfterPopupCreation)
{
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* box = new TreeSelect(&window);
    layout->addWidget(box);
    QStandardItemModel model;
    populate(model);
    box->setModel(&model);
    box->setEmptyText(QStringLiteral("没有匹配的团队"));
    box->setSearchText(QStringLiteral("missing"));
    ASSERT_TRUE(show(window, *box));
    box->open();
    auto* label = window.findChild<fluent::textfields::Label*>(QStringLiteral("TreeSelectEmpty"));
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), box->emptyText());
    QSignalSpy changes(box, &TreeSelect::emptyTextChanged);
    box->setEmptyText(QStringLiteral("请试试其他名称"));
    box->setEmptyText(QStringLiteral("请试试其他名称"));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(label->text(), box->emptyText());
    box->close();
    box->setSelectionMode(TreeSelect::SelectionMode::Multiple);
    box->setSearchText(QStringLiteral("missing"));
    box->open();
    EXPECT_EQ(label->text(), QStringLiteral("请试试其他名称"));
    box->setEmptyText(QString());
    EXPECT_FALSE(label->isVisible());
    EXPECT_EQ(box->model(), &model);
    EXPECT_EQ(model.item(0)->text(), QStringLiteral("产品研发"));
}

TEST(TreeSelectTest, CompactRowsFitWithoutPhantomScrollAndChevronIsDecorative)
{
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    layout->setAlignment(Qt::AlignTop);
    auto* box = new TreeSelect(&window);
    layout->addWidget(box);
    QStandardItemModel model;
    for (int row = 0; row < 8; ++row)
        model.appendRow(item(QString::number(row), QStringLiteral("Team %1").arg(row)));
    box->setModel(&model);
    ASSERT_TRUE(show(window, *box));
    auto* edit = input(*box);
    auto* decoration = edit->findChild<fluent::FontIcon*>(QStringLiteral("AutoSuggestBoxQueryDecoration"));
    auto* action = edit->findChild<fluent::basicinput::Button*>(QStringLiteral("AutoSuggestBoxQueryButton"));
    ASSERT_TRUE(decoration && action);
    EXPECT_TRUE(decoration->isVisible());
    EXPECT_TRUE(action->isHidden());
    const auto point = decoration->geometry().center();
    EXPECT_NE(edit->childAt(point), decoration);
    QTest::mouseClick(edit, Qt::LeftButton, Qt::NoModifier, point);
    ASSERT_TRUE(box->isOpen());
    QApplication::processEvents();
    auto* view = tree(*box);
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->visualRect(view->model()->index(0, 0)).height(), 32);
    EXPECT_EQ(view->verticalScrollBar()->maximum(), 0);
    QTest::mouseClick(edit, Qt::LeftButton, Qt::NoModifier, QPoint(20, edit->height() / 2));
    EXPECT_TRUE(box->isOpen());
    QTest::keyClick(edit, Qt::Key_Escape);
    EXPECT_FALSE(box->isOpen());
    model.appendRow(item("overflow", "One more team"));
    box->open();
    QApplication::processEvents();
    EXPECT_GT(view->verticalScrollBar()->maximum(), 0);
}

TEST(TreeSelectTest, MultipleSelectionKeepsBranchExpansionAndCheckTargetsSeparate)
{
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    layout->setAlignment(Qt::AlignTop);
    auto* box = new TreeSelect(&window);
    layout->addWidget(box);
    QStandardItemModel model;
    auto* branch = item("department", "Department");
    branch->appendRow(item("team", "Team"));
    model.appendRow(branch);
    box->setSelectionMode(TreeSelect::SelectionMode::Multiple);
    box->setModel(&model);
    ASSERT_TRUE(show(window, *box));
    box->open();
    QApplication::processEvents();
    auto* view = tree(*box);
    ASSERT_NE(view, nullptr);
    const auto index = view->model()->index(0, 0);
    ASSERT_TRUE(view->isExpanded(index));
    const auto row = view->visualRect(index);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.left() + 16, row.center().y()));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !view->isExpanded(index); }, 1000));
    EXPECT_TRUE(box->selectedKeys().isEmpty());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.left() + 36, row.center().y()));
    EXPECT_EQ(box->selectedKeys(), QStringList{"department"});
    EXPECT_FALSE(view->isExpanded(index));
}

TEST(TreeSelectTest, BorrowedModelStableKeysAndIndependentSelection)
{
    QStandardItemModel model;
    populate(model);
    TreeSelect box;
    box.setModel(&model);
    EXPECT_EQ(model.parent(), nullptr);
    EXPECT_EQ(box.model(), &model);
    QSignalSpy changes(&box, &TreeSelect::selectionChanged);
    box.setSelectedKeys({"archived", "engineering", "client", "brand"});
    EXPECT_EQ(box.selectedKeys(), QStringList{"client"});
    box.setSelectedKeys({"client"});
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(box.findChild<TagInput*>(), nullptr);
    box.setSelectionMode(TreeSelect::SelectionMode::Multiple);
    box.setSelectedKeys({"brand", "brand-design", "design", "design"});
    EXPECT_EQ(box.selectedCount(), 3);
    EXPECT_EQ(box.selectedItems().last().toMap().value("path").toString(), QStringLiteral("产品研发 / 设计"));
    EXPECT_FALSE(model.item(1)->data(Qt::CheckStateRole).isValid());
    box.setSearchText(QStringLiteral("品牌"));
    EXPECT_EQ(box.selectedCount(), 3);
    model.sort(0, Qt::DescendingOrder);
    EXPECT_EQ(box.selectedKeys(), QStringList({"brand", "brand-design", "design"}));
    box.setSelectionMode(TreeSelect::SelectionMode::Single);
    EXPECT_EQ(box.selectedKeys(), QStringList{"brand"});
}

TEST(TreeSelectTest, DuplicateMissingKeysResetAndBorrowedDestruction)
{
    TreeSelect box;
    auto* model = new QStandardItemModel;
    populate(*model);
    box.setModel(model);
    box.setSelectionMode(TreeSelect::SelectionMode::Multiple);
    box.setSelectedKeys({"client", "design"});
    model->item(0)->child(0)->setText(QStringLiteral("桌面客户端"));
    EXPECT_EQ(box.selectedItems().first().toMap().value("text").toString(), QStringLiteral("桌面客户端"));
    model->item(0)->removeRow(1);
    EXPECT_EQ(box.selectedKeys(), QStringList{"client"});
    model->appendRow(item("client", "Duplicate"));
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    box.setSelectedKeys({"client", "missing"});
    EXPECT_EQ(box.selectedCount(), 0);
    delete model;
    EXPECT_EQ(box.model(), nullptr);
    EXPECT_EQ(box.selectedCount(), 0);
}

TEST(TreeSelectTest, SearchPreservesAncestorsAndKeyboardSelectsThenCloses)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TreeSelect box;
    layout.addWidget(&box);
    layout.addStretch();
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    ASSERT_TRUE(show(window, box));
    box.open();
    ASSERT_TRUE(box.isOpen());
    ASSERT_NE(tree(box), nullptr);
    box.setSearchText(QStringLiteral("设计"));
    auto* view = tree(box);
    EXPECT_EQ(view->model()->rowCount(), 2);
    EXPECT_EQ(view->model()->rowCount(view->model()->index(0, 0)), 1);
    QTest::keyClick(input(box), Qt::Key_Down);
    QTest::keyClick(input(box), Qt::Key_Return);
    EXPECT_EQ(box.selectedKeys(), QStringList{"design"});
    EXPECT_FALSE(box.isOpen());
    EXPECT_FALSE(view->isVisible());
    EXPECT_EQ(input(box)->text(), QStringLiteral("产品研发 / 设计"));
    box.open();
    box.setSearchText("no results");
    QTest::keyClick(input(box), Qt::Key_Return);
    EXPECT_EQ(box.selectedKeys(), QStringList{"design"});
    QTest::keyClick(input(box), Qt::Key_Escape);
    EXPECT_FALSE(box.isOpen());
    EXPECT_TRUE(box.searchText().isEmpty());
}

TEST(TreeSelectTest, MultipleCheckStateDoesNotModifySourceAndTabExits)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TreeSelect box;
    fluent::basicinput::Button after("After");
    layout.addWidget(&box);
    layout.addWidget(&after);
    layout.addStretch();
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    box.setSelectionMode(TreeSelect::SelectionMode::Multiple);
    ASSERT_TRUE(show(window, box));
    box.open();
    auto* view = tree(box);
    ASSERT_NE(view, nullptr);
    const auto branch = view->model()->index(1, 0);
    ASSERT_TRUE(view->model()->setData(branch, Qt::Checked, Qt::CheckStateRole));
    EXPECT_EQ(box.selectedKeys(), QStringList{"brand"});
    EXPECT_EQ(view->model()->index(0, 0, branch).data(Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    EXPECT_FALSE(model.item(1)->data(Qt::CheckStateRole).isValid());
    QTest::keyClick(input(box), Qt::Key_Tab);
    QApplication::processEvents();
    EXPECT_FALSE(box.isOpen());
    EXPECT_EQ(QApplication::focusWidget(), &after);
}

TEST(TreeSelectTest, InputMethodPreeditDoesNotFilterChooseOrDismiss)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TreeSelect box;
    layout.addWidget(&box);
    layout.addStretch();
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    ASSERT_TRUE(show(window, box));
    box.open();
    QInputMethodEvent preedit(QStringLiteral("she ji"), {});
    QApplication::sendEvent(input(box), &preedit);
    EXPECT_TRUE(box.searchText().isEmpty());
    QTest::keyClick(input(box), Qt::Key_Return);
    EXPECT_EQ(box.selectedCount(), 0);
    EXPECT_TRUE(box.isOpen());
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("设计"));
    QApplication::sendEvent(input(box), &commit);
    QApplication::processEvents();
    EXPECT_EQ(box.searchText(), QStringLiteral("设计"));
    EXPECT_EQ(box.selectedCount(), 0);
    QTest::keyClick(input(box), Qt::Key_Down);
    QTest::keyClick(input(box), Qt::Key_Return);
    EXPECT_EQ(box.selectedKeys(), QStringList{"design"});
}

TEST(TreeSelectTest, AccessibleValueOpenStateReadOnlyAndReentrantDeletion)
{
    TreeSelect box;
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    box.setAccessibleName("Project team");
    box.setSelectedKeys({"client"});
    auto* accessible = QAccessible::queryAccessibleInterface(&box);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->role(), QAccessible::ComboBox);
    EXPECT_EQ(accessible->text(QAccessible::Value), QStringLiteral("产品研发 / 客户端"));
    box.setSelectionMode(TreeSelect::SelectionMode::Multiple);
    auto* close = box.findChild<fluent::basicinput::Button*>(QStringLiteral("TagInputRemoveButton"));
    ASSERT_NE(close, nullptr);
    EXPECT_EQ(close->accessibleDescription(), QStringLiteral("产品研发 / 客户端"));
    box.setReadOnly(true);
    box.show();
    box.open();
    EXPECT_FALSE(box.isOpen());
    EXPECT_TRUE(accessible->state().readOnly);
    auto* transient = new TreeSelect;
    transient->setModel(&model);
    QPointer<TreeSelect> guard(transient);
    QObject::connect(transient, &TreeSelect::selectionChanged, transient, [transient] { delete transient; });
    transient->setSelectedKeys({"client"});
    EXPECT_TRUE(guard.isNull());
}

TEST(TreeSelectTest, OpeningCallbackCanDeleteTheControlAndItsReparentedPopup)
{
    QWidget window;
    QStandardItemModel model;
    populate(model);
    auto* box = new TreeSelect(&window);
    box->setGeometry(30, 30, 320, 40);
    box->setModel(&model);
    ASSERT_TRUE(show(window, *box));
    QPointer<TreeSelect> guard(box);
    QObject::connect(box, &TreeSelect::isOpenChanged, box, [box](bool open) {
        if (open)
            delete box;
    });
    box->open();
    EXPECT_TRUE(guard.isNull());
    EXPECT_TRUE(window.findChildren<TreeView*>().isEmpty());
}

TEST(TreeSelectTest, ClosedSelectionIsReplacedByInputMethodQueryWithoutChangingValue)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TreeSelect box;
    layout.addWidget(&box);
    layout.addStretch();
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    box.setSelectedKeys({"client"});
    ASSERT_TRUE(show(window, box));
    QInputMethodEvent preedit(QStringLiteral("she ji"), {});
    QApplication::sendEvent(input(box), &preedit);
    EXPECT_TRUE(box.searchText().isEmpty());
    EXPECT_TRUE(input(box)->text().isEmpty());
    QInputMethodEvent commit;
    commit.setCommitString(QStringLiteral("设计"));
    QApplication::sendEvent(input(box), &commit);
    QApplication::processEvents();
    EXPECT_EQ(box.searchText(), QStringLiteral("设计"));
    EXPECT_EQ(box.selectedKeys(), QStringList{"client"});
}

TEST(TreeSelectTest, LazilyCreatesOnlyTheActiveInputAndOneTreePopup)
{
    QWidget window;
    TreeSelect box(&window);
    box.setGeometry(20, 20, 320, 40);
    QStandardItemModel model;
    populate(model);
    box.setModel(&model);
    box.setSelectedKeys({"client"});
    EXPECT_EQ(box.findChildren<AutoSuggestBox*>().size(), 1);
    EXPECT_TRUE(box.findChildren<fluent::dialogs_flyouts::Flyout*>().isEmpty());
    EXPECT_TRUE(box.findChildren<TreeView*>().isEmpty());
    box.setModel(nullptr);
    EXPECT_EQ(box.model(), nullptr);
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    box.setModel(&model);
    box.setSelectedKeys({"client"});
    for (int cycle = 0; cycle < 3; ++cycle) {
        box.setSelectionMode(TreeSelect::SelectionMode::Multiple);
        EXPECT_EQ(box.findChildren<AutoSuggestBox*>().size(), 1);
        EXPECT_EQ(box.findChild<TagInput*>()->selectedKeys(), QStringList{"client"});
        box.setSelectionMode(TreeSelect::SelectionMode::Single);
        EXPECT_EQ(box.findChildren<AutoSuggestBox*>().size(), 1);
        EXPECT_EQ(box.findChild<TagInput*>(), nullptr);
        EXPECT_EQ(box.selectedKeys(), QStringList{"client"});
    }
    ASSERT_TRUE(show(window, box));
    box.open();
    auto* view = tree(box);
    ASSERT_NE(view, nullptr);
    box.close();
    box.open();
    EXPECT_EQ(tree(box), view);
    EXPECT_EQ(window.findChildren<fluent::dialogs_flyouts::Flyout*>().size(), 1);
    EXPECT_TRUE(window.findChildren<QWidget*>("AutoSuggestBoxSuggestionPopup").isEmpty());
}

TEST(TreeSelectTest, LargeModelUpdatesVisitChangedNodesAndKeepSelectionSynchronous)
{
    class CountingTeams final : public QStandardItemModel {
    public:
        mutable int keyReads = 0;
        QVariant data(const QModelIndex& index, int role) const override
        {
            if (role == Qt::UserRole)
                ++keyReads;
            return QStandardItemModel::data(index, role);
        }
    } model;
    for (int row = 0; row < 10000; ++row)
        model.appendRow(item(QString::number(row), QStringLiteral("Team %1").arg(row)));
    TreeSelect box;
    box.setModel(&model);
    box.setSelectedKeys({"0"});
    const int widgets = box.findChildren<QWidget*>().size();
    model.keyReads = 0;
    for (int row = 0; row < 100; ++row)
        model.item(row)->setText(QStringLiteral("Renamed %1").arg(row));
    EXPECT_LT(model.keyReads, 1000);
    EXPECT_EQ(box.selectedItems().first().toMap().value("text").toString(), QStringLiteral("Renamed 0"));
    model.keyReads = 0;
    for (int row = 10000; row < 10100; ++row)
        model.appendRow(item(QString::number(row), QStringLiteral("New team")));
    EXPECT_LT(model.keyReads, 1000);
    EXPECT_EQ(box.findChildren<QWidget*>().size(), widgets);
    box.setSelectedKeys({"10099"});
    EXPECT_EQ(box.selectedKeys(), QStringList{"10099"});
    model.keyReads = 0;
    model.removeRows(10000, 100);
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    EXPECT_LT(model.keyReads, 1000);
}

TEST(TreeSelectTest, IncrementalKeysRecoverFromDuplicatesAndFollowSorting)
{
    QStandardItemModel model;
    populate(model);
    TreeSelect box;
    box.setModel(&model);
    box.setSelectedKeys({"client"});
    model.appendRow(item("client", "Duplicate"));
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    model.item(3)->setData("other", Qt::UserRole);
    box.setSelectedKeys({"client"});
    EXPECT_EQ(box.selectedKeys(), QStringList{"client"});
    model.sort(0);
    auto* client = model.findItems(QStringLiteral("产品研发")).first()->child(0);
    client->setData("renamed-key", Qt::UserRole);
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    box.setSelectedKeys({"renamed-key"});
    EXPECT_EQ(box.selectedKeys(), QStringList{"renamed-key"});
    model.removeRows(0, model.rowCount());
    EXPECT_TRUE(box.selectedKeys().isEmpty());
    model.setColumnCount(0);
    model.setRowCount(2);
    EXPECT_TRUE(box.selectedKeys().isEmpty());
}

TEST(TreeSelectTest, BatchedModelRefreshPreservesCollapsedBranchesAndScroll)
{
    QWidget window;
    TreeSelect box(&window);
    box.setGeometry(20, 20, 320, 40);
    QStandardItemModel model;
    for (int group = 0; group < 40; ++group) {
        auto* branch = item(QString("group-%1").arg(group), QString("Group %1").arg(group));
        branch->setSelectable(false);
        branch->appendRow(item(QString("leaf-%1").arg(group), QString("Leaf %1").arg(group)));
        model.appendRow(branch);
    }
    box.setModel(&model);
    ASSERT_TRUE(show(window, box));
    box.open();
    auto* view = tree(box);
    QApplication::processEvents();
    const auto first = view->model()->index(0, 0);
    view->collapse(first);
    view->verticalScrollBar()->setValue(300);
    const int offset = view->verticalScrollBar()->value();
    for (int row = 0; row < 40; ++row)
        model.item(row)->setText(QString("Updated %1").arg(row));
    QApplication::processEvents();
    EXPECT_FALSE(view->isExpanded(view->model()->index(0, 0)));
    EXPECT_EQ(view->verticalScrollBar()->value(), offset);
    box.setSearchText("Leaf 0");
    EXPECT_TRUE(view->isExpanded(view->model()->index(0, 0)));
    box.setSearchText(QString());
    EXPECT_FALSE(view->isExpanded(view->model()->index(0, 0)));
}

TEST(TreeSelectTest, Contract_SingleInputHintsAndExplicitFontStayConsistent)
{
    TreeSelect box;
    EXPECT_EQ(box.minimumSizeHint().height(), box.sizeHint().height());
    auto* input = box.findChild<AutoSuggestBox*>();
    ASSERT_TRUE(input);
    QFont large = box.font();
    large.setPointSize(28);
    box.setFont(large);
    EXPECT_EQ(input->font(), large);
    EXPECT_GE(box.height(), input->fontMetrics().height() + 8);
    EXPECT_EQ(box.minimumSizeHint().height(), box.sizeHint().height());
    input->onThemeUpdated();
    EXPECT_EQ(input->font(), large);
    EXPECT_GE(box.height(), input->fontMetrics().height() + 8);
}

TEST(TreeSelectTest, Contract_LargeTreeTypingCoalescesAndConfirmationFlushesTheQuery)
{
    class Model : public QStandardItemModel {
    public:
        mutable int reads = 0;
        QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
        {
            if (role == Qt::DisplayRole)
                ++reads;
            return QStandardItemModel::data(index, role);
        }
    } model;
    for (int i = 0; i < 3000; ++i) {
        auto* item = new QStandardItem(QString("Node %1").arg(i));
        item->setData(QString::number(i), Qt::UserRole);
        model.appendRow(item);
    }
    TreeSelect box;
    box.setModel(&model);
    box.resize(320, box.sizeHint().height());
    box.show();
    box.activateWindow();
    box.open();
    QApplication::processEvents();
    auto* input = box.findChild<AutoSuggestBox*>();
    ASSERT_TRUE(input);
    input->setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return input->hasFocus(); }));
    model.reads = 0;
    QTest::keyClicks(input, "Node 2999", Qt::NoModifier, 0);
    EXPECT_EQ(box.searchText(), "Node 2999");
    EXPECT_LT(model.reads, 1000);
    QTest::keyClick(input, Qt::Key_Return);
    EXPECT_EQ(box.selectedKeys(), QStringList{"2999"});
    box.open();
    input->setFocus();
    input->selectAll();
    QTest::keyClicks(input, "Node 1001", Qt::NoModifier, 0);
    const QPersistentModelIndex moved = model.index(1001, 0);
    model.sort(0, Qt::DescendingOrder);
    EXPECT_NE(moved.row(), 1001);
    QTest::keyClick(input, Qt::Key_Return);
    EXPECT_EQ(box.selectedKeys(), QStringList{"1001"});
    box.setSearchText("Node 2998");
    box.open();
    auto* tree = box.findChild<TreeView*>();
    if (!tree)
        tree = box.window()->findChild<TreeView*>();
    ASSERT_TRUE(tree);
    EXPECT_EQ(tree->model()->rowCount(), 1);
    EXPECT_EQ(tree->model()->index(0, 0).data().toString(), "Node 2998");
}
