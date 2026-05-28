#include <gtest/gtest.h>

#include <QAbstractAnimation>
#include <QApplication>
#include <QCoreApplication>
#include <QMetaEnum>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "view/basicinput/Button.h"
#include "view/collections/FlowView.h"
#include "view/collections/GridView.h"
#include "view/scrolling/ScrollBar.h"
#include "view/textfields/Label.h"

using namespace view;
using namespace view::basicinput;
using namespace view::collections;
using namespace view::textfields;

namespace {

using Edge = AnchorLayout::Edge;

enum { DelegateSizeRole = Qt::UserRole + 301 };

class FlowItemDelegate : public QStyledItemDelegate {
public:
    explicit FlowItemDelegate(FluentElement* themeHost, QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_themeHost(themeHost)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        const QVariant delegateSize = index.data(DelegateSizeRole);
        if (delegateSize.canConvert<QSize>())
            return delegateSize.toSize();
        return QSize();
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        FluentElement::Colors colors{};
        if (m_themeHost)
            colors = m_themeHost->themeColors();

        QColor fill = colors.bgLayerAlt.isValid() ? colors.bgLayerAlt : QColor(245, 245, 245);
        QColor text = colors.textPrimary.isValid() ? colors.textPrimary : QColor(20, 20, 20);
        if (!(option.state & QStyle::State_Enabled)) {
            text = colors.textDisabled;
        } else if (option.state & QStyle::State_Selected) {
            fill = colors.accentDefault;
            text = colors.textOnAccent;
        } else if (option.state & QStyle::State_MouseOver) {
            fill = colors.subtleSecondary;
        }

        const QRectF card = QRectF(option.rect).adjusted(2, 2, -2, -2);
        QPainterPath path;
        path.addRoundedRect(card, CornerRadius::Control, CornerRadius::Control);
        painter->fillPath(path, fill);

        painter->setFont(option.font);
        painter->setPen(text);
        painter->drawText(card.adjusted(10, 0, -10, 0), Qt::AlignCenter,
                          index.data(Qt::DisplayRole).toString());
        painter->restore();
    }

private:
    FluentElement* m_themeHost = nullptr;
};

class FlowTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(palette);
        setAutoFillBackground(true);
    }
};

void processEvents()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

void showOffscreen(QWidget* widget)
{
    widget->setAttribute(Qt::WA_DontShowOnScreen, true);
    widget->show();
    QTest::qWait(50);
    processEvents();
}

QStandardItemModel* createModel(QObject* parent, const QStringList& labels, const QList<QSize>& roleSizes = {},
                                const QList<QSize>& delegateSizes = {})
{
    auto* model = new QStandardItemModel(parent);
    for (int row = 0; row < labels.size(); ++row) {
        auto* item = new QStandardItem(labels.at(row));
        if (row < roleSizes.size() && roleSizes.at(row).isValid())
            item->setData(roleSizes.at(row), Qt::SizeHintRole);
        if (row < delegateSizes.size() && delegateSizes.at(row).isValid())
            item->setData(delegateSizes.at(row), DelegateSizeRole);
        model->appendRow(item);
    }
    return model;
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

void attachDelegate(FlowView* flow)
{
    flow->setItemDelegate(new FlowItemDelegate(static_cast<FluentElement*>(flow), flow));
}

} // namespace

class FlowViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<FlowSelectionMode>("FlowSelectionMode");
    }

    void SetUp() override
    {
        FluentElement::setTheme(FluentElement::Light);
        window = new FlowTestWindow();
        window->resize(720, 520);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        FluentElement::setTheme(FluentElement::Light);
    }

    FlowTestWindow* window = nullptr;
};

TEST_F(FlowViewTest, DefaultsPropertiesAndGridViewRemainSeparate)
{
    FlowView flow;
    EXPECT_EQ(flow.selectionMode(), FlowSelectionMode::Single);
    EXPECT_TRUE(flow.borderVisible());
    EXPECT_TRUE(flow.isBorderVisible());
    EXPECT_EQ(flow.defaultItemSize(), QSize(120, 64));
    EXPECT_EQ(flow.minimumItemSize(), QSize(24, 24));
    EXPECT_EQ(flow.itemSizeRole(), static_cast<int>(Qt::SizeHintRole));
    EXPECT_FALSE(flow.canReorderItems());
    EXPECT_NE(dynamic_cast<QAbstractItemView*>(&flow), nullptr);
    EXPECT_NE(dynamic_cast<FluentElement*>(&flow), nullptr);
    EXPECT_NE(dynamic_cast<QMLPlus*>(&flow), nullptr);

    QSignalSpy selectionSpy(&flow, &FlowView::selectionModeChanged);
    QSignalSpy borderSpy(&flow, &FlowView::borderVisibleChanged);
    QSignalSpy headerSpy(&flow, &FlowView::headerTextChanged);
    QSignalSpy spacingSpy(&flow, &FlowView::horizontalSpacingChanged);
    flow.setSelectionMode(FlowSelectionMode::Multiple);
    flow.setBorderVisible(false);
    flow.setHeaderText(QStringLiteral("Flow samples"));
    flow.setHorizontalSpacing(12);

    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_EQ(borderSpy.count(), 1);
    EXPECT_FALSE(flow.borderVisible());
    EXPECT_FALSE(flow.isBorderVisible());
    EXPECT_EQ(headerSpy.count(), 1);
    EXPECT_EQ(spacingSpy.count(), 1);
    EXPECT_EQ(flow.accessibleName(), QStringLiteral("Flow samples"));

    GridView grid;
    EXPECT_EQ(grid.cellSize(), QSize(112, 112));
    EXPECT_EQ(grid.gridSize(), QSize(116, 116));
}

TEST_F(FlowViewTest, RowWrapLayoutVisualRectAndHitTestingUseVariableSizes)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 300, 220);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    attachDelegate(flow);
    flow->setModel(createModel(flow, {"A", "B", "C"}, {QSize(100, 40), QSize(120, 80), QSize(160, 50)}));

    showOffscreen(window);

    const QRect r0 = flow->visualRect(flow->model()->index(0, 0));
    const QRect r1 = flow->visualRect(flow->model()->index(1, 0));
    const QRect r2 = flow->visualRect(flow->model()->index(2, 0));

    EXPECT_EQ(r0.top(), r1.top());
    EXPECT_GT(r2.top(), r0.top());
    EXPECT_EQ(r2.top(), 90);
    EXPECT_EQ(r1.left(), r0.right() + 1 + 10);
    EXPECT_EQ(flow->indexAt(r0.center()).row(), 0);
    EXPECT_EQ(flow->indexAt(r1.center()).row(), 1);
    EXPECT_EQ(flow->indexAt(r2.center()).row(), 2);
    EXPECT_FALSE(flow->indexAt(QPoint(290, 10)).isValid());

    flow->setGeometry(0, 0, 220, 220);
    processEvents();
    const QRect narrowR1 = flow->visualRect(flow->model()->index(1, 0));
    EXPECT_GT(narrowR1.top(), flow->visualRect(flow->model()->index(0, 0)).top());
}

TEST_F(FlowViewTest, ItemSizeResolutionUsesRoleDelegateAndDefaultWithClamping)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 640, 220);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(0);
    flow->setVerticalSpacing(0);
    flow->setDefaultItemSize(QSize(90, 40));
    flow->setMinimumItemSize(QSize(40, 30));
    flow->setMaximumItemSize(QSize(150, 90));
    attachDelegate(flow);
    flow->setModel(createModel(flow,
                               {"Role", "Delegate", "Default"},
                               {QSize(300, 5), QSize(), QSize()},
                               {QSize(), QSize(130, 70), QSize()}));

    showOffscreen(window);

    EXPECT_EQ(flow->visualRect(flow->model()->index(0, 0)).size(), QSize(150, 30));
    EXPECT_EQ(flow->visualRect(flow->model()->index(1, 0)).size(), QSize(130, 70));
    EXPECT_EQ(flow->visualRect(flow->model()->index(2, 0)).size(), QSize(90, 40));
}

TEST_F(FlowViewTest, ScrollingHeaderPlaceholderThemeAndAccessibility)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 180, 140);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(5);
    flow->setVerticalSpacing(5);
    flow->setHeaderText(QStringLiteral("Flow content"));
    flow->setPlaceholderText(QStringLiteral("No flow items"));
    attachDelegate(flow);

    QStringList labels;
    QList<QSize> sizes;
    for (int row = 0; row < 10; ++row) {
        labels << QStringLiteral("Item %1").arg(row);
        sizes << QSize(100, 40);
    }
    flow->setModel(createModel(flow, labels, sizes));

    showOffscreen(window);
    EXPECT_EQ(flow->accessibleName(), QStringLiteral("Flow content"));
    EXPECT_GT(flow->verticalScrollBar()->maximum(), 0);
    ASSERT_NE(flow->verticalFluentScrollBar(), nullptr);
    EXPECT_TRUE(flow->verticalFluentScrollBar()->isVisible());

    const QModelIndex last = flow->model()->index(flow->model()->rowCount() - 1, 0);
    flow->scrollTo(last, QAbstractItemView::PositionAtBottom);
    processEvents();
    EXPECT_LE(flow->visualRect(last).bottom(), flow->viewport()->height());

    FluentElement::setTheme(FluentElement::Dark);
    processEvents();
    EXPECT_EQ(FluentElement::currentTheme(), FluentElement::Dark);
}

TEST_F(FlowViewTest, PointerSelectionKeyboardNavigationAndDisabledState)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 230, 180);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    attachDelegate(flow);
    flow->setModel(createModel(flow, {"A", "B", "C"}, {QSize(100, 40), QSize(100, 40), QSize(100, 40)}));

    showOffscreen(window);
    QSignalSpy clickSpy(flow, &FlowView::itemClicked);
    const QRect r1 = flow->visualRect(flow->model()->index(1, 0));
    QTest::mouseClick(flow->viewport(), Qt::LeftButton, Qt::NoModifier, r1.center());
    processEvents();
    EXPECT_EQ(flow->selectedIndex(), 1);
    ASSERT_EQ(clickSpy.count(), 1);
    EXPECT_EQ(clickSpy.takeFirst().at(0).toInt(), 1);

    flow->setCurrentIndex(flow->model()->index(0, 0));
    flow->setFocus();
    QTest::keyClick(flow, Qt::Key_Right);
    EXPECT_EQ(flow->currentIndex().row(), 1);
    flow->setCurrentIndex(flow->model()->index(0, 0));
    QTest::keyClick(flow, Qt::Key_Down);
    EXPECT_EQ(flow->currentIndex().row(), 2);

    flow->setEnabled(false);
    const QRect r2 = flow->visualRect(flow->model()->index(2, 0));
    QTest::mouseClick(flow->viewport(), Qt::LeftButton, Qt::NoModifier, r2.center());
    processEvents();
    EXPECT_EQ(clickSpy.count(), 0);
}

TEST_F(FlowViewTest, MultiSelectRequiresControlClickAndDragDoesNotRubberBandSelect)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 260, 180);
    flow->setSelectionMode(FlowSelectionMode::Extended);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    attachDelegate(flow);
    flow->setModel(createModel(flow, {"A", "B", "C"}, {QSize(100, 40), QSize(100, 40), QSize(100, 40)}));

    showOffscreen(window);
    const QRect r0 = flow->visualRect(flow->model()->index(0, 0));
    const QRect r1 = flow->visualRect(flow->model()->index(1, 0));
    const QRect r2 = flow->visualRect(flow->model()->index(2, 0));

    QTest::mouseClick(flow->viewport(), Qt::LeftButton, Qt::NoModifier, r0.center());
    QTest::mouseClick(flow->viewport(), Qt::LeftButton, Qt::ControlModifier, r1.center());
    processEvents();
    EXPECT_EQ(flow->selectedRows(), QList<int>({0, 1}));

    QTest::mousePress(flow->viewport(), Qt::LeftButton, Qt::NoModifier, r0.topLeft() + QPoint(2, 2));
    QTest::mouseMove(flow->viewport(), r2.bottomRight() + QPoint(2, 2));
    QTest::mouseRelease(flow->viewport(), Qt::LeftButton, Qt::NoModifier, r2.bottomRight() + QPoint(2, 2));
    processEvents();
    EXPECT_EQ(flow->selectedRows(), QList<int>({0, 1}));
}

TEST_F(FlowViewTest, DragReorderUsesVariableGeometryAndPreservesSelection)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 360, 220);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    flow->setCanReorderItems(true);
    attachDelegate(flow);
    auto* model = createModel(flow, {"A", "B", "C", "D"},
                              {QSize(80, 40), QSize(120, 50), QSize(80, 40), QSize(140, 50)});
    flow->setModel(model);

    showOffscreen(window);
    flow->setSelectedIndex(0);
    QSignalSpy reorderSpy(flow, &FlowView::itemReordered);

    const QRect sourceRect = flow->visualRect(model->index(0, 0));
    const QRect targetRect = flow->visualRect(model->index(2, 0));
    const QPoint start = sourceRect.center();
    const QPoint dragStart = start + QPoint(QApplication::startDragDistance() + 4, 0);
    const QPoint dropPoint(targetRect.right() + 18, targetRect.center().y());

    QTest::mousePress(flow->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(flow->viewport(), dragStart);
    QTest::mouseMove(flow->viewport(), dropPoint);
    QTest::mouseRelease(flow->viewport(), Qt::LeftButton, Qt::NoModifier, dropPoint);
    processEvents();

    ASSERT_EQ(reorderSpy.count(), 1);
    EXPECT_EQ(reorderSpy.first().at(0).toInt(), 0);
    EXPECT_EQ(reorderSpy.first().at(1).toInt(), 2);
    EXPECT_EQ(modelTexts(model), QStringList({"B", "C", "A", "D"}));
    EXPECT_EQ(flow->selectedRows(), QList<int>({2}));
}

TEST_F(FlowViewTest, DragReorderWithoutModifierSelectsOnlyDraggedItem)
{
    auto* flow = new FlowView(window);
    flow->setGeometry(0, 0, 360, 220);
    flow->setSelectionMode(FlowSelectionMode::Extended);
    flow->setContentMargins(QMargins());
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    flow->setCanReorderItems(true);
    attachDelegate(flow);
    auto* model = createModel(flow, {"A", "B", "C", "D"},
                              {QSize(80, 40), QSize(120, 50), QSize(80, 40), QSize(140, 50)});
    flow->setModel(model);

    showOffscreen(window);
    QTest::mouseClick(flow->viewport(), Qt::LeftButton, Qt::NoModifier,
                      flow->visualRect(model->index(0, 0)).center());
    processEvents();
    EXPECT_EQ(flow->selectedRows(), QList<int>({0}));

    const QRect sourceRect = flow->visualRect(model->index(2, 0));
    const QRect targetRect = flow->visualRect(model->index(3, 0));
    const QPoint start = sourceRect.center();
    const QPoint dragStart = start + QPoint(QApplication::startDragDistance() + 4, 0);
    const QPoint dropPoint(targetRect.right() + 18, targetRect.center().y());

    QTest::mousePress(flow->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(flow->viewport(), dragStart);
    QTest::mouseMove(flow->viewport(), dropPoint);
    QTest::mouseRelease(flow->viewport(), Qt::LeftButton, Qt::NoModifier, dropPoint);
    processEvents();

    const QList<int> selectedRows = flow->selectedRows();
    ASSERT_EQ(selectedRows.size(), 1);
    EXPECT_EQ(model->index(selectedRows.first(), 0).data(Qt::DisplayRole).toString(), QStringLiteral("C"));
}

TEST_F(FlowViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->resize(960, 640);
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(QStringLiteral("FlowView"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 32};
    title->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(title);

    auto* flow = new FlowView(window);
    flow->setHeaderText(QStringLiteral("Variable size flow"));
    flow->setPlaceholderText(QStringLiteral("No items"));
    flow->setCanReorderItems(true);
    flow->setSelectionMode(FlowSelectionMode::Extended);
    flow->setContentMargins(QMargins(12, 12, 12, 12));
    flow->setHorizontalSpacing(10);
    flow->setVerticalSpacing(10);
    attachDelegate(flow);
    flow->anchors()->top = {title, Edge::Bottom, 24};
    flow->anchors()->left = {title, Edge::Left, 0};
    flow->anchors()->right = {title, Edge::Right, -280};
    flow->anchors()->bottom = {window, Edge::Bottom, -32};
    layout->addWidget(flow);

    auto* model = createModel(flow,
                              {"Compact", "Wide card", "Tall", "Chip", "Large", "Small", "Dashboard", "Tag", "Media", "Action"},
                              {QSize(116, 48), QSize(220, 72), QSize(120, 124), QSize(92, 40), QSize(260, 96),
                               QSize(84, 44), QSize(180, 110), QSize(100, 40), QSize(210, 120), QSize(138, 54)});
    flow->setModel(model);

    auto* disabled = new FlowView(window);
    disabled->setHeaderText(QStringLiteral("Disabled"));
    disabled->setEnabled(false);
    disabled->setContentMargins(QMargins(10, 10, 10, 10));
    disabled->setHorizontalSpacing(8);
    disabled->setVerticalSpacing(8);
    attachDelegate(disabled);
    disabled->setModel(createModel(disabled, {"One", "Two", "Three"}, {QSize(96, 44), QSize(136, 56), QSize(112, 44)}));
    disabled->anchors()->top = {flow, Edge::Top, 0};
    disabled->anchors()->left = {flow, Edge::Right, 32};
    disabled->anchors()->right = {title, Edge::Right, 0};
    disabled->anchors()->bottom = {flow, Edge::Top, 180};
    layout->addWidget(disabled);

    auto* addButton = new Button(QStringLiteral("Add"), window);
    addButton->setFixedSize(96, 32);
    addButton->anchors()->top = {disabled, Edge::Bottom, 24};
    addButton->anchors()->left = {disabled, Edge::Left, 0};
    layout->addWidget(addButton);
    QObject::connect(addButton, &Button::clicked, flow, [model]() {
        auto* item = new QStandardItem(QStringLiteral("New item"));
        item->setData(QSize(132, 48), Qt::SizeHintRole);
        model->appendRow(item);
    });

    auto* themeButton = new Button(QStringLiteral("Dark"), window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setIconGlyph(Typography::Icons::Color, 16);
    themeButton->setFluentLayout(Button::IconBefore);
    themeButton->setFixedSize(112, 32);
    themeButton->anchors()->top = {addButton, Edge::Bottom, 12};
    themeButton->anchors()->left = {addButton, Edge::Left, 0};
    layout->addWidget(themeButton);
    QObject::connect(themeButton, &Button::clicked, themeButton, [themeButton]() {
        const bool dark = FluentElement::currentTheme() == FluentElement::Dark;
        FluentElement::setTheme(dark ? FluentElement::Light : FluentElement::Dark);
        themeButton->setText(dark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
}