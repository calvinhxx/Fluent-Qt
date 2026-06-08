#include <gtest/gtest.h>
#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QColor>
#include <QItemSelectionModel>
#include <QImage>
#include <QLabel>
#include <QMetaType>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include "compatibility/QtCompat.h"
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "FluentTreeItemDelegate.h"
#include "components/collections/TreeView.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

#include "components/basicinput/Button.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/Label.h"

using namespace fluent::collections;
using namespace fluent;
using fluent::basicinput::Button;
using fluent::textfields::Label;

namespace {

int defaultTreeRowHeight() {
    return Spacing::ControlHeight::Standard + Spacing::Gap::Tight;
}

/** 业务组装：为 TreeView 挂上 Fluent 行代理 */
void attachFluentDelegate(TreeView* tv, int rowHeight = defaultTreeRowHeight()) {
    tv->setItemDelegate(new treeview_test::FluentTreeItemDelegate(
        static_cast<fluent::FluentElement*>(tv), rowHeight, tv, tv));
}

/** 挂上带 CheckBox 可见的代理 */
treeview_test::FluentTreeItemDelegate* attachCheckableDelegate(TreeView* tv) {
    auto* d = new treeview_test::FluentTreeItemDelegate(
        static_cast<fluent::FluentElement*>(tv), defaultTreeRowHeight(), tv, tv);
    d->setCheckBoxVisible(true);
    tv->setItemDelegate(d);
    return d;
}

/**
 * 创建一个带层级结构的 QStandardItemModel:
 *   Work Documents
 *     ├─ XYZ Functional Spec
 *     └─ Feature Schedule
 *   Personal Documents
 *     └─ Home Remodel
 *         ├─ Contractor Contact Info
 *         └─ Paint Color Scheme
 *   Pictures
 */
QStandardItemModel* createSampleTreeModel(QObject* parent = nullptr) {
    auto* model = new QStandardItemModel(parent);

    auto* work = new QStandardItem("Work Documents");
    work->appendRow(new QStandardItem("XYZ Functional Spec"));
    work->appendRow(new QStandardItem("Feature Schedule"));
    model->appendRow(work);

    auto* personal = new QStandardItem("Personal Documents");
    auto* remodel = new QStandardItem("Home Remodel");
    remodel->appendRow(new QStandardItem("Contractor Contact Info"));
    remodel->appendRow(new QStandardItem("Paint Color Scheme"));
    personal->appendRow(remodel);
    model->appendRow(personal);

    model->appendRow(new QStandardItem("Pictures"));

    return model;
}

/** 绑定 sample model + delegate */
QStandardItemModel* attachSampleModel(TreeView* tv) {
    auto* model = createSampleTreeModel(tv);
    tv->setModel(model);
    attachFluentDelegate(tv);
    return model;
}

int topLevelCount(TreeView* tv) {
    return tv->model() ? tv->model()->rowCount() : 0;
}

QString itemText(TreeView* tv, const QModelIndex& index) {
    return index.isValid() ? index.data(Qt::DisplayRole).toString() : QString();
}

/** 离屏显示: 触发布局计算但不弹出可见窗口 */
void showOffscreen(QWidget* w) {
    w->setAttribute(Qt::WA_DontShowOnScreen, true);
    w->show();
    QApplication::processEvents();
}

void processEvents() {
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

bool isAccentLike(const QColor& color, const QColor& accent) {
    constexpr int kTolerance = 42;
    return color.alpha() > 160
        && qAbs(color.red() - accent.red()) <= kTolerance
        && qAbs(color.green() - accent.green()) <= kTolerance
        && qAbs(color.blue() - accent.blue()) <= kTolerance;
}

QImage renderWidget(QWidget* widget) {
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    painter.end();
    return image;
}

int leftMostAccentPixelX(QWidget* widget, const QRect& rowRect, const QColor& accent) {
    const QImage image = renderWidget(widget);
    const int top = qBound(0, rowRect.top(), image.height() - 1);
    const int bottom = qBound(0, rowRect.bottom(), image.height() - 1);
    for (int x = 0; x < image.width(); ++x) {
        for (int y = top; y <= bottom; ++y) {
            if (isAccentLike(QColor::fromRgba(image.pixel(x, y)), accent))
                return x;
        }
    }
    return -1;
}

bool hasAccentPixelInRect(QWidget* widget, const QRect& rect, const QColor& accent) {
    const QImage image = renderWidget(widget);
    const QRect bounded = rect.intersected(QRect(0, 0, image.width(), image.height()));
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (isAccentLike(QColor::fromRgba(image.pixel(x, y)), accent))
                return true;
        }
    }
    return false;
}

/**
 * 创建一个带 CheckStateRole 的可勾选树模型 (WinUI Multi-selection 模式)
 */
QStandardItemModel* createCheckableTreeModel(QObject* parent = nullptr) {
    auto* model = new QStandardItemModel(parent);

    auto makeCheckable = [](QStandardItem* item, Qt::CheckState state) {
        item->setCheckable(true);
        item->setCheckState(state);
        return item;
    };

    auto* work = makeCheckable(new QStandardItem("Work Documents"), Qt::PartiallyChecked);
    work->appendRow(makeCheckable(new QStandardItem("XYZ Functional Spec"), Qt::Checked));
    work->appendRow(makeCheckable(new QStandardItem("Feature Schedule"), Qt::Unchecked));
    model->appendRow(work);

    auto* personal = makeCheckable(new QStandardItem("Personal Documents"), Qt::Checked);
    auto* remodel = makeCheckable(new QStandardItem("Home Remodel"), Qt::Checked);
    remodel->appendRow(makeCheckable(new QStandardItem("Contractor Contact Info"), Qt::Checked));
    remodel->appendRow(makeCheckable(new QStandardItem("Paint Color Scheme"), Qt::Checked));
    personal->appendRow(remodel);
    model->appendRow(personal);

    model->appendRow(makeCheckable(new QStandardItem("Pictures"), Qt::Unchecked));
    return model;
}

void selectCheckedRows(TreeView* tv, const QModelIndex& parent = QModelIndex()) {
    if (!tv || !tv->model() || !tv->selectionModel())
        return;

    for (int row = 0; row < tv->model()->rowCount(parent); ++row) {
        const QModelIndex index = tv->model()->index(row, 0, parent);
        const QVariant checkState = index.data(Qt::CheckStateRole);
        if (checkState.isValid() && static_cast<Qt::CheckState>(checkState.toInt()) == Qt::Checked)
            tv->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selectCheckedRows(tv, index);
    }
}

/**
 * 创建带图标字形的树模型 (WinUI ItemTemplateSelector 模式)
 * 文件夹使用 Folder 图标，文档使用 Document 图标
 */
QStandardItemModel* createIconTreeModel(QObject* parent = nullptr) {
    auto* model = new QStandardItemModel(parent);

    auto makeFolder = [](const QString& text) {
        auto* item = new QStandardItem(text);
        item->setData(Typography::Icons::Folder, treeview_test::IconGlyphRole);
        return item;
    };
    auto makeDoc = [](const QString& text) {
        auto* item = new QStandardItem(text);
        item->setData(Typography::Icons::Document, treeview_test::IconGlyphRole);
        return item;
    };

    auto* work = makeFolder("Work Documents");
    work->appendRow(makeDoc("XYZ Functional Spec"));
    work->appendRow(makeDoc("Feature Schedule"));
    model->appendRow(work);

    auto* personal = makeFolder("Personal Documents");
    auto* remodel = makeFolder("Home Remodel");
    remodel->appendRow(makeDoc("Contractor Contact Info"));
    remodel->appendRow(makeDoc("Paint Color Scheme"));
    personal->appendRow(remodel);
    model->appendRow(personal);

    model->appendRow(makeFolder("Pictures"));
    return model;
}

} // namespace

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class TreeViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        qRegisterMetaType<fluent::collections::TreeView::IndicatorVerticalDirection>(
            "fluent::collections::TreeView::IndicatorVerticalDirection");
        qRegisterMetaType<fluent::collections::TreeView::IndicatorHierarchyTransition>(
            "fluent::collections::TreeView::IndicatorHierarchyTransition");
    }

    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 500);
        window->setWindowTitle("Fluent TreeView Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Model & Data
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, EmptyTreeView) {
    TreeView* tv = new TreeView(window);
    EXPECT_EQ(topLevelCount(tv), 0);
    EXPECT_FALSE(tv->selectedItem().isValid());
}

TEST_F(TreeViewTest, SampleModelStructure) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    EXPECT_EQ(topLevelCount(tv), 3);
    EXPECT_EQ(model->item(0)->text(), "Work Documents");
    EXPECT_EQ(model->item(0)->rowCount(), 2);
    EXPECT_EQ(model->item(1)->text(), "Personal Documents");
    EXPECT_EQ(model->item(1)->rowCount(), 1);
    EXPECT_EQ(model->item(2)->text(), "Pictures");
    EXPECT_EQ(model->item(2)->rowCount(), 0);
}

TEST_F(TreeViewTest, DeepNesting) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    // Personal Documents → Home Remodel → children
    auto* personal = model->item(1);
    auto* remodel = personal->child(0);
    ASSERT_NE(remodel, nullptr);
    EXPECT_EQ(remodel->text(), "Home Remodel");
    EXPECT_EQ(remodel->rowCount(), 2);
    EXPECT_EQ(remodel->child(0)->text(), "Contractor Contact Info");
    EXPECT_EQ(remodel->child(1)->text(), "Paint Color Scheme");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Selection
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, DefaultSelectionMode) {
    TreeView* tv = new TreeView(window);
    EXPECT_EQ(tv->selectionMode(), TreeSelectionMode::Single);
}

TEST_F(TreeViewTest, SelectionModeSwitch) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);

    QSignalSpy spy(tv, &TreeView::selectionModeChanged);

    tv->setSelectionMode(TreeSelectionMode::Multiple);
    EXPECT_EQ(tv->selectionMode(), TreeSelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);

    tv->setSelectionMode(TreeSelectionMode::None);
    EXPECT_EQ(tv->selectionMode(), TreeSelectionMode::None);
    EXPECT_EQ(spy.count(), 2);

    tv->setSelectionMode(TreeSelectionMode::Extended);
    EXPECT_EQ(tv->selectionMode(), TreeSelectionMode::Extended);
    EXPECT_EQ(spy.count(), 3);
}

TEST_F(TreeViewTest, SelectionModeDuplicateIgnored) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::selectionModeChanged);

    tv->setSelectionMode(TreeSelectionMode::Single);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(TreeViewTest, SetSelectedItem) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->setSelectedItem(model->index(0, 0));
    EXPECT_EQ(tv->selectedItem(), model->index(0, 0));
}

TEST_F(TreeViewTest, ClearSelectionOnInvalidIndex) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->setSelectedItem(model->index(0, 0));
    EXPECT_TRUE(tv->selectedItem().isValid());

    tv->setSelectedItem(QModelIndex());
    EXPECT_FALSE(tv->selectedItem().isValid());
}

TEST_F(TreeViewTest, IndicatorMotionDefaultsAndAnimationToggle) {
    TreeView* tv = new TreeView(window);

    EXPECT_DOUBLE_EQ(tv->indicatorMotionProgress(), 1.0);
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::None);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::None);
    EXPECT_TRUE(tv->isIndicatorMotionAnimationEnabled());

    QSignalSpy animationSpy(tv, &TreeView::indicatorMotionAnimationEnabledChanged);
    tv->setIndicatorMotionAnimationEnabled(true);
    EXPECT_EQ(animationSpy.count(), 0);

    tv->setIndicatorMotionAnimationEnabled(false);
    EXPECT_FALSE(tv->isIndicatorMotionAnimationEnabled());
    EXPECT_EQ(animationSpy.count(), 1);
    EXPECT_DOUBLE_EQ(tv->indicatorMotionProgress(), 1.0);
}

TEST_F(TreeViewTest, IndicatorMotionClassifiesInitialUpwardDownwardAndSameLevel) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    const QModelIndex pictures = model->index(2, 0);

    tv->setSelectedItem(work);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), work);
    EXPECT_FALSE(tv->indicatorMotionPreviousIndex().isValid());
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::None);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::None);

    tv->setSelectedItem(pictures);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionPreviousIndex(), work);
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), pictures);
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Down);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::SameLevel);

    tv->setSelectedItem(personal);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Up);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::SameLevel);
    EXPECT_DOUBLE_EQ(tv->selectedIndicatorProgress(personal), 1.0);
}

TEST_F(TreeViewTest, IndicatorMotionClassifiesHierarchyTransitions) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex workChild0 = model->item(0)->child(0)->index();
    const QModelIndex workChild1 = model->item(0)->child(1)->index();

    tv->setSelectedItem(work);
    processEvents();
    tv->setSelectedItem(workChild0);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Down);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::Inward);

    tv->setSelectedItem(workChild1);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Down);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::SameLevel);

    tv->setSelectedItem(work);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Up);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::Outward);
}

TEST_F(TreeViewTest, IndicatorMotionClearsOnInvalidResetSelectionModelAndCollapse) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex child = model->item(0)->child(0)->index();
    tv->setSelectedItem(work);
    tv->setSelectedItem(child);
    processEvents();
    EXPECT_TRUE(tv->indicatorMotionCurrentIndex().isValid());

    tv->setSelectedItem(QModelIndex());
    processEvents();
    EXPECT_FALSE(tv->indicatorMotionCurrentIndex().isValid());
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::None);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::None);

    tv->setSelectedItem(work);
    tv->setSelectedItem(child);
    processEvents();
    tv->setSelectionModel(new QItemSelectionModel(model, tv));
    processEvents();
    EXPECT_FALSE(tv->indicatorMotionCurrentIndex().isValid());

    tv->setSelectedItem(work);
    tv->setSelectedItem(child);
    processEvents();
    tv->collapse(work);
    processEvents();
    EXPECT_FALSE(tv->indicatorMotionCurrentIndex().isValid());

    tv->expand(work);
    tv->setSelectedItem(work);
    tv->setSelectedItem(model->item(0)->child(0)->index());
    processEvents();
    model->clear();
    processEvents();
    EXPECT_FALSE(tv->indicatorMotionCurrentIndex().isValid());
}

TEST_F(TreeViewTest, IndicatorMotionSuppressesDuplicateSelectionAndSnapsWhenAnimationDisabled) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    QSignalSpy progressSpy(tv, &TreeView::indicatorMotionProgressChanged);
    QSignalSpy directionSpy(tv, &TreeView::indicatorMotionDirectionChanged);
    QSignalSpy hierarchySpy(tv, &TreeView::indicatorHierarchyTransitionChanged);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    tv->setSelectedItem(work);
    processEvents();
    const int directionAfterInitial = directionSpy.count();
    const int hierarchyAfterInitial = hierarchySpy.count();
    const int progressAfterInitial = progressSpy.count();

    tv->setSelectedItem(work);
    processEvents();
    EXPECT_EQ(directionSpy.count(), directionAfterInitial);
    EXPECT_EQ(hierarchySpy.count(), hierarchyAfterInitial);
    EXPECT_EQ(progressSpy.count(), progressAfterInitial);

    tv->setSelectedItem(personal);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Down);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::SameLevel);
    EXPECT_DOUBLE_EQ(tv->indicatorMotionProgress(), 1.0);
    const int directionAfterMove = directionSpy.count();
    const int hierarchyAfterMove = hierarchySpy.count();
    const int progressAfterMove = progressSpy.count();

    tv->setSelectedItem(personal);
    processEvents();
    EXPECT_EQ(directionSpy.count(), directionAfterMove);
    EXPECT_EQ(hierarchySpy.count(), hierarchyAfterMove);
    EXPECT_EQ(progressSpy.count(), progressAfterMove);
}

TEST_F(TreeViewTest, IndicatorMotionProgressAnimatesWhenEnabled) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    tv->setSelectedItem(work);
    processEvents();
    tv->setSelectedItem(personal);
    processEvents();

    EXPECT_EQ(tv->indicatorMotionDirection(), TreeView::IndicatorVerticalDirection::Down);
    EXPECT_LT(tv->indicatorMotionProgress(), 1.0);
    EXPECT_LT(tv->selectedIndicatorProgress(personal), 1.0);

    QTest::qWait(320);
    processEvents();
    EXPECT_DOUBLE_EQ(tv->indicatorMotionProgress(), 1.0);
    EXPECT_DOUBLE_EQ(tv->selectedIndicatorProgress(personal), 1.0);
}

TEST_F(TreeViewTest, SelectionIndicatorVisibilityAndStyleSetters) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    EXPECT_FALSE(tv->selectionIndicatorVisible());
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorInset(), 6.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorStyle().width, 3.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorStyle().height, 16.0);
    EXPECT_EQ(tv->selectionIndicatorStyle().insetRole, -1);

    tv->setSelectionIndicatorVisible(true);
    EXPECT_TRUE(tv->selectionIndicatorVisible());

    QSignalSpy styleSpy(tv, &TreeView::selectionIndicatorStyleChanged);
    constexpr int kIndicatorInsetRole = Qt::UserRole + 120;
    TreeView::SelectionIndicatorStyle style = tv->selectionIndicatorStyle();
    style.inset = 8.0;
    style.width = 5.0;
    style.height = 12.0;
    style.insetRole = kIndicatorInsetRole;
    tv->setSelectionIndicatorStyle(style);
    EXPECT_EQ(styleSpy.count(), 1);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorInset(), 8.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorStyle().width, 5.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorStyle().height, 12.0);
    EXPECT_EQ(tv->selectionIndicatorStyle().insetRole, kIndicatorInsetRole);

    tv->setSelectionIndicatorInset(7.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorInset(), 7.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorStyle().width, 5.0);
    EXPECT_EQ(tv->selectionIndicatorStyle().insetRole, kIndicatorInsetRole);

    tv->setFixedSize(350, 400);
    showOffscreen(window);
    const QModelIndex work = model->index(0, 0);
    model->item(0)->setData(42.0, kIndicatorInsetRole);
    tv->setSelectedItem(work);
    processEvents();
    EXPECT_NEAR(tv->selectedIndicatorRect(1.0).left(),
                tv->visualRect(work).left() + 42.0,
                0.01);
}

TEST_F(TreeViewTest, SelectionIndicatorPaintsAccentPillNearLeftOfSelectedRow) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setSelectionIndicatorVisible(true);
    tv->setSelectionIndicatorInset(7.0);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    tv->setSelectedItem(work);
    processEvents();

    const QColor accent = tv->themeColors().accentDefault;
    const QRect rowRect = tv->visualRect(work);
    // The pill sits a few px inside the left edge of the row.
    const QRect pillProbe(0, rowRect.top(), 14, rowRect.height());
    EXPECT_TRUE(hasAccentPixelInRect(tv->viewport(), pillProbe, accent));
    // ...and nothing accent-colored on the far right of the row.
    const QRect rightProbe(rowRect.right() - 20, rowRect.top(), 20, rowRect.height());
    EXPECT_FALSE(hasAccentPixelInRect(tv->viewport(), rightProbe, accent));
}

TEST_F(TreeViewTest, AnimatedCollapseDefersModelCollapseUntilFinished) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    tv->toggleExpanded(work);
    processEvents();
    EXPECT_TRUE(tv->isExpanded(work));

    // Collapse is animated while on screen: the row stays expanded mid-flight.
    tv->toggleExpanded(work);
    processEvents();
    EXPECT_TRUE(tv->isExpanded(work));

    // After the reveal finishes the real collapse lands.
    for (int i = 0; i < 60 && tv->isExpanded(work); ++i)
        QTest::qWait(10);
    EXPECT_FALSE(tv->isExpanded(work));
}

TEST_F(TreeViewTest, AnimatedCollapseReversesBackToExpandedOnRetoggle) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    tv->toggleExpanded(work);
    for (int i = 0; i < 60 && !tv->isExpanded(work); ++i)
        QTest::qWait(10);
    QTest::qWait(300);   // settle the expand reveal
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));

    // Begin an animated collapse, then immediately re-toggle to reverse it.
    tv->toggleExpanded(work);   // start deferred collapse
    QTest::qWait(40);           // let it run partway
    tv->toggleExpanded(work);   // reverse → expand again
    QTest::qWait(320);
    processEvents();
    EXPECT_TRUE(tv->isExpanded(work));   // never actually collapsed
}

TEST_F(TreeViewTest, ExpandRevealCommitsWhenAnotherParentToggles) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    ASSERT_FALSE(tv->isExpanded(work));
    ASSERT_FALSE(tv->isExpanded(personal));

    tv->toggleExpanded(work);
    QTest::qWait(40);
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));

    tv->toggleExpanded(personal);
    processEvents();
    EXPECT_TRUE(tv->isExpanded(work));
    EXPECT_TRUE(tv->isExpanded(personal));
    EXPECT_DOUBLE_EQ(tv->chevronRotation(work), 1.0);
}

TEST_F(TreeViewTest, CollapseRevealFinalizesWhenAnotherParentToggles) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    tv->expandAll();
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));
    ASSERT_TRUE(tv->isExpanded(personal));

    tv->toggleExpanded(work);
    QTest::qWait(40);
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));

    tv->toggleExpanded(personal);
    processEvents();
    EXPECT_FALSE(tv->isExpanded(work));
    EXPECT_TRUE(tv->isExpanded(personal));

    for (int i = 0; i < 60 && tv->isExpanded(personal); ++i)
        QTest::qWait(10);
    EXPECT_FALSE(tv->isExpanded(personal));
}

TEST_F(TreeViewTest, AnimatedCollapseHitTestingFollowsVisualRows) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex firstChild = model->item(0)->child(0)->index();
    const QModelIndex secondChild = model->item(0)->child(1)->index();
    const QModelIndex personal = model->index(1, 0);

    tv->toggleExpanded(work);
    QTest::qWait(320);
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));

    const qreal subtreeHeight = tv->visualRect(firstChild).height() + tv->visualRect(secondChild).height();
    const QRect personalRect = tv->visualRect(personal);
    ASSERT_FALSE(personalRect.isEmpty());

    tv->toggleExpanded(work);
    QTest::qWait(80);
    processEvents();

    const qreal progress = tv->chevronRotation(work);
    ASSERT_GT(progress, 0.0);
    ASSERT_LT(progress, 1.0);

    const QPoint visualPersonalCenter = personalRect.center()
        - QPoint(0, qRound(subtreeHeight * (1.0 - progress)));
    EXPECT_EQ(tv->indexAt(visualPersonalCenter), personal);

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, visualPersonalCenter);
    processEvents();
    EXPECT_EQ(tv->selectedItem(), personal);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expand / Collapse
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ExpandCollapseTopLevel) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    QModelIndex workIdx = model->index(0, 0);

    // Initially collapsed
    EXPECT_FALSE(tv->isExpanded(workIdx));

    // Expand
    tv->expand(workIdx);
    EXPECT_TRUE(tv->isExpanded(workIdx));

    // Collapse
    tv->collapse(workIdx);
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, ToggleExpanded) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    QModelIndex workIdx = model->index(0, 0);

    EXPECT_FALSE(tv->isExpanded(workIdx));
    tv->toggleExpanded(workIdx);
    EXPECT_TRUE(tv->isExpanded(workIdx));
    tv->toggleExpanded(workIdx);
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, ExpandAll) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->expandAll();

    EXPECT_TRUE(tv->isExpanded(model->index(0, 0)));
    EXPECT_TRUE(tv->isExpanded(model->index(1, 0)));
    // Personal Documents → Home Remodel
    EXPECT_TRUE(tv->isExpanded(model->item(1)->child(0)->index()));
}

TEST_F(TreeViewTest, CollapseAll) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->expandAll();
    tv->collapseAll();

    EXPECT_FALSE(tv->isExpanded(model->index(0, 0)));
    EXPECT_FALSE(tv->isExpanded(model->index(1, 0)));
}

TEST_F(TreeViewTest, ToggleInvalidIndexNoOp) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);

    // Should not crash
    tv->toggleExpanded(QModelIndex());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expand Animation
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ExpandAnimationTriggered) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab(); // force layout

    QModelIndex workIdx = model->index(0, 0);
    tv->expand(workIdx);
    QApplication::processEvents();

    // State changes immediately even during animation
    EXPECT_TRUE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, ExpandAnimationExposesChildrenDuringReveal) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    showOffscreen(window);

    QModelIndex workIdx = model->index(0, 0);
    QModelIndex childIdx = model->index(0, 0, workIdx);
    ASSERT_TRUE(childIdx.isValid());
    EXPECT_TRUE(tv->visualRect(childIdx).isEmpty());

    tv->expand(workIdx);
    QApplication::processEvents();

    EXPECT_TRUE(tv->isExpanded(workIdx));
    EXPECT_FALSE(tv->visualRect(childIdx).isEmpty());
}

TEST_F(TreeViewTest, ExpandAnimationCompletesCleanly) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab();

    QModelIndex workIdx = model->index(0, 0);
    tv->expand(workIdx);

    // Let animation finish (Normal = 250ms)
    QTest::qWait(300);
    QApplication::processEvents();

    EXPECT_TRUE(tv->isExpanded(workIdx));
    // Verify children are visible
    EXPECT_GT(model->rowCount(workIdx), 0);
}

TEST_F(TreeViewTest, RapidToggleNoCrash) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab();

    QModelIndex workIdx = model->index(0, 0);

    // Rapidly toggle multiple times — should not crash
    for (int i = 0; i < 10; ++i) {
        tv->toggleExpanded(workIdx);
        QApplication::processEvents();
    }
    // Final state depends on even/odd — just verify no crash
    SUCCEED();
}

TEST_F(TreeViewTest, ExpandAllSkipsAnimation) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab();

    tv->expandAll();
    QApplication::processEvents();

    // All nodes expanded immediately (no animation)
    EXPECT_TRUE(tv->isExpanded(model->index(0, 0)));
    EXPECT_TRUE(tv->isExpanded(model->index(1, 0)));
}

TEST_F(TreeViewTest, CollapseStopsExpandAnimation) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->resize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab();

    QModelIndex workIdx = model->index(0, 0);

    // Expand (starts animation)
    tv->expand(workIdx);
    QApplication::processEvents();
    EXPECT_TRUE(tv->isExpanded(workIdx));

    // Immediately collapse — should stop animation and not crash
    tv->collapse(workIdx);
    QApplication::processEvents();
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Appearance Properties
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, DefaultBorderVisible) {
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->borderVisible());
    EXPECT_TRUE(tv->isBorderVisible());
}

TEST_F(TreeViewTest, SetBorderVisible) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::borderVisibleChanged);

    tv->setBorderVisible(false);
    EXPECT_FALSE(tv->borderVisible());
    EXPECT_FALSE(tv->isBorderVisible());
    EXPECT_EQ(spy.count(), 1);

    tv->setBorderVisible(false); // duplicate
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, DefaultBackgroundVisible) {
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->backgroundVisible());
    EXPECT_TRUE(tv->isBackgroundVisible());
}

TEST_F(TreeViewTest, SetBackgroundVisible) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::backgroundVisibleChanged);

    tv->setBackgroundVisible(false);
    EXPECT_FALSE(tv->backgroundVisible());
    EXPECT_FALSE(tv->isBackgroundVisible());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, HeaderText) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::headerTextChanged);

    EXPECT_TRUE(tv->headerText().isEmpty());

    tv->setHeaderText("Documents");
    EXPECT_EQ(tv->headerText(), "Documents");
    EXPECT_EQ(spy.count(), 1);

    tv->setHeaderText("Documents"); // duplicate
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, PlaceholderText) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::placeholderTextChanged);

    tv->setPlaceholderText("No items");
    EXPECT_EQ(tv->placeholderText(), "No items");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, FontRole) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::fontRoleChanged);

    EXPECT_EQ(tv->fontRole(), Typography::FontRole::Body);

    tv->setFontRole(Typography::FontRole::Caption);
    EXPECT_EQ(tv->fontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(spy.count(), 1);

    tv->setFontRole(Typography::FontRole::Caption); // duplicate
    EXPECT_EQ(spy.count(), 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Tree-specific
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, IndentationDefault) {
    TreeView* tv = new TreeView(window);
    EXPECT_EQ(tv->indentation(), 16);
}

TEST_F(TreeViewTest, RootIsDecoratedOff) {
    TreeView* tv = new TreeView(window);
    EXPECT_FALSE(tv->rootIsDecorated());
}

TEST_F(TreeViewTest, HeaderHiddenByDefault) {
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->isHeaderHidden());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Fluent Scroll Bar
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, FluentScrollBarExists) {
    TreeView* tv = new TreeView(window);
    EXPECT_NE(tv->verticalFluentScrollBar(), nullptr);
    EXPECT_NE(tv->horizontalFluentScrollBar(), nullptr);
}

TEST_F(TreeViewTest, FluentScrollBarHiddenWhenShort) {
    TreeView* tv = new TreeView(window);
    tv->setFixedSize(300, 400);
    attachSampleModel(tv);  // only 3 top-level items, should fit

    showOffscreen(window);

    // 3 collapsed items at 36px each = 108px < 400px → no scroll needed
    EXPECT_FALSE(tv->verticalFluentScrollBar()->isVisible());
}

TEST_F(TreeViewTest, FluentScrollBarVisibleWhenTall) {
    TreeView* tv = new TreeView(window);
    tv->setFixedSize(300, 60);  // very small → will need scrolling

    auto* model = new QStandardItemModel(tv);
    for (int i = 0; i < 20; ++i) {
        model->appendRow(new QStandardItem(QString("Item %1").arg(i)));
    }
    tv->setModel(model);
    attachFluentDelegate(tv);

    showOffscreen(window);
    QTest::qWait(50);

    EXPECT_TRUE(tv->verticalFluentScrollBar()->isVisible());
}

// ═══════════════════════════════════════════════════════════════════════════════
// itemClicked signal
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ItemClickedSignalEmitted) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);

    showOffscreen(window);

    QSignalSpy spy(tv, &TreeView::itemClicked);

    QModelIndex idx = model->index(0, 0);
    QRect rect = tv->visualRect(idx);
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    EXPECT_GE(spy.count(), 1);
    QModelIndex clickedIdx = spy.at(0).at(0).value<QModelIndex>();
    EXPECT_EQ(clickedIdx, idx);
}

TEST_F(TreeViewTest, ItemPressedSignalEmittedOnMousePress) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);

    showOffscreen(window);

    QSignalSpy spy(tv, &TreeView::itemPressed);

    const QModelIndex idx = model->index(0, 0);
    const QRect rect = tv->visualRect(idx);
    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QApplication::processEvents();

    EXPECT_GE(spy.count(), 1);
    const QModelIndex pressedIdx = spy.at(0).at(0).value<QModelIndex>();
    EXPECT_EQ(pressedIdx, idx);
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Chevron-only expand & checkbox click
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ClickRowBodyDoesNotExpand) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    QModelIndex workIdx = model->index(0, 0);
    EXPECT_FALSE(tv->isExpanded(workIdx));

    // Click on the text area (far right) — should NOT expand
    QRect rect = tv->visualRect(workIdx);
    QPoint textCenter(rect.right() - 20, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, textCenter);
    QApplication::processEvents();
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, ClickChevronAreaExpands) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    QModelIndex workIdx = model->index(0, 0);
    EXPECT_FALSE(tv->isExpanded(workIdx));

    // Click on the chevron area (near left side of the row)
    QRect rect = tv->visualRect(workIdx);
    // Chevron is at option.rect.left() + 12 + chevronAreaW/2 = left + 22
    QPoint chevronCenter(rect.left() + 22, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, chevronCenter);
    QApplication::processEvents();
    EXPECT_TRUE(tv->isExpanded(workIdx));

    // Click again to collapse. When the pane is on screen the collapse is
    // animated, so the real model collapse is deferred until the reveal finishes.
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, chevronCenter);
    for (int i = 0; i < 60 && tv->isExpanded(workIdx); ++i)
        QTest::qWait(10);
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, CheckBoxClickTogglesState) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    auto* d = attachCheckableDelegate(tv);
    (void)d;
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    // Pictures is index(2, 0) — starts as Unchecked
    QModelIndex picturesIdx = model->index(2, 0);
    EXPECT_EQ(model->item(2)->checkState(), Qt::Unchecked);

    // Click on checkbox area
    QRect rect = tv->visualRect(picturesIdx);
    // With checkable delegate, checkbox is at option.rect.left() + 12 + cbAreaW/2
    QPoint cbCenter(rect.left() + 12 + 11, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(model->item(2)->checkState(), Qt::Checked);

    // Click again to uncheck
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(model->item(2)->checkState(), Qt::Unchecked);
}

TEST_F(TreeViewTest, ParentCheckCascadesToChildren) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // "Work Documents" starts PartiallyChecked, children: Checked + Unchecked
    QModelIndex workIdx = model->index(0, 0);
    EXPECT_EQ(model->item(0)->checkState(), Qt::PartiallyChecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Unchecked);

    // Click parent checkbox → should become Checked, both children Checked
    QRect rect = tv->visualRect(workIdx);
    QPoint cbCenter(rect.left() + 12 + 11, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(model->item(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Checked);

    // Click again → Unchecked, all children Unchecked
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(model->item(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Unchecked);
}

TEST_F(TreeViewTest, ChildCheckUpdatesParentTriState) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // "Personal Documents" starts Checked, child "Home Remodel" Checked
    QStandardItem* personal = model->item(1);
    QStandardItem* remodel = personal->child(0);
    EXPECT_EQ(personal->checkState(), Qt::Checked);
    EXPECT_EQ(remodel->checkState(), Qt::Checked);

    // Uncheck "Home Remodel" → parent should become Unchecked (only child)
    QModelIndex remodelIdx = model->index(0, 0, model->index(1, 0));
    QRect rect = tv->visualRect(remodelIdx);
    QPoint cbCenter(rect.left() + 12 + 11, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(remodel->checkState(), Qt::Unchecked);
    EXPECT_EQ(personal->checkState(), Qt::Unchecked);

    // Re-check "Home Remodel" → parent back to Checked
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();
    EXPECT_EQ(remodel->checkState(), Qt::Checked);
    EXPECT_EQ(personal->checkState(), Qt::Checked);
}

TEST_F(TreeViewTest, DeepCascadeGrandchildren) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // Check "Personal Documents" (top) → all descendants should be Checked
    QStandardItem* personal = model->item(1);
    QStandardItem* remodel = personal->child(0);

    // First uncheck "Contractor Contact Info" to make tree partially checked
    remodel->child(0)->setCheckState(Qt::Unchecked);
    remodel->setCheckState(Qt::PartiallyChecked);
    personal->setCheckState(Qt::PartiallyChecked);
    QApplication::processEvents();

    // Click on "Personal Documents" checkbox → full cascade to Checked
    QModelIndex personalIdx = model->index(1, 0);
    QRect rect = tv->visualRect(personalIdx);
    QPoint cbCenter(rect.left() + 12 + 11, rect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    QApplication::processEvents();

    EXPECT_EQ(personal->checkState(), Qt::Checked);
    EXPECT_EQ(remodel->checkState(), Qt::Checked);
    EXPECT_EQ(remodel->child(0)->checkState(), Qt::Checked);
    EXPECT_EQ(remodel->child(1)->checkState(), Qt::Checked);
}

TEST_F(TreeViewTest, ChevronRotationApi) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);
    window->resize(320, 420);
    QPixmap pm = tv->grab();

    QModelIndex workIdx = model->index(0, 0);

    // Initially collapsed → rotation should be 0
    EXPECT_DOUBLE_EQ(tv->chevronRotation(workIdx), 0.0);

    // Expand and wait for animation to finish
    tv->expand(workIdx);
    QTest::qWait(300);
    QApplication::processEvents();

    EXPECT_DOUBLE_EQ(tv->chevronRotation(workIdx), 1.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drag reorder (file-manager style)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, DefaultCanReorderItems) {
    TreeView* tv = new TreeView(window);
    EXPECT_FALSE(tv->canReorderItems());
}

TEST_F(TreeViewTest, SetCanReorderItems) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::canReorderItemsChanged);
    tv->setCanReorderItems(true);
    EXPECT_TRUE(tv->canReorderItems());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, CanReorderItemsSignalNotDuplicate) {
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::canReorderItemsChanged);
    tv->setCanReorderItems(true);
    tv->setCanReorderItems(true); // same
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, DragReorderTopLevelSiblings) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    // Top-level: "Work Documents", "Personal Documents", "Pictures"
    EXPECT_EQ(model->item(0)->text(), "Work Documents");
    EXPECT_EQ(model->item(1)->text(), "Personal Documents");
    EXPECT_EQ(model->item(2)->text(), "Pictures");

    // Simulate model-level reorder (same as Between drop does)
    auto row = model->takeRow(0);
    model->insertRow(1, row);
    EXPECT_EQ(model->item(0)->text(), "Personal Documents");
    EXPECT_EQ(model->item(1)->text(), "Work Documents");
    EXPECT_EQ(model->item(2)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragReorderChildSiblings) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // Work Documents children: "XYZ Functional Spec", "Feature Schedule"
    QStandardItem* work = model->item(0);
    EXPECT_EQ(work->child(0)->text(), "XYZ Functional Spec");
    EXPECT_EQ(work->child(1)->text(), "Feature Schedule");

    // Move child 0 to after child 1
    auto row = work->takeRow(0);
    work->insertRow(1, row);
    EXPECT_EQ(work->child(0)->text(), "Feature Schedule");
    EXPECT_EQ(work->child(1)->text(), "XYZ Functional Spec");
}

TEST_F(TreeViewTest, DragReorderEmitsSignal) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    QSignalSpy spy(tv, &TreeView::itemReordered);

    // Simulate full drag: press → move past threshold → move to target → release
    QModelIndex srcIdx = model->index(0, 0);
    QRect srcRect = tv->visualRect(srcIdx);
    QPoint start = srcRect.center();

    // Target: bottom edge of row 1 (insert after Personal Documents)
    QModelIndex targetIdx = model->index(1, 0);
    QRect targetRect = tv->visualRect(targetIdx);
    QPoint end(start.x(), targetRect.bottom() + 2);

    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    // Move past drag distance
    QPoint moved(start.x(), start.y() + QApplication::startDragDistance() + 5);
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();

    // Move to target
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();

    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    EXPECT_EQ(spy.count(), 1);

    // Verify order changed
    EXPECT_EQ(model->item(0)->text(), "Personal Documents");
    EXPECT_EQ(model->item(1)->text(), "Work Documents");
    EXPECT_EQ(model->item(2)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragDisabledDoesNotReorder) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    QModelIndex srcIdx = model->index(0, 0);
    QRect srcRect = tv->visualRect(srcIdx);
    QPoint start = srcRect.center();

    QModelIndex targetIdx = model->index(2, 0);
    QRect targetRect = tv->visualRect(targetIdx);
    QPoint end(start.x(), targetRect.bottom() + 5);

    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QPoint moved(start.x(), start.y() + QApplication::startDragDistance() + 5);
    QMouseEvent moveEvent(QEvent::MouseMove, QPointF(moved), QPointF(moved),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // Order unchanged
    EXPECT_EQ(model->item(0)->text(), "Work Documents");
    EXPECT_EQ(model->item(1)->text(), "Personal Documents");
    EXPECT_EQ(model->item(2)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragPreservesChildHierarchy) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    // Move "Work Documents" (with children) from row 0 to row 2
    QStandardItem* root = model->invisibleRootItem();
    auto row = root->takeRow(0);
    root->insertRow(1, row);

    // "Work Documents" at row 1 should still have its 2 children
    QStandardItem* work = model->item(1);
    EXPECT_EQ(work->text(), "Work Documents");
    EXPECT_EQ(work->rowCount(), 2);
    EXPECT_EQ(work->child(0)->text(), "XYZ Functional Spec");
    EXPECT_EQ(work->child(1)->text(), "Feature Schedule");
}

TEST_F(TreeViewTest, DragDropOntoFolder) {
    // File-manager style: drag an item onto a folder (item with children)
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // Drag "Pictures" (root row 2, leaf) onto "Work Documents" (root row 0, has children)
    QStandardItem* work = model->item(0);
    int workChildrenBefore = work->rowCount(); // 2
    EXPECT_EQ(model->rowCount(), 3);

    QModelIndex srcIdx = model->index(2, 0); // Pictures
    QRect srcRect = tv->visualRect(srcIdx);
    QPoint start = srcRect.center();

    // Target: center of "Work Documents" row (OnItem zone — middle 50%)
    QModelIndex workIdx = model->index(0, 0);
    QRect workRect = tv->visualRect(workIdx);
    QPoint end = workRect.center();

    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QPoint moved(start.x(), start.y() - QApplication::startDragDistance() - 5);
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // "Pictures" should now be a child of "Work Documents"
    EXPECT_EQ(model->rowCount(), 2); // only 2 root items left
    EXPECT_EQ(work->rowCount(), workChildrenBefore + 1);
    EXPECT_EQ(work->child(work->rowCount() - 1)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragCrossParentBetween) {
    // File-manager style: drag child item to root level (between root items)
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // Drag "XYZ Functional Spec" (Work Documents → child 0) to top edge of "Personal Documents"
    // This should insert it at root level between row 0 and row 1
    QStandardItem* work = model->item(0);
    EXPECT_EQ(work->child(0)->text(), "XYZ Functional Spec");

    QModelIndex srcIdx = work->child(0)->index();
    QRect srcRect = tv->visualRect(srcIdx);
    QPoint start = srcRect.center();

    // Target: top edge of "Personal Documents" (Between zone at root, row 1)
    QModelIndex pd = model->index(1, 0);
    QRect pdRect = tv->visualRect(pd);
    QPoint end(pdRect.center().x(), pdRect.top() + 2); // top zone → insert before

    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QPoint moved(start.x(), start.y() + QApplication::startDragDistance() + 5);
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // "XYZ Functional Spec" removed from Work Documents, inserted at root level
    EXPECT_EQ(work->rowCount(), 1);    // only 1 child left
    EXPECT_EQ(model->rowCount(), 4);   // 4 root items now
    EXPECT_EQ(model->item(1)->text(), "XYZ Functional Spec");
}

TEST_F(TreeViewTest, DragCannotDropAncestorOntoDescendant) {
    // Cycle prevention: dragging a parent onto its own child should be ignored
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setCanReorderItems(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    // Try to drag "Work Documents" (row 0) onto its child "XYZ Functional Spec"
    QModelIndex srcIdx = model->index(0, 0); // Work Documents
    QStandardItem* work = model->item(0);
    QRect srcRect = tv->visualRect(srcIdx);
    QPoint start = srcRect.center();

    QModelIndex childIdx = work->child(0)->index(); // XYZ Functional Spec
    QRect childRect = tv->visualRect(childIdx);
    QPoint end = childRect.center();

    int rootCountBefore = model->rowCount();

    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QPoint moved(start.x(), start.y() + QApplication::startDragDistance() + 5);
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // Nothing should have moved — model structure intact
    EXPECT_EQ(model->rowCount(), rootCountBefore);
    EXPECT_EQ(model->item(0)->text(), "Work Documents");
    EXPECT_EQ(work->rowCount(), 2);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Theme
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ThemeSwitchDoesNotCrash) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);

    showOffscreen(window);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QApplication::processEvents();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    QApplication::processEvents();

    // Should not crash
    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dynamic model changes
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, AddRemoveTopLevelItems) {
    TreeView* tv = new TreeView(window);
    auto* model = new QStandardItemModel(tv);
    tv->setModel(model);
    attachFluentDelegate(tv);

    EXPECT_EQ(topLevelCount(tv), 0);

    model->appendRow(new QStandardItem("Root 1"));
    EXPECT_EQ(topLevelCount(tv), 1);

    model->appendRow(new QStandardItem("Root 2"));
    EXPECT_EQ(topLevelCount(tv), 2);

    model->removeRow(0);
    EXPECT_EQ(topLevelCount(tv), 1);
    EXPECT_EQ(model->item(0)->text(), "Root 2");
}

TEST_F(TreeViewTest, AddChildrenDynamically) {
    TreeView* tv = new TreeView(window);
    auto* model = new QStandardItemModel(tv);
    tv->setModel(model);
    attachFluentDelegate(tv);

    auto* root = new QStandardItem("Root");
    model->appendRow(root);
    EXPECT_EQ(model->item(0)->rowCount(), 0);
    EXPECT_FALSE(model->hasChildren(model->index(0, 0)));

    root->appendRow(new QStandardItem("Child 1"));
    EXPECT_EQ(model->item(0)->rowCount(), 1);
    EXPECT_TRUE(model->hasChildren(model->index(0, 0)));

    root->appendRow(new QStandardItem("Child 2"));
    EXPECT_EQ(model->item(0)->rowCount(), 2);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Delegate
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, DelegateSizeHint) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);

    auto* delegate = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    ASSERT_NE(delegate, nullptr);
    EXPECT_EQ(delegate->rowHeight(), defaultTreeRowHeight());
}

TEST_F(TreeViewTest, DelegateCustomRowHeight) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    auto* delegate = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    delegate->setRowHeight(48);
    EXPECT_EQ(delegate->rowHeight(), 48);

    QStyleOptionViewItem opt;
    opt.rect = QRect(0, 0, 200, 48);
    QSize hint = delegate->sizeHint(opt, model->index(0, 0));
    EXPECT_EQ(hint.height(), 48);
}

TEST_F(TreeViewTest, DelegateConsumesIndicatorMotionWithoutChangingRowMetrics) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    auto* delegate = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    ASSERT_NE(delegate, nullptr);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex personal = model->index(1, 0);
    tv->setSelectedItem(work);
    processEvents();
    tv->setSelectedItem(personal);
    processEvents();

    const QRect movingRect = tv->visualRect(personal);
    QStyleOptionViewItem option;
    option.rect = movingRect;
    option.widget = tv;
    option.font = tv->font();
    const QSize movingSizeHint = delegate->sizeHint(option, personal);

    EXPECT_TRUE(tv->isIndicatorMotionActiveForIndex(personal));
    EXPECT_LT(tv->selectedIndicatorProgress(personal), 1.0);
    tv->viewport()->grab(movingRect);

    QTest::qWait(320);
    processEvents();
    EXPECT_EQ(tv->visualRect(personal), movingRect);
    EXPECT_EQ(delegate->sizeHint(option, personal), movingSizeHint);
    EXPECT_DOUBLE_EQ(tv->selectedIndicatorProgress(personal), 1.0);
}

TEST_F(TreeViewTest, DelegateSelectedIndicatorFollowsHierarchicalRowLocalPosition) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex parent = model->index(0, 0);
    const QModelIndex child = model->item(0)->child(0)->index();
    const QColor accent = tv->themeColors().accentDefault;

    tv->setSelectedItem(parent);
    processEvents();
    const QRect parentRect = tv->visualRect(parent);
    const int parentAccentX = leftMostAccentPixelX(tv->viewport(), parentRect, accent);
    ASSERT_GE(parentAccentX, 0);

    tv->setSelectedItem(child);
    processEvents();
    const QRect childRect = tv->visualRect(child);
    const int childAccentX = leftMostAccentPixelX(tv->viewport(), childRect, accent);
    ASSERT_GE(childAccentX, 0);

    EXPECT_NEAR(parentRect.left() + 4, parentAccentX, 2);
    EXPECT_NEAR(childRect.left() + 4, childAccentX, 2);
    EXPECT_GT(childAccentX, parentAccentX);
}

TEST_F(TreeViewTest, SelectedIndicatorAnchorsToTargetDuringHierarchyTransitions) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    tv->setSelectionIndicatorVisible(true);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex parent = model->index(0, 0);
    const QModelIndex child = model->item(0)->child(0)->index();

    tv->setSelectedItem(parent);
    processEvents();
    const QRectF parentSettled = tv->selectedIndicatorRect(1.0);

    tv->setSelectedItem(child);
    processEvents();
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::Inward);
    const QRectF inwardTarget = tv->selectedIndicatorRect(1.0);
    const QRectF inwardMid = tv->selectedIndicatorRect(0.5);
    const qreal inwardLinearX = (parentSettled.left() + inwardTarget.left()) / 2.0;
    EXPECT_GT(inwardMid.left(), inwardLinearX);
    EXPECT_NEAR(inwardMid.left(), inwardTarget.left(), 0.01);
    EXPECT_NEAR(inwardMid.height(), inwardTarget.height(), 0.01);

    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setSelectedItem(child);
    processEvents();
    const QRectF childSettled = tv->selectedIndicatorRect(1.0);

    tv->setIndicatorMotionAnimationEnabled(true);
    tv->setSelectedItem(parent);
    processEvents();
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::Outward);
    const QRectF outwardTarget = tv->selectedIndicatorRect(1.0);
    const QRectF outwardMid = tv->selectedIndicatorRect(0.5);
    const qreal outwardLinearX = (childSettled.left() + outwardTarget.left()) / 2.0;
    EXPECT_LT(outwardMid.left(), outwardLinearX);
    EXPECT_NEAR(outwardMid.left(), outwardTarget.left(), 0.01);
    EXPECT_NEAR(outwardMid.height(), outwardTarget.height(), 0.01);
}

TEST_F(TreeViewTest, CollapsingParentAfterChildToParentSelectionKeepsParentIndicator) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    tv->setSelectionIndicatorVisible(true);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex parent = model->index(0, 0);
    const QModelIndex child = model->item(0)->child(0)->index();
    ASSERT_TRUE(tv->isExpanded(parent));

    tv->setSelectedItem(child);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), child);

    tv->setSelectedItem(parent);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), parent);
    EXPECT_EQ(tv->indicatorMotionPreviousIndex(), child);
    EXPECT_EQ(tv->indicatorHierarchyTransition(), TreeView::IndicatorHierarchyTransition::Outward);

    tv->toggleExpanded(parent);
    for (int i = 0; i < 60 && tv->isExpanded(parent); ++i)
        QTest::qWait(10);
    processEvents();

    EXPECT_FALSE(tv->isExpanded(parent));
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), parent);
    EXPECT_FALSE(tv->selectedIndicatorRect(1.0).isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// paint / render (no crash)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, PaintWithExpandedItems) {
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);

    tv->expandAll();

    // Select a deep item
    auto* remodel = model->item(1)->child(0);
    tv->setSelectedItem(remodel->child(0)->index());

    // grab() 触发完整绘制流程但不弹窗
    tv->grab();
    EXPECT_TRUE(true);
}

TEST_F(TreeViewTest, PaintEmptyWithPlaceholder) {
    TreeView* tv = new TreeView(window);
    tv->setPlaceholderText("Empty tree");
    tv->setFixedSize(300, 400);

    // grab() 触发完整绘制流程但不弹窗
    tv->grab();
    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CheckBox delegate
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, CheckBoxVisibleDefault) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);
    auto* d = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    EXPECT_FALSE(d->checkBoxVisible());
}

TEST_F(TreeViewTest, CheckBoxVisibleToggle) {
    TreeView* tv = new TreeView(window);
    attachSampleModel(tv);
    auto* d = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    d->setCheckBoxVisible(true);
    EXPECT_TRUE(d->checkBoxVisible());
    d->setCheckBoxVisible(false);
    EXPECT_FALSE(d->checkBoxVisible());
}

TEST_F(TreeViewTest, CheckableModelPaintNoCrash) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    auto* d = attachCheckableDelegate(tv);
    (void)d;
    tv->setFixedSize(350, 400);

    tv->expandAll();
    tv->grab();
    EXPECT_TRUE(true);
}

TEST_F(TreeViewTest, CheckStateTracksMultiSelectionChanges) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(TreeSelectionMode::Multiple);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex spec = model->item(0)->child(0)->index();
    const QModelIndex schedule = model->item(0)->child(1)->index();
    EXPECT_EQ(model->item(0)->checkState(), Qt::PartiallyChecked);

    tv->selectionModel()->select(work, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    processEvents();
    EXPECT_TRUE(tv->selectionModel()->isSelected(work));
    EXPECT_TRUE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Checked);

    tv->selectionModel()->select(spec, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    processEvents();
    EXPECT_FALSE(tv->selectionModel()->isSelected(work));
    EXPECT_FALSE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::PartiallyChecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Checked);

    tv->selectionModel()->select(spec, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    processEvents();
    EXPECT_TRUE(tv->selectionModel()->isSelected(work));
    EXPECT_TRUE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Checked);

    tv->selectionModel()->select(work, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    processEvents();
    EXPECT_FALSE(tv->selectionModel()->isSelected(work));
    EXPECT_FALSE(tv->selectionModel()->isSelected(spec));
    EXPECT_FALSE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Unchecked);
}

TEST_F(TreeViewTest, CheckBoxClickSyncsMultiSelection) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(TreeSelectionMode::Multiple);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex pictures = model->index(2, 0);
    const QRect rowRect = tv->visualRect(pictures);
    const QPoint cbCenter(rowRect.left() + 12 + 11, rowRect.center().y());

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    processEvents();
    EXPECT_TRUE(tv->selectionModel()->isSelected(pictures));
    EXPECT_EQ(model->item(2)->checkState(), Qt::Checked);

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, cbCenter);
    processEvents();
    EXPECT_FALSE(tv->selectionModel()->isSelected(pictures));
    EXPECT_EQ(model->item(2)->checkState(), Qt::Unchecked);
}

TEST_F(TreeViewTest, CheckBoxParentAndChildClicksKeepTriStateSelection) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(TreeSelectionMode::Multiple);
    attachCheckableDelegate(tv);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex spec = model->item(0)->child(0)->index();
    const QModelIndex schedule = model->item(0)->child(1)->index();

    const QRect workRect = tv->visualRect(work);
    const QPoint workCheckBox(workRect.left() + 12 + 11, workRect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, workCheckBox);
    processEvents();
    EXPECT_TRUE(tv->selectionModel()->isSelected(work));
    EXPECT_TRUE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Checked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Checked);

    const QRect specRect = tv->visualRect(spec);
    const QPoint specCheckBox(specRect.left() + 12 + 11, specRect.center().y());
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, specCheckBox);
    processEvents();
    EXPECT_FALSE(tv->selectionModel()->isSelected(work));
    EXPECT_FALSE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::PartiallyChecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Checked);

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, specCheckBox);
    processEvents();
    EXPECT_TRUE(tv->selectionModel()->isSelected(work));
    EXPECT_TRUE(tv->selectionModel()->isSelected(spec));
    EXPECT_TRUE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Checked);

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, workCheckBox);
    processEvents();
    EXPECT_FALSE(tv->selectionModel()->isSelected(work));
    EXPECT_FALSE(tv->selectionModel()->isSelected(spec));
    EXPECT_FALSE(tv->selectionModel()->isSelected(schedule));
    EXPECT_EQ(model->item(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Unchecked);
    EXPECT_EQ(model->item(0)->child(1)->checkState(), Qt::Unchecked);
}

TEST_F(TreeViewTest, CheckableSelectionDoesNotPaintIndicator) {
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(TreeSelectionMode::Multiple);
    attachCheckableDelegate(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex pictures = model->index(2, 0);
    tv->selectionModel()->select(pictures, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    processEvents();

    const QRect rowRect = tv->visualRect(pictures);
    ASSERT_FALSE(rowRect.isEmpty());
    const QColor accent = tv->themeColors().accentDefault;
    const QRect indicatorGutter(rowRect.left() + 2, rowRect.top(), 8, rowRect.height());
    const QRect checkboxArea(rowRect.left() + 12, rowRect.top(), 24, rowRect.height());

    EXPECT_FALSE(hasAccentPixelInRect(tv->viewport(), indicatorGutter, accent));
    EXPECT_TRUE(hasAccentPixelInRect(tv->viewport(), checkboxArea, accent));
}

TEST_F(TreeViewTest, CheckStateRoleRead) {
    auto* model = createCheckableTreeModel(nullptr);
    // Work Documents → PartiallyChecked
    EXPECT_EQ(model->item(0)->checkState(), Qt::PartiallyChecked);
    // Work Documents → child 0 → Checked
    EXPECT_EQ(model->item(0)->child(0)->checkState(), Qt::Checked);
    // Pictures → Unchecked
    EXPECT_EQ(model->item(2)->checkState(), Qt::Unchecked);
    delete model;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Icon glyph delegate
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, IconGlyphRoleRead) {
    auto* model = createIconTreeModel(nullptr);
    // Work Documents → Folder icon
    EXPECT_EQ(model->item(0)->data(treeview_test::IconGlyphRole).toString(),
              Typography::Icons::Folder);
    // Work Documents → child 0 → Document icon
    EXPECT_EQ(model->item(0)->child(0)->data(treeview_test::IconGlyphRole).toString(),
              Typography::Icons::Document);
    delete model;
}

TEST_F(TreeViewTest, IconModelPaintNoCrash) {
    TreeView* tv = new TreeView(window);
    auto* model = createIconTreeModel(tv);
    tv->setModel(model);
    attachFluentDelegate(tv);
    tv->setFixedSize(350, 400);

    tv->expandAll();
    tv->grab();
    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// VisualCheck
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->setFixedSize(800, 600);
    using Edge = AnchorLayout::Edge;

    // --- ScrollArea 容器 ---
    auto* scrollArea = new QScrollArea(window);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setGeometry(0, 0, 800, 600);

    // Fluent 自定义垂直滚动条覆盖在 scrollArea 上
    auto* fluentVBar = new fluent::scrolling::ScrollBar(Qt::Vertical, scrollArea);
    fluentVBar->setObjectName("fluentScrollAreaVBar");
    auto* nativeVBar = scrollArea->verticalScrollBar();
    QObject::connect(nativeVBar,  &QScrollBar::valueChanged, fluentVBar, &QScrollBar::setValue);
    QObject::connect(fluentVBar, &QScrollBar::valueChanged, nativeVBar,  &QScrollBar::setValue);

    auto syncFluentBar = [scrollArea, fluentVBar, nativeVBar]() {
        fluentVBar->setRange(nativeVBar->minimum(), nativeVBar->maximum());
        fluentVBar->setPageStep(nativeVBar->pageStep());
        const bool need = nativeVBar->maximum() > nativeVBar->minimum();
        fluentVBar->setVisible(need);
        if (!need) return;
        const QRect r = scrollArea->rect();
        const int x = r.right() - fluentVBar->thickness() + 1;
        fluentVBar->setGeometry(x, r.top() + 2, fluentVBar->thickness(), r.height() - 4);
        fluentVBar->raise();
    };
    QObject::connect(nativeVBar, &QScrollBar::rangeChanged, scrollArea, syncFluentBar);

    auto* content = new FluentTestWindow();
    content->setMinimumWidth(800);
    content->onThemeUpdated();
    auto* innerLayout = new AnchorLayout(content);
    content->setLayout(innerLayout);
    scrollArea->setWidget(content);

    // ── TreeView 0: Selected indicator motion sample ────────────────────
    Label* motionHeader = new Label("Selected indicator motion transitions.", content);
    motionHeader->setFluentTypography("BodyStrong");
    motionHeader->anchors()->top = {content, Edge::Top, 20};
    motionHeader->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(motionHeader);

    Button* upwardBtn = new Button("Up", content);
    upwardBtn->setFixedSize(84, 32);
    upwardBtn->anchors()->top = {motionHeader, Edge::Bottom, 8};
    upwardBtn->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(upwardBtn);

    Button* downwardBtn = new Button("Down", content);
    downwardBtn->setFixedSize(84, 32);
    downwardBtn->anchors()->top = {upwardBtn, Edge::Top, 0};
    downwardBtn->anchors()->left = {upwardBtn, Edge::Right, 8};
    innerLayout->addWidget(downwardBtn);

    Button* inwardBtn = new Button("Inward", content);
    inwardBtn->setFixedSize(84, 32);
    inwardBtn->anchors()->top = {upwardBtn, Edge::Top, 0};
    inwardBtn->anchors()->left = {downwardBtn, Edge::Right, 8};
    innerLayout->addWidget(inwardBtn);

    Button* outwardBtn = new Button("Outward", content);
    outwardBtn->setFixedSize(92, 32);
    outwardBtn->anchors()->top = {upwardBtn, Edge::Top, 0};
    outwardBtn->anchors()->left = {inwardBtn, Edge::Right, 8};
    innerLayout->addWidget(outwardBtn);

    TreeView* motionTree = new TreeView(content);
    motionTree->setHeaderText("Nested sample for selected indicator direction and depth.");
    motionTree->setBorderVisible(true);
    motionTree->setIndicatorMotionAnimationEnabled(true);
    QModelIndex motionWork;
    QModelIndex motionPictures;
    QModelIndex motionSpec;
    QModelIndex motionSchedule;
    {
        auto* model = createSampleTreeModel(motionTree);
        motionTree->setModel(model);
        attachFluentDelegate(motionTree);
        motionTree->expand(model->index(0, 0));
        motionTree->expand(model->index(1, 0));
        motionTree->expand(model->item(1)->child(0)->index());
        motionWork = model->index(0, 0);
        motionPictures = model->index(2, 0);
        motionSpec = model->item(0)->child(0)->index();
        motionSchedule = model->item(0)->child(1)->index();
        motionTree->setSelectedItem(motionWork);
    }
    motionTree->setFixedHeight(280);
    motionTree->anchors()->top = {upwardBtn, Edge::Bottom, 8};
    motionTree->anchors()->left = {content, Edge::Left, 20};
    motionTree->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(motionTree);

    auto triggerMotion = [motionTree](const QModelIndex& from, const QModelIndex& to) {
        motionTree->setSelectedItem(from);
        QApplication::processEvents();
        motionTree->setSelectedItem(to);
    };
    QObject::connect(upwardBtn, &Button::clicked, [triggerMotion, motionPictures, motionWork]() {
        triggerMotion(motionPictures, motionWork);
    });
    QObject::connect(downwardBtn, &Button::clicked, [triggerMotion, motionWork, motionPictures]() {
        triggerMotion(motionWork, motionPictures);
    });
    QObject::connect(inwardBtn, &Button::clicked, [triggerMotion, motionWork, motionSpec]() {
        triggerMotion(motionWork, motionSpec);
    });
    QObject::connect(outwardBtn, &Button::clicked, [triggerMotion, motionSchedule, motionWork]() {
        triggerMotion(motionSchedule, motionWork);
    });

    // ── TreeView 1: A simple TreeView with drag and drop support ─────────
    TreeView* tv1 = new TreeView(content);
    tv1->setHeaderText("A simple TreeView with drag and drop support.");
    tv1->setBorderVisible(true);
    tv1->setCanReorderItems(true);
    {
        auto* model = createSampleTreeModel(tv1);
        tv1->setModel(model);
        attachFluentDelegate(tv1);
        tv1->expand(model->index(0, 0));  // Work Documents
        tv1->expand(model->index(1, 0));  // Personal Documents
        tv1->expand(model->item(1)->child(0)->index()); // Home Remodel
        tv1->setSelectedItem(model->item(0)->child(0)->index()); // XYZ Functional Spec
    }
    tv1->setFixedHeight(300);
    tv1->anchors()->top   = {motionTree, Edge::Bottom,  16};
    tv1->anchors()->left  = {content, Edge::Left, 20};
    tv1->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(tv1);

    // ── TreeView 2: Multi-selection with CheckBox ────────────────────────
    Label* header2 = new Label("A TreeView with Multi-selection enabled.", content);
    header2->setFluentTypography("BodyStrong");
    header2->anchors()->top  = {tv1, Edge::Bottom, 16};
    header2->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header2);

    TreeView* tv2 = new TreeView(content);
    tv2->setBorderVisible(true);
    tv2->setSelectionMode(TreeSelectionMode::Multiple);
    {
        auto* model = createCheckableTreeModel(tv2);
        tv2->setModel(model);
        auto* d = new treeview_test::FluentTreeItemDelegate(
            static_cast<fluent::FluentElement*>(tv2), defaultTreeRowHeight(), tv2, tv2);
        d->setCheckBoxVisible(true);
        tv2->setItemDelegate(d);
        tv2->expand(model->index(0, 0));  // Work Documents
        tv2->expand(model->index(1, 0));  // Personal Documents
        tv2->expand(model->item(1)->child(0)->index()); // Home Remodel
        selectCheckedRows(tv2);
    }
    tv2->setFixedHeight(300);
    tv2->anchors()->top   = {header2, Edge::Bottom, 8};
    tv2->anchors()->left  = {content, Edge::Left, 20};
    tv2->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(tv2);

    // ── TreeView 3: DataBinding using ItemSource ─────────────────────────
    Label* header3 = new Label("A TreeView with DataBinding Using ItemSource.", content);
    header3->setFluentTypography("BodyStrong");
    header3->anchors()->top  = {tv2, Edge::Bottom, 16};
    header3->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header3);

    TreeView* tv3 = new TreeView(content);
    tv3->setBorderVisible(true);
    tv3->setCanReorderItems(true);
    {
        auto* model = new QStandardItemModel(tv3);

        auto* desktop = new QStandardItem("Desktop");
        desktop->appendRow(new QStandardItem("folder1"));
        desktop->appendRow(new QStandardItem("folder2"));
        desktop->appendRow(new QStandardItem("folder3"));
        model->appendRow(desktop);

        auto* documents = new QStandardItem("Documents");
        auto* myDocs = new QStandardItem("My Documents");
        myDocs->appendRow(new QStandardItem("Binder1"));
        myDocs->appendRow(new QStandardItem("Binder2"));
        documents->appendRow(myDocs);
        model->appendRow(documents);

        auto* downloads = new QStandardItem("Downloads");
        model->appendRow(downloads);

        auto* pictures = new QStandardItem("Pictures");
        auto* camera = new QStandardItem("Camera Roll");
        camera->appendRow(new QStandardItem("IMG_001.jpg"));
        camera->appendRow(new QStandardItem("IMG_002.jpg"));
        pictures->appendRow(camera);
        pictures->appendRow(new QStandardItem("Wallpapers"));
        model->appendRow(pictures);

        tv3->setModel(model);
        attachFluentDelegate(tv3);
        tv3->expand(model->index(1, 0));  // Documents
    }
    tv3->setFixedHeight(280);
    tv3->anchors()->top   = {header3, Edge::Bottom, 8};
    tv3->anchors()->left  = {content, Edge::Left, 20};
    tv3->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(tv3);

    // ── TreeView 4: ItemTemplateSelector (folder/document icons) ─────────
    Label* header4 = new Label("A TreeView with ItemTemplateSelector.", content);
    header4->setFluentTypography("BodyStrong");
    header4->anchors()->top  = {tv3, Edge::Bottom, 16};
    header4->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header4);

    TreeView* tv4 = new TreeView(content);
    tv4->setBorderVisible(true);
    {
        auto* model = createIconTreeModel(tv4);
        tv4->setModel(model);
        attachFluentDelegate(tv4);
        tv4->expand(model->index(0, 0));  // Work Documents
        tv4->expand(model->index(1, 0));  // Personal Documents
        tv4->expand(model->item(1)->child(0)->index()); // Home Remodel
    }
    tv4->setFixedHeight(300);
    tv4->anchors()->top   = {header4, Edge::Bottom, 8};
    tv4->anchors()->left  = {content, Edge::Left, 20};
    tv4->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(tv4);

    // --- Switch Theme 按钮 ---
    Button* themeBtn = new Button("Switch Theme", content);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->top  = {tv4, Edge::Bottom, 24};
    themeBtn->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(themeBtn);

    // content 最小高度
    content->setMinimumHeight(1660);

    QObject::connect(themeBtn, &Button::clicked, [scrollArea, content]() {
        fluent::FluentElement::setTheme(
            fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
        content->onThemeUpdated();
        scrollArea->setStyleSheet(content->styleSheet());
    });

    content->onThemeUpdated();
    scrollArea->setStyleSheet(content->styleSheet());
    window->show();
    syncFluentBar();
    qApp->exec();
}
