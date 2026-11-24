#include "compatibility/QtCompat.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QHeaderView>
#include <QMetaType>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <gtest/gtest.h>

#include "FluentTreeItemDelegate.h"
#include "components/collections/TreeView.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Spacing.h"
#include "design/Typography.h"

#include "components/scrolling/ScrollBar.h"

using namespace fluent::collections;
using namespace fluent;

namespace {

class PaintRegionTreeView : public TreeView {
public:
    using TreeView::TreeView;

    void resetAnimatedPaintObservations()
    {
        observedAnimatedPaint = false;
        observedFullViewportAnimatedPaint = false;
    }

    bool observedAnimatedPaint = false;
    bool observedFullViewportAnimatedPaint = false;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        const qreal progress = indicatorMotionProgress();
        if (progress > 0.0 && progress < 1.0) {
            observedAnimatedPaint = true;
            observedFullViewportAnimatedPaint = observedFullViewportAnimatedPaint
                || event->region().contains(viewport()->rect());
        }
        TreeView::paintEvent(event);
    }
};

int defaultTreeRowHeight()
{
    return Spacing::ControlHeight::Standard + Spacing::Gap::Tight;
}

/** 业务组装：为 TreeView 挂上 Fluent 行代理 */
void attachFluentDelegate(TreeView* tv, int rowHeight = defaultTreeRowHeight())
{
    tv->setItemDelegate(
        new treeview_test::FluentTreeItemDelegate(static_cast<fluent::FluentElement*>(tv), rowHeight, tv, tv));
}

/** 挂上带 CheckBox 可见的代理 */
treeview_test::FluentTreeItemDelegate* attachCheckableDelegate(TreeView* tv)
{
    auto* d = new treeview_test::FluentTreeItemDelegate(static_cast<fluent::FluentElement*>(tv), defaultTreeRowHeight(),
        tv, tv);
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
QStandardItemModel* createSampleTreeModel(QObject* parent = nullptr)
{
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
QStandardItemModel* attachSampleModel(TreeView* tv)
{
    auto* model = createSampleTreeModel(tv);
    tv->setModel(model);
    attachFluentDelegate(tv);
    return model;
}

/** 离屏显示: 触发布局计算但不弹出可见窗口 */
void showOffscreen(QWidget* w)
{
    w->setAttribute(Qt::WA_DontShowOnScreen, true);
    w->show();
    QApplication::processEvents();
}

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

bool isAccentLike(const QColor& color, const QColor& accent)
{
    constexpr int kTolerance = 42;
    return color.alpha() > 160 && qAbs(color.red() - accent.red()) <= kTolerance
        && qAbs(color.green() - accent.green()) <= kTolerance && qAbs(color.blue() - accent.blue()) <= kTolerance;
}

QImage renderWidget(QWidget* widget)
{
    QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget->render(&painter);
    painter.end();
    return image;
}

int leftMostAccentPixelX(QWidget* widget, const QRect& rowRect, const QColor& accent)
{
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

bool hasAccentPixelInRect(QWidget* widget, const QRect& rect, const QColor& accent)
{
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

bool hasAccentPixelInImage(const QImage& image, const QRect& rect, const QColor& accent)
{
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
QStandardItemModel* createCheckableTreeModel(QObject* parent = nullptr)
{
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

void selectCheckedRows(TreeView* tv, const QModelIndex& parent = QModelIndex())
{
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

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

} // namespace

class TreeViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::collections::TreeView::IndicatorVerticalDirection>(
            "fluent::collections::TreeView::IndicatorVerticalDirection");
        qRegisterMetaType<fluent::collections::TreeView::IndicatorHierarchyTransition>(
            "fluent::collections::TreeView::IndicatorHierarchyTransition");
    }

    void SetUp() override
    {
        window = new FluentTestWindow();
        window->setFixedSize(500, 500);
        window->setWindowTitle("Fluent TreeView Test");
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    FluentTestWindow* window;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Model & Data
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, EmptyTreeView)
{
    TreeView* tv = new TreeView(window);
    EXPECT_EQ(tv->model(), nullptr);
    EXPECT_FALSE(tv->selectedItem().isValid());
}

TEST_F(TreeViewTest, Contract_FluentSelectionKeepsKeyboardFocusWithoutNativeOuterRing)
{
    TreeView tree(window);
    tree.setGeometry(10, 10, 280, 160);
    tree.setBorderVisible(false);
    tree.setSelectionIndicatorVisible(true);
    attachFluentDelegate(&tree);
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("First")));
    model.appendRow(new QStandardItem(QStringLiteral("Second")));
    tree.setModel(&model);
    window->show();
    window->activateWindow();
    for (auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        tree.setProperty("fluentThemeOverride", static_cast<int>(theme));
        tree.setCurrentIndex(model.index(0, 0));
        tree.setFocus(Qt::TabFocusReason);
        ASSERT_TRUE(QTest::qWaitFor([&] { return tree.hasFocus(); }, 1000));
        EXPECT_FALSE(tree.testAttribute(Qt::WA_MacShowFocusRect));
        EXPECT_TRUE(tree.selectionIndicatorVisible());
        QTest::keyClick(&tree, Qt::Key_Down);
        EXPECT_EQ(tree.currentIndex(), model.index(1, 0));
        EXPECT_EQ(tree.selectedItem(), model.index(1, 0));
        EXPECT_TRUE(tree.hasFocus());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Selection
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, SelectionModeSwitch)
{
    struct Mapping {
        SelectionMode mode;
        QAbstractItemView::SelectionMode qtMode;
    };
    const Mapping modes[] = {
        {SelectionMode::Multiple, QAbstractItemView::MultiSelection},
        {SelectionMode::None, QAbstractItemView::NoSelection},
        {SelectionMode::Extended, QAbstractItemView::ExtendedSelection},
        {SelectionMode::Single, QAbstractItemView::SingleSelection},
    };
    TreeView view(window);
    attachSampleModel(&view);
    EXPECT_EQ(view.selectionMode(), SelectionMode::Single);
    QSignalSpy spy(&view, &TreeView::selectionModeChanged);
    view.setSelectionMode(SelectionMode::Single);
    EXPECT_EQ(spy.count(), 0);

    int expectedSignals = 0;
    for (const auto& mode : modes) {
        SCOPED_TRACE(static_cast<int>(mode.mode));
        ++expectedSignals;
        view.setSelectionMode(mode.mode);
        EXPECT_EQ(view.selectionMode(), mode.mode);
        EXPECT_EQ(static_cast<QAbstractItemView*>(&view)->selectionMode(), mode.qtMode);
        EXPECT_EQ(spy.count(), expectedSignals);
        view.setSelectionMode(mode.mode);
        EXPECT_EQ(spy.count(), expectedSignals);
    }
}

TEST_F(TreeViewTest, SetSelectedItem)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->setSelectedItem(model->index(0, 0));
    EXPECT_EQ(tv->selectedItem(), model->index(0, 0));
}

TEST_F(TreeViewTest, ClearSelectionOnInvalidIndex)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    tv->setSelectedItem(model->index(0, 0));
    EXPECT_TRUE(tv->selectedItem().isValid());

    tv->setSelectedItem(QModelIndex());
    EXPECT_FALSE(tv->selectedItem().isValid());
}

TEST_F(TreeViewTest, IndicatorMotionDefaultsAndAnimationToggle)
{
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

TEST_F(TreeViewTest, IndicatorMotionClassifiesInitialUpwardDownwardAndSameLevel)
{
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

TEST_F(TreeViewTest, IndicatorMotionClassifiesHierarchyTransitions)
{
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

TEST_F(TreeViewTest, IndicatorMotionClearsOnInvalidResetSelectionModelAndCollapse)
{
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

TEST_F(TreeViewTest, IndicatorReturnsWhenCurrentRowIsSelectedAgain)
{
    auto* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setSelectionIndicatorVisible(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex leaf = model->item(1)->child(0)->child(0)->index();
    tv->setSelectedItem(leaf);
    processEvents();
    ASSERT_FALSE(tv->selectedIndicatorRect().isEmpty());

    tv->clearSelection();
    EXPECT_EQ(tv->currentIndex(), leaf);
    EXPECT_TRUE(tv->selectedIndicatorRect().isEmpty());

    tv->selectionModel()->select(leaf, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    processEvents();
    EXPECT_EQ(tv->indicatorMotionCurrentIndex(), leaf);
    ASSERT_FALSE(tv->selectedIndicatorRect().isEmpty());
    EXPECT_TRUE(hasAccentPixelInRect(tv->viewport(), tv->selectedIndicatorRect().toAlignedRect(),
        tv->themeColors().accentDefault));
}

TEST_F(TreeViewTest, IndicatorMotionSuppressesDuplicateSelectionAndSnapsWhenAnimationDisabled)
{
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

TEST_F(TreeViewTest, IndicatorMotionProgressAnimatesWhenEnabled)
{
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

TEST_F(TreeViewTest, IndicatorMotionRepaintsWholeTransparentViewportDuringAnimation)
{
    auto* tv = new PaintRegionTreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setBackgroundVisible(false);
    tv->setBorderVisible(false);
    tv->setSelectionIndicatorVisible(true);
    tv->setFixedSize(350, 400);
    tv->expandAll();
    showOffscreen(window);

    const QModelIndex parent = model->index(0, 0);
    const QModelIndex child = model->item(0)->child(0)->index();
    tv->setSelectedItem(parent);
    processEvents();

    tv->resetAnimatedPaintObservations();
    tv->setSelectedItem(child);
    for (int i = 0; i < 30 && !tv->observedFullViewportAnimatedPaint; ++i) {
        QTest::qWait(10);
        processEvents();
    }

    EXPECT_TRUE(tv->observedAnimatedPaint);
    EXPECT_TRUE(tv->observedFullViewportAnimatedPaint);
}

TEST_F(TreeViewTest, SelectionIndicatorVisibilityAndStyleSetters)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    EXPECT_FALSE(tv->selectionIndicatorVisible());
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorInset(), 6.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorHeight(), 16.0);
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

    tv->setSelectionIndicatorHeight(14.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorHeight(), 14.0);
    EXPECT_EQ(styleSpy.count(), 3);
    tv->setSelectionIndicatorHeight(14.0);
    EXPECT_EQ(styleSpy.count(), 3);
    tv->setSelectionIndicatorHeight(-1.0);
    EXPECT_DOUBLE_EQ(tv->selectionIndicatorHeight(), 0.0);
    EXPECT_EQ(styleSpy.count(), 4);
    tv->setSelectionIndicatorHeight(14.0);

    tv->setFixedSize(350, 400);
    showOffscreen(window);
    const QModelIndex work = model->index(0, 0);
    model->item(0)->setData(42.0, kIndicatorInsetRole);
    tv->setSelectedItem(work);
    processEvents();
    EXPECT_NEAR(tv->selectedIndicatorRect(1.0).left(), tv->visualRect(work).left() + 42.0, 0.01);
}

TEST_F(TreeViewTest, NativeRowPanelIsSuppressedForDelegateOwnedBackgrounds)
{
    TreeView tv;
    QImage image(120, defaultTreeRowHeight(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QStyleOptionViewItem option;
    option.initFrom(&tv);
    option.rect = image.rect();
    option.state |= QStyle::State_Enabled | QStyle::State_Selected | QStyle::State_MouseOver;
    option.palette.setColor(QPalette::Highlight, QColor(255, 0, 0, 255));

    QPainter painter(&image);
    tv.style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, &painter, &tv);
    painter.end();

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x)
            EXPECT_EQ(QColor::fromRgba(image.pixel(x, y)).alpha(), 0);
    }
}

TEST_F(TreeViewTest, SelectionIndicatorPaintsAccentPillNearLeftOfSelectedRow)
{
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

TEST_F(TreeViewTest, AnimatedCollapseDefersModelCollapseUntilFinished)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    // Start from a settled expansion; reversing a just-started reveal can finish
    // its collapse in one tick and does not exercise a full collapse transition.
    // zh_CN: 从稳定展开态开始，避免刚开始的展开动画被反转后在一帧内结束。
    tv->expandAll();
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));
    QSignalSpy collapsed(tv, &QTreeView::collapsed);

    // Collapse is animated while on screen: the row stays expanded mid-flight.
    tv->toggleExpanded(work);
    EXPECT_TRUE(tv->isExpanded(work));
    EXPECT_EQ(collapsed.count(), 0);

    // After the reveal finishes the real collapse lands.
    ASSERT_TRUE(QTest::qWaitFor([&] { return !tv->isExpanded(work); }, 1000));
    ASSERT_EQ(collapsed.count(), 1);
    EXPECT_EQ(collapsed.first().first().value<QModelIndex>(), work);
    EXPECT_FALSE(tv->isExpanded(work));
}

TEST_F(TreeViewTest, AnimatedCollapseReversesBackToExpandedOnRetoggle)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    tv->toggleExpanded(work);
    for (int i = 0; i < 60 && !tv->isExpanded(work); ++i)
        QTest::qWait(10);
    QTest::qWait(300); // settle the expand reveal
    processEvents();
    ASSERT_TRUE(tv->isExpanded(work));

    // Begin an animated collapse, then immediately re-toggle to reverse it.
    tv->toggleExpanded(work); // start deferred collapse
    QTest::qWait(40);         // let it run partway
    tv->toggleExpanded(work); // reverse → expand again
    QTest::qWait(320);
    processEvents();
    EXPECT_TRUE(tv->isExpanded(work)); // never actually collapsed
}

TEST_F(TreeViewTest, ExpandRevealCommitsWhenAnotherParentToggles)
{
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

TEST_F(TreeViewTest, CollapseRevealFinalizesWhenAnotherParentToggles)
{
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

TEST_F(TreeViewTest, AnimatedCollapseHitTestingFollowsVisualRows)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    const QModelIndex firstChild = model->item(0)->child(0)->index();
    const QModelIndex secondChild = model->item(0)->child(1)->index();
    const QModelIndex personal = model->index(1, 0);

    tv->toggleExpanded(work);
    ASSERT_TRUE(QTest::qWaitFor([&] { return tv->chevronRotation(work) == 1.0; }, 1000));
    ASSERT_TRUE(tv->isExpanded(work));

    const qreal subtreeHeight = tv->visualRect(firstChild).height() + tv->visualRect(secondChild).height();
    const QRect personalRect = tv->visualRect(personal);
    ASSERT_FALSE(personalRect.isEmpty());

    tv->toggleExpanded(work);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const qreal progress = tv->chevronRotation(work);
            return progress > 0.0 && progress < 1.0;
        },
        1000));

    const qreal progress = tv->chevronRotation(work);
    ASSERT_GT(progress, 0.0);
    ASSERT_LT(progress, 1.0);

    const QPoint visualPersonalCenter = personalRect.center() - QPoint(0, qRound(subtreeHeight * (1.0 - progress)));
    EXPECT_EQ(tv->indexAt(visualPersonalCenter), personal);

    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, visualPersonalCenter);
    processEvents();
    EXPECT_EQ(tv->selectedItem(), personal);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expand / Collapse
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ToggleExpanded)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    QModelIndex workIdx = model->index(0, 0);

    EXPECT_FALSE(tv->isExpanded(workIdx));
    tv->toggleExpanded(workIdx);
    EXPECT_TRUE(tv->isExpanded(workIdx));
    tv->toggleExpanded(QModelIndex());
    EXPECT_TRUE(tv->isExpanded(workIdx));
    tv->toggleExpanded(workIdx);
    EXPECT_FALSE(tv->isExpanded(workIdx));
}

TEST_F(TreeViewTest, ExpandAndCollapseAllLevels)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);

    showOffscreen(window);
    tv->expandAll();

    EXPECT_TRUE(tv->isExpanded(model->index(0, 0)));
    EXPECT_TRUE(tv->isExpanded(model->index(1, 0)));
    // Personal Documents → Home Remodel
    EXPECT_TRUE(tv->isExpanded(model->item(1)->child(0)->index()));

    tv->collapseAll();
    EXPECT_FALSE(tv->isExpanded(model->index(0, 0)));
    EXPECT_FALSE(tv->isExpanded(model->index(1, 0)));
    EXPECT_FALSE(tv->isExpanded(model->item(1)->child(0)->index()));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Expand Animation
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ExpandAnimationExposesChildrenDuringReveal)
{
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

TEST_F(TreeViewTest, RapidToggleNoCrash)
{
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

TEST_F(TreeViewTest, CollapseStopsExpandAnimation)
{
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

TEST_F(TreeViewTest, SetBorderVisible)
{
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->borderVisible());
    EXPECT_TRUE(tv->isBorderVisible());
    QSignalSpy spy(tv, &TreeView::borderVisibleChanged);

    tv->setBorderVisible(false);
    EXPECT_FALSE(tv->borderVisible());
    EXPECT_FALSE(tv->isBorderVisible());
    EXPECT_EQ(spy.count(), 1);

    tv->setBorderVisible(false); // duplicate
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, SetBackgroundVisible)
{
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->backgroundVisible());
    EXPECT_TRUE(tv->isBackgroundVisible());
    QSignalSpy spy(tv, &TreeView::backgroundVisibleChanged);

    tv->setBackgroundVisible(false);
    EXPECT_FALSE(tv->backgroundVisible());
    EXPECT_FALSE(tv->isBackgroundVisible());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, HeaderText)
{
    TreeView* tv = new TreeView(window);
    QSignalSpy spy(tv, &TreeView::headerTextChanged);

    EXPECT_TRUE(tv->headerText().isEmpty());

    tv->setHeaderText("Documents");
    EXPECT_EQ(tv->headerText(), "Documents");
    EXPECT_EQ(spy.count(), 1);

    tv->setHeaderText("Documents"); // duplicate
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, PlaceholderTextAppearsAndClears)
{
    TreeView view;
    view.resize(300, 180);
    showOffscreen(&view);
    const QRect center = view.viewport()->rect().adjusted(15, 25, -15, -25);
    const QImage blank = view.viewport()->grab(center).toImage();
    ASSERT_FALSE(blank.isNull());
    QSignalSpy changes(&view, &TreeView::placeholderTextChanged);
    view.setPlaceholderText("Empty tree");
    EXPECT_EQ(view.placeholderText(), "Empty tree");
    EXPECT_EQ(changes.count(), 1);
    EXPECT_NE(view.viewport()->grab(center).toImage(), blank);
    view.setPlaceholderText({});
    EXPECT_EQ(view.viewport()->grab(center).toImage(), blank);
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(TreeViewTest, FontRole)
{
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

TEST_F(TreeViewTest, IndentationDefault)
{
    TreeView* tv = new TreeView(window);
    EXPECT_EQ(tv->indentation(), 16);
}

TEST_F(TreeViewTest, RootIsDecoratedOff)
{
    TreeView* tv = new TreeView(window);
    EXPECT_FALSE(tv->rootIsDecorated());
}

TEST_F(TreeViewTest, HeaderHiddenByDefault)
{
    TreeView* tv = new TreeView(window);
    EXPECT_TRUE(tv->isHeaderHidden());
}

TEST_F(TreeViewTest, Contract_ColumnHeaderRemainsAboveRowsAfterLayoutChanges)
{
    TreeView tree;
    QStandardItemModel model;
    model.setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size")});
    model.appendRow({new QStandardItem(QStringLiteral("First")), new QStandardItem(QStringLiteral("10 KB"))});
    model.appendRow({new QStandardItem(QStringLiteral("Second")), new QStandardItem(QStringLiteral("20 KB"))});
    tree.setModel(&model);
    tree.setHeaderHidden(false);
    tree.resize(420, 240);
    showOffscreen(&tree);
    QSignalSpy headerClick(tree.header(), &QHeaderView::sectionClicked);
    tree.header()->setSectionsClickable(true);

    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        tree.setLayoutDirection(direction);
        for (const auto& caption : {QString(), QStringLiteral("Files")}) {
            tree.setHeaderText(caption);
            for (const QSize size : {QSize(420, 240), QSize(530, 310)}) {
                tree.resize(size);
                processEvents();
                const QRect headerRect = tree.header()->geometry();
                const QRect viewRect = tree.viewport()->geometry();
                EXPECT_FALSE(tree.isHeaderHidden());
                EXPECT_GE(headerRect.top(), 0);
                EXPECT_GT(headerRect.height(), 0);
                EXPECT_EQ(headerRect.bottom() + 1, viewRect.top());
                EXPECT_EQ(headerRect.left(), viewRect.left());
                EXPECT_EQ(headerRect.width(), viewRect.width());
                if (!caption.isEmpty()) {
                    auto* title = tree.findChild<QLabel*>(QStringLiteral("fluentTreeViewHeader"));
                    ASSERT_NE(title, nullptr);
                    EXPECT_EQ(title->geometry().bottom() + 1, headerRect.top());
                }
                QTest::mouseClick(tree.viewport(), Qt::LeftButton, Qt::NoModifier,
                    tree.visualRect(model.index(0, 0)).center());
                EXPECT_EQ(tree.currentIndex(), model.index(0, 0));
                const int x = tree.header()->sectionViewportPosition(1) + tree.header()->sectionSize(1) / 2;
                QTest::mouseClick(tree.header()->viewport(), Qt::LeftButton, Qt::NoModifier,
                    QPoint(x, tree.header()->height() / 2));
                ASSERT_FALSE(headerClick.isEmpty());
                EXPECT_EQ(headerClick.takeLast().at(0).toInt(), 1);
            }
        }
    }
    tree.setHeaderText(QString());
    tree.setHeaderHidden(true);
    processEvents();
    EXPECT_EQ(tree.viewport()->y(), 2);
    tree.setHeaderHidden(false);
    processEvents();
    EXPECT_EQ(tree.header()->y(), 0);
    EXPECT_EQ(tree.viewport()->y(), tree.header()->height());
}

TEST_F(TreeViewTest, Contract_HeaderSpacePreservesScrollRangeAndLastRow)
{
    TreeView tree;
    QStandardItemModel model;
    model.setHorizontalHeaderLabels({QStringLiteral("Name")});
    for (int row = 0; row < 40; ++row)
        model.appendRow(new QStandardItem(QStringLiteral("Row %1").arg(row)));
    tree.setModel(&model);
    tree.resize(400, 240);
    showOffscreen(&tree);
    for (bool hidden : {true, false}) {
        tree.setHeaderHidden(hidden);
        for (const auto& caption : {QString(), QStringLiteral("Files")}) {
            SCOPED_TRACE(::testing::Message() << "hidden=" << hidden << " caption=" << caption.toStdString());
            tree.setHeaderText(caption);
            processEvents();
            tree.scrollToBottom();
            processEvents();
            EXPECT_EQ(tree.verticalScrollBar()->pageStep(), tree.viewport()->height());
            const QRect last = tree.visualRect(model.index(39, 0));
            EXPECT_GE(last.top(), 0);
            EXPECT_LE(last.bottom(), tree.viewport()->rect().bottom());
            if (!hidden) {
                auto* scrollBar = tree.verticalFluentScrollBar();
                ASSERT_NE(scrollBar, nullptr);
                EXPECT_FALSE(scrollBar->geometry().intersects(tree.header()->geometry()));
            }
        }
    }
}

TEST_F(TreeViewTest, Contract_ExternalHeaderCanMeetFirstRowWithoutChangingRowHeight)
{
    TreeView tree;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("First")));
    tree.setModel(&model);
    tree.resize(400, 240);
    showOffscreen(&tree);
    const auto first = model.index(0, 0);
    const int height = tree.visualRect(first).height();
    ASSERT_GT(height, 0);
    ASSERT_EQ(tree.headerlessTopInset(), 2);
    QSignalSpy insetChanged(&tree, &TreeView::headerlessTopInsetChanged);
    tree.setHeaderlessTopInset(0);
    tree.setHeaderlessTopInset(-1);
    EXPECT_EQ(insetChanged.count(), 1);
    EXPECT_EQ(tree.viewport()->mapTo(&tree, tree.visualRect(first).topLeft()).y(), 0);
    EXPECT_EQ(tree.visualRect(first).height(), height);
    tree.setHeaderHidden(false);
    tree.setHeaderlessTopInset(10);
    processEvents();
    EXPECT_EQ(tree.header()->y(), 0);
    EXPECT_EQ(tree.viewport()->y(), tree.header()->height());
    tree.setHeaderHidden(true);
    processEvents();
    EXPECT_EQ(tree.viewport()->y(), 10);
    EXPECT_EQ(tree.visualRect(first).height(), height);
}

TEST_F(TreeViewTest, Contract_ReplacedColumnHeaderKeepsCaptionAndHorizontalAlignment)
{
    TreeView tree;
    QStandardItemModel model;
    model.setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Size")});
    model.appendRow({new QStandardItem(QStringLiteral("Report")), new QStandardItem(QStringLiteral("12 KB"))});
    tree.setModel(&model);
    tree.setHeaderText(QStringLiteral("Files"));
    tree.setHeaderHidden(false);
    tree.resize(400, 240);
    showOffscreen(&tree);
    auto* replacement = new QHeaderView(Qt::Horizontal, &tree);
    tree.setHeader(replacement);
    tree.setHeaderHidden(false);
    replacement->setFixedHeight(45);
    tree.setColumnWidth(0, 600);
    tree.setColumnWidth(1, 200);
    tree.setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    processEvents();
    tree.horizontalScrollBar()->setValue(tree.horizontalScrollBar()->maximum());
    processEvents();
    auto* title = tree.findChild<QLabel*>(QStringLiteral("fluentTreeViewHeader"));
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(tree.header(), replacement);
    EXPECT_EQ(replacement->height(), 45);
    EXPECT_EQ(replacement->y(), title->geometry().bottom() + 1);
    EXPECT_EQ(replacement->geometry().bottom() + 1, tree.viewport()->y());
    EXPECT_EQ(tree.visualRect(model.index(0, 1)).x(), replacement->sectionViewportPosition(1));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Fluent Scroll Bar
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, FluentScrollBarExists)
{
    TreeView* tv = new TreeView(window);
    EXPECT_NE(tv->verticalFluentScrollBar(), nullptr);
    EXPECT_NE(tv->horizontalFluentScrollBar(), nullptr);
}

TEST_F(TreeViewTest, FluentScrollBarHiddenWhenShort)
{
    TreeView* tv = new TreeView(window);
    tv->setFixedSize(300, 400);
    attachSampleModel(tv); // only 3 top-level items, should fit

    showOffscreen(window);

    // 3 collapsed items at 36px each = 108px < 400px → no scroll needed
    EXPECT_FALSE(tv->verticalFluentScrollBar()->isVisible());
}

TEST_F(TreeViewTest, FluentScrollBarVisibleWhenTall)
{
    TreeView* tv = new TreeView(window);
    tv->setFixedSize(300, 60); // very small → will need scrolling

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

TEST_F(TreeViewTest, ItemClickedSignalEmitted)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);

    showOffscreen(window);

    QSignalSpy inheritedClickSpy(tv, &QAbstractItemView::clicked);
    QSignalSpy itemClickSpy(tv, &TreeView::itemClicked);

    QModelIndex idx = model->index(0, 0);
    QRect rect = tv->visualRect(idx);
    QTest::mouseClick(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

    ASSERT_EQ(inheritedClickSpy.count(), 1);
    EXPECT_EQ(inheritedClickSpy.at(0).at(0).value<QModelIndex>(), idx);
    ASSERT_EQ(itemClickSpy.count(), 1);
    QModelIndex clickedIdx = itemClickSpy.at(0).at(0).value<QModelIndex>();
    EXPECT_EQ(clickedIdx, idx);
}

TEST_F(TreeViewTest, ItemPressedSignalEmittedOnMousePress)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    tv->setFixedSize(300, 400);

    showOffscreen(window);

    QSignalSpy inheritedPressSpy(tv, &QAbstractItemView::pressed);
    QSignalSpy itemPressSpy(tv, &TreeView::itemPressed);

    const QModelIndex idx = model->index(0, 0);
    const QRect rect = tv->visualRect(idx);
    QTest::mousePress(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QApplication::processEvents();

    ASSERT_EQ(inheritedPressSpy.count(), 1);
    EXPECT_EQ(inheritedPressSpy.at(0).at(0).value<QModelIndex>(), idx);
    ASSERT_EQ(itemPressSpy.count(), 1);
    const QModelIndex pressedIdx = itemPressSpy.at(0).at(0).value<QModelIndex>();
    EXPECT_EQ(pressedIdx, idx);
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Chevron-only expand & checkbox click
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, ClickRowBodyDoesNotExpand)
{
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

TEST_F(TreeViewTest, ClickChevronAreaExpands)
{
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

TEST_F(TreeViewTest, CheckBoxClickTogglesState)
{
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

TEST_F(TreeViewTest, ParentCheckCascadesToChildren)
{
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

TEST_F(TreeViewTest, ChildCheckUpdatesParentTriState)
{
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

TEST_F(TreeViewTest, DeepCascadeGrandchildren)
{
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

TEST_F(TreeViewTest, ChevronRotationApi)
{
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

TEST_F(TreeViewTest, SetCanReorderItems)
{
    TreeView* tv = new TreeView(window);
    EXPECT_FALSE(tv->canReorderItems());
    QSignalSpy spy(tv, &TreeView::canReorderItemsChanged);
    tv->setCanReorderItems(true);
    EXPECT_TRUE(tv->canReorderItems());
    EXPECT_EQ(spy.count(), 1);

    tv->setCanReorderItems(true);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(TreeViewTest, ScrollChainingPropertyControlsBoundaryWheel)
{
    auto* tv = new TreeView(window);
    tv->setGeometry(0, 0, 260, 140);
    auto* model = new QStandardItemModel(tv);
    for (int i = 0; i < 40; ++i)
        model->appendRow(new QStandardItem(QStringLiteral("Item %1").arg(i)));
    tv->setModel(model);
    attachFluentDelegate(tv);
    showOffscreen(window);
    tv->doItemsLayout();
    QTest::qWait(20);

    ASSERT_GT(tv->verticalScrollBar()->maximum(), 0);
    tv->verticalScrollBar()->setValue(tv->verticalScrollBar()->minimum());
    EXPECT_FALSE(tv->isScrollChainingEnabled());

    QSignalSpy spy(tv, &TreeView::scrollChainingEnabledChanged);
    tv->setScrollChainingEnabled(true);
    EXPECT_TRUE(tv->isScrollChainingEnabled());
    EXPECT_EQ(spy.count(), 1);
    tv->setScrollChainingEnabled(true);
    EXPECT_EQ(spy.count(), 1);

    const QPoint wheelPoint = tv->viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(chainedWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(tv->viewport(), &chainedWheel);
    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(tv->verticalScrollBar()->value(), tv->verticalScrollBar()->minimum());

    tv->setScrollChainingEnabled(false);
    FLUENT_MAKE_WHEEL_EVENT(containedWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    containedWheel.setAccepted(false);
    QApplication::sendEvent(tv->viewport(), &containedWheel);
    EXPECT_TRUE(containedWheel.isAccepted());
}

TEST_F(TreeViewTest, WheelPassesThroughWhenContentFits)
{
    auto* tv = new TreeView(window);
    tv->setGeometry(0, 0, 300, 220);
    auto* model = new QStandardItemModel(tv);
    model->appendRow(new QStandardItem(QStringLiteral("A")));
    model->appendRow(new QStandardItem(QStringLiteral("B")));
    tv->setModel(model);
    attachFluentDelegate(tv);
    showOffscreen(window);
    tv->doItemsLayout();
    QTest::qWait(20);

    ASSERT_EQ(tv->verticalScrollBar()->maximum(), tv->verticalScrollBar()->minimum());

    const QPoint wheelPoint = tv->viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    wheel.setAccepted(false);
    QApplication::sendEvent(tv->viewport(), &wheel);
    EXPECT_FALSE(wheel.isAccepted());
}

TEST_F(TreeViewTest, DragReorderTopLevelSiblings)
{
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

TEST_F(TreeViewTest, DragReorderChildSiblings)
{
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

TEST_F(TreeViewTest, DragReorderEmitsSignal)
{
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
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();

    // Move to target
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
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

TEST_F(TreeViewTest, DragDisabledDoesNotReorder)
{
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
    QMouseEvent moveEvent(QEvent::MouseMove, QPointF(moved), QPointF(moved), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // Order unchanged
    EXPECT_EQ(model->item(0)->text(), "Work Documents");
    EXPECT_EQ(model->item(1)->text(), "Personal Documents");
    EXPECT_EQ(model->item(2)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragPreservesChildHierarchy)
{
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

TEST_F(TreeViewTest, DragDropOntoFolder)
{
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
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // "Pictures" should now be a child of "Work Documents"
    EXPECT_EQ(model->rowCount(), 2); // only 2 root items left
    EXPECT_EQ(work->rowCount(), workChildrenBefore + 1);
    EXPECT_EQ(work->child(work->rowCount() - 1)->text(), "Pictures");
}

TEST_F(TreeViewTest, DragCrossParentBetween)
{
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
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent2);
    QApplication::processEvents();
    QTest::mouseRelease(tv->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QApplication::processEvents();

    // "XYZ Functional Spec" removed from Work Documents, inserted at root level
    EXPECT_EQ(work->rowCount(), 1);  // only 1 child left
    EXPECT_EQ(model->rowCount(), 4); // 4 root items now
    EXPECT_EQ(model->item(1)->text(), "XYZ Functional Spec");
}

TEST_F(TreeViewTest, DragCannotDropAncestorOntoDescendant)
{
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
    QMouseEvent moveEvent1(QEvent::MouseMove, QPointF(moved), QPointF(moved), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tv->viewport(), &moveEvent1);
    QApplication::processEvents();
    QMouseEvent moveEvent2(QEvent::MouseMove, QPointF(end), QPointF(end), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
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

TEST_F(TreeViewTest, ThemeSwitchDoesNotCrash)
{
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

TEST_F(TreeViewTest, VisibleRowsFollowModelInsertionAndRemoval)
{
    QStandardItemModel model;
    TreeView view;
    view.setModel(&model);
    view.resize(300, 200);
    showOffscreen(&view);

    auto* parent = new QStandardItem("Parent");
    model.appendRow(parent);
    auto* child = new QStandardItem("Child");
    parent->appendRow(child);
    view.expandAll();
    const QRect row = view.visualRect(child->index());
    ASSERT_FALSE(row.isEmpty());
    EXPECT_EQ(view.indexAt(row.center()), child->index());
    view.setSelectedItem(child->index());
    ASSERT_EQ(view.selectedItem(), child->index());

    model.removeRow(0);
    EXPECT_FALSE(view.indexAt(row.center()).isValid());
    EXPECT_FALSE(view.selectedItem().isValid());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Delegate
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, DelegateConsumesIndicatorMotionWithoutChangingRowMetrics)
{
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

TEST_F(TreeViewTest, DelegateSkipsAccentBarWhenTreeViewOverlayIndicatorVisible)
{
    TreeView* tv = new TreeView(window);
    auto* model = attachSampleModel(tv);
    auto* delegate = qobject_cast<treeview_test::FluentTreeItemDelegate*>(tv->itemDelegate());
    ASSERT_NE(delegate, nullptr);
    tv->setIndicatorMotionAnimationEnabled(false);
    tv->setFixedSize(350, 400);
    showOffscreen(window);

    const QModelIndex work = model->index(0, 0);
    tv->setSelectedItem(work);
    processEvents();

    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 240, defaultTreeRowHeight());
    option.state = QStyle::State_Selected | QStyle::State_Enabled;
    option.widget = tv;
    option.font = tv->font();
    const QColor accent = tv->themeColors().accentDefault;
    const QRect indicatorProbe(0, 0, 16, option.rect.height());

    const auto renderDelegateRow = [&]() {
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        delegate->paint(&painter, option, work);
        painter.end();
        return image;
    };

    tv->setSelectionIndicatorVisible(false);
    EXPECT_TRUE(hasAccentPixelInImage(renderDelegateRow(), indicatorProbe, accent));

    tv->setSelectionIndicatorVisible(true);
    EXPECT_FALSE(hasAccentPixelInImage(renderDelegateRow(), indicatorProbe, accent));
}

TEST_F(TreeViewTest, DelegateSelectedIndicatorFollowsHierarchicalRowLocalPosition)
{
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

TEST_F(TreeViewTest, SelectedIndicatorAnchorsToTargetDuringHierarchyTransitions)
{
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

TEST_F(TreeViewTest, CollapsingParentAfterChildToParentSelectionKeepsParentIndicator)
{
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
// CheckBox delegate
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(TreeViewTest, CheckStateTracksMultiSelectionChanges)
{
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(SelectionMode::Multiple);
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

TEST_F(TreeViewTest, CheckBoxClickSyncsMultiSelection)
{
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(SelectionMode::Multiple);
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

TEST_F(TreeViewTest, CheckBoxParentAndChildClicksKeepTriStateSelection)
{
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(SelectionMode::Multiple);
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

TEST_F(TreeViewTest, CheckableSelectionDoesNotPaintIndicator)
{
    TreeView* tv = new TreeView(window);
    auto* model = createCheckableTreeModel(tv);
    tv->setModel(model);
    tv->setSelectionMode(SelectionMode::Multiple);
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

TEST_F(TreeViewTest, OffscreenHeaderAndDisabledStateWorkWithoutShow)
{
    TreeView tv(window);
    auto* model = createSampleTreeModel(&tv);
    tv.setModel(model);
    tv.setHeaderText(QStringLiteral("Hierarchy"));
    tv.setFixedSize(300, 220);

    EXPECT_FALSE(tv.isVisible());
    ASSERT_NE(tv.viewport(), nullptr);
    EXPECT_GT(tv.viewport()->geometry().top(), 0);
    EXPECT_EQ(tv.palette().color(QPalette::Disabled, QPalette::Text), tv.themeColors().textDisabled);

    auto* hdr = tv.findChild<QLabel*>();
    ASSERT_NE(hdr, nullptr);
    QFont customFont = tv.font();
    customFont.setPixelSize(29);
    tv.setFont(customFont);
    const QString enabledHeaderStyle = hdr->styleSheet();
    tv.setEnabled(false);
    EXPECT_NE(hdr->styleSheet(), enabledHeaderStyle);
    EXPECT_EQ(tv.font(), customFont);
    tv.setEnabled(true);
    EXPECT_EQ(tv.font(), customFont);
    EXPECT_EQ(hdr->styleSheet(), enabledHeaderStyle);
}

TEST_F(TreeViewTest, DefaultDelegateShowsSelectionAndSupportsPointerExpansion)
{
    for (auto theme : {FluentElement::Light, FluentElement::Dark}) {
        FluentElement::setTheme(theme);
        TreeView view;
        view.resize(520, 270);
        view.setIndicatorMotionAnimationEnabled(false);
        QStandardItemModel model;
        auto* parent = new QStandardItem(QStringLiteral("Parent"));
        parent->appendRow(new QStandardItem(QStringLiteral("Child")));
        model.appendRow(parent);
        view.setModel(&model);
        view.show();
        QApplication::processEvents();
        const auto index = model.index(0, 0);
        const auto collapsed = view.grab().toImage();
        const QRect row = view.visualRect(index);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.left() + 22, row.center().y()));
        EXPECT_TRUE(view.isExpanded(index));
        EXPECT_NE(view.grab().toImage(), collapsed);
        view.selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
        const auto selected = view.grab().toImage();
        view.clearSelection();
        EXPECT_NE(view.grab().toImage(), selected);
        view.setEnabled(false);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(row.left() + 22, row.center().y()));
        EXPECT_TRUE(view.isExpanded(index));
    }
    FluentElement::setTheme(FluentElement::Light);
}
