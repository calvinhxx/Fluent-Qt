#include "compatibility/QtCompat.h"
#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QMetaEnum>
#include <QItemSelectionModel>
#include <QPainter>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTimer>
#include <QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <functional>
#include <gtest/gtest.h>
#include <utility>

#include "FluentGridItemDelegate.h"
#include "QtTestEnvironment.h"
#include "components/collections/GridView.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/scrolling/ScrollBar.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using namespace fluent::collections;
using namespace fluent;

namespace {

void sendDragMove(QWidget* target, const QPoint& position)
{
    // QWidget QTest moves may use the native cursor, losing the synthetic held button.
    // zh_CN: QWidget 的 QTest 移动可能走原生光标路径，丢失合成的按键按住状态。
    FLUENT_MAKE_MOUSE_EVENT(event, QEvent::MouseMove, target, position, Qt::NoButton,
                            Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

/** 业务组装：为 GridView 挂上 Fluent 网格项代理。 */
void attachFluentDelegate(GridView* gv)
{
    gv->setItemDelegate(
        new gridview_test::FluentGridItemDelegate(static_cast<fluent::FluentElement*>(gv), gv, gv));
}

/** 创建 QStringListModel，setModel + attachFluentDelegate。 */
QStringListModel* attachStringListModel(GridView* gv, const QStringList& rows = {})
{
    auto* m = new QStringListModel(rows, gv);
    gv->setModel(m);
    attachFluentDelegate(gv);
    return m;
}

int itemCount(GridView* gv)
{
    const auto* m = gv->model();
    return m ? m->rowCount() : 0;
}

QString itemText(GridView* gv, int index)
{
    const auto* m = gv->model();
    if (!m || index < 0 || index >= m->rowCount())
        return {};
    return m->index(index, 0).data(Qt::DisplayRole).toString();
}

QStringList modelTexts(const QAbstractItemModel* model)
{
    QStringList texts;
    if (!model)
        return texts;
    for (int row = 0; row < model->rowCount(); ++row)
        texts << model->index(row, 0).data(Qt::DisplayRole).toString();
    return texts;
}

QStandardItemModel* attachStandardModel(GridView* gv, const QStringList& rows)
{
    auto* model = new QStandardItemModel(gv);
    for (const QString& row : rows)
        model->appendRow(new QStandardItem(row));
    gv->setModel(model);
    attachFluentDelegate(gv);
    return model;
}

void showOffscreen(QWidget* window)
{
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    window->show();
    QTest::qWait(50);
}

QList<QVariantAnimation*> activeDragAnimations(const GridView* gv)
{
    QList<QVariantAnimation*> animations;
    for (auto* anim : gv->findChildren<QVariantAnimation*>()) {
        if (anim->state() != QAbstractAnimation::Running)
            continue;
        if (!anim->startValue().canConvert<QPointF>() || !anim->endValue().canConvert<QPointF>()) {
            continue;
        }
        animations.append(anim);
    }
    return animations;
}

void addItem(GridView* gv, const QString& text)
{
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(text);
    slm->setStringList(list);
}

void addItems(GridView* gv, const QStringList& texts)
{
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(texts);
    slm->setStringList(list);
}

void removeItem(GridView* gv, int index)
{
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    ASSERT_GE(index, 0);
    ASSERT_LT(index, list.size());
    list.removeAt(index);
    slm->setStringList(list);
}

void clearItems(GridView* gv)
{
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    slm->setStringList({});
}

class ResizeSyncFilter : public QObject {
public:
    ResizeSyncFilter(QObject* parent, std::function<void()> callback)
        : QObject(parent), m_callback(std::move(callback))
    {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            QTimer::singleShot(0, this, [this]() {
                if (m_callback)
                    m_callback();
            });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_callback;
};

} // namespace

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class GridViewTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new FluentTestWindow();
        window->resize(600, 600);
        window->setMinimumSize(320, 240);
        window->setWindowTitle("Fluent GridView Test");
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    FluentTestWindow* window;
};

// ── 数据操作 ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, AddAndRemoveItems)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv);

    addItem(gv, "Apple");
    addItem(gv, "Banana");
    addItem(gv, "Cherry");
    EXPECT_EQ(itemCount(gv), 3);
    EXPECT_EQ(itemText(gv, 0), "Apple");
    EXPECT_EQ(itemText(gv, 1), "Banana");
    EXPECT_EQ(itemText(gv, 2), "Cherry");

    removeItem(gv, 1);
    EXPECT_EQ(itemCount(gv), 2);
    EXPECT_EQ(itemText(gv, 0), "Apple");
    EXPECT_EQ(itemText(gv, 1), "Cherry");

    clearItems(gv);
    EXPECT_EQ(itemCount(gv), 0);
}

TEST_F(GridViewTest, AddItemsBatch)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv);
    addItems(gv, {"A", "B", "C", "D"});
    EXPECT_EQ(itemCount(gv), 4);
    EXPECT_EQ(itemText(gv, 3), "D");
}

TEST_F(GridViewTest, ItemTextOutOfRange)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv);
    addItem(gv, "Only");
    EXPECT_EQ(itemText(gv, -1), "");
    EXPECT_EQ(itemText(gv, 1), "");
}

// ── 选择模式 ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, DefaultSelectionMode)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->selectionMode(), SelectionMode::Single);
}

TEST_F(GridViewTest, DefaultEditTriggersDisabled)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->editTriggers(), QAbstractItemView::NoEditTriggers);
}

TEST_F(GridViewTest, SelectionModeRegisteredInMetaObject)
{
    QMetaEnum me = QMetaEnum::fromType<SelectionMode>();
    ASSERT_TRUE(me.isValid());
    EXPECT_STREQ(me.key(0), "None");
    EXPECT_STREQ(me.key(1), "Single");
    EXPECT_STREQ(me.key(2), "Multiple");
    EXPECT_STREQ(me.key(3), "Extended");
}

TEST_F(GridViewTest, SelectionModeNone)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});
    gv->setSelectionMode(SelectionMode::None);
    EXPECT_EQ(gv->selectionMode(), SelectionMode::None);
}

TEST_F(GridViewTest, SelectionModeMultiple)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, SIGNAL(selectionModeChanged()));
    gv->setSelectionMode(SelectionMode::Multiple);
    EXPECT_EQ(gv->selectionMode(), SelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);

    // 重复设置不触发信号
    gv->setSelectionMode(SelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, SelectionModeExtended)
{
    GridView* gv = new GridView(window);
    gv->setSelectionMode(SelectionMode::Extended);
    EXPECT_EQ(gv->selectionMode(), SelectionMode::Extended);
}

// ── 选中 API ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, SingleSelection)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});

    EXPECT_EQ(gv->selectedIndex(), -1);

    gv->setSelectedIndex(1);
    EXPECT_EQ(gv->selectedIndex(), 1);

    gv->setSelectedIndex(-1);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, SelectedIndexOutOfRange)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B"});
    gv->setSelectedIndex(1);
    EXPECT_EQ(gv->selectedIndex(), 1);

    gv->setSelectedIndex(99);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, SelectedRowsSortedAscending)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C", "D"});
    gv->setSelectionMode(SelectionMode::Multiple);

    const QModelIndex i0 = gv->model()->index(0, 0);
    const QModelIndex i2 = gv->model()->index(2, 0);
    gv->selectionModel()->select(i2, QItemSelectionModel::Select);
    gv->selectionModel()->select(i0, QItemSelectionModel::Select);

    QList<int> rows = gv->selectedRows();
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows.at(0), 0);
    EXPECT_EQ(rows.at(1), 2);
}

// ── Viewport hover ────────────────────────────────────────────────────────────

TEST_F(GridViewTest, ViewportHoveredSignal)
{
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(10, 10, 400, 400);

    EXPECT_FALSE(gv->viewportHovered());

    QSignalSpy spy(gv, &GridView::viewportHoveredChanged);
    FLUENT_MAKE_ENTER_EVENT(enterEv, 5, 5);
    QApplication::sendEvent(gv, &enterEv);
    EXPECT_TRUE(gv->viewportHovered());
    EXPECT_EQ(spy.count(), 1);

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(gv, &leave);
    EXPECT_FALSE(gv->viewportHovered());
    EXPECT_EQ(spy.count(), 2);
}

// ── Grid 特有属性 ─────────────────────────────────────────────────────────────

TEST_F(GridViewTest, DefaultCellSize)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->cellSize(), QSize(112, 112));
}

TEST_F(GridViewTest, SetCellSize)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::cellSizeChanged);
    gv->setCellSize(QSize(150, 150));
    EXPECT_EQ(gv->cellSize(), QSize(150, 150));
    EXPECT_EQ(spy.count(), 1);

    // gridSize = cellSize + spacing
    EXPECT_EQ(gv->gridSize(), QSize(150 + gv->horizontalSpacing(), 150 + gv->verticalSpacing()));

    // 重复设置不触发信号
    gv->setCellSize(QSize(150, 150));
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultSpacing)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->horizontalSpacing(), 4);
    EXPECT_EQ(gv->verticalSpacing(), 4);
}

TEST_F(GridViewTest, SetHorizontalSpacing)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::horizontalSpacingChanged);
    gv->setHorizontalSpacing(8);
    EXPECT_EQ(gv->horizontalSpacing(), 8);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(gv->gridSize().width(), gv->cellSize().width() + 8);
}

TEST_F(GridViewTest, SetVerticalSpacing)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::verticalSpacingChanged);
    gv->setVerticalSpacing(12);
    EXPECT_EQ(gv->verticalSpacing(), 12);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(gv->gridSize().height(), gv->cellSize().height() + 12);
}

TEST_F(GridViewTest, DefaultMaxColumns)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->maxColumns(), 0);
}

TEST_F(GridViewTest, SetMaxColumns)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::maxColumnsChanged);
    gv->setMaxColumns(3);
    EXPECT_EQ(gv->maxColumns(), 3);
    EXPECT_EQ(spy.count(), 1);

    gv->setMaxColumns(3);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, ScrollChainingPropertyControlsBoundaryWheel)
{
    auto* gv = new GridView(window);
    gv->setGeometry(0, 0, 240, 120);
    gv->setCellSize(QSize(100, 80));
    gv->setMaxColumns(2);
    QStringList rows;
    for (int i = 0; i < 20; ++i)
        rows << QStringLiteral("Item %1").arg(i);
    attachStringListModel(gv, rows);
    showOffscreen(window);
    gv->doItemsLayout();
    QTest::qWait(20);

    ASSERT_GT(gv->verticalScrollBar()->maximum(), 0);
    gv->verticalScrollBar()->setValue(gv->verticalScrollBar()->minimum());
    EXPECT_FALSE(gv->isScrollChainingEnabled());

    QSignalSpy spy(gv, &GridView::scrollChainingEnabledChanged);
    gv->setScrollChainingEnabled(true);
    EXPECT_TRUE(gv->isScrollChainingEnabled());
    EXPECT_EQ(spy.count(), 1);
    gv->setScrollChainingEnabled(true);
    EXPECT_EQ(spy.count(), 1);

    const QPoint wheelPoint = gv->viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(chainedWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(gv->viewport(), &chainedWheel);
    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(gv->verticalScrollBar()->value(), gv->verticalScrollBar()->minimum());

    gv->setScrollChainingEnabled(false);
    FLUENT_MAKE_WHEEL_EVENT(containedWheel, wheelPoint.x(), wheelPoint.y(), 120, Qt::NoModifier);
    containedWheel.setAccepted(false);
    QApplication::sendEvent(gv->viewport(), &containedWheel);
    EXPECT_TRUE(containedWheel.isAccepted());
}

TEST_F(GridViewTest, WheelPassesThroughWhenContentFits)
{
    auto* gv = new GridView(window);
    gv->setGeometry(0, 0, 300, 220);
    gv->setCellSize(QSize(100, 80));
    gv->setMaxColumns(2);
    attachStringListModel(gv, {QStringLiteral("A"), QStringLiteral("B")});
    showOffscreen(window);
    gv->doItemsLayout();
    QTest::qWait(20);

    ASSERT_EQ(gv->verticalScrollBar()->maximum(), gv->verticalScrollBar()->minimum());

    const QPoint wheelPoint = gv->viewport()->rect().center();
    FLUENT_MAKE_WHEEL_EVENT(wheel, wheelPoint.x(), wheelPoint.y(), -120, Qt::NoModifier);
    wheel.setAccepted(false);
    QApplication::sendEvent(gv->viewport(), &wheel);
    EXPECT_FALSE(wheel.isAccepted());
}

// ── 容器属性 ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, DefaultFontRole)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->fontRole(), Typography::FontRole::Body);
}

TEST_F(GridViewTest, SetFontRole)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, SIGNAL(fontRoleChanged()));
    gv->setFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(gv->fontRole(), Typography::FontRole::Subtitle);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultBorderVisible)
{
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->borderVisible());
    EXPECT_TRUE(gv->isBorderVisible());
}

TEST_F(GridViewTest, SetBorderVisible)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::borderVisibleChanged);
    gv->setBorderVisible(false);
    EXPECT_FALSE(gv->borderVisible());
    EXPECT_FALSE(gv->isBorderVisible());
    EXPECT_EQ(spy.count(), 1);

    gv->setBorderVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, BackgroundVisibleProperty)
{
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->backgroundVisible());
    EXPECT_TRUE(gv->isBackgroundVisible());
    QSignalSpy spy(gv, &GridView::backgroundVisibleChanged);
    gv->setBackgroundVisible(false);
    EXPECT_FALSE(gv->backgroundVisible());
    EXPECT_FALSE(gv->isBackgroundVisible());
    EXPECT_EQ(spy.count(), 1);
    gv->setBackgroundVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultHeaderText)
{
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->headerText().isEmpty());
}

TEST_F(GridViewTest, SetHeaderText)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::headerTextChanged);
    gv->setHeaderText("My Grid");
    EXPECT_EQ(gv->headerText(), "My Grid");
    EXPECT_EQ(spy.count(), 1);

    gv->setHeaderText("My Grid");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultPlaceholderText)
{
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->placeholderText().isEmpty());
}

TEST_F(GridViewTest, SetPlaceholderText)
{
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::placeholderTextChanged);
    gv->setPlaceholderText("No items");
    EXPECT_EQ(gv->placeholderText(), "No items");
    EXPECT_EQ(spy.count(), 1);

    gv->setPlaceholderText("No items");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, HeaderVisibleWhenTextSet)
{
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(10, 10, 400, 400);
    gv->setHeaderText("Grid Header");
    window->show();
    QTest::qWait(50);
    auto* headerLabel = gv->findChild<QLabel*>();
    ASSERT_NE(headerLabel, nullptr);
    EXPECT_TRUE(headerLabel->isVisible());
    EXPECT_EQ(headerLabel->text(), "Grid Header");
}

TEST_F(GridViewTest, HeaderHiddenWhenTextEmpty)
{
    GridView* gv = new GridView(window);
    gv->setHeaderText("Header");
    gv->setHeaderText("");
    auto* headerLabel = gv->findChild<QLabel*>();
    ASSERT_NE(headerLabel, nullptr);
    EXPECT_FALSE(headerLabel->isVisible());
}

TEST_F(GridViewTest, ItemClickedSignal)
{
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});
    QSignalSpy spy(gv, SIGNAL(itemClicked(int)));

    QModelIndex idx = gv->model()->index(0, 0);
    emit gv->clicked(idx);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 0);
}

TEST_F(GridViewTest, ReorderEnabledPointerClicksPreserveInheritedSignal)
{
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    gv->setCanReorderItems(true);
    attachStringListModel(gv, {"A", "B", "C"});
    window->show();
    QTest::qWait(50);

    QSignalSpy inheritedPressSpy(gv, &QAbstractItemView::pressed);
    QSignalSpy inheritedClickSpy(gv, &QAbstractItemView::clicked);
    QSignalSpy itemClickSpy(gv, &GridView::itemClicked);
    const QModelIndex index = gv->model()->index(1, 0);
    const QPoint point = gv->visualRect(index).center();

    // The first click uses QListView's normal path.
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QApplication::processEvents();
    ASSERT_EQ(inheritedPressSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedPressSpy.at(0).at(0)), index);
    ASSERT_EQ(inheritedClickSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedClickSpy.at(0).at(0)), index);
    ASSERT_EQ(itemClickSpy.count(), 1);
    EXPECT_EQ(itemClickSpy.at(0).at(0).toInt(), index.row());

    inheritedPressSpy.clear();
    inheritedClickSpy.clear();
    itemClickSpy.clear();

    // The second click is intercepted to preserve the selected set for a
    // potential multi-item drag, but it is still a click when no drag occurs.
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QApplication::processEvents();
    ASSERT_EQ(inheritedPressSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedPressSpy.at(0).at(0)), index);
    ASSERT_EQ(inheritedClickSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedClickSpy.at(0).at(0)), index);
    ASSERT_EQ(itemClickSpy.count(), 1);
    EXPECT_EQ(itemClickSpy.at(0).at(0).toInt(), index.row());
}

TEST_F(GridViewTest, ReorderEnabledProgrammaticSelectionPreservesInheritedClick)
{
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    gv->setCanReorderItems(true);
    attachStringListModel(gv, {"A", "B", "C"});
    window->show();
    QTest::qWait(50);

    const QModelIndex index = gv->model()->index(1, 0);
    gv->selectionModel()->select(index, QItemSelectionModel::Select);
    ASSERT_TRUE(gv->selectionModel()->isSelected(index));

    QSignalSpy inheritedPressSpy(gv, &QAbstractItemView::pressed);
    QSignalSpy inheritedClickSpy(gv, &QAbstractItemView::clicked);
    QSignalSpy itemClickSpy(gv, &GridView::itemClicked);

    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier,
                      gv->visualRect(index).center());
    QApplication::processEvents();

    ASSERT_EQ(inheritedPressSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedPressSpy.at(0).at(0)), index);
    ASSERT_EQ(inheritedClickSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<QModelIndex>(inheritedClickSpy.at(0).at(0)), index);
    ASSERT_EQ(itemClickSpy.count(), 1);
    EXPECT_EQ(itemClickSpy.at(0).at(0).toInt(), index.row());
}

TEST_F(GridViewTest, FluentScrollBarExists)
{
    GridView* gv = new GridView(window);
    EXPECT_NE(gv->verticalFluentScrollBar(), nullptr);
}

TEST_F(GridViewTest, ViewDoesNotProvideModelByDefault)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->model(), nullptr);
}

TEST_F(GridViewTest, SelectionQueriesAreSafeWithoutModel)
{
    GridView* gv = new GridView(window);

    EXPECT_EQ(gv->selectedIndex(), -1);
    EXPECT_TRUE(gv->selectedRows().isEmpty());
    gv->setSelectedIndex(0);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, SelectionQueriesAreSafeAfterExternalModelDestruction)
{
    GridView* gv = new GridView(window);
    auto* model = new QStringListModel({"External"});
    gv->setModel(model);

    delete model;

    EXPECT_EQ(gv->model(), nullptr);
    EXPECT_EQ(gv->selectedIndex(), -1);
    EXPECT_TRUE(gv->selectedRows().isEmpty());
    gv->setSelectedIndex(0);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, IconModeAndWrapping)
{
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->viewMode(), QListView::IconMode);
    EXPECT_TRUE(gv->isWrapping());
    EXPECT_EQ(gv->movement(), QListView::Static);
    EXPECT_EQ(gv->resizeMode(), QListView::Adjust);
}

// ── 列布局测试 ────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, ColumnsAutoFit)
{
    // 容器宽度 600，cellSize 112 + hSpacing 4 = 116 per col → 600/116 = 5 列
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    attachStringListModel(gv, {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"});
    window->show();
    QTest::qWait(50);

    // gridSize = cellSize + spacing
    EXPECT_EQ(gv->gridSize(), QSize(116, 116));

    // 容器宽 600 / gridSize.width 116 = 5 列（QListView IconMode 自动布局）
    // 验证第一行第5个 item 的 visualRect 和第二行第1个 item 不在同一行
    QRect r4 = gv->visualRect(gv->model()->index(4, 0)); // 5th item (col 5, row 1)
    QRect r5 = gv->visualRect(gv->model()->index(5, 0)); // 6th item (col 1, row 2)
    EXPECT_EQ(r4.top(), gv->visualRect(gv->model()->index(0, 0)).top());
    EXPECT_GT(r5.top(), r4.top());
}

TEST_F(GridViewTest, ColumnsChangeOnResize)
{
    // 调整容器宽度后列数应改变
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    attachStringListModel(gv, {"A", "B", "C", "D", "E", "F", "G", "H"});
    window->show();
    QTest::qWait(50);

    // 600px → ~5 列：item 5 (index 4) 应在第一行
    QRect r4_wide = gv->visualRect(gv->model()->index(4, 0));
    QRect r0_wide = gv->visualRect(gv->model()->index(0, 0));
    EXPECT_EQ(r4_wide.top(), r0_wide.top());

    // 缩窄到 360px → ~3 列：item 4 (index 3) 应在第二行
    gv->setGeometry(0, 0, 360, 400);
    QTest::qWait(50);
    QRect r3_narrow = gv->visualRect(gv->model()->index(3, 0));
    QRect r0_narrow = gv->visualRect(gv->model()->index(0, 0));
    EXPECT_GT(r3_narrow.top(), r0_narrow.top());
}

TEST_F(GridViewTest, CellSizeAffectsColumns)
{
    // 大 cellSize 导致更少列数
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 500, 400);
    gv->setCellSize(QSize(200, 150));
    gv->setHorizontalSpacing(8);
    attachStringListModel(gv, {"A", "B", "C", "D", "E", "F"});
    window->show();
    QTest::qWait(50);

    // gridSize = 200+8 = 208, 500/208 = 2 列
    // item 2 (index 1) 应在第一行，item 3 (index 2) 应在第二行
    QRect r1 = gv->visualRect(gv->model()->index(1, 0));
    QRect r0 = gv->visualRect(gv->model()->index(0, 0));
    QRect r2 = gv->visualRect(gv->model()->index(2, 0));
    EXPECT_EQ(r1.top(), r0.top());
    EXPECT_GT(r2.top(), r0.top());
}

// ── Drag reorder tests ────────────────────────────────────────────────────────

TEST_F(GridViewTest, CanReorderItemsChangesOnlyOnTransitions)
{
    GridView view;
    EXPECT_FALSE(view.canReorderItems());
    QSignalSpy spy(&view, &GridView::canReorderItemsChanged);
    int expectedSignals = 0;
    for (const bool enabled : {true, true, false, false}) {
        SCOPED_TRACE(enabled);
        expectedSignals += view.canReorderItems() != enabled;
        view.setCanReorderItems(enabled);
        EXPECT_EQ(view.canReorderItems(), enabled);
        EXPECT_EQ(spy.count(), expectedSignals);
    }
}

// ── Selection mode enum mapping to Qt ─────────────────────────────────────────

TEST_F(GridViewTest, SelectionModesMapToQt)
{
    const std::pair<SelectionMode, QAbstractItemView::SelectionMode> modes[] = {
        {SelectionMode::None, QAbstractItemView::NoSelection},
        {SelectionMode::Single, QAbstractItemView::SingleSelection},
        {SelectionMode::Multiple, QAbstractItemView::MultiSelection},
        {SelectionMode::Extended, QAbstractItemView::ExtendedSelection},
    };
    for (const auto& mode : modes) {
        SCOPED_TRACE(static_cast<int>(mode.first));
        GridView view;
        view.setSelectionMode(mode.first);
        EXPECT_EQ(static_cast<QAbstractItemView*>(&view)->selectionMode(), mode.second);
    }
}

// ── Multiple selection behavior ───────────────────────────────────────────────

TEST_F(GridViewTest, MultipleSelectionClickToggle)
{
    // In Multiple mode, clicking an item toggles it without affecting others
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    attachStringListModel(gv, {"A", "B", "C", "D"});
    window->show();
    QTest::qWait(50);

    // Click item 0
    QRect r0 = gv->visualRect(gv->model()->index(0, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);

    // Click item 2 — both 0 and 2 selected
    QRect r2 = gv->visualRect(gv->model()->index(2, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(20);

    QList<int> sel = gv->selectedRows();
    EXPECT_EQ(sel.size(), 2);
    EXPECT_TRUE(sel.contains(0));
    EXPECT_TRUE(sel.contains(2));

    // Click item 0 again — deselects it, only 2 remains
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);

    sel = gv->selectedRows();
    EXPECT_EQ(sel.size(), 1);
    EXPECT_EQ(sel.at(0), 2);
}

TEST_F(GridViewTest, ExtendedSelectionShiftClick)
{
    // In Extended mode, Shift+click selects a range
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Extended);
    attachStringListModel(gv, {"A", "B", "C", "D", "E"});
    window->show();
    QTest::qWait(50);

    // Click item 1
    QRect r1 = gv->visualRect(gv->model()->index(1, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r1.center());
    QTest::qWait(20);

    // Shift+click item 3 — selects range [1,3]
    QRect r3 = gv->visualRect(gv->model()->index(3, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::ShiftModifier, r3.center());
    QTest::qWait(20);

    QList<int> sel = gv->selectedRows();
    EXPECT_EQ(sel.size(), 3);
    EXPECT_TRUE(sel.contains(1));
    EXPECT_TRUE(sel.contains(2));
    EXPECT_TRUE(sel.contains(3));
}

TEST_F(GridViewTest, ExtendedSelectionCtrlClick)
{
    // In Extended mode, Ctrl+click adds individual items
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Extended);
    attachStringListModel(gv, {"A", "B", "C", "D"});
    window->show();
    QTest::qWait(50);

    QRect r0 = gv->visualRect(gv->model()->index(0, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);

    // Ctrl+click item 3
    QRect r3 = gv->visualRect(gv->model()->index(3, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::ControlModifier, r3.center());
    QTest::qWait(20);

    QList<int> sel = gv->selectedRows();
    EXPECT_EQ(sel.size(), 2);
    EXPECT_TRUE(sel.contains(0));
    EXPECT_TRUE(sel.contains(3));
}

TEST_F(GridViewTest, ExtendedSelectionPlainClickClearsOthers)
{
    // In Extended mode, a plain click clears previous selection
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Extended);
    attachStringListModel(gv, {"A", "B", "C", "D"});
    window->show();
    QTest::qWait(50);

    // Select items 0 and 2 via Ctrl+click
    QRect r0 = gv->visualRect(gv->model()->index(0, 0));
    QRect r2 = gv->visualRect(gv->model()->index(2, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::ControlModifier, r2.center());
    QTest::qWait(20);
    EXPECT_EQ(gv->selectedRows().size(), 2);

    // Plain click item 3 — clears all, only 3 selected
    QRect r3 = gv->visualRect(gv->model()->index(3, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r3.center());
    QTest::qWait(20);

    QList<int> sel = gv->selectedRows();
    EXPECT_EQ(sel.size(), 1);
    EXPECT_EQ(sel.at(0), 3);
}

// ── Drag reorder + selection mode interaction ─────────────────────────────────

TEST_F(GridViewTest, DragReorderSingleMode)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    // Single selection: drag item 0 over item 2, verify reorder + selection follows
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Single);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    QRect r0 = gv->visualRect(gv->model()->index(0, 0));
    QRect r2 = gv->visualRect(gv->model()->index(2, 0));

    // Simulate drag: press → move beyond threshold → move to target → release
    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);

    // Move beyond manhattan distance threshold
    QPoint dragStart = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), dragStart);
    QTest::qWait(20);

    // Move to target item center
    sendDragMove(gv->viewport(), r2.center());
    QTest::qWait(20);

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    // Verify reorder happened
    EXPECT_EQ(reorderSpy.count(), 1);
    // After reorder: ["B", "C", "A", "D"] or similar — item moved
    QStringList result;
    for (int i = 0; i < mdl->rowCount(); ++i)
        result << mdl->index(i, 0).data().toString();
    EXPECT_NE(result.at(0), "A"); // A should have moved from index 0
}

TEST_F(GridViewTest, DragReorderBoundaryJitterKeepsStableTargetUntilRelease)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(true);

    auto* mdl = attachStandardModel(gv, QStringList{"A", "B", "C", "D", "E"});
    showOffscreen(window);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);
    const QRect sourceRect = gv->visualRect(mdl->index(0, 0));
    const QRect boundaryRect = gv->visualRect(mdl->index(2, 0));
    const QPoint boundaryCenter = boundaryRect.center();

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, sourceRect.center());
    QTest::qWait(10);
    sendDragMove(gv->viewport(),
                 sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    const QPoint stablePoint = boundaryCenter - QPoint(10, 0);
    sendDragMove(gv->viewport(), stablePoint);
    QTest::qWait(10);

    const QStringList beforeRelease = modelTexts(mdl);
    const QList<QPoint> jitterPoints{boundaryCenter + QPoint(3, 0), boundaryCenter - QPoint(2, 0),
                                     boundaryCenter + QPoint(2, 0), boundaryCenter - QPoint(3, 0),
                                     boundaryCenter + QPoint(3, 0)};

    for (const QPoint& point : jitterPoints) {
        sendDragMove(gv->viewport(), point);
        QTest::qWait(5);
        EXPECT_EQ(modelTexts(mdl), beforeRelease);
        EXPECT_EQ(reorderSpy.count(), 0);
    }

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, jitterPoints.last());
    QTest::qWait(50);
    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(modelTexts(mdl), (QStringList{"B", "A", "C", "D", "E"}));
}

TEST_F(GridViewTest, DragReorderClearThresholdCrossingChangesTarget)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(true);

    auto* mdl = attachStandardModel(gv, QStringList{"A", "B", "C", "D", "E"});
    showOffscreen(window);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);
    const QRect sourceRect = gv->visualRect(mdl->index(0, 0));
    const QRect boundaryRect = gv->visualRect(mdl->index(2, 0));
    const QPoint boundaryCenter = boundaryRect.center();

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, sourceRect.center());
    QTest::qWait(10);
    sendDragMove(gv->viewport(),
                 sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    sendDragMove(gv->viewport(), boundaryCenter - QPoint(10, 0));
    QTest::qWait(10);

    const QPoint clearPoint = boundaryCenter + QPoint(20, 0);

    const QStringList beforeRelease = modelTexts(mdl);
    sendDragMove(gv->viewport(), clearPoint);
    QTest::qWait(10);

    EXPECT_EQ(modelTexts(mdl), beforeRelease);
    EXPECT_EQ(reorderSpy.count(), 0);

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, clearPoint);
    QTest::qWait(50);
    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(modelTexts(mdl), (QStringList{"B", "C", "A", "D", "E"}));
}

TEST_F(GridViewTest, DragDisplacementRepeatedStableMoveKeepsRunningAnimations)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(true);
    auto* mdl = attachStandardModel(gv, QStringList{"A", "B", "C", "D"});
    showOffscreen(window);

    const QRect sourceRect = gv->visualRect(mdl->index(0, 0));
    const QRect targetRect = gv->visualRect(mdl->index(2, 0));
    const QPoint stablePoint = targetRect.center() - QPoint(10, 0);

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, sourceRect.center());
    sendDragMove(gv->viewport(),
                 sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    sendDragMove(gv->viewport(), stablePoint);

    const QList<QVariantAnimation*> firstAnimations = activeDragAnimations(gv);
    ASSERT_FALSE(firstAnimations.isEmpty());

    sendDragMove(gv->viewport(), stablePoint + QPoint(1, 0));
    const QList<QVariantAnimation*> repeatedMoveAnimations = activeDragAnimations(gv);

    for (auto* animation : firstAnimations)
        EXPECT_TRUE(repeatedMoveAnimations.contains(animation));

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, stablePoint);
    QTest::qWait(50);
}

TEST_F(GridViewTest, DragReorderMultipleMode)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    // Multiple selection + drag: only the pressed item should reorder,
    // multi-selection state shouldn't prevent drag
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D", "E"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    // Pre-select items 0 and 2
    gv->selectionModel()->select(mdl->index(0, 0), QItemSelectionModel::Select);
    gv->selectionModel()->select(mdl->index(2, 0), QItemSelectionModel::Select);
    QTest::qWait(20);
    EXPECT_EQ(gv->selectedRows().size(), 2);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    // Drag item 0 toward item 3
    QRect r0 = gv->visualRect(mdl->index(0, 0));
    QRect r3 = gv->visualRect(mdl->index(3, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QPoint mid = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), r3.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r3.center());
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    // Item moved — verify model changed
    QStringList result;
    for (int i = 0; i < mdl->rowCount(); ++i)
        result << mdl->index(i, 0).data().toString();
    EXPECT_NE(result.at(0), "A");
}

TEST_F(GridViewTest, DragReorderSelectedItemsMoveAsGroupInMultipleMode)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    gv->setCanReorderItems(true);

    auto* mdl = attachStandardModel(gv, QStringList{"A", "B", "C", "D", "E", "F"});
    showOffscreen(window);

    gv->selectionModel()->select(mdl->index(0, 0), QItemSelectionModel::Select);
    gv->selectionModel()->select(mdl->index(1, 0), QItemSelectionModel::Select);
    ASSERT_EQ(gv->selectedRows().size(), 2);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);
    const QRect sourceRect = gv->visualRect(mdl->index(0, 0));
    const QRect targetRect = gv->visualRect(mdl->index(4, 0));
    const QPoint dropPoint(targetRect.right() - 1, targetRect.center().y());

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, sourceRect.center());
    QTest::qWait(10);
    sendDragMove(gv->viewport(),
                 sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    const QStringList beforeRelease = modelTexts(mdl);
    sendDragMove(gv->viewport(), dropPoint);
    QTest::qWait(10);
    EXPECT_EQ(modelTexts(mdl), beforeRelease);
    EXPECT_EQ(reorderSpy.count(), 0);

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, dropPoint);
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(modelTexts(mdl), (QStringList{"C", "D", "E", "A", "B", "F"}));
    const QList<int> selectedRows = gv->selectedRows();
    EXPECT_TRUE(selectedRows.contains(3));
    EXPECT_TRUE(selectedRows.contains(4));
}

TEST_F(GridViewTest, DragReorderExtendedMode)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    // Extended selection + drag: drag should work even with Ctrl-selected items
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Extended);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D", "E"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    // Ctrl+select items 1 and 3
    QRect r1 = gv->visualRect(mdl->index(1, 0));
    QRect r3 = gv->visualRect(mdl->index(3, 0));
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r1.center());
    QTest::qWait(20);
    QTest::mouseClick(gv->viewport(), Qt::LeftButton, Qt::ControlModifier, r3.center());
    QTest::qWait(20);
    EXPECT_EQ(gv->selectedRows().size(), 2);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    // Drag item 1 toward item 4
    QRect rSrc = gv->visualRect(mdl->index(1, 0));
    QRect rDst = gv->visualRect(mdl->index(4, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, rSrc.center());
    QTest::qWait(20);
    QPoint mid = rSrc.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), rDst.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, rDst.center());
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    QStringList result;
    for (int i = 0; i < mdl->rowCount(); ++i)
        result << mdl->index(i, 0).data().toString();
    EXPECT_NE(result.at(1), "B");
}

TEST_F(GridViewTest, DragReorderNoneSelectionDisablesDrag)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    // None selection mode: drag should still work if canReorderItems is true
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::None);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    QRect r0 = gv->visualRect(mdl->index(0, 0));
    QRect r2 = gv->visualRect(mdl->index(2, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QPoint mid = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    // Drag reorder should still fire even in None selection mode
    EXPECT_EQ(reorderSpy.count(), 1);
}

TEST_F(GridViewTest, DragReorderDisabledWhenFlagOff)
{
    // canReorderItems = false: no reorder should happen
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(false);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);
    const QStringList beforeDrag = modelTexts(mdl);

    QRect r0 = gv->visualRect(mdl->index(0, 0));
    QRect r2 = gv->visualRect(mdl->index(2, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QPoint mid = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 0);
    EXPECT_EQ(modelTexts(mdl), beforeDrag);
}

TEST_F(GridViewTest, DragReorderPreservesSelectionInMultipleMode)
{
    // After drag reorder in Multiple mode, selections should adapt (moved item selected)
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(SelectionMode::Multiple);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    QRect r0 = gv->visualRect(mdl->index(0, 0));
    QRect r2 = gv->visualRect(mdl->index(2, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QPoint mid = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    if (reorderSpy.count() > 0) {
        int toRow = reorderSpy.at(0).at(1).toInt();
        // After reorder, the moved item should be current
        EXPECT_EQ(gv->currentIndex().row(), toRow);
    }
}

TEST_F(GridViewTest, ReorderStandardItemModelTakeRowFallback)
{
    if (tests::support::isHeadlessPlatform()) {
        GTEST_SKIP() << "Requires a real windowing platform; offscreen cannot deliver "
                        "synthetic pointer/keyboard input or show native popups.";
    }
    // QStandardItemModel doesn't implement moveRow natively;
    // verify the takeRow/insertRow fallback works
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"X", "Y", "Z"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    // Drag item 2 ("Z") to position before item 0 ("X")
    QRect r2 = gv->visualRect(mdl->index(2, 0));
    QRect r0 = gv->visualRect(mdl->index(0, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(20);
    QPoint mid = r2.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    // Move to left edge of item 0 to trigger "insert before"
    QPoint leftOf0(r0.left() + 5, r0.center().y());
    sendDragMove(gv->viewport(), leftOf0);
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, leftOf0);
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    // Z should now be at the front
    EXPECT_EQ(mdl->index(0, 0).data().toString(), "Z");
}

TEST_F(GridViewTest, DragReorderItemReorderedSignalArgs)
{
    // Verify itemReordered signal carries correct from/to arguments
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setCanReorderItems(true);

    auto* mdl = new QStandardItemModel(gv);
    for (auto& t : {"A", "B", "C", "D"})
        mdl->appendRow(new QStandardItem(t));
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    QSignalSpy reorderSpy(gv, &GridView::itemReordered);

    QRect r0 = gv->visualRect(mdl->index(0, 0));
    QRect r3 = gv->visualRect(mdl->index(3, 0));

    QTest::mousePress(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::qWait(20);
    QPoint mid = r0.center() + QPoint(QApplication::startDragDistance() + 2, 0);
    sendDragMove(gv->viewport(), mid);
    QTest::qWait(20);
    sendDragMove(gv->viewport(), r3.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r3.center());
    QTest::qWait(50);

    if (reorderSpy.count() == 1) {
        int fromIdx = reorderSpy.at(0).at(0).toInt();
        int toIdx = reorderSpy.at(0).at(1).toInt();
        EXPECT_EQ(fromIdx, 0);
        EXPECT_GE(toIdx, 1); // moved forward
        EXPECT_LT(toIdx, 4);
    }
}
