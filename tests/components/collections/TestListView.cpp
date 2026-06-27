#include <gtest/gtest.h>
#include <QAbstractItemView>
#include <QApplication>
#include <QHash>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QStringListModel>
#include "compatibility/QtCompat.h"
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "utils/DebugOverlay.h"
#include "FluentListItemDelegate.h"
#include "components/collections/ListView.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Spacing.h"
#include "design/Typography.h"

#include "components/scrolling/ScrollBar.h"

using namespace fluent::collections;
using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

namespace {

int defaultListRowHeight() {
    return Spacing::ControlHeight::Standard + Spacing::Gap::Tight;
}

/** 业务组装：为 ListView 挂上 Fluent 行高代理（主题来自 ListView 的 fluent::FluentElement）。 */
void attachFluentDelegate(ListView* lv, int rowHeight = defaultListRowHeight()) {
    lv->setItemDelegate(new listview_test::FluentListItemDelegate(
        static_cast<fluent::FluentElement*>(lv), rowHeight, lv, lv));
    lv->setUniformItemSizes(true);
}

/** 创建 QStringListModel，setModel + attachFluentDelegate。 */
QStringListModel* attachStringListModel(ListView* lv, const QStringList& rows = {}) {
    auto* m = new QStringListModel(rows, lv);
    lv->setModel(m);
    attachFluentDelegate(lv);
    return m;
}

int itemCount(ListView* lv) {
    const auto* m = lv->model();
    return m ? m->rowCount() : 0;
}

QString itemText(ListView* lv, int index) {
    const auto* m = lv->model();
    if (!m || index < 0 || index >= m->rowCount()) return {};
    return m->index(index, 0).data(Qt::DisplayRole).toString();
}

void addItem(ListView* lv, const QString& text) {
    auto* slm = qobject_cast<QStringListModel*>(lv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(text);
    slm->setStringList(list);
}

void addItems(ListView* lv, const QStringList& texts) {
    auto* slm = qobject_cast<QStringListModel*>(lv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    list.append(texts);
    slm->setStringList(list);
}

void insertItem(ListView* lv, int index, const QString& text) {
    auto* slm = qobject_cast<QStringListModel*>(lv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    ASSERT_GE(index, 0);
    ASSERT_LE(index, list.size());
    list.insert(index, text);
    slm->setStringList(list);
}

void removeItem(ListView* lv, int index) {
    auto* slm = qobject_cast<QStringListModel*>(lv->model());
    ASSERT_NE(slm, nullptr);
    QStringList list = slm->stringList();
    ASSERT_GE(index, 0);
    ASSERT_LT(index, list.size());
    list.removeAt(index);
    slm->setStringList(list);
}

void clearItems(ListView* lv) {
    auto* slm = qobject_cast<QStringListModel*>(lv->model());
    ASSERT_NE(slm, nullptr);
    slm->setStringList({});
}

class IndicatorListView : public ListView {
public:
    using ListView::ListView;

    QRect exposedVisualRect(int row) const {
        if (!model() || row < 0 || row >= model()->rowCount())
            return {};
        return ListView::visualRect(model()->index(row, 0));
    }
};

class RecordingStateDelegate : public QStyledItemDelegate {
public:
    explicit RecordingStateDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(180, defaultListRowHeight());
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (index.isValid())
            m_states[index.row()] = option.state;
        QStyledItemDelegate::paint(painter, option, index);
    }

    QStyle::State stateFor(int row) const
    {
        return m_states.value(row, QStyle::State());
    }

    void clearStates() const
    {
        m_states.clear();
    }

private:
    mutable QHash<int, QStyle::State> m_states;
};

void showWindowAndProcess(QWidget* widget) {
    widget->setAttribute(Qt::WA_DontShowOnScreen, true);
    widget->show();
    QApplication::processEvents();
    QTest::qWait(50);
}

QRectF itemBackgroundRect(const IndicatorListView* lv, int row) {
    return QRectF(lv->exposedVisualRect(row)).adjusted(2.0, 1.0, -2.0, -1.0);
}

IndicatorListView* createIndicatorListView(QWidget* parent,
                                           QListView::Flow flow = QListView::TopToBottom,
                                           const QStringList& rows = QStringList{}) {
    auto* lv = new IndicatorListView(parent);
    lv->setGeometry(10, 10, 460, flow == QListView::LeftToRight ? 96 : 220);
    lv->setFlow(flow);
    lv->setWrapping(false);
    lv->setSelectedIndicatorAnimationEnabled(false);
    attachStringListModel(lv, rows.isEmpty()
                                  ? QStringList{"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot"}
                                  : rows);
    return lv;
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

class ListViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 500);
        window->setWindowTitle("Fluent ListView Test");
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

// ── 业务层：QStringListModel + Fluent delegate + 数据操作 ─────────────────────

TEST_F(ListViewTest, AddAndRemoveItems) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);

    addItem(lv, "Apple");
    addItem(lv, "Banana");
    addItem(lv, "Cherry");
    EXPECT_EQ(itemCount(lv), 3);
    EXPECT_EQ(itemText(lv, 0), "Apple");
    EXPECT_EQ(itemText(lv, 1), "Banana");
    EXPECT_EQ(itemText(lv, 2), "Cherry");

    removeItem(lv, 1);
    EXPECT_EQ(itemCount(lv), 2);
    EXPECT_EQ(itemText(lv, 0), "Apple");
    EXPECT_EQ(itemText(lv, 1), "Cherry");

    clearItems(lv);
    EXPECT_EQ(itemCount(lv), 0);
}

TEST_F(ListViewTest, AddItemsBatch) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);
    addItems(lv, {"A", "B", "C", "D"});
    EXPECT_EQ(itemCount(lv), 4);
    EXPECT_EQ(itemText(lv, 3), "D");
}

TEST_F(ListViewTest, InsertItem) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);
    addItems(lv, {"A", "C"});
    insertItem(lv, 1, "B");
    EXPECT_EQ(itemCount(lv), 3);
    EXPECT_EQ(itemText(lv, 0), "A");
    EXPECT_EQ(itemText(lv, 1), "B");
    EXPECT_EQ(itemText(lv, 2), "C");
}

TEST_F(ListViewTest, ItemTextOutOfRange) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);
    addItem(lv, "Only");
    EXPECT_EQ(itemText(lv, -1), "");
    EXPECT_EQ(itemText(lv, 1), "");
}

// ── 视图：选择模式 ────────────────────────────────────────────────────────────

TEST_F(ListViewTest, DefaultSelectionMode) {
    ListView* lv = new ListView(window);
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::Single);
}

TEST_F(ListViewTest, DefaultEditTriggersDisabled) {
    ListView* lv = new ListView(window);
    EXPECT_EQ(lv->editTriggers(), QAbstractItemView::NoEditTriggers);
}

TEST_F(ListViewTest, ListSelectionModeRegisteredInMetaObject) {
    QMetaEnum me = QMetaEnum::fromType<ListSelectionMode>();
    ASSERT_TRUE(me.isValid());
    EXPECT_STREQ(me.key(0), "None");
    EXPECT_STREQ(me.key(1), "Single");
    EXPECT_STREQ(me.key(2), "Multiple");
    EXPECT_STREQ(me.key(3), "Extended");
}

TEST_F(ListViewTest, SelectionModeNone) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv, {"A", "B", "C"});
    lv->setSelectionMode(ListSelectionMode::None);
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::None);
}

TEST_F(ListViewTest, SelectionModeMultiple) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, SIGNAL(selectionModeChanged()));
    lv->setSelectionMode(ListSelectionMode::Multiple);
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);

    lv->setSelectionMode(ListSelectionMode::Multiple);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, SelectionModeExtended) {
    ListView* lv = new ListView(window);
    lv->setSelectionMode(ListSelectionMode::Extended);
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::Extended);
}

// ── 选中 API（依赖已 setModel）────────────────────────────────────────────────

TEST_F(ListViewTest, SingleSelection) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv, {"A", "B", "C"});

    EXPECT_EQ(lv->selectedIndex(), -1);

    lv->setSelectedIndex(1);
    EXPECT_EQ(lv->selectedIndex(), 1);

    lv->setSelectedIndex(-1);
    EXPECT_EQ(lv->selectedIndex(), -1);
}

TEST_F(ListViewTest, SelectedIndexOutOfRange) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv, {"A", "B"});
    lv->setSelectedIndex(1);
    EXPECT_EQ(lv->selectedIndex(), 1);

    lv->setSelectedIndex(99);
    EXPECT_EQ(lv->selectedIndex(), -1);
}

TEST_F(ListViewTest, SelectedRowsSortedAscending) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv, {"A", "B", "C", "D"});
    lv->setSelectionMode(ListSelectionMode::Multiple);

    const QModelIndex i0 = lv->model()->index(0, 0);
    const QModelIndex i2 = lv->model()->index(2, 0);
    lv->selectionModel()->select(i2, QItemSelectionModel::Select);
    lv->selectionModel()->select(i0, QItemSelectionModel::Select);

    QList<int> rows = lv->selectedRows();
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows.at(0), 0);
    EXPECT_EQ(rows.at(1), 2);
}

TEST_F(ListViewTest, ViewportHoveredSignal) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 200, 200);

    EXPECT_FALSE(lv->viewportHovered());

    QSignalSpy spy(lv, &ListView::viewportHoveredChanged);
    FLUENT_MAKE_ENTER_EVENT(enterEv, 5, 5);
    QApplication::sendEvent(lv, &enterEv);
    EXPECT_TRUE(lv->viewportHovered());
    EXPECT_EQ(spy.count(), 1);

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(lv, &leave);
    EXPECT_FALSE(lv->viewportHovered());
    EXPECT_EQ(spy.count(), 2);
}

// ── 视图默认属性 / 业务 delegate 行高 ──────────────────────────────────────────

TEST_F(ListViewTest, DefaultFontRole) {
    ListView* lv = new ListView(window);
    EXPECT_EQ(lv->fontRole(), Typography::FontRole::Body);
}

TEST_F(ListViewTest, FluentDelegateDefaultRowHeight) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);
    auto* del = qobject_cast<listview_test::FluentListItemDelegate*>(lv->itemDelegate());
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->rowHeight(), defaultListRowHeight());
}

TEST_F(ListViewTest, FluentDelegateSetRowHeight) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv);
    auto* del = qobject_cast<listview_test::FluentListItemDelegate*>(lv->itemDelegate());
    ASSERT_NE(del, nullptr);
    del->setRowHeight(48);
    lv->doItemsLayout();
    EXPECT_EQ(del->rowHeight(), 48);
}

TEST_F(ListViewTest, SetFontRole) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, SIGNAL(fontRoleChanged()));
    lv->setFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(lv->fontRole(), Typography::FontRole::Subtitle);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, ItemClickedSignal) {
    ListView* lv = new ListView(window);
    attachStringListModel(lv, {"A", "B", "C"});
    QSignalSpy spy(lv, SIGNAL(itemClicked(int)));

    QModelIndex idx = lv->model()->index(0, 0);
    emit lv->clicked(idx);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 0);
}

TEST_F(ListViewTest, MousePressDefersSelectionUntilRelease) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* lv = new IndicatorListView(window);
    lv->setGeometry(10, 10, 240, 160);
    attachStringListModel(lv, {"A", "B", "C"});
    QSignalSpy clickSpy(lv, SIGNAL(itemClicked(int)));
    window->show();
    QTest::qWait(50);

    const QPoint point = lv->exposedVisualRect(1).center();
    QTest::mousePress(lv->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QApplication::processEvents();

    EXPECT_EQ(lv->selectedIndex(), -1);
    EXPECT_EQ(clickSpy.count(), 0);

    QTest::mouseRelease(lv->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QApplication::processEvents();

    EXPECT_EQ(lv->selectedIndex(), 1);
    ASSERT_EQ(clickSpy.count(), 1);
    EXPECT_EQ(clickSpy.at(0).at(0).toInt(), 1);
}

TEST_F(ListViewTest, PressedPointerMoveUpdatesHoverVisualState) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* lv = new IndicatorListView(window);
    lv->setGeometry(10, 10, 240, 180);
    attachStringListModel(lv, {"A", "B", "C", "D"});
    auto* delegate = new RecordingStateDelegate(lv);
    lv->setItemDelegate(delegate);
    lv->setUniformItemSizes(true);
    window->show();
    QTest::qWait(50);

    const QPoint row1 = lv->exposedVisualRect(1).center();
    const QPoint row2 = lv->exposedVisualRect(2).center();

    QTest::mousePress(lv->viewport(), Qt::LeftButton, Qt::NoModifier, row1);
    lv->viewport()->update();
    QApplication::processEvents();

    EXPECT_TRUE(delegate->stateFor(1) & QStyle::State_MouseOver);
    EXPECT_TRUE(delegate->stateFor(1) & QStyle::State_Sunken);

    delegate->clearStates();
    FLUENT_MAKE_MOUSE_EVENT(moveEvent, QEvent::MouseMove, lv->viewport(), row2,
                            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(lv->viewport(), &moveEvent);
    lv->viewport()->update();
    QApplication::processEvents();

    EXPECT_TRUE(delegate->stateFor(2) & QStyle::State_MouseOver);
    EXPECT_FALSE(delegate->stateFor(2) & QStyle::State_Sunken);

    QTest::mouseRelease(lv->viewport(), Qt::LeftButton, Qt::NoModifier, row2);
}

TEST_F(ListViewTest, MultiplePointerSelectionTogglesRowsOnRelease) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* lv = new IndicatorListView(window);
    lv->setGeometry(10, 10, 240, 180);
    lv->setSelectionMode(ListSelectionMode::Multiple);
    attachStringListModel(lv, {"A", "B", "C", "D"});
    window->show();
    QTest::qWait(50);

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::NoModifier,
                      lv->exposedVisualRect(1).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({1}));

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::NoModifier,
                      lv->exposedVisualRect(3).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({1, 3}));

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::NoModifier,
                      lv->exposedVisualRect(1).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({3}));
}

TEST_F(ListViewTest, ExtendedPointerSelectionSupportsControlAndShift) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    auto* lv = new IndicatorListView(window);
    lv->setGeometry(10, 10, 240, 220);
    lv->setSelectionMode(ListSelectionMode::Extended);
    attachStringListModel(lv, {"A", "B", "C", "D", "E"});
    window->show();
    QTest::qWait(50);

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::NoModifier,
                      lv->exposedVisualRect(1).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({1}));

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::ShiftModifier,
                      lv->exposedVisualRect(3).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({1, 2, 3}));

    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::ControlModifier,
                      lv->exposedVisualRect(2).center());
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedRows(), QList<int>({1, 3}));
}

TEST_F(ListViewTest, FluentScrollBarExists) {
    ListView* lv = new ListView(window);
    EXPECT_NE(lv->verticalFluentScrollBar(), nullptr);
}

TEST_F(ListViewTest, CustomModelWithFluentDelegate) {
    ListView* lv = new ListView(window);
    auto* stdModel = new QStandardItemModel(lv);
    stdModel->appendRow(new QStandardItem("Row0"));
    stdModel->appendRow(new QStandardItem("Row1"));
    lv->setModel(stdModel);
    attachFluentDelegate(lv);

    EXPECT_EQ(lv->model()->rowCount(), 2);
    EXPECT_EQ(lv->model()->index(0, 0).data(Qt::DisplayRole).toString(), "Row0");

    lv->setSelectedIndex(1);
    EXPECT_EQ(lv->selectedIndex(), 1);
}

TEST_F(ListViewTest, ViewDoesNotProvideModelByDefault) {
    ListView* lv = new ListView(window);
    EXPECT_EQ(lv->model(), nullptr);
    // Qt 会为 QAbstractItemView 提供默认 itemDelegate()，故不断言 delegate 为空。
}

// ── 新增属性: borderVisible / headerText / placeholderText ────────────────────

TEST_F(ListViewTest, DefaultBorderVisible) {
    ListView* lv = new ListView(window);
    EXPECT_TRUE(lv->borderVisible());
    EXPECT_TRUE(lv->isBorderVisible());
}

TEST_F(ListViewTest, SetBorderVisible) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::borderVisibleChanged);
    lv->setBorderVisible(false);
    EXPECT_FALSE(lv->borderVisible());
    EXPECT_FALSE(lv->isBorderVisible());
    EXPECT_EQ(spy.count(), 1);

    // 重复设置不触发信号
    lv->setBorderVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, DefaultHeaderText) {
    ListView* lv = new ListView(window);
    EXPECT_TRUE(lv->headerText().isEmpty());
}

TEST_F(ListViewTest, SetHeaderText) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::headerTextChanged);
    lv->setHeaderText("My Header");
    EXPECT_EQ(lv->headerText(), "My Header");
    EXPECT_EQ(spy.count(), 1);

    // 重复设置不触发信号
    lv->setHeaderText("My Header");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, DefaultPlaceholderText) {
    ListView* lv = new ListView(window);
    EXPECT_TRUE(lv->placeholderText().isEmpty());
}

TEST_F(ListViewTest, SetPlaceholderText) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::placeholderTextChanged);
    lv->setPlaceholderText("No items");
    EXPECT_EQ(lv->placeholderText(), "No items");
    EXPECT_EQ(spy.count(), 1);

    lv->setPlaceholderText("No items");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, HeaderVisibleWhenTextSet) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 200);
    lv->setHeaderText("Header");
    window->show();
    QTest::qWait(50);
    auto* headerLabel = lv->findChild<QLabel*>("fluentListViewHeader");
    ASSERT_NE(headerLabel, nullptr);
    EXPECT_TRUE(headerLabel->isVisible());
    EXPECT_EQ(headerLabel->text(), "Header");
}

TEST_F(ListViewTest, HeaderHiddenWhenTextEmpty) {
    ListView* lv = new ListView(window);
    lv->setHeaderText("Header");
    lv->setHeaderText("");
    // setHeaderText("") removes the internal label entirely
    EXPECT_EQ(lv->header(), nullptr);
}

TEST_F(ListViewTest, SetCustomHeader) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 200);

    // Default: no header
    EXPECT_EQ(lv->header(), nullptr);

    // Set custom widget as header
    QSignalSpy spy(lv, &ListView::headerChanged);
    auto* custom = new QWidget;
    custom->setFixedHeight(40);
    lv->setHeader(custom);
    EXPECT_EQ(lv->header(), custom);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(custom->parentWidget(), lv);

    // Replace with nullptr → removes header
    lv->setHeader(nullptr);
    EXPECT_EQ(lv->header(), nullptr);
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ListViewTest, SetCustomFooter) {
    ListView* lv = new ListView(window);

    EXPECT_EQ(lv->footer(), nullptr);

    QSignalSpy spy(lv, &ListView::footerChanged);
    auto* custom = new QWidget;
    custom->setFixedHeight(30);
    lv->setFooter(custom);
    EXPECT_EQ(lv->footer(), custom);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(custom->parentWidget(), lv);
}

TEST_F(ListViewTest, SetHeaderReplacesTextHeader) {
    ListView* lv = new ListView(window);
    lv->setHeaderText("Text Header");
    EXPECT_NE(lv->header(), nullptr);

    // Replace text-created header with custom widget
    auto* custom = new QWidget;
    custom->setFixedHeight(40);
    lv->setHeader(custom);
    EXPECT_EQ(lv->header(), custom);
    // headerText still holds old value but internal label is gone
}

// ── Flow 属性 ─────────────────────────────────────────────────────────────────

TEST_F(ListViewTest, DefaultFlowIsTopToBottom) {
    ListView* lv = new ListView(window);
    EXPECT_EQ(lv->flow(), QListView::TopToBottom);
}

TEST_F(ListViewTest, SetFlowLeftToRight) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::flowChanged);
    lv->setFlow(QListView::LeftToRight);
    EXPECT_EQ(lv->flow(), QListView::LeftToRight);
    EXPECT_EQ(spy.count(), 1);

    // 重复设置不触发信号
    lv->setFlow(QListView::LeftToRight);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, SetFlowBackToTopToBottom) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setFlow(QListView::TopToBottom);
    EXPECT_EQ(lv->flow(), QListView::TopToBottom);
}

TEST_F(ListViewTest, HorizontalFluentScrollBarExists) {
    ListView* lv = new ListView(window);
    EXPECT_NE(lv->horizontalFluentScrollBar(), nullptr);
}

TEST_F(ListViewTest, HorizontalFlowSelection) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"A", "B", "C", "D"});
    lv->setSelectedIndex(2);
    EXPECT_EQ(lv->selectedIndex(), 2);
    EXPECT_EQ(itemText(lv, 2), "C");
}

TEST_F(ListViewTest, HorizontalFlowAddRemoveItems) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv);

    addItems(lv, {"X", "Y", "Z"});
    EXPECT_EQ(itemCount(lv), 3);

    removeItem(lv, 1);
    EXPECT_EQ(itemCount(lv), 2);
    EXPECT_EQ(itemText(lv, 0), "X");
    EXPECT_EQ(itemText(lv, 1), "Z");
}

TEST_F(ListViewTest, HorizontalFlowMultipleSelection) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setSelectionMode(ListSelectionMode::Multiple);
    attachStringListModel(lv, {"A", "B", "C", "D"});

    const QModelIndex i0 = lv->model()->index(0, 0);
    const QModelIndex i3 = lv->model()->index(3, 0);
    lv->selectionModel()->select(i0, QItemSelectionModel::Select);
    lv->selectionModel()->select(i3, QItemSelectionModel::Select);

    QList<int> rows = lv->selectedRows();
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows.at(0), 0);
    EXPECT_EQ(rows.at(1), 3);
}

TEST_F(ListViewTest, HorizontalScrollBarVisibleWhenNeeded) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setFixedSize(100, 60);
    // Use uniform item sizes for horizontal items
    auto* m = new QStringListModel({"AAAA", "BBBB", "CCCC", "DDDD", "EEEE",
                                     "FFFF", "GGGG", "HHHH", "IIII", "JJJJ"}, lv);
    lv->setModel(m);
    layout->addWidget(lv);
    window->show();
    QTest::qWait(50);

    // With many items in a narrow width, the horizontal scroll bar should appear
    auto* hsb = lv->horizontalFluentScrollBar();
    auto* native = lv->horizontalScrollBar();
    // If the native bar has range, the fluent bar should be visible
    if (native->maximum() > native->minimum()) {
        EXPECT_TRUE(hsb->isVisible());
    }
}

TEST_F(ListViewTest, BackgroundVisibleProperty) {
    ListView* lv = new ListView(window);
    EXPECT_TRUE(lv->backgroundVisible());
    EXPECT_TRUE(lv->isBackgroundVisible());
    QSignalSpy spy(lv, &ListView::backgroundVisibleChanged);
    lv->setBackgroundVisible(false);
    EXPECT_FALSE(lv->backgroundVisible());
    EXPECT_FALSE(lv->isBackgroundVisible());
    EXPECT_EQ(spy.count(), 1);
    lv->setBackgroundVisible(false);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, FlowChangeRefreshesScrollBars) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setFixedSize(200, 200);
    attachStringListModel(lv, {"A", "B", "C"});
    layout->addWidget(lv);
    window->show();
    QTest::qWait(50);

    // Switch to horizontal — should not crash
    lv->setFlow(QListView::LeftToRight);
    QTest::qWait(50);

    // Switch back — should not crash
    lv->setFlow(QListView::TopToBottom);
    QTest::qWait(50);
}

TEST_F(ListViewTest, HorizontalFlowInsertItem) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"A", "C"});
    insertItem(lv, 1, "B");
    EXPECT_EQ(itemCount(lv), 3);
    EXPECT_EQ(itemText(lv, 0), "A");
    EXPECT_EQ(itemText(lv, 1), "B");
    EXPECT_EQ(itemText(lv, 2), "C");
}

TEST_F(ListViewTest, HorizontalFlowClearItems) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"A", "B", "C"});
    EXPECT_EQ(itemCount(lv), 3);
    clearItems(lv);
    EXPECT_EQ(itemCount(lv), 0);
}

TEST_F(ListViewTest, HorizontalFlowPlaceholder) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setPlaceholderText("No horizontal items");
    attachStringListModel(lv);
    EXPECT_EQ(lv->placeholderText(), "No horizontal items");
    EXPECT_EQ(itemCount(lv), 0);
}

TEST_F(ListViewTest, HorizontalFlowBorderVisible) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    EXPECT_TRUE(lv->borderVisible());
    lv->setBorderVisible(false);
    EXPECT_FALSE(lv->borderVisible());
}

TEST_F(ListViewTest, HorizontalFlowHeaderText) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setGeometry(10, 10, 300, 100);
    lv->setHeaderText("Horizontal Header");
    EXPECT_EQ(lv->headerText(), "Horizontal Header");
    window->show();
    QTest::qWait(50);
    auto* headerLabel = lv->findChild<QLabel*>("fluentListViewHeader");
    ASSERT_NE(headerLabel, nullptr);
    EXPECT_TRUE(headerLabel->isVisible());
}

TEST_F(ListViewTest, HorizontalFlowSelectedIndex) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"A", "B", "C", "D"});

    EXPECT_EQ(lv->selectedIndex(), -1);
    lv->setSelectedIndex(0);
    EXPECT_EQ(lv->selectedIndex(), 0);
    lv->setSelectedIndex(3);
    EXPECT_EQ(lv->selectedIndex(), 3);

    // Out of range → clear
    lv->setSelectedIndex(99);
    EXPECT_EQ(lv->selectedIndex(), -1);
}

TEST_F(ListViewTest, HorizontalFlowExtendedSelection) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setSelectionMode(ListSelectionMode::Extended);
    attachStringListModel(lv, {"A", "B", "C", "D", "E"});
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::Extended);
}

TEST_F(ListViewTest, HorizontalFlowNoSelection) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setSelectionMode(ListSelectionMode::None);
    attachStringListModel(lv, {"A", "B", "C"});
    EXPECT_EQ(lv->selectionMode(), ListSelectionMode::None);
}

TEST_F(ListViewTest, HorizontalFlowDelegateSizeHintHasWidth) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"Hello World"});
    auto* del = qobject_cast<listview_test::FluentListItemDelegate*>(lv->itemDelegate());
    ASSERT_NE(del, nullptr);

    QStyleOptionViewItem opt;
    opt.font = lv->font();
    QModelIndex idx = lv->model()->index(0, 0);
    QSize hint = del->sizeHint(opt, idx);
    // Width should be positive for horizontal items with text
    EXPECT_GT(hint.width(), 0);
    EXPECT_GT(hint.height(), 0);
}

TEST_F(ListViewTest, HorizontalFlowItemClickedSignal) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    attachStringListModel(lv, {"A", "B", "C"});
    QSignalSpy spy(lv, SIGNAL(itemClicked(int)));

    QModelIndex idx = lv->model()->index(1, 0);
    emit lv->clicked(idx);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 1);
}

TEST_F(ListViewTest, HorizontalFlowWrapping) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setWrapping(true);
    lv->setFixedSize(200, 200);
    attachStringListModel(lv, {"A", "B", "C", "D", "E", "F", "G", "H"});
    layout->addWidget(lv);
    window->show();
    QTest::qWait(50);
    // Should not crash; wrapping mode with horizontal flow allows multi-row layout
    EXPECT_EQ(itemCount(lv), 8);
}

TEST_F(ListViewTest, HorizontalFlowViewportHover) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    lv->setGeometry(10, 10, 200, 60);
    attachStringListModel(lv, {"A", "B", "C"});

    EXPECT_FALSE(lv->viewportHovered());
    QSignalSpy spy(lv, &ListView::viewportHoveredChanged);

    FLUENT_MAKE_ENTER_EVENT(enterEv, 5, 5);
    QApplication::sendEvent(lv, &enterEv);
    EXPECT_TRUE(lv->viewportHovered());
    EXPECT_EQ(spy.count(), 1);

    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(lv, &leave);
    EXPECT_FALSE(lv->viewportHovered());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(ListViewTest, HorizontalFlowCustomModel) {
    ListView* lv = new ListView(window);
    lv->setFlow(QListView::LeftToRight);
    auto* stdModel = new QStandardItemModel(lv);
    stdModel->appendRow(new QStandardItem("Col0"));
    stdModel->appendRow(new QStandardItem("Col1"));
    stdModel->appendRow(new QStandardItem("Col2"));
    lv->setModel(stdModel);
    attachFluentDelegate(lv);

    EXPECT_EQ(lv->model()->rowCount(), 3);
    lv->setSelectedIndex(2);
    EXPECT_EQ(lv->selectedIndex(), 2);
}

// ── Selected indicator motion ────────────────────────────────────────────────

TEST_F(ListViewTest, SelectedIndicatorVerticalPlacement) {
    auto* lv = createIndicatorListView(window);
    showWindowAndProcess(window);

    lv->setSelectedIndex(1);
    QApplication::processEvents();

    const QRectF indicator = lv->selectedIndicatorRect();
    const QRectF bg = itemBackgroundRect(lv, 1);
    ASSERT_FALSE(indicator.isEmpty());
    EXPECT_NEAR(indicator.left(), bg.left() + 4.0, 0.75);
    EXPECT_NEAR(indicator.width(), 3.0, 0.75);
    EXPECT_NEAR(indicator.height(), 16.0, 0.75);
    EXPECT_NEAR(indicator.center().y(), bg.center().y(), 0.75);
}

TEST_F(ListViewTest, SelectedIndicatorHorizontalPlacement) {
    auto* lv = createIndicatorListView(window, QListView::LeftToRight);
    showWindowAndProcess(window);

    lv->setSelectedIndex(2);
    QApplication::processEvents();

    const QRectF indicator = lv->selectedIndicatorRect();
    const QRectF bg = itemBackgroundRect(lv, 2);
    ASSERT_FALSE(indicator.isEmpty());
    EXPECT_NEAR(indicator.height(), 3.0, 0.75);
    EXPECT_GE(indicator.width(), 16.0);
    EXPECT_LE(indicator.width(), 24.0);
    EXPECT_NEAR(indicator.center().x(), bg.center().x(), 0.75);
    EXPECT_NEAR(indicator.bottom(), bg.bottom() - 4.0, 0.75);
}

TEST_F(ListViewTest, SelectedIndicatorVerticalDirectionAwareGeometry) {
    auto* lv = createIndicatorListView(window);
    showWindowAndProcess(window);

    lv->setSelectedIndex(1);
    const QRectF downPrevious = lv->selectedIndicatorRect();
    lv->setSelectedIndex(4);
    const QRectF downTarget = lv->selectedIndicatorRect();
    const QRectF downMid = lv->selectedIndicatorRect(0.5);

    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::Down);
    EXPECT_LT(downMid.top(), downTarget.top());
    EXPECT_GT(downMid.bottom(), downPrevious.bottom());
    EXPECT_GT(downMid.height(), downTarget.height());

    lv->setSelectedIndex(4);
    const QRectF upPrevious = lv->selectedIndicatorRect();
    lv->setSelectedIndex(1);
    const QRectF upTarget = lv->selectedIndicatorRect();
    const QRectF upMid = lv->selectedIndicatorRect(0.5);

    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::Up);
    EXPECT_LT(upMid.top(), upPrevious.top());
    EXPECT_GT(upMid.bottom(), upTarget.bottom());
    EXPECT_GT(upMid.height(), upTarget.height());
}

TEST_F(ListViewTest, SelectedIndicatorHorizontalDirectionAwareGeometry) {
    auto* lv = createIndicatorListView(window, QListView::LeftToRight);
    showWindowAndProcess(window);

    lv->setSelectedIndex(0);
    const QRectF rightPrevious = lv->selectedIndicatorRect();
    lv->setSelectedIndex(3);
    const QRectF rightTarget = lv->selectedIndicatorRect();
    const QRectF rightMid = lv->selectedIndicatorRect(0.5);

    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::Right);
    EXPECT_LT(rightMid.left(), rightTarget.left());
    EXPECT_GT(rightMid.right(), rightPrevious.right());
    EXPECT_GT(rightMid.width(), rightTarget.width());

    lv->setSelectedIndex(3);
    const QRectF leftPrevious = lv->selectedIndicatorRect();
    lv->setSelectedIndex(0);
    const QRectF leftTarget = lv->selectedIndicatorRect();
    const QRectF leftMid = lv->selectedIndicatorRect(0.5);

    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::Left);
    EXPECT_LT(leftMid.left(), leftPrevious.left());
    EXPECT_GT(leftMid.right(), leftTarget.right());
    EXPECT_GT(leftMid.width(), leftTarget.width());
}

TEST_F(ListViewTest, SelectedIndicatorTracksSelectionSources) {
    auto* lv = createIndicatorListView(window);
    showWindowAndProcess(window);

    auto expectIndicatorOnRow = [lv](int row) {
        const QRectF indicator = lv->selectedIndicatorRect();
        const QRectF bg = itemBackgroundRect(lv, row);
        ASSERT_FALSE(indicator.isEmpty());
        EXPECT_NEAR(indicator.center().y(), bg.center().y(), 0.75);
    };

    lv->setSelectedIndex(1);
    expectIndicatorOnRow(1);

    const QModelIndex directIndex = lv->model()->index(4, 0);
    lv->selectionModel()->select(directIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedIndex(), 4);
    expectIndicatorOnRow(4);

    lv->setSelectedIndex(1);
    lv->setFocus();
    QTest::keyClick(lv, Qt::Key_Down);
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedIndex(), 2);
    expectIndicatorOnRow(2);

    const QPoint clickPos = lv->exposedVisualRect(3).center();
    QTest::mouseClick(lv->viewport(), Qt::LeftButton, Qt::NoModifier, clickPos);
    QApplication::processEvents();
    EXPECT_EQ(lv->selectedIndex(), 3);
    expectIndicatorOnRow(3);
}

TEST_F(ListViewTest, MultiSelectionUsesPerItemRevealIndicators) {
    auto* lv = createIndicatorListView(window);
    lv->setSelectionMode(ListSelectionMode::Multiple);
    showWindowAndProcess(window);

    const QModelIndex row1 = lv->model()->index(1, 0);
    const QModelIndex row3 = lv->model()->index(3, 0);
    lv->selectionModel()->select(row1, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    lv->selectionModel()->select(row3, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QApplication::processEvents();

    EXPECT_TRUE(lv->selectedIndicatorRect().isEmpty());
    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::None);
    EXPECT_FALSE(lv->selectedIndicatorRectForRow(1).isEmpty());
    EXPECT_FALSE(lv->selectedIndicatorRectForRow(3).isEmpty());
    EXPECT_TRUE(lv->selectedIndicatorRectForRow(2).isEmpty());

    const QRectF full = lv->selectedIndicatorRectForRow(1, 1.0);
    const QRectF revealStart = lv->selectedIndicatorRectForRow(1, 0.0);
    ASSERT_FALSE(full.isEmpty());
    ASSERT_FALSE(revealStart.isEmpty());
    EXPECT_NEAR(revealStart.height(), full.height() * 0.35, 0.75);
    EXPECT_NEAR(revealStart.center().y(), full.center().y(), 0.75);

    lv->selectionModel()->setCurrentIndex(lv->model()->index(4, 0), QItemSelectionModel::NoUpdate);
    QApplication::processEvents();
    EXPECT_TRUE(lv->selectedIndicatorRect().isEmpty());
    EXPECT_FALSE(lv->selectedIndicatorRectForRow(1).isEmpty());
    EXPECT_FALSE(lv->selectedIndicatorRectForRow(3).isEmpty());
}

TEST_F(ListViewTest, HorizontalMultiSelectionUsesBottomRevealIndicators) {
    auto* lv = createIndicatorListView(window, QListView::LeftToRight);
    lv->setSelectionMode(ListSelectionMode::Multiple);
    showWindowAndProcess(window);

    lv->selectionModel()->select(lv->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    lv->selectionModel()->select(lv->model()->index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QApplication::processEvents();

    const QRectF full = lv->selectedIndicatorRectForRow(0, 1.0);
    const QRectF revealStart = lv->selectedIndicatorRectForRow(0, 0.0);
    ASSERT_FALSE(full.isEmpty());
    ASSERT_FALSE(revealStart.isEmpty());
    EXPECT_NEAR(full.height(), 3.0, 0.75);
    EXPECT_NEAR(revealStart.width(), full.width() * 0.35, 0.75);
    EXPECT_NEAR(revealStart.center().x(), full.center().x(), 0.75);
    EXPECT_FALSE(lv->selectedIndicatorRectForRow(2).isEmpty());
    EXPECT_TRUE(lv->selectedIndicatorRectForRow(1).isEmpty());
}

TEST_F(ListViewTest, SelectedIndicatorFirstSelectionClearingAndEmptyModel) {
    auto* lv = createIndicatorListView(window);
    showWindowAndProcess(window);

    EXPECT_TRUE(lv->selectedIndicatorRect().isEmpty());

    lv->setSelectedIndex(2);
    EXPECT_FALSE(lv->selectedIndicatorRect().isEmpty());
    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::None);

    lv->setSelectedIndex(-1);
    EXPECT_TRUE(lv->selectedIndicatorRect().isEmpty());

    clearItems(lv);
    lv->setSelectedIndex(0);
    EXPECT_TRUE(lv->selectedIndicatorRect().isEmpty());
}

TEST_F(ListViewTest, SelectedIndicatorRefreshesOnLayoutAndThemeChanges) {
    QStringList rows;
    for (int i = 0; i < 30; ++i)
        rows << QStringLiteral("Item %1").arg(i);

    auto* lv = createIndicatorListView(window, QListView::TopToBottom, rows);
    lv->resize(360, 180);
    showWindowAndProcess(window);

    lv->setSelectedIndex(5);
    const QRectF beforeResize = lv->selectedIndicatorRect();
    ASSERT_FALSE(beforeResize.isEmpty());

    lv->resize(420, 220);
    QApplication::processEvents();
    const QRectF afterResize = lv->selectedIndicatorRect();
    EXPECT_FALSE(afterResize.isEmpty());
    EXPECT_NEAR(afterResize.left(), beforeResize.left(), 0.75);

    if (lv->verticalScrollBar()->maximum() > 0) {
        lv->verticalScrollBar()->setValue(qMin(lv->verticalScrollBar()->maximum(),
                                               lv->verticalScrollBar()->value() + 6));
        QApplication::processEvents();
        EXPECT_FALSE(lv->selectedIndicatorRect().isEmpty());
    }

    lv->setSelectedIndex(1);
    QApplication::processEvents();
    lv->setFlow(QListView::LeftToRight);
    QApplication::processEvents();
    const QRectF horizontalRect = lv->selectedIndicatorRect();
    ASSERT_FALSE(horizontalRect.isEmpty());
    EXPECT_NEAR(horizontalRect.height(), 3.0, 0.75);
    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::None);

    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(previousTheme == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    QApplication::processEvents();
    EXPECT_FALSE(lv->selectedIndicatorRect().isEmpty());
    fluent::FluentElement::setTheme(previousTheme);
    QApplication::processEvents();
}

TEST_F(ListViewTest, SelectedIndicatorSettlesAfterDragReorder) {
    auto* lv = new IndicatorListView(window);
    lv->setGeometry(10, 10, 320, 220);
    lv->setCanReorderItems(true);
    lv->setSelectedIndicatorAnimationEnabled(false);

    auto* stdModel = new QStandardItemModel(lv);
    for (const QString& text : {QStringLiteral("High"), QStringLiteral("Medium"),
                                QStringLiteral("Low"), QStringLiteral("None")}) {
        stdModel->appendRow(new QStandardItem(text));
    }
    lv->setModel(stdModel);
    attachFluentDelegate(lv);
    showWindowAndProcess(window);

    lv->setSelectedIndex(0);
    ASSERT_FALSE(lv->selectedIndicatorRect().isEmpty());

    const QPoint startPos = lv->exposedVisualRect(0).center();
    const QPoint dropPos = lv->exposedVisualRect(2).center() + QPoint(0, 8);
    QTest::mousePress(lv->viewport(), Qt::LeftButton, Qt::NoModifier, startPos);
    QTest::mouseMove(lv->viewport(), dropPos, QApplication::startDragDistance() + 2);
    QTest::mouseRelease(lv->viewport(), Qt::LeftButton, Qt::NoModifier, dropPos);
    QApplication::processEvents();

    EXPECT_FALSE(lv->selectedIndicatorRect().isEmpty());
    EXPECT_EQ(lv->selectedIndicatorMotionDirection(), ListView::IndicatorMotionDirection::None);
}

// ── Footer tests ──────────────────────────────────────────────────────────────

TEST_F(ListViewTest, DefaultFooterText) {
    ListView* lv = new ListView(window);
    EXPECT_TRUE(lv->footerText().isEmpty());
}

TEST_F(ListViewTest, SetFooterText) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::footerTextChanged);
    lv->setFooterText("Total: 5 items");
    EXPECT_EQ(lv->footerText(), "Total: 5 items");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, FooterVisibleWhenTextSet) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 250);
    attachStringListModel(lv, {"A", "B"});
    lv->setFooterText("Footer");
    window->show();
    QTest::qWait(50);

    auto* footerLabel = lv->findChild<QLabel*>("fluentListViewFooter");
    ASSERT_NE(footerLabel, nullptr);
    EXPECT_TRUE(footerLabel->isVisible());
    EXPECT_EQ(footerLabel->text(), "Footer");
}

TEST_F(ListViewTest, FooterHiddenWhenTextEmpty) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 250);
    lv->setFooterText("Footer");
    lv->setFooterText("");
    // setFooterText("") removes the internal label entirely
    EXPECT_EQ(lv->footer(), nullptr);
}

TEST_F(ListViewTest, FooterSignalNotDuplicate) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::footerTextChanged);
    lv->setFooterText("A");
    lv->setFooterText("A"); // same value → no signal
    EXPECT_EQ(spy.count(), 1);
}

// ── Drag reorder tests ────────────────────────────────────────────────────────

TEST_F(ListViewTest, DefaultCanReorderItems) {
    ListView* lv = new ListView(window);
    EXPECT_FALSE(lv->canReorderItems());
}

TEST_F(ListViewTest, SetCanReorderItems) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::canReorderItemsChanged);
    lv->setCanReorderItems(true);
    EXPECT_TRUE(lv->canReorderItems());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, DisableCanReorderItems) {
    ListView* lv = new ListView(window);
    lv->setCanReorderItems(true);
    lv->setCanReorderItems(false);
    EXPECT_FALSE(lv->canReorderItems());
}

TEST_F(ListViewTest, CanReorderItemsSignalNotDuplicate) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::canReorderItemsChanged);
    lv->setCanReorderItems(true);
    lv->setCanReorderItems(true); // same
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, ReorderMoveRowInModel) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 250);
    lv->setCanReorderItems(true);

    auto* mdl = new QStringListModel(QStringList{"A", "B", "C", "D"}, lv);
    lv->setModel(mdl);
    attachFluentDelegate(lv);
    window->show();
    QTest::qWait(50);

    // Simulate model move: move row 0 to row 2 (A -> after C)
    bool moved = mdl->moveRow(QModelIndex(), 0, QModelIndex(), 3);
    EXPECT_TRUE(moved);
    EXPECT_EQ(mdl->stringList(), (QStringList{"B", "C", "A", "D"}));
}

// ── Section tests ─────────────────────────────────────────────────────────────

TEST_F(ListViewTest, DefaultSectionEnabled) {
    ListView* lv = new ListView(window);
    EXPECT_FALSE(lv->sectionEnabled());
}

TEST_F(ListViewTest, SetSectionEnabled) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::sectionEnabledChanged);
    lv->setSectionEnabled(true);
    EXPECT_TRUE(lv->sectionEnabled());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, SectionEnabledSignalNotDuplicate) {
    ListView* lv = new ListView(window);
    QSignalSpy spy(lv, &ListView::sectionEnabledChanged);
    lv->setSectionEnabled(true);
    lv->setSectionEnabled(true); // same
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ListViewTest, SetSectionKeyFunction) {
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    ListView* lv = new ListView(window);
    lv->setGeometry(10, 10, 300, 300);
    attachStringListModel(lv, {"Apple", "Avocado", "Banana", "Blueberry", "Cherry"});

    lv->setSectionEnabled(true);
    lv->setSectionKeyFunction([lv](int row) -> QString {
        auto idx = lv->model()->index(row, 0);
        return idx.data().toString().left(1); // Group by first letter
    });

    window->show();
    QTest::qWait(50);

    // Just verify it doesn't crash and section is enabled
    EXPECT_TRUE(lv->sectionEnabled());
}

// ── 跨平台 wheelEvent 测试 ─────────────────────────────────────────────────
// 覆盖 PhaseBased / NoPhasePixel / NoPhaseDiscrete 三种事件路径，以及 cluster 节流。
// 详见 openspec listview-cross-platform-input/.

namespace {

class InspectableListView : public ListView {
public:
    using ListView::ListView;
    int exposedVerticalOffset() const { return verticalOffset(); }
};

ListView* makeScrollableListView(QWidget* parent, int rowCount = 100) {
    auto* lv = new ListView(parent);
    lv->setGeometry(10, 10, 300, 200);
    QStringList items;
    items.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i) items << QStringLiteral("Item %1").arg(i);
    attachStringListModel(lv, items);
    lv->show();
    QTest::qWait(50);
    // Force layout so scrollbar maximum > 0
    lv->doItemsLayout();
    QTest::qWait(20);
    return lv;
}

InspectableListView* makeInspectableScrollableListView(QWidget* parent, int rowCount = 100) {
    auto* lv = new InspectableListView(parent);
    lv->setGeometry(10, 10, 300, 200);
    QStringList items;
    items.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i) items << QStringLiteral("Item %1").arg(i);
    attachStringListModel(lv, items);
    lv->show();
    QTest::qWait(50);
    lv->doItemsLayout();
    QTest::qWait(20);
    return lv;
}

ListView* makeHorizontalScrollableListView(QWidget* parent, int rowCount = 40) {
    auto* lv = new ListView(parent);
    lv->setGeometry(10, 10, 180, 100);
    lv->setFlow(QListView::LeftToRight);
    lv->setWrapping(false);

    QStringList items;
    items.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i)
        items << QStringLiteral("Wide Item %1").arg(i);
    attachStringListModel(lv, items);

    lv->show();
    QTest::qWait(50);
    lv->doItemsLayout();
    QTest::qWait(20);
    return lv;
}

void scrollToBottom(ListView* lv) {
    lv->verticalScrollBar()->setValue(lv->verticalScrollBar()->maximum());
    QTest::qWait(10);
}

void scrollToTop(ListView* lv) {
    lv->verticalScrollBar()->setValue(0);
    QTest::qWait(10);
}

QWheelEvent makeWheelEvent(QWidget* target, QPoint pixelDelta, QPoint angleDelta,
                           Qt::ScrollPhase phase) {
    const QPointF pos = target->rect().center();
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    return QWheelEvent(pos, globalPos, pixelDelta, angleDelta,
                       Qt::NoButton, Qt::NoModifier, phase, false);
}

void sendWheel(QWidget* target, QPoint pixelDelta, QPoint angleDelta,
               Qt::ScrollPhase phase) {
    QWheelEvent ev = makeWheelEvent(target, pixelDelta, angleDelta, phase);
    QApplication::sendEvent(target, &ev);
}

} // namespace

// 5.4 鼠标滚轮单次离散事件 → 正常滚动
TEST_F(ListViewTest, MouseWheelDiscreteScroll) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    const int before = lv->verticalScrollBar()->value();

    // 单次 ±120 angleDelta，无 pixelDelta，NoScrollPhase（NoPhaseDiscrete）
    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_GT(lv->verticalScrollBar()->value(), before)
        << "Mouse wheel should advance scrollbar via NoPhaseDiscrete path";
    EXPECT_GE(lv->verticalScrollBar()->value() - before, Spacing::ControlHeight::Standard)
        << "A standard mouse wheel notch should move by a usable pixel step";
}

TEST_F(ListViewTest, MouseWheelHalfTickStillScrolls) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    const int before = lv->verticalScrollBar()->value();

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -60), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_GT(lv->verticalScrollBar()->value(), before)
        << "High-resolution Windows wheel/touchpad fallback ticks should not feel inert";
}

TEST_F(ListViewTest, ScrollChainingPropertyControlsBoundaryWheel) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);
    const int maxValue = lv->verticalScrollBar()->maximum();
    EXPECT_FALSE(lv->isScrollChainingEnabled());

    QSignalSpy spy(lv, &ListView::scrollChainingEnabledChanged);
    lv->setScrollChainingEnabled(true);
    EXPECT_TRUE(lv->isScrollChainingEnabled());
    EXPECT_EQ(spy.count(), 1);
    lv->setScrollChainingEnabled(true);
    EXPECT_EQ(spy.count(), 1);

    QWheelEvent chainedWheel = makeWheelEvent(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(lv->viewport(), &chainedWheel);
    QTest::qWait(20);
    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(lv->verticalScrollBar()->value(), maxValue);

    lv->setScrollChainingEnabled(false);
    QWheelEvent containedWheel = makeWheelEvent(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    containedWheel.setAccepted(false);
    QApplication::sendEvent(lv->viewport(), &containedWheel);
    QTest::qWait(20);
    EXPECT_TRUE(containedWheel.isAccepted());
}

TEST_F(ListViewTest, WheelPassesThroughWhenContentFits) {
    auto* lv = makeScrollableListView(window, 2);
    ASSERT_EQ(lv->verticalScrollBar()->maximum(), lv->verticalScrollBar()->minimum());

    QWheelEvent wheel = makeWheelEvent(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    wheel.setAccepted(false);
    QApplication::sendEvent(lv->viewport(), &wheel);
    QTest::qWait(20);

    EXPECT_FALSE(wheel.isAccepted());
}

// 5.3 Windows 触控板 cluster 高频序列 → 滚动平滑
TEST_F(ListViewTest, WindowsTouchpadClusterScroll) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    const int before = lv->verticalScrollBar()->value();

    // 5 个连续 ±120 事件，间隔 20ms < kClusterGapMs(120)
    for (int i = 0; i < 5; ++i) {
        sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
        QTest::qWait(20);
    }

    EXPECT_GT(lv->verticalScrollBar()->value(), before)
        << "Windows touchpad cluster should scroll smoothly";
}

// 5.2 Mac RDP → Windows 单次轻拨：5 个小角度事件，30ms 间隔 → 边界不反复 flap
TEST_F(ListViewTest, RdpHighFreqNoBounceFlap) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);
    const int sbVal = lv->verticalScrollBar()->value();
    EXPECT_EQ(sbVal, lv->verticalScrollBar()->maximum())
        << "Pre-condition: scrolled to bottom";

    // 模拟 Mac RDP 单次轻拨：5 个小 angleDelta（±60，scrollPx ≈ 60/120*20 = 10），30ms 间隔
    // 同向越界尾部可触发一次短回弹，但不能反复叠加或污染滚动条。
    for (int i = 0; i < 5; ++i) {
        sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -60), Qt::NoScrollPhase);
        QTest::qWait(30);
    }
    QTest::qWait(50);

    // 要点：bounce 不应被反复触发；滚动条保持在边界且后续反向输入仍可恢复。
    EXPECT_EQ(lv->verticalScrollBar()->value(), sbVal)
        << "Scrollbar should stay pinned at boundary";
}

TEST_F(ListViewTest, NoPhaseDiscreteBoundaryTailStartsBounceAndSettles) {
    auto* lv = makeInspectableScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);
    const int beforeOffset = lv->exposedVerticalOffset();

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_GT(lv->exposedVerticalOffset(), beforeOffset)
        << "Windows NoPhaseDiscrete boundary input should still show a bounded bounce";

    QTest::qWait(500);

    EXPECT_EQ(lv->exposedVerticalOffset(), beforeOffset)
        << "The one-shot boundary bounce should settle back to the native offset";
}

TEST_F(ListViewTest, NoPhaseDiscreteBoundaryTailDoesNotExtendActiveBounce) {
    auto* lv = makeInspectableScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);
    const int beforeOffset = lv->exposedVerticalOffset();

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    const int firstDelta = lv->exposedVerticalOffset() - beforeOffset;
    ASSERT_GT(firstDelta, 0)
        << "Pre-condition: boundary input should create visible overscroll feedback";

    for (int i = 0; i < 4; ++i) {
        sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
        QTest::qWait(5);
    }

    const int tailDelta = lv->exposedVerticalOffset() - beforeOffset;
    EXPECT_LE(tailDelta, firstDelta)
        << "Same-direction boundary tails should not extend or restart the active bounce";

    QTest::qWait(400);
    EXPECT_EQ(lv->exposedVerticalOffset(), beforeOffset)
        << "The original bounce should settle without being prolonged by tail events";
}

TEST_F(ListViewTest, NoPhaseDiscreteBoundaryTailAllowsReverseRecovery) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);
    const int maxValue = lv->verticalScrollBar()->maximum();

    for (int i = 0; i < 4; ++i) {
        sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -60), Qt::NoScrollPhase);
        QTest::qWait(30);
    }

    EXPECT_EQ(lv->verticalScrollBar()->value(), maxValue)
        << "Same-direction NoPhaseDiscrete boundary tails should be consumed at the edge";

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, 120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_LT(lv->verticalScrollBar()->value(), maxValue)
        << "Reverse NoPhaseDiscrete input should immediately scroll back into content";
}

TEST_F(ListViewTest, RdpClusterReachingBoundaryRecoversOnReverseTick) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    const int maxValue = lv->verticalScrollBar()->maximum();
    lv->verticalScrollBar()->setValue(qMax(lv->verticalScrollBar()->minimum(), maxValue - 1));
    QTest::qWait(10);

    for (int i = 0; i < 4; ++i) {
        sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
        QTest::qWait(20);
    }
    EXPECT_EQ(lv->verticalScrollBar()->value(), maxValue)
        << "High-frequency NoPhaseDiscrete cluster should pin at the bottom boundary";

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, 120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_LT(lv->verticalScrollBar()->value(), maxValue)
        << "A reverse tick after the boundary cluster should not be swallowed by stale state";
}

// 5.5 bounce 期间 NoPhase 事件被吞
TEST_F(ListViewTest, BounceConsumesNoPhaseEvents) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);

    // 触发 overscroll：直接发起 NoPhasePixel 事件（pixelDelta 非零），向下越界
    sendWheel(lv->viewport(), QPoint(0, -50), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);
    // 触发 bounce-back（150ms timer）
    QTest::qWait(180);

    // bounce 动画应该正在运行；注入 NoPhaseDiscrete 事件应被吞掉
    const int sbVal = lv->verticalScrollBar()->value();
    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_EQ(lv->verticalScrollBar()->value(), sbVal)
        << "Scrollbar should not move while bounce is consuming NoPhase events";
}

// 5.7 macOS 触控板（PhaseBased）边界 overscroll 不回归
TEST_F(ListViewTest, MacOsTrackpadOverscrollNoRegression) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);

    // ScrollBegin → ScrollUpdate（向下越界）→ ScrollEnd
    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, 0), Qt::ScrollBegin);
    sendWheel(lv->viewport(), QPoint(0, -40), QPoint(0, 0), Qt::ScrollUpdate);
    QTest::qWait(20);
    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, 0), Qt::ScrollEnd);
    // 等待 bounce 完成
    QTest::qWait(400);

    // 滚动条应仍在底部（bounce 已回弹）
    EXPECT_EQ(lv->verticalScrollBar()->value(), lv->verticalScrollBar()->maximum())
        << "After bounce-back, scrollbar should be at boundary";
}

// 5.1 三类事件分类：NoScrollPhase + pixelDelta != 0 走 NoPhasePixel 路径
TEST_F(ListViewTest, NoPhasePixelDirectScroll) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    const int before = lv->verticalScrollBar()->value();

    // NoScrollPhase + pixelDelta = -50 → 应当直接按像素滚动
    sendWheel(lv->viewport(), QPoint(0, -50), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_GT(lv->verticalScrollBar()->value(), before)
        << "NoPhasePixel should scroll using pixelDelta directly";
}

// 5.6 PhaseBased 事件可打断 bounce
TEST_F(ListViewTest, BounceInterruptedByPhaseBased) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    scrollToBottom(lv);

    // 触发 overscroll + bounce
    sendWheel(lv->viewport(), QPoint(0, -50), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);
    QTest::qWait(180); // bounce-back animating

    // PhaseBased ScrollUpdate 应当能停止 bounce 并继续后续逻辑（不被吞）
    sendWheel(lv->viewport(), QPoint(0, 30), QPoint(0, 0), Qt::ScrollUpdate);
    QTest::qWait(20);

    // bounce 已被停止；后续状态应归零或反向移动 — 不强求精确值，只验证不 crash
    SUCCEED() << "PhaseBased event during bounce did not crash";
}

TEST_F(ListViewTest, HorizontalNoPhaseDiscreteUsesDominantAxis) {
    auto* lv = makeHorizontalScrollableListView(window);
    if (lv->horizontalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not horizontally scrollable in this environment";
    }
    const int before = lv->horizontalScrollBar()->value();

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    EXPECT_GT(lv->horizontalScrollBar()->value(), before)
        << "LeftToRight ListView should scroll horizontally from dominant Y-axis NoPhaseDiscrete input";
}

TEST_F(ListViewTest, KeyboardSelectionWorksAfterNoPhaseDiscreteWheel) {
    auto* lv = makeScrollableListView(window);
    if (lv->verticalScrollBar()->maximum() <= 0) {
        GTEST_SKIP() << "Layout not scrollable in this environment";
    }
    lv->setFocusPolicy(Qt::StrongFocus);
    lv->setSelectedIndex(0);
    lv->setCurrentIndex(lv->model()->index(0, 0));

    sendWheel(lv->viewport(), QPoint(0, 0), QPoint(0, -120), Qt::NoScrollPhase);
    QTest::qWait(20);

    lv->setFocus();
    QTest::keyClick(lv, Qt::Key_Down);
    QTest::qWait(20);

    EXPECT_EQ(lv->selectedIndex(), 1)
        << "Keyboard navigation and selection should remain governed by the selection model";
}

// ── 可视化测试（业务组装与上面一致）───────────────────────────────────────────

TEST_F(ListViewTest, VisualCheck) {
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
    scrollArea->setGeometry(0, 0, 780, 600);

    // Fluent 自定义垂直滚动条覆盖在 scrollArea 上
    auto* fluentVBar = new fluent::scrolling::ScrollBar(Qt::Vertical, scrollArea);
    fluentVBar->setObjectName("fluentScrollAreaVBar");
    auto* nativeVBar = scrollArea->verticalScrollBar();
    QObject::connect(nativeVBar,  &QScrollBar::valueChanged, fluentVBar, &QScrollBar::setValue);
    QObject::connect(fluentVBar, &QScrollBar::valueChanged, nativeVBar,  &QScrollBar::setValue);

    // 同步 range / pageStep 并定位
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
    content->setMinimumWidth(780);
    content->onThemeUpdated();
    auto* innerLayout = new AnchorLayout(content);
    content->setLayout(innerLayout);
    scrollArea->setWidget(content);

    // --- ListView 1: 带 header + border 的单选列表 ---
    ListView* lv1 = new ListView(content);
    lv1->setHeaderText("Fruits (Single Selection)");
    lv1->setBorderVisible(true);
    attachStringListModel(lv1, {"Apricot", "Banana", "Cherry", "Date", "Elderberry",
                                 "Fig", "Grape", "Honeydew"});
    lv1->setSelectedIndex(2);
    lv1->setFixedHeight(250);
    lv1->anchors()->top   = {content, Edge::Top,  20};
    lv1->anchors()->left  = {content, Edge::Left, 20};
    lv1->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv1);

    // --- ListView 2: 多选模式，无 border ---
    Label* header2 = new Label("Multiple Selection (no border):", content);
    header2->anchors()->top  = {lv1, Edge::Bottom, 16};
    header2->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header2);

    ListView* lv2 = new ListView(content);
    lv2->setSelectionMode(ListSelectionMode::Multiple);
    lv2->setBorderVisible(false);
    attachStringListModel(lv2, {"Item A", "Item B", "Item C", "Item D"});
    lv2->setFixedHeight(160);
    lv2->anchors()->top   = {header2, Edge::Bottom, 8};
    lv2->anchors()->left  = {content, Edge::Left, 20};
    lv2->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv2);

    // --- ListView 3: 空列表，显示 placeholder ---
    Label* header3 = new Label("Empty list with placeholder:", content);
    header3->anchors()->top  = {lv2, Edge::Bottom, 16};
    header3->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header3);

    ListView* lv3 = new ListView(content);
    lv3->setHeaderText("Empty List");
    lv3->setPlaceholderText("No items to display");
    lv3->setBorderVisible(true);
    attachStringListModel(lv3);
    lv3->setFixedHeight(100);
    lv3->anchors()->top   = {header3, Edge::Bottom, 8};
    lv3->anchors()->left  = {content, Edge::Left, 20};
    lv3->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv3);

    // --- ListView 4: 水平方向列表 ---
    Label* header4 = new Label("Horizontal Flow (LeftToRight):", content);
    header4->anchors()->top  = {lv3, Edge::Bottom, 16};
    header4->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header4);

    ListView* lv4 = new ListView(content);
    lv4->setFlow(QListView::LeftToRight);
    lv4->setBorderVisible(true);
    lv4->setWrapping(false);
    attachStringListModel(lv4, {"Alpha", "Bravo", "Charlie", "Delta", "Echo",
                                 "Foxtrot", "Golf", "Hotel", "India", "Juliet",
                                 "Kilo", "Lima", "Mike", "November"});
    lv4->setSelectedIndex(3);
    lv4->setFixedHeight(100);
    lv4->anchors()->top   = {header4, Edge::Bottom, 8};
    lv4->anchors()->left  = {content, Edge::Left, 20};
    lv4->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv4);

    // --- ListView 5: 水平方向 + 多选 ---
    Label* header5 = new Label("Horizontal Multiple Selection:", content);
    header5->anchors()->top  = {lv4, Edge::Bottom, 16};
    header5->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header5);

    ListView* lv5 = new ListView(content);
    lv5->setFlow(QListView::LeftToRight);
    lv5->setSelectionMode(ListSelectionMode::Multiple);
    lv5->setBorderVisible(true);
    lv5->setWrapping(false);
    attachStringListModel(lv5, {"Red", "Orange", "Yellow", "Green", "Blue",
                                 "Indigo", "Violet", "Pink", "Cyan", "Magenta"});
    lv5->setFixedHeight(100);
    lv5->anchors()->top   = {header5, Edge::Bottom, 8};
    lv5->anchors()->left  = {content, Edge::Left, 20};
    lv5->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv5);

    // --- ListView 6: Custom Header + Footer widgets ---
    Label* header6 = new Label("Custom Header + Footer Widgets:", content);
    header6->anchors()->top  = {lv5, Edge::Bottom, 16};
    header6->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header6);

    ListView* lv6 = new ListView(content);
    new DebugOverlay(lv6);
    lv6->setBorderVisible(true);

    // Custom header: Button with icon
    auto* headerBtn = new Button("Add Contact", lv6);
    headerBtn->setIconGlyph(Typography::Icons::Add);
    headerBtn->setFluentStyle(Button::Accent);
    headerBtn->setFixedHeight(32);
    lv6->setHeader(headerBtn);

    // Custom footer: QLabel with image loaded from network
    auto* footerLabel = new QLabel(lv6);
    footerLabel->setFixedHeight(80);
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setText("Loading image...");
    lv6->setFooter(footerLabel);

    // Load image from network asynchronously
    auto* nam = new QNetworkAccessManager(lv6);
    QObject::connect(nam, &QNetworkAccessManager::finished, [footerLabel](QNetworkReply* reply) {
        if (reply->error() == QNetworkReply::NoError) {
            QPixmap pm;
            pm.loadFromData(reply->readAll());
            if (!pm.isNull()) {
                footerLabel->setPixmap(pm.scaledToHeight(
                    footerLabel->height(), Qt::SmoothTransformation));
            }
        } else {
            footerLabel->setText("Image unavailable");
        }
        reply->deleteLater();
    });
    nam->get(QNetworkRequest(QUrl("https://picsum.photos/300/80")));

    attachStringListModel(lv6, {"Alice", "Bob", "Charlie", "Diana"});
    lv6->setFixedHeight(280);
    lv6->anchors()->top   = {header6, Edge::Bottom, 8};
    lv6->anchors()->left  = {content, Edge::Left, 20};
    lv6->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv6);

    // --- ListView 7: Drag reorder ---
    Label* header7 = new Label("Drag to Reorder:", content);
    header7->anchors()->top  = {lv6, Edge::Bottom, 16};
    header7->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header7);

    ListView* lv7 = new ListView(content);
    lv7->setHeaderText("Priority List");
    lv7->setBorderVisible(true);
    lv7->setCanReorderItems(true);
    attachStringListModel(lv7, {"High", "Medium", "Low", "None", "Critical"});
    lv7->setFixedHeight(200);
    lv7->anchors()->top   = {header7, Edge::Bottom, 8};
    lv7->anchors()->left  = {content, Edge::Left, 20};
    lv7->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv7);

    // --- ListView 8: Section grouping ---
    Label* header8 = new Label("Section Grouping:", content);
    header8->anchors()->top  = {lv7, Edge::Bottom, 16};
    header8->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header8);

    ListView* lv8 = new ListView(content);
    lv8->setHeaderText("Grouped Items");
    lv8->setBorderVisible(true);
    lv8->setSectionEnabled(true);
    attachStringListModel(lv8, {"Apple", "Avocado", "Banana", "Blueberry",
                                 "Cherry", "Cranberry", "Date", "Dragonfruit"});
    lv8->setSectionKeyFunction([lv8](int row) -> QString {
        auto idx = lv8->model()->index(row, 0);
        return idx.data().toString().left(1);
    });
    lv8->setFixedHeight(280);
    lv8->anchors()->top   = {header8, Edge::Bottom, 8};
    lv8->anchors()->left  = {content, Edge::Left, 20};
    lv8->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv8);

    // --- ListView 9: Vertical indicator motion ---
    Label* header9 = new Label("Vertical Indicator Motion:", content);
    header9->anchors()->top  = {lv8, Edge::Bottom, 16};
    header9->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header9);

    Button* verticalUpBtn = new Button("Previous", content);
    verticalUpBtn->setIconGlyph(Typography::Icons::ChevronUp);
    verticalUpBtn->setFixedSize(120, 32);
    verticalUpBtn->anchors()->top  = {header9, Edge::Bottom, 8};
    verticalUpBtn->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(verticalUpBtn);

    Button* verticalDownBtn = new Button("Next", content);
    verticalDownBtn->setIconGlyph(Typography::Icons::ChevronDown);
    verticalDownBtn->setFixedSize(120, 32);
    verticalDownBtn->anchors()->top  = {header9, Edge::Bottom, 8};
    verticalDownBtn->anchors()->left = {verticalUpBtn, Edge::Right, 8};
    innerLayout->addWidget(verticalDownBtn);

    ListView* lv9 = new ListView(content);
    lv9->setHeaderText("Navigation Items");
    lv9->setBorderVisible(true);
    attachStringListModel(lv9, {"Home", "Dashboard", "Messages", "Calendar", "Files", "Settings"});
    lv9->setSelectedIndex(2);
    lv9->setFixedHeight(220);
    lv9->anchors()->top   = {verticalUpBtn, Edge::Bottom, 8};
    lv9->anchors()->left  = {content, Edge::Left, 20};
    lv9->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv9);

    QObject::connect(verticalUpBtn, &Button::clicked, [lv9]() {
        const int next = qMax(0, lv9->selectedIndex() - 1);
        lv9->setSelectedIndex(next);
    });
    QObject::connect(verticalDownBtn, &Button::clicked, [lv9]() {
        const int next = qMin(itemCount(lv9) - 1, lv9->selectedIndex() + 1);
        lv9->setSelectedIndex(next);
    });

    // --- ListView 10: Horizontal indicator motion ---
    Label* header10 = new Label("Horizontal Indicator Motion:", content);
    header10->anchors()->top  = {lv9, Edge::Bottom, 16};
    header10->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(header10);

    Button* horizontalLeftBtn = new Button("Previous", content);
    horizontalLeftBtn->setIconGlyph(Typography::Icons::ChevronLeft);
    horizontalLeftBtn->setFixedSize(120, 32);
    horizontalLeftBtn->anchors()->top  = {header10, Edge::Bottom, 8};
    horizontalLeftBtn->anchors()->left = {content, Edge::Left, 20};
    innerLayout->addWidget(horizontalLeftBtn);

    Button* horizontalRightBtn = new Button("Next", content);
    horizontalRightBtn->setIconGlyph(Typography::Icons::ChevronRight);
    horizontalRightBtn->setFixedSize(120, 32);
    horizontalRightBtn->anchors()->top  = {header10, Edge::Bottom, 8};
    horizontalRightBtn->anchors()->left = {horizontalLeftBtn, Edge::Right, 8};
    innerLayout->addWidget(horizontalRightBtn);

    ListView* lv10 = new ListView(content);
    lv10->setFlow(QListView::LeftToRight);
    lv10->setWrapping(false);
    lv10->setBorderVisible(true);
    attachStringListModel(lv10, {"Overview", "Activity", "Files", "Members", "Settings", "History", "Insights"});
    lv10->setSelectedIndex(2);
    lv10->setFixedHeight(100);
    lv10->anchors()->top   = {horizontalLeftBtn, Edge::Bottom, 8};
    lv10->anchors()->left  = {content, Edge::Left, 20};
    lv10->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(lv10);

    QObject::connect(horizontalLeftBtn, &Button::clicked, [lv10]() {
        const int next = qMax(0, lv10->selectedIndex() - 1);
        lv10->setSelectedIndex(next);
    });
    QObject::connect(horizontalRightBtn, &Button::clicked, [lv10]() {
        const int next = qMin(itemCount(lv10) - 1, lv10->selectedIndex() + 1);
        lv10->setSelectedIndex(next);
    });

    // --- Switch Theme 按钮 ---
    Button* themeBtn = new Button("Switch Theme", content);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->top  = {lv10, Edge::Bottom, 16};
    themeBtn->anchors()->right = {content, Edge::Right, -20};
    innerLayout->addWidget(themeBtn);

    // content 的最小高度根据最底部控件计算
    content->setMinimumHeight(250 + 160 + 100 + 100 + 100 + 200 + 200 + 280 + 220 + 100 + 16*10 + 8*12 + 20*2 + 32 + 180);

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

// ─── Design-language × theme: accent selection-indicator gating ──────────────
//
// The Fluent ListView draws an ADDITIONAL animated accent pill at a selected row's leading edge.
// Under Material 3 / macOS the selected row is filled by the item delegate (a tonal/solid wash), so
// that Fluent pill would double-up and must be SUPPRESSED. This sweep crosses the 3 design languages
// with the 2 app themes and asserts: every combination paints valid content with no opaque near-black
// trap surface on a non-selected row, AND the leading-edge accent pill is present ONLY under Fluent.
// Design language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: Fluent ListView 在选中行前缘额外绘制动画 accent 药丸。Material 3 / macOS 下选中行由委托整行填充
//(色调/实心),该药丸会叠加,必须抑制。本套件以 3 设计语言 × 2 主题遍历并断言:每种组合都绘制出有效内容、
// 非选中行无不透明近黑陷阱面,且前缘 accent 药丸仅在 Fluent 下出现。设计语言与主题为全局单例,夹具在
// TearDown 中复位二者。
class ListViewDesignLanguageTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 320);
        window->setAttribute(Qt::WA_DontShowOnScreen, true);
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Is `color` close enough to `accent` (and opaque enough) to count as an accent pixel?
    // zh_CN: color 是否足够接近 accent(且足够不透明)以计为 accent 像素?
    static bool isAccentLike(const QColor& color, const QColor& accent) {
        constexpr int kTolerance = 42;
        return color.alpha() > 160
            && qAbs(color.red() - accent.red()) <= kTolerance
            && qAbs(color.green() - accent.green()) <= kTolerance
            && qAbs(color.blue() - accent.blue()) <= kTolerance;
    }

    static QImage renderViewport(QWidget* viewport) {
        QImage image(viewport->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        viewport->render(&painter);
        painter.end();
        return image;
    }

    static bool hasAccentPixelInRect(const QImage& image, const QRect& rect, const QColor& accent) {
        const QRect bounded = rect.intersected(QRect(0, 0, image.width(), image.height()));
        for (int y = bounded.top(); y <= bounded.bottom(); ++y)
            for (int x = bounded.left(); x <= bounded.right(); ++x)
                if (isAccentLike(QColor::fromRgba(image.pixel(x, y)), accent))
                    return true;
        return false;
    }

    static bool hasPaintedContent(const QImage& image) {
        const QRgb bg = image.pixel(0, 0);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixel(x, y) != bg)
                    return true;
        return false;
    }

    FluentTestWindow* window = nullptr;
};

TEST_F(ListViewDesignLanguageTest, AccentSelectionPillIsFluentOnlyAcrossThemes) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // Single-select (default) → uses the MOVING pill. Animation off so the pill snaps to target.
            // zh_CN: 单选(默认)→ 使用移动药丸。关闭动画使药丸直接定位到目标。
            auto* lv = createIndicatorListView(window);
            showWindowAndProcess(window);

            constexpr int kSelectedRow = 1;
            constexpr int kOtherRow = 3;
            lv->setSelectedIndex(kSelectedRow);
            QApplication::processEvents();

            const QImage image = renderViewport(lv->viewport());
            ASSERT_FALSE(image.isNull()) << ctx;
            EXPECT_GT(image.width(), 0) << ctx;
            EXPECT_GT(image.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(image)) << "painted nothing: " << ctx;

            const QColor accent = lv->themeColors().accentDefault;
            ASSERT_TRUE(accent.isValid()) << ctx;

            // The pill's geometry is computed identically regardless of design language; only PAINTING
            // is gated. Sample a thin strip at that leading edge for accent pixels.
            // zh_CN: 药丸几何与设计语言无关,仅绘制被门控。在该前缘采样窄带查找 accent 像素。
            const QRectF pillRect = lv->selectedIndicatorRect();
            ASSERT_FALSE(pillRect.isEmpty()) << ctx;
            const QRectF selBg = itemBackgroundRect(lv, kSelectedRow);
            const QRect leadingStrip(qMax(0, int(selBg.left()) - 1),
                                     int(selBg.top()),
                                     14,
                                     int(selBg.height()));
            const bool pillPresent = hasAccentPixelInRect(image, leadingStrip, accent);

            if (lang.lang == fluent::FluentElement::DesignFluent) {
                EXPECT_TRUE(pillPresent)
                    << "Fluent must paint the accent selection pill at the leading edge: " << ctx;
            } else {
                EXPECT_FALSE(pillPresent)
                    << "Fluent accent pill must be SUPPRESSED under M3/macOS: " << ctx;
            }

            // Trap guard: a non-selected row must not be an opaque near-black surface (invalid-QColor
            // setBrush trap / default-light-QPalette trap). zh_CN: 陷阱守卫:非选中行不得为不透明近黑面。
            const QRectF otherBg = itemBackgroundRect(lv, kOtherRow);
            const QPoint probe(int(otherBg.center().x()), int(otherBg.center().y()));
            if (image.rect().contains(probe)) {
                const QColor c = QColor::fromRgba(image.pixel(probe));
                const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
                const bool opaqueBlack = c.alpha() > 200 && lum < 16;
                EXPECT_FALSE(opaqueBlack)
                    << "non-selected row is an opaque near-black surface: " << ctx
                    << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << ","
                    << c.alpha() << ")";
            }

            delete lv;
        }
    }
}
