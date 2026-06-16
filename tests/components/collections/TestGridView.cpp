#include <gtest/gtest.h>
#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTimer>
#include <QVariantAnimation>
#include <functional>
#include <utility>
#include "compatibility/QtCompat.h"
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "FluentGridItemDelegate.h"
#include "components/collections/GridView.h"
#include "components/basicinput/Button.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/Label.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using namespace fluent::collections;
using namespace fluent::textfields;
using namespace fluent;

namespace {

/** 业务组装：为 GridView 挂上 Fluent 网格项代理。 */
void attachFluentDelegate(GridView* gv) {
    gv->setItemDelegate(new gridview_test::FluentGridItemDelegate(
        static_cast<fluent::FluentElement*>(gv), gv, gv));
}

/** 创建 QStringListModel，setModel + attachFluentDelegate。 */
QStringListModel* attachStringListModel(GridView* gv, const QStringList& rows = {}) {
    auto* m = new QStringListModel(rows, gv);
    gv->setModel(m);
    attachFluentDelegate(gv);
    return m;
}

int itemCount(GridView* gv) {
    const auto* m = gv->model();
    return m ? m->rowCount() : 0;
}

QString itemText(GridView* gv, int index) {
    const auto* m = gv->model();
    if (!m || index < 0 || index >= m->rowCount()) return {};
    return m->index(index, 0).data(Qt::DisplayRole).toString();
}

QStringList modelTexts(const QAbstractItemModel* model) {
    QStringList texts;
    if (!model) return texts;
    for (int row = 0; row < model->rowCount(); ++row)
        texts << model->index(row, 0).data(Qt::DisplayRole).toString();
    return texts;
}

QStandardItemModel* attachStandardModel(GridView* gv, const QStringList& rows) {
    auto* model = new QStandardItemModel(gv);
    for (const QString& row : rows)
        model->appendRow(new QStandardItem(row));
    gv->setModel(model);
    attachFluentDelegate(gv);
    return model;
}

void showOffscreen(QWidget* window) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    window->show();
    QTest::qWait(50);
}

QList<QVariantAnimation*> activeDragAnimations(const GridView* gv) {
    QList<QVariantAnimation*> animations;
    for (auto* anim : gv->findChildren<QVariantAnimation*>()) {
        if (anim->state() != QAbstractAnimation::Running)
            continue;
        if (!anim->startValue().canConvert<QPointF>() ||
            !anim->endValue().canConvert<QPointF>()) {
            continue;
        }
        animations.append(anim);
    }
    return animations;
}

void addItem(GridView* gv, const QString& text) {
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(text);
    slm->setStringList(list);
}

void addItems(GridView* gv, const QStringList& texts) {
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(texts);
    slm->setStringList(list);
}

void removeItem(GridView* gv, int index) {
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    ASSERT_GE(index, 0);
    ASSERT_LT(index, list.size());
    list.removeAt(index);
    slm->setStringList(list);
}

void clearItems(GridView* gv) {
    auto* slm = qobject_cast<QStringListModel*>(gv->model());
    ASSERT_NE(slm, nullptr);
    slm->setStringList({});
}

class ResizeSyncFilter : public QObject {
public:
    ResizeSyncFilter(QObject* parent, std::function<void()> callback)
        : QObject(parent)
        , m_callback(std::move(callback))
    {
    }

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
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class GridViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->resize(600, 600);
        window->setMinimumSize(320, 240);
        window->setWindowTitle("Fluent GridView Test");
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

// ── 数据操作 ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, AddAndRemoveItems) {
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

TEST_F(GridViewTest, AddItemsBatch) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv);
    addItems(gv, {"A", "B", "C", "D"});
    EXPECT_EQ(itemCount(gv), 4);
    EXPECT_EQ(itemText(gv, 3), "D");
}

TEST_F(GridViewTest, ItemTextOutOfRange) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv);
    addItem(gv, "Only");
    EXPECT_EQ(itemText(gv, -1), "");
    EXPECT_EQ(itemText(gv, 1), "");
}

// ── 选择模式 ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, DefaultSelectionMode) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->selectionMode(), GridSelectionMode::Single);
}

TEST_F(GridViewTest, DefaultEditTriggersDisabled) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->editTriggers(), QAbstractItemView::NoEditTriggers);
}

TEST_F(GridViewTest, GridSelectionModeRegisteredInMetaObject) {
    QMetaEnum me = QMetaEnum::fromType<GridSelectionMode>();
    ASSERT_TRUE(me.isValid());
    EXPECT_STREQ(me.key(0), "None");
    EXPECT_STREQ(me.key(1), "Single");
    EXPECT_STREQ(me.key(2), "Multiple");
    EXPECT_STREQ(me.key(3), "Extended");
}

TEST_F(GridViewTest, SelectionModeNone) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});
    gv->setSelectionMode(GridSelectionMode::None);
    EXPECT_EQ(gv->selectionMode(), GridSelectionMode::None);
}

TEST_F(GridViewTest, SelectionModeMultiple) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, SIGNAL(selectionModeChanged()));
    gv->setSelectionMode(GridSelectionMode::Multiple);
    EXPECT_EQ(gv->selectionMode(), GridSelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);

    // 重复设置不触发信号
    gv->setSelectionMode(GridSelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, SelectionModeExtended) {
    GridView* gv = new GridView(window);
    gv->setSelectionMode(GridSelectionMode::Extended);
    EXPECT_EQ(gv->selectionMode(), GridSelectionMode::Extended);
}

// ── 选中 API ──────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, SingleSelection) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});

    EXPECT_EQ(gv->selectedIndex(), -1);

    gv->setSelectedIndex(1);
    EXPECT_EQ(gv->selectedIndex(), 1);

    gv->setSelectedIndex(-1);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, SelectedIndexOutOfRange) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B"});
    gv->setSelectedIndex(1);
    EXPECT_EQ(gv->selectedIndex(), 1);

    gv->setSelectedIndex(99);
    EXPECT_EQ(gv->selectedIndex(), -1);
}

TEST_F(GridViewTest, SelectedRowsSortedAscending) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C", "D"});
    gv->setSelectionMode(GridSelectionMode::Multiple);

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

TEST_F(GridViewTest, ViewportHoveredSignal) {
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

TEST_F(GridViewTest, DefaultCellSize) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->cellSize(), QSize(112, 112));
}

TEST_F(GridViewTest, SetCellSize) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::cellSizeChanged);
    gv->setCellSize(QSize(150, 150));
    EXPECT_EQ(gv->cellSize(), QSize(150, 150));
    EXPECT_EQ(spy.count(), 1);

    // gridSize = cellSize + spacing
    EXPECT_EQ(gv->gridSize(), QSize(150 + gv->horizontalSpacing(),
                                    150 + gv->verticalSpacing()));

    // 重复设置不触发信号
    gv->setCellSize(QSize(150, 150));
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultSpacing) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->horizontalSpacing(), 4);
    EXPECT_EQ(gv->verticalSpacing(), 4);
}

TEST_F(GridViewTest, SetHorizontalSpacing) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::horizontalSpacingChanged);
    gv->setHorizontalSpacing(8);
    EXPECT_EQ(gv->horizontalSpacing(), 8);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(gv->gridSize().width(), gv->cellSize().width() + 8);
}

TEST_F(GridViewTest, SetVerticalSpacing) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::verticalSpacingChanged);
    gv->setVerticalSpacing(12);
    EXPECT_EQ(gv->verticalSpacing(), 12);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(gv->gridSize().height(), gv->cellSize().height() + 12);
}

TEST_F(GridViewTest, DefaultMaxColumns) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->maxColumns(), 0);
}

TEST_F(GridViewTest, SetMaxColumns) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::maxColumnsChanged);
    gv->setMaxColumns(3);
    EXPECT_EQ(gv->maxColumns(), 3);
    EXPECT_EQ(spy.count(), 1);

    gv->setMaxColumns(3);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, ScrollChainingPropertyControlsBoundaryWheel) {
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

TEST_F(GridViewTest, WheelPassesThroughWhenContentFits) {
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

TEST_F(GridViewTest, DefaultFontRole) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->fontRole(), Typography::FontRole::Body);
}

TEST_F(GridViewTest, SetFontRole) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, SIGNAL(fontRoleChanged()));
    gv->setFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(gv->fontRole(), Typography::FontRole::Subtitle);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultBorderVisible) {
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->borderVisible());
    EXPECT_TRUE(gv->isBorderVisible());
}

TEST_F(GridViewTest, SetBorderVisible) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::borderVisibleChanged);
    gv->setBorderVisible(false);
    EXPECT_FALSE(gv->borderVisible());
    EXPECT_FALSE(gv->isBorderVisible());
    EXPECT_EQ(spy.count(), 1);

    gv->setBorderVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultHeaderText) {
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->headerText().isEmpty());
}

TEST_F(GridViewTest, SetHeaderText) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::headerTextChanged);
    gv->setHeaderText("My Grid");
    EXPECT_EQ(gv->headerText(), "My Grid");
    EXPECT_EQ(spy.count(), 1);

    gv->setHeaderText("My Grid");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DefaultPlaceholderText) {
    GridView* gv = new GridView(window);
    EXPECT_TRUE(gv->placeholderText().isEmpty());
}

TEST_F(GridViewTest, SetPlaceholderText) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::placeholderTextChanged);
    gv->setPlaceholderText("No items");
    EXPECT_EQ(gv->placeholderText(), "No items");
    EXPECT_EQ(spy.count(), 1);

    gv->setPlaceholderText("No items");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, HeaderVisibleWhenTextSet) {
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

TEST_F(GridViewTest, HeaderHiddenWhenTextEmpty) {
    GridView* gv = new GridView(window);
    gv->setHeaderText("Header");
    gv->setHeaderText("");
    auto* headerLabel = gv->findChild<QLabel*>();
    ASSERT_NE(headerLabel, nullptr);
    EXPECT_FALSE(headerLabel->isVisible());
}

TEST_F(GridViewTest, ItemClickedSignal) {
    GridView* gv = new GridView(window);
    attachStringListModel(gv, {"A", "B", "C"});
    QSignalSpy spy(gv, SIGNAL(itemClicked(int)));

    QModelIndex idx = gv->model()->index(0, 0);
    emit gv->clicked(idx);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 0);
}

TEST_F(GridViewTest, FluentScrollBarExists) {
    GridView* gv = new GridView(window);
    EXPECT_NE(gv->verticalFluentScrollBar(), nullptr);
}

TEST_F(GridViewTest, ViewDoesNotProvideModelByDefault) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->model(), nullptr);
}

TEST_F(GridViewTest, IconModeAndWrapping) {
    GridView* gv = new GridView(window);
    EXPECT_EQ(gv->viewMode(), QListView::IconMode);
    EXPECT_TRUE(gv->isWrapping());
    EXPECT_EQ(gv->movement(), QListView::Static);
    EXPECT_EQ(gv->resizeMode(), QListView::Adjust);
}

// ── 列布局测试 ────────────────────────────────────────────────────────────────

TEST_F(GridViewTest, ColumnsAutoFit) {
    // 容器宽度 600，cellSize 112 + hSpacing 4 = 116 per col → 600/116 = 5 列
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    attachStringListModel(gv, {"A","B","C","D","E","F","G","H","I","J"});
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

TEST_F(GridViewTest, ColumnsChangeOnResize) {
    // 调整容器宽度后列数应改变
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    attachStringListModel(gv, {"A","B","C","D","E","F","G","H"});
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

TEST_F(GridViewTest, CellSizeAffectsColumns) {
    // 大 cellSize 导致更少列数
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 500, 400);
    gv->setCellSize(QSize(200, 150));
    gv->setHorizontalSpacing(8);
    attachStringListModel(gv, {"A","B","C","D","E","F"});
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

// ── 可视化测试 ────────────────────────────────────────────────────────────────

namespace {

constexpr int kMinimumShimmerVisibleMs = 1200;

/** 异步加载网络图片到 QStandardItem 的 ImageRole */
void loadNetworkImage(QStandardItem* item, const QUrl& url) {
    item->setData(true, gridview_test::ImageLoadingRole);
    QTimer::singleShot(kMinimumShimmerVisibleMs, qApp, [item, url]() {
        auto* nam = new QNetworkAccessManager(qApp);
        auto* reply = nam->get(QNetworkRequest(url));
        QObject::connect(reply, &QNetworkReply::finished, qApp, [item, reply, nam]() {
            QPixmap pix;
            if (reply->error() == QNetworkReply::NoError)
                pix.loadFromData(reply->readAll());
            reply->deleteLater();
            nam->deleteLater();

            if (!pix.isNull())
                item->setData(pix, gridview_test::ImageRole);
            item->setData(false, gridview_test::ImageLoadingRole);
        });
    });
}

} // namespace

// ── Drag reorder tests ────────────────────────────────────────────────────────

TEST_F(GridViewTest, DefaultCanReorderItems) {
    GridView* gv = new GridView(window);
    EXPECT_FALSE(gv->canReorderItems());
}

TEST_F(GridViewTest, SetCanReorderItems) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::canReorderItemsChanged);
    gv->setCanReorderItems(true);
    EXPECT_TRUE(gv->canReorderItems());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, DisableCanReorderItems) {
    GridView* gv = new GridView(window);
    gv->setCanReorderItems(true);
    gv->setCanReorderItems(false);
    EXPECT_FALSE(gv->canReorderItems());
}

TEST_F(GridViewTest, CanReorderItemsSignalNotDuplicate) {
    GridView* gv = new GridView(window);
    QSignalSpy spy(gv, &GridView::canReorderItemsChanged);
    gv->setCanReorderItems(true);
    gv->setCanReorderItems(true); // same
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(GridViewTest, ReorderMoveRowInModel) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(10, 10, 400, 300);
    gv->setCanReorderItems(true);

    auto* mdl = new QStringListModel(QStringList{"A", "B", "C", "D"}, gv);
    gv->setModel(mdl);
    attachFluentDelegate(gv);
    window->show();
    QTest::qWait(50);

    // Simulate model move: move row 0 to row 2 (A -> after C)
    bool moved = mdl->moveRow(QModelIndex(), 0, QModelIndex(), 3);
    EXPECT_TRUE(moved);
    EXPECT_EQ(mdl->stringList(), (QStringList{"B", "C", "A", "D"}));
}

// ── Selection mode enum mapping to Qt ─────────────────────────────────────────

TEST_F(GridViewTest, SelectionModeNoneMapsToNoSelection) {
    GridView* gv = new GridView(window);
    gv->setSelectionMode(GridSelectionMode::None);
    EXPECT_EQ(static_cast<QAbstractItemView*>(gv)->selectionMode(),
              QAbstractItemView::NoSelection);
}

TEST_F(GridViewTest, SelectionModeSingleMapsToSingleSelection) {
    GridView* gv = new GridView(window);
    gv->setSelectionMode(GridSelectionMode::Single);
    EXPECT_EQ(static_cast<QAbstractItemView*>(gv)->selectionMode(),
              QAbstractItemView::SingleSelection);
}

TEST_F(GridViewTest, SelectionModeMultipleMapsToMultiSelection) {
    GridView* gv = new GridView(window);
    gv->setSelectionMode(GridSelectionMode::Multiple);
    EXPECT_EQ(static_cast<QAbstractItemView*>(gv)->selectionMode(),
              QAbstractItemView::MultiSelection);
}

TEST_F(GridViewTest, SelectionModeExtendedMapsToExtendedSelection) {
    GridView* gv = new GridView(window);
    gv->setSelectionMode(GridSelectionMode::Extended);
    EXPECT_EQ(static_cast<QAbstractItemView*>(gv)->selectionMode(),
              QAbstractItemView::ExtendedSelection);
}

// ── Multiple selection behavior ───────────────────────────────────────────────

TEST_F(GridViewTest, MultipleSelectionClickToggle) {
    // In Multiple mode, clicking an item toggles it without affecting others
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Multiple);
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

TEST_F(GridViewTest, ExtendedSelectionShiftClick) {
    // In Extended mode, Shift+click selects a range
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Extended);
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

TEST_F(GridViewTest, ExtendedSelectionCtrlClick) {
    // In Extended mode, Ctrl+click adds individual items
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Extended);
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

TEST_F(GridViewTest, ExtendedSelectionPlainClickClearsOthers) {
    // In Extended mode, a plain click clears previous selection
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Extended);
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

TEST_F(GridViewTest, DragReorderSingleMode) {
    // Single selection: drag item 0 over item 2, verify reorder + selection follows
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Single);
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
    QTest::mouseMove(gv->viewport(), dragStart);
    QTest::qWait(20);

    // Move to target item center
    QTest::mouseMove(gv->viewport(), r2.center());
    QTest::qWait(20);

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    // Verify reorder happened
    EXPECT_EQ(reorderSpy.count(), 1);
    // After reorder: ["B", "C", "A", "D"] or similar — item moved
    QStringList result;
    for (int i = 0; i < mdl->rowCount(); ++i)
        result << mdl->index(i, 0).data().toString();
    EXPECT_NE(result.at(0), "A");  // A should have moved from index 0
}

TEST_F(GridViewTest, DragReorderBoundaryJitterKeepsStableTargetUntilRelease) {
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
    QTest::mouseMove(gv->viewport(), sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    const QPoint stablePoint = boundaryCenter - QPoint(10, 0);
    QTest::mouseMove(gv->viewport(), stablePoint);
    QTest::qWait(10);

    const QStringList beforeRelease = modelTexts(mdl);
    const QList<QPoint> jitterPoints{
        boundaryCenter + QPoint(3, 0),
        boundaryCenter - QPoint(2, 0),
        boundaryCenter + QPoint(2, 0),
        boundaryCenter - QPoint(3, 0),
        boundaryCenter + QPoint(3, 0)
    };

    for (const QPoint& point : jitterPoints) {
        QTest::mouseMove(gv->viewport(), point);
        QTest::qWait(5);
        EXPECT_EQ(modelTexts(mdl), beforeRelease);
        EXPECT_EQ(reorderSpy.count(), 0);
    }

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, jitterPoints.last());
    QTest::qWait(50);
    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(modelTexts(mdl), (QStringList{"B", "A", "C", "D", "E"}));
}

TEST_F(GridViewTest, DragReorderClearThresholdCrossingChangesTarget) {
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
    QTest::mouseMove(gv->viewport(), sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    QTest::mouseMove(gv->viewport(), boundaryCenter - QPoint(10, 0));
    QTest::qWait(10);

    const QPoint clearPoint = boundaryCenter + QPoint(20, 0);

    const QStringList beforeRelease = modelTexts(mdl);
    QTest::mouseMove(gv->viewport(), clearPoint);
    QTest::qWait(10);

    EXPECT_EQ(modelTexts(mdl), beforeRelease);
    EXPECT_EQ(reorderSpy.count(), 0);

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, clearPoint);
    QTest::qWait(50);
    EXPECT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(modelTexts(mdl), (QStringList{"B", "C", "A", "D", "E"}));
}

TEST_F(GridViewTest, DragDisplacementRepeatedStableMoveKeepsRunningAnimations) {
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
    QTest::mouseMove(gv->viewport(), sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::mouseMove(gv->viewport(), stablePoint);

    const QList<QVariantAnimation*> firstAnimations = activeDragAnimations(gv);
    ASSERT_FALSE(firstAnimations.isEmpty());

    QTest::mouseMove(gv->viewport(), stablePoint + QPoint(1, 0));
    const QList<QVariantAnimation*> repeatedMoveAnimations = activeDragAnimations(gv);

    for (auto* animation : firstAnimations)
        EXPECT_TRUE(repeatedMoveAnimations.contains(animation));

    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, stablePoint);
    QTest::qWait(50);
}

TEST_F(GridViewTest, DragReorderMultipleMode) {
    // Multiple selection + drag: only the pressed item should reorder,
    // multi-selection state shouldn't prevent drag
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Multiple);
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), r3.center());
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

TEST_F(GridViewTest, DragReorderSelectedItemsMoveAsGroupInMultipleMode) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Multiple);
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
    QTest::mouseMove(gv->viewport(), sourceRect.center() + QPoint(QApplication::startDragDistance() + 2, 0));
    QTest::qWait(10);

    const QStringList beforeRelease = modelTexts(mdl);
    QTest::mouseMove(gv->viewport(), dropPoint);
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

TEST_F(GridViewTest, DragReorderExtendedMode) {
    // Extended selection + drag: drag should work even with Ctrl-selected items
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Extended);
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), rDst.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, rDst.center());
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    QStringList result;
    for (int i = 0; i < mdl->rowCount(); ++i)
        result << mdl->index(i, 0).data().toString();
    EXPECT_NE(result.at(1), "B");
}

TEST_F(GridViewTest, DragReorderNoneSelectionDisablesDrag) {
    // None selection mode: drag should still work if canReorderItems is true
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::None);
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    // Drag reorder should still fire even in None selection mode
    EXPECT_EQ(reorderSpy.count(), 1);
}

TEST_F(GridViewTest, DragReorderDisabledWhenFlagOff) {
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 0);
    EXPECT_EQ(modelTexts(mdl), beforeDrag);
}

TEST_F(GridViewTest, DragReorderPreservesSelectionInMultipleMode) {
    // After drag reorder in Multiple mode, selections should adapt (moved item selected)
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    GridView* gv = new GridView(window);
    gv->setGeometry(0, 0, 600, 400);
    gv->setSelectionMode(GridSelectionMode::Multiple);
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), r2.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    QTest::qWait(50);

    if (reorderSpy.count() > 0) {
        int toRow = reorderSpy.at(0).at(1).toInt();
        // After reorder, the moved item should be current
        EXPECT_EQ(gv->currentIndex().row(), toRow);
    }
}

TEST_F(GridViewTest, ReorderStandardItemModelTakeRowFallback) {
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    // Move to left edge of item 0 to trigger "insert before"
    QPoint leftOf0(r0.left() + 5, r0.center().y());
    QTest::mouseMove(gv->viewport(), leftOf0);
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, leftOf0);
    QTest::qWait(50);

    EXPECT_EQ(reorderSpy.count(), 1);
    // Z should now be at the front
    EXPECT_EQ(mdl->index(0, 0).data().toString(), "Z");
}

TEST_F(GridViewTest, DragReorderItemReorderedSignalArgs) {
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
    QTest::mouseMove(gv->viewport(), mid);
    QTest::qWait(20);
    QTest::mouseMove(gv->viewport(), r3.center());
    QTest::qWait(20);
    QTest::mouseRelease(gv->viewport(), Qt::LeftButton, Qt::NoModifier, r3.center());
    QTest::qWait(50);

    if (reorderSpy.count() == 1) {
        int fromIdx = reorderSpy.at(0).at(0).toInt();
        int toIdx   = reorderSpy.at(0).at(1).toInt();
        EXPECT_EQ(fromIdx, 0);
        EXPECT_GE(toIdx, 1);     // moved forward
        EXPECT_LT(toIdx, 4);
    }
}

TEST_F(GridViewTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(960, 720);
    window->setMinimumSize(420, 360);
    using Edge = AnchorLayout::Edge;

    // --- ScrollArea 容器 ---
    auto* scrollArea = new QScrollArea(window);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    {
        AnchorLayout::Anchors scrollAnchors;
        scrollAnchors.fill = true;
        layout->addAnchoredWidget(scrollArea, scrollAnchors);
    }

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
    auto* resizeSync = new ResizeSyncFilter(scrollArea, syncFluentBar);
    scrollArea->installEventFilter(resizeSync);
    scrollArea->viewport()->installEventFilter(resizeSync);

    auto* content = new FluentTestWindow();
    content->setMinimumWidth(360);
    content->onThemeUpdated();
    auto* innerLayout = new AnchorLayout(content);
    content->setLayout(innerLayout);
    scrollArea->setWidget(content);

    // ── Loading sample: keep a stable shimmer state visible for review ──
    GridView* gvLoading = new GridView(content);
    gvLoading->setHeaderText("Loading image placeholders");
    gvLoading->setBorderVisible(true);
    gvLoading->setCellSize(QSize(140, 96));
    gvLoading->setHorizontalSpacing(6);
    gvLoading->setVerticalSpacing(6);
    auto* loadingModel = new QStandardItemModel(gvLoading);
    for (int i = 0; i < 5; ++i) {
        auto* item = new QStandardItem(QString("Loading %1").arg(i + 1));
        item->setData(true, gridview_test::ImageLoadingRole);
        loadingModel->appendRow(item);
    }
    gvLoading->setModel(loadingModel);
    attachFluentDelegate(gvLoading);
    gvLoading->setFixedHeight(160);
    gvLoading->anchors()->top = {content, Edge::Top, 20};
    gvLoading->anchors()->left = {content, Edge::Left, 20};
    gvLoading->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(gvLoading);

    // ── GridView 1: 带图片的网格 (单选, 对应 WinUI GridView with Layout Customization) ──
    GridView* gv1 = new GridView(content);
    gv1->setHeaderText("GridView with Layout Customization");
    gv1->setBorderVisible(true);
    gv1->setCellSize(QSize(140, 100));
    gv1->setHorizontalSpacing(6);
    gv1->setVerticalSpacing(6);

    auto* model1 = new QStandardItemModel(gv1);
    struct ItemInfo { QString name; QString likes; QString seed; };
    QList<ItemInfo> items1 = {
        {"Item 1", "90 Likes", "red-forest"},
        {"Item 2", "84 Likes", "carousel"},
        {"Item 3", "96 Likes", "waterfall"},
        {"Item 4", "79 Likes", "green-valley"},
        {"Item 5", "32 Likes", "lake-pier"},
        {"Item 6", "34 Likes", "blue-sky"},
        {"Item 7", "48 Likes", "stone-arch"},
        {"Item 8", "90 Likes", "mountain-snow"},
    };
    for (const auto& info : items1) {
        auto* item = new QStandardItem(info.name);
        item->setData(info.likes, Qt::ToolTipRole);
        loadNetworkImage(item,
            QUrl(QString("https://picsum.photos/seed/%1/280/200").arg(info.seed)));
        model1->appendRow(item);
    }
    gv1->setModel(model1);
    attachFluentDelegate(gv1);
    gv1->setSelectedIndex(5);
    gv1->setFixedHeight(280);
    gv1->anchors()->top   = {gvLoading, Edge::Bottom,  16};
    gv1->anchors()->left  = {content, Edge::Left, 20};
    gv1->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(gv1);

    // ── GridView 2: 多选 + 图片 + check 浮层 (对应 WinUI Content inside of a GridView) ──
    Label* header2 = new Label("Content inside of a GridView.", content);
    header2->setFluentTypography("BodyStrong");
    header2->anchors()->top  = {gv1, Edge::Bottom, 16};
    header2->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header2);

    GridView* gv2 = new GridView(content);
    gv2->setSelectionMode(GridSelectionMode::Multiple);
    gv2->setCellSize(QSize(160, 120));
    gv2->setHorizontalSpacing(8);
    gv2->setVerticalSpacing(8);
    gv2->setBorderVisible(false);

    auto* model2 = new QStandardItemModel(gv2);
    QStringList seeds2 = {"autumn-leaves", "merry-go-round", "bicycle-field",
                          "green-meadow", "harbor-boats", "beach-run",
                          "castle-gate", "mountain-range"};
    for (int i = 0; i < seeds2.size(); ++i) {
        auto* item = new QStandardItem(QString("Photo %1").arg(i + 1));
        loadNetworkImage(item,
            QUrl(QString("https://picsum.photos/seed/%1/320/240").arg(seeds2[i])));
        model2->appendRow(item);
    }
    gv2->setModel(model2);
    attachFluentDelegate(gv2);
    gv2->selectionModel()->select(model2->index(0, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(1, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(2, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(3, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(5, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(6, 0), QItemSelectionModel::Select);
    gv2->selectionModel()->select(model2->index(7, 0), QItemSelectionModel::Select);
    gv2->setFixedHeight(300);
    gv2->anchors()->top   = {header2, Edge::Bottom, 8};
    gv2->anchors()->left  = {content, Edge::Left, 20};
    gv2->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(gv2);

    // ── GridView 3: 空网格 + placeholder ──
    GridView* gv3 = new GridView(content);
    gv3->setHeaderText("Empty Grid");
    gv3->setPlaceholderText("No items to display");
    gv3->setBorderVisible(true);
    attachStringListModel(gv3);
    gv3->setFixedHeight(80);
    gv3->anchors()->top   = {gv2, Edge::Bottom, 16};
    gv3->anchors()->left  = {content, Edge::Left, 20};
    gv3->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(gv3);

    // ── GridView 4: 拖拽重排 (对应 WinUI CanReorderItems) ──
    Label* header4 = new Label("Drag to reorder items.", content);
    header4->setFluentTypography("BodyStrong");
    header4->anchors()->top  = {gv3, Edge::Bottom, 16};
    header4->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header4);

    GridView* gv4 = new GridView(content);
    gv4->setCanReorderItems(true);
    gv4->setCellSize(QSize(120, 90));
    gv4->setHorizontalSpacing(6);
    gv4->setVerticalSpacing(6);
    gv4->setBorderVisible(true);

    auto* model4 = new QStandardItemModel(gv4);
    QStringList seeds4 = {"sunset-bay", "forest-path", "city-lights",
                          "ocean-wave", "desert-dune", "snowy-peak",
                          "river-bend", "flower-field", "night-sky"};
    for (int i = 0; i < seeds4.size(); ++i) {
        auto* item = new QStandardItem(QString("Tile %1").arg(i + 1));
        loadNetworkImage(item,
            QUrl(QString("https://picsum.photos/seed/%1/240/180").arg(seeds4[i])));
        model4->appendRow(item);
    }
    gv4->setModel(model4);
    attachFluentDelegate(gv4);
    gv4->setFixedHeight(230);
    gv4->anchors()->top   = {header4, Edge::Bottom, 8};
    gv4->anchors()->left  = {content, Edge::Left, 20};
    gv4->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(gv4);

    // ── Section 5: Selection Mode Comparison (None / Single / Multiple / Extended) ──
    Label* header5 = new Label("Selection Mode Comparison", content);
    header5->setFluentTypography("BodyStrong");
    header5->anchors()->top  = {gv4, Edge::Bottom, 24};
    header5->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header5);

    // Helper: create a small GridView for mode demo
    auto createModeGrid = [](QWidget* parent) -> GridView* {
        auto* gv = new GridView(parent);
        gv->setBorderVisible(true);
        gv->setCellSize(QSize(80, 60));
        gv->setHorizontalSpacing(4);
        gv->setVerticalSpacing(4);
        auto* model = new QStandardItemModel(gv);
        for (int i = 1; i <= 8; i++)
            model->appendRow(new QStandardItem(QString("Item %1").arg(i)));
        gv->setModel(model);
        attachFluentDelegate(gv);
        return gv;
    };

    // Row 1: None + Single
    auto* modeRow1 = new QWidget(content);
    auto* row1Lay = new QHBoxLayout(modeRow1);
    row1Lay->setSpacing(12);
    row1Lay->setContentsMargins(0, 0, 0, 0);

    auto* gvNone = createModeGrid(modeRow1);
    gvNone->setHeaderText("None");
    gvNone->setSelectionMode(GridSelectionMode::None);

    auto* gvSingleDemo = createModeGrid(modeRow1);
    gvSingleDemo->setHeaderText("Single");
    gvSingleDemo->setSelectionMode(GridSelectionMode::Single);
    gvSingleDemo->setSelectedIndex(2);

    row1Lay->addWidget(gvNone);
    row1Lay->addWidget(gvSingleDemo);
    modeRow1->setFixedHeight(170);
    {
        AnchorLayout::Anchors a;
        a.top   = {header5, Edge::Bottom, 8};
        a.left  = {content, Edge::Left,  20};
        a.right = {content, Edge::Right, -20};
        innerLayout->addAnchoredWidget(modeRow1, a);
    }

    // Row 2: Multiple + Extended
    auto* modeRow2 = new QWidget(content);
    auto* row2Lay = new QHBoxLayout(modeRow2);
    row2Lay->setSpacing(12);
    row2Lay->setContentsMargins(0, 0, 0, 0);

    auto* gvMultiDemo = createModeGrid(modeRow2);
    gvMultiDemo->setHeaderText("Multiple (click toggles)");
    gvMultiDemo->setSelectionMode(GridSelectionMode::Multiple);
    gvMultiDemo->selectionModel()->select(gvMultiDemo->model()->index(0, 0), QItemSelectionModel::Select);
    gvMultiDemo->selectionModel()->select(gvMultiDemo->model()->index(2, 0), QItemSelectionModel::Select);
    gvMultiDemo->selectionModel()->select(gvMultiDemo->model()->index(5, 0), QItemSelectionModel::Select);

    auto* gvExtDemo = createModeGrid(modeRow2);
    gvExtDemo->setHeaderText("Extended (Ctrl/Shift+click)");
    gvExtDemo->setSelectionMode(GridSelectionMode::Extended);
    gvExtDemo->selectionModel()->select(
        QItemSelection(gvExtDemo->model()->index(1, 0), gvExtDemo->model()->index(4, 0)),
        QItemSelectionModel::Select);

    row2Lay->addWidget(gvMultiDemo);
    row2Lay->addWidget(gvExtDemo);
    modeRow2->setFixedHeight(170);
    {
        AnchorLayout::Anchors a;
        a.top   = {modeRow1, Edge::Bottom, 8};
        a.left  = {content,  Edge::Left,  20};
        a.right = {content,  Edge::Right, -20};
        innerLayout->addAnchoredWidget(modeRow2, a);
    }

    // ── Section 6: Drag Reorder × Selection Mode ──
    Label* header6 = new Label("Drag Reorder \u00d7 Selection Mode", content);
    header6->setFluentTypography("BodyStrong");
    header6->anchors()->top  = {modeRow2, Edge::Bottom, 24};
    header6->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header6);

    auto* dragRow = new QWidget(content);
    auto* dragRowLay = new QHBoxLayout(dragRow);
    dragRowLay->setSpacing(12);
    dragRowLay->setContentsMargins(0, 0, 0, 0);

    // Multiple + Drag
    auto* gvMultiDrag = new GridView(dragRow);
    gvMultiDrag->setHeaderText("Multiple + Drag");
    gvMultiDrag->setSelectionMode(GridSelectionMode::Multiple);
    gvMultiDrag->setCanReorderItems(true);
    gvMultiDrag->setBorderVisible(true);
    gvMultiDrag->setCellSize(QSize(90, 70));
    gvMultiDrag->setHorizontalSpacing(4);
    gvMultiDrag->setVerticalSpacing(4);
    {
        auto* mdl = new QStandardItemModel(gvMultiDrag);
        QStringList seeds = {"alpine-lake", "bamboo-grove", "coral-reef",
                             "desert-bloom", "emerald-isle", "frozen-fjord",
                             "golden-gate", "highland-mist"};
        for (int i = 0; i < seeds.size(); ++i) {
            auto* item = new QStandardItem(QString("Tile %1").arg(i + 1));
            loadNetworkImage(item,
                QUrl(QString("https://picsum.photos/seed/%1/180/140").arg(seeds[i])));
            mdl->appendRow(item);
        }
        gvMultiDrag->setModel(mdl);
        attachFluentDelegate(gvMultiDrag);
        gvMultiDrag->selectionModel()->select(mdl->index(1, 0), QItemSelectionModel::Select);
        gvMultiDrag->selectionModel()->select(mdl->index(3, 0), QItemSelectionModel::Select);
        gvMultiDrag->selectionModel()->select(mdl->index(5, 0), QItemSelectionModel::Select);
    }

    // Extended + Drag
    auto* gvExtDrag = new GridView(dragRow);
    gvExtDrag->setHeaderText("Extended + Drag");
    gvExtDrag->setSelectionMode(GridSelectionMode::Extended);
    gvExtDrag->setCanReorderItems(true);
    gvExtDrag->setBorderVisible(true);
    gvExtDrag->setCellSize(QSize(90, 70));
    gvExtDrag->setHorizontalSpacing(4);
    gvExtDrag->setVerticalSpacing(4);
    {
        auto* mdl = new QStandardItemModel(gvExtDrag);
        QStringList seeds = {"ivory-tower", "jade-garden", "karst-peaks",
                             "lavender-row", "marble-arch", "nordic-wood",
                             "opal-cave", "prairie-wind"};
        for (int i = 0; i < seeds.size(); ++i) {
            auto* item = new QStandardItem(QString("Tile %1").arg(i + 1));
            loadNetworkImage(item,
                QUrl(QString("https://picsum.photos/seed/%1/180/140").arg(seeds[i])));
            mdl->appendRow(item);
        }
        gvExtDrag->setModel(mdl);
        attachFluentDelegate(gvExtDrag);
        gvExtDrag->selectionModel()->select(
            QItemSelection(mdl->index(0, 0), mdl->index(2, 0)),
            QItemSelectionModel::Select);
    }

    dragRowLay->addWidget(gvMultiDrag);
    dragRowLay->addWidget(gvExtDrag);
    dragRow->setFixedHeight(210);
    {
        AnchorLayout::Anchors a;
        a.top   = {header6, Edge::Bottom, 8};
        a.left  = {content, Edge::Left,  20};
        a.right = {content, Edge::Right, -20};
        innerLayout->addAnchoredWidget(dragRow, a);
    }

    // 主题切换按钮
    auto* themeBtn = new fluent::basicinput::Button("Switch Theme", content);
    themeBtn->setFluentStyle(fluent::basicinput::Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->top  = {dragRow, Edge::Bottom, 24};
    themeBtn->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(themeBtn);

    // content 的最小高度根据最底部控件计算
    // loading(160) + gv1(280) + gv2 header+grid(16+20+8+300) + gv3(16+80) + gv4 header+grid(16+20+8+230)
    // + header5+modeRow1+modeRow2(24+20+8+170+8+170) + header6+dragRow(24+20+8+210)
    // + themeBtn(24+32) + margins(20+20)
    content->setMinimumHeight(20 + 160 + 16 + 280 + 16 + 20 + 8 + 300 + 16 + 80 + 16 + 20 + 8 + 230
                              + 24 + 20 + 8 + 170 + 8 + 170
                              + 24 + 20 + 8 + 210
                              + 24 + 32 + 20);

    QObject::connect(themeBtn, &fluent::basicinput::Button::clicked, [scrollArea, content]() {
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
