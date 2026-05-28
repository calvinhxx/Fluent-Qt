#include <gtest/gtest.h>

#include <QApplication>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>

#include "design/Typography.h"
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "view/basicinput/Button.h"
#include "view/navigation/Pivot.h"
#include "view/navigation/StackContentHost.h"
#include "view/textfields/Label.h"

using view::AnchorLayout;
using view::basicinput::Button;
using view::navigation::Pivot;
using view::navigation::PivotItem;
using view::navigation::StackContentHost;
using view::textfields::Label;

namespace {

class PivotTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, themeColors().bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

void addAnchored(AnchorLayout* layout, QWidget* widget)
{
    layout->addWidget(widget);
}

void showAndProcess(QWidget& widget)
{
    if (widget.window() && widget.window() != &widget)
        widget.window()->show();
    widget.show();
    QApplication::processEvents();
}

QWidget* createPage(const QString& title, QWidget* parent = nullptr)
{
    using Edge = AnchorLayout::Edge;
    auto* page = new QWidget(parent);
    auto* layout = new AnchorLayout(page);
    page->setLayout(layout);

    auto* heading = new Label(title, page);
    heading->setFluentTypography(QStringLiteral("BodyStrong"));
    heading->anchors()->top = {page, Edge::Top, 18};
    heading->anchors()->left = {page, Edge::Left, 18};
    heading->anchors()->right = {page, Edge::Right, -18};
    addAnchored(layout, heading);

    auto* summary = new Label(QStringLiteral("Page content hosted by StackContentHost and driven by Pivot selection."), page);
    summary->setFluentTypography(QStringLiteral("Caption"));
    summary->setTextElideMode(Qt::ElideRight);
    summary->anchors()->top = {heading, Edge::Bottom, 8};
    summary->anchors()->left = {heading, Edge::Left, 0};
    summary->anchors()->right = {heading, Edge::Right, 0};
    addAnchored(layout, summary);

    auto* action = new Button(QStringLiteral("Open"), page);
    action->setFluentStyle(Button::Accent);
    action->setFixedSize(88, 32);
    action->anchors()->top = {summary, Edge::Bottom, 16};
    action->anchors()->left = {heading, Edge::Left, 0};
    addAnchored(layout, action);
    return page;
}

} // namespace

class PivotTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<PivotItem>("view::navigation::PivotItem");
        qRegisterMetaType<Pivot::OverflowBehavior>("view::navigation::Pivot::OverflowBehavior");
        qRegisterMetaType<QVector<int>>("QVector<int>");
    }

    void SetUp() override
    {
        FluentElement::setTheme(FluentElement::Light);
        window = new PivotTestWindow();
        window->resize(760, 420);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        FluentElement::setTheme(FluentElement::Light);
    }

    PivotTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(PivotTest, DefaultsInheritanceAndMetatypesMatchComponentPattern)
{
    Pivot pivot;

    EXPECT_EQ(pivot.itemCount(), 0);
    EXPECT_EQ(pivot.selectedIndex(), -1);
    EXPECT_EQ(pivot.overflowBehavior(), Pivot::OverflowBehavior::ScrollButtons);
    EXPECT_TRUE(pivot.isEnabled());
    EXPECT_EQ(pivot.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(pivot.sizeHint().isEmpty());
    EXPECT_FALSE(pivot.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&pivot), nullptr);
    EXPECT_NE(dynamic_cast<FluentElement*>(&pivot), nullptr);
    EXPECT_NE(dynamic_cast<view::QMLPlus*>(&pivot), nullptr);
    EXPECT_GT(qMetaTypeId<PivotItem>(), 0);
    EXPECT_GT(qMetaTypeId<Pivot::OverflowBehavior>(), 0);
}

TEST_F(PivotTest, PropertySignalsEmitOnlyForEffectiveChanges)
{
    Pivot pivot;
    pivot.addItem(QStringLiteral("All"));
    pivot.addItem(QStringLiteral("Unread"));

    QSignalSpy selectionSpy(&pivot, &Pivot::selectedIndexChanged);
    QSignalSpy currentSpy(&pivot, &Pivot::currentChanged);
    QSignalSpy overflowSpy(&pivot, &Pivot::overflowBehaviorChanged);
    QSignalSpy itemFontSpy(&pivot, &Pivot::itemFontRoleChanged);
    QSignalSpy iconFontSpy(&pivot, &Pivot::iconFontFamilyChanged);

    pivot.setSelectedIndex(1);
    pivot.setSelectedIndex(1);
    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_EQ(currentSpy.count(), 1);

    pivot.setOverflowBehavior(Pivot::OverflowBehavior::MoreButton);
    pivot.setOverflowBehavior(Pivot::OverflowBehavior::MoreButton);
    pivot.setItemFontRole(QStringLiteral("Caption"));
    pivot.setItemFontRole(QStringLiteral("Caption"));
    pivot.setIconFontFamily(QStringLiteral("Custom Icon Font"));
    pivot.setIconFontFamily(QStringLiteral("Custom Icon Font"));

    EXPECT_EQ(overflowSpy.count(), 1);
    EXPECT_EQ(itemFontSpy.count(), 1);
    EXPECT_EQ(iconFontSpy.count(), 1);
}

TEST_F(PivotTest, ItemManagementPreservesOrderMetadataAndSignals)
{
    Pivot pivot;
    QSignalSpy itemsSpy(&pivot, &Pivot::itemsChanged);
    QSignalSpy selectionSpy(&pivot, &Pivot::selectedIndexChanged);

    EXPECT_EQ(pivot.addItem(QStringLiteral("All")), 0);
    EXPECT_EQ(pivot.itemCount(), 1);
    EXPECT_EQ(pivot.selectedIndex(), 0);

    PivotItem flagged(QStringLiteral("Flagged"), Typography::Icons::Flag, false, 42, QStringLiteral("Flagged mail"));
    EXPECT_EQ(pivot.addItem(flagged), 1);
    EXPECT_FALSE(pivot.itemAt(1).enabled);
    EXPECT_EQ(pivot.itemAt(1).data.toInt(), 42);
    EXPECT_EQ(pivot.itemAt(1).accessibleName, QStringLiteral("Flagged mail"));

    EXPECT_TRUE(pivot.insertItem(1, QStringLiteral("Unread")));
    EXPECT_EQ(pivot.itemAt(1).header, QStringLiteral("Unread"));
    EXPECT_EQ(pivot.itemAt(2).header, QStringLiteral("Flagged"));
    EXPECT_FALSE(pivot.insertItem(-1, QStringLiteral("Bad")));
    EXPECT_FALSE(pivot.insertItem(99, QStringLiteral("Bad")));
    EXPECT_TRUE(pivot.itemAt(99).header.isEmpty());

    EXPECT_TRUE(pivot.setItemHeader(1, QStringLiteral("Unread mail")));
    EXPECT_TRUE(pivot.setItemIconGlyph(1, Typography::Icons::Mail));
    EXPECT_TRUE(pivot.setItemEnabled(2, true));
    EXPECT_TRUE(pivot.setItemData(1, QStringLiteral("payload")));
    EXPECT_TRUE(pivot.setItemAccessibleName(1, QStringLiteral("Unread inbox")));
    EXPECT_EQ(pivot.itemAt(1).iconGlyph, Typography::Icons::Mail);
    EXPECT_EQ(pivot.itemAt(1).data.toString(), QStringLiteral("payload"));
    EXPECT_FALSE(pivot.setItemHeader(1, QStringLiteral("Unread mail")));
    EXPECT_FALSE(pivot.setItemEnabled(99, true));

    EXPECT_FALSE(pivot.removeItem(-1));
    EXPECT_FALSE(pivot.removeItem(99));
    EXPECT_TRUE(pivot.removeItem(1));
    EXPECT_EQ(pivot.itemCount(), 2);
    pivot.clearItems();
    EXPECT_EQ(pivot.itemCount(), 0);
    EXPECT_EQ(pivot.selectedIndex(), -1);
    EXPECT_GE(itemsSpy.count(), 8);
    EXPECT_GE(selectionSpy.count(), 1);
}

TEST_F(PivotTest, SelectionPointerKeyboardAndDisabledBehavior)
{
    using Edge = AnchorLayout::Edge;
    auto* pivot = new Pivot(window);
    pivot->addItem(QStringLiteral("All"));
    pivot->addItem(QStringLiteral("Unread"));
    pivot->addItem(QStringLiteral("Flagged"));
    pivot->setItemEnabled(2, false);
    pivot->setFixedSize(520, 44);
    pivot->anchors()->top = {window, Edge::Top, 20};
    pivot->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, pivot);
    showAndProcess(*window);

    QSignalSpy activationSpy(pivot, &Pivot::itemActivated);
    QSignalSpy selectionSpy(pivot, &Pivot::selectedIndexChanged);

    const QPoint unreadCenter = pivot->itemHeaderGeometry(1).center();
    QTest::mouseClick(pivot, Qt::LeftButton, Qt::NoModifier, unreadCenter);
    QApplication::processEvents();
    EXPECT_EQ(pivot->selectedIndex(), 1);
    EXPECT_EQ(activationSpy.count(), 1);
    EXPECT_EQ(selectionSpy.count(), 1);

    const QPoint disabledCenter = pivot->itemHeaderGeometry(2).center();
    QTest::mouseClick(pivot, Qt::LeftButton, Qt::NoModifier, disabledCenter);
    QApplication::processEvents();
    EXPECT_EQ(pivot->selectedIndex(), 1);
    EXPECT_EQ(activationSpy.count(), 1);

    pivot->setSelectedIndex(2);
    EXPECT_EQ(pivot->selectedIndex(), 1);
    EXPECT_TRUE(pivot->setItemEnabled(2, true));
    pivot->setFocus();
    QTest::keyClick(pivot, Qt::Key_Right);
    QTest::keyClick(pivot, Qt::Key_Space);
    EXPECT_EQ(pivot->selectedIndex(), 2);

    EXPECT_TRUE(pivot->setItemEnabled(2, false));
    EXPECT_NE(pivot->selectedIndex(), 2);
}

TEST_F(PivotTest, SelectionCanDriveExternalStackContentHostPages)
{
    using Edge = AnchorLayout::Edge;
    auto* pivot = new Pivot(window);
    auto* allPage = createPage(QStringLiteral("All"));
    auto* unreadPage = createPage(QStringLiteral("Unread"));
    pivot->addItem(QStringLiteral("All"));
    pivot->addItem(QStringLiteral("Unread"));
    pivot->setFixedSize(520, 44);
    pivot->anchors()->top = {window, Edge::Top, 20};
    pivot->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, pivot);

    auto* host = new StackContentHost(window);
    host->setFixedSize(520, 180);
    host->insertPage(0, allPage);
    host->insertPage(1, unreadPage);
    host->anchors()->top = {pivot, Edge::Bottom, 0};
    host->anchors()->left = {pivot, Edge::Left, 0};
    addAnchored(layout, host);

    QObject::connect(pivot, &Pivot::currentChanged, host, [host](int index) {
        host->setCurrentIndex(index, 0, true);
    });
    host->setCurrentIndex(pivot->selectedIndex(), 0, false);
    showAndProcess(*window);

    ASSERT_TRUE(host->transitionAnimationEnabled());
    EXPECT_EQ(host->currentIndex(), 0);
    EXPECT_TRUE(allPage->isVisible());
    EXPECT_FALSE(unreadPage->isVisible());

    pivot->setSelectedIndex(1);
    QApplication::processEvents();

    EXPECT_EQ(host->currentIndex(), 1);
    EXPECT_TRUE(host->busy());
    EXPECT_FALSE(allPage->isVisible());
    EXPECT_TRUE(unreadPage->isVisible());
    EXPECT_LT(unreadPage->pos().x(), 0);
    EXPECT_GT(unreadPage->pos().x(), -host->width());

    QTRY_VERIFY_WITH_TIMEOUT(!host->busy(), 1000);
    EXPECT_FALSE(allPage->isVisible());
    EXPECT_TRUE(unreadPage->isVisible());

    pivot->setSelectedIndex(0);
    QApplication::processEvents();

    EXPECT_EQ(host->currentIndex(), 0);
    EXPECT_TRUE(host->busy());
    EXPECT_TRUE(allPage->isVisible());
    EXPECT_FALSE(unreadPage->isVisible());
    EXPECT_LT(allPage->pos().x(), 0);
    EXPECT_GT(allPage->pos().x(), -host->width());

    QTRY_VERIFY_WITH_TIMEOUT(!host->busy(), 1000);
    EXPECT_TRUE(allPage->isVisible());
    EXPECT_FALSE(unreadPage->isVisible());
}

TEST_F(PivotTest, GeometryOverflowAndResizeRecomputeDeterministically)
{
    using Edge = AnchorLayout::Edge;
    auto* pivot = new Pivot(window);
    for (int index = 0; index < 10; ++index)
        pivot->addItem(QStringLiteral("Header %1 with long text").arg(index));
    pivot->setFixedSize(260, 44);
    pivot->anchors()->top = {window, Edge::Top, 20};
    pivot->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, pivot);
    showAndProcess(*window);

    EXPECT_FALSE(pivot->headerRowGeometry().isEmpty());
    EXPECT_FALSE(pivot->itemHeaderGeometry(0).isEmpty());
    EXPECT_FALSE(pivot->selectedIndicatorGeometry(0).isEmpty());
    EXPECT_LT(pivot->visibleItemIndexes().size(), pivot->itemCount());
    EXPECT_FALSE(pivot->hiddenItemIndexes().isEmpty());
    EXPECT_FALSE(pivot->overflowForwardGeometry().isEmpty());

    const QVector<int> before = pivot->visibleItemIndexes();
    QTest::mouseClick(pivot, Qt::LeftButton, Qt::NoModifier, pivot->overflowForwardGeometry().center());
    QApplication::processEvents();
    EXPECT_NE(pivot->visibleItemIndexes(), before);

    pivot->setSelectedIndex(9);
    QApplication::processEvents();
    EXPECT_TRUE(pivot->visibleItemIndexes().contains(9));
    EXPECT_FALSE(pivot->itemHeaderGeometry(9).isEmpty());

    pivot->setFixedSize(2400, 44);
    QApplication::processEvents();
    EXPECT_EQ(pivot->hiddenItemIndexes().size(), 0);
    EXPECT_TRUE(pivot->overflowForwardGeometry().isEmpty());
}

TEST_F(PivotTest, ThemeAndAccessibilityPreserveSelectionAndPageState)
{
    Pivot pivot;
    pivot.addItem(QStringLiteral("All"));
    pivot.addItem(QStringLiteral("Urgent"));
    pivot.setItemAccessibleName(1, QStringLiteral("Urgent messages"));
    pivot.setSelectedIndex(1);

    EXPECT_TRUE(pivot.accessibleName().contains(QStringLiteral("Pivot")));
    EXPECT_TRUE(pivot.accessibleDescription().contains(QStringLiteral("Urgent messages")));
    EXPECT_TRUE(pivot.accessibleDescription().contains(QStringLiteral("Item count: 2")));

    FluentElement::setTheme(FluentElement::Dark);
    pivot.onThemeUpdated();
    EXPECT_EQ(pivot.selectedIndex(), 1);
    FluentElement::setTheme(FluentElement::Light);
    pivot.onThemeUpdated();
    EXPECT_EQ(pivot.selectedIndex(), 1);
}

TEST_F(PivotTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->resize(980, 700);

    auto* emailTitle = new Label(QStringLiteral("EMAIL"), window);
    emailTitle->setFluentTypography(QStringLiteral("BodyStrong"));
    emailTitle->setFixedSize(180, 24);
    emailTitle->anchors()->top = {window, Edge::Top, 18};
    emailTitle->anchors()->left = {window, Edge::Left, 30};
    addAnchored(layout, emailTitle);

    auto* emailPivot = new Pivot(window);
    emailPivot->addItem(QStringLiteral("All"));
    emailPivot->addItem(QStringLiteral("Unread"));
    emailPivot->addItem(QStringLiteral("Flagged"));
    emailPivot->addItem(QStringLiteral("Urgent"));
    emailPivot->setFixedSize(920, 44);
    emailPivot->anchors()->top = {emailTitle, Edge::Bottom, 4};
    emailPivot->anchors()->left = {emailTitle, Edge::Left, 0};
    addAnchored(layout, emailPivot);

    auto* emailHost = new StackContentHost(window);
    emailHost->setFixedSize(920, 216);
    emailHost->insertPage(0, createPage(QStringLiteral("All messages")));
    emailHost->insertPage(1, createPage(QStringLiteral("Unread messages")));
    emailHost->insertPage(2, createPage(QStringLiteral("Flagged messages")));
    emailHost->insertPage(3, createPage(QStringLiteral("Urgent messages")));
    emailHost->anchors()->top = {emailPivot, Edge::Bottom, 0};
    emailHost->anchors()->left = {emailPivot, Edge::Left, 0};
    addAnchored(layout, emailHost);
    QObject::connect(emailPivot, &Pivot::currentChanged, emailHost, [emailHost](int index) {
        emailHost->setCurrentIndex(index, 0, true);
    });
    emailHost->setCurrentIndex(emailPivot->selectedIndex(), 0, false);

    auto* compactPivot = new Pivot(window);
    compactPivot->addItem(QStringLiteral("Overview"));
    compactPivot->addItem(QStringLiteral("Disabled"));
    compactPivot->addItem(QStringLiteral("Activity"));
    compactPivot->setItemEnabled(1, false);
    compactPivot->setFixedSize(420, 44);
    compactPivot->anchors()->top = {emailHost, Edge::Bottom, 28};
    compactPivot->anchors()->left = {emailPivot, Edge::Left, 0};
    addAnchored(layout, compactPivot);

    auto* overflowTitle = new Label(QStringLiteral("Overflow"), window);
    overflowTitle->setFluentTypography(QStringLiteral("BodyStrong"));
    overflowTitle->setFixedSize(180, 24);
    overflowTitle->anchors()->top = {compactPivot, Edge::Top, 0};
    overflowTitle->anchors()->left = {compactPivot, Edge::Right, 48};
    addAnchored(layout, overflowTitle);

    auto* overflowPivot = new Pivot(window);
    for (int index = 0; index < 9; ++index)
        overflowPivot->addItem(QStringLiteral("Category %1").arg(index + 1));
    overflowPivot->setFixedSize(420, 44);
    overflowPivot->anchors()->top = {overflowTitle, Edge::Bottom, 4};
    overflowPivot->anchors()->left = {overflowTitle, Edge::Left, 0};
    addAnchored(layout, overflowPivot);

    auto* switchTheme = new Button(QStringLiteral("Switch theme"), window);
    switchTheme->setFixedSize(140, 32);
    switchTheme->anchors()->top = {compactPivot, Edge::Bottom, 28};
    switchTheme->anchors()->left = {compactPivot, Edge::Left, 0};
    QObject::connect(switchTheme, &Button::clicked, window, [this, emailTitle, emailPivot, emailHost, compactPivot, overflowTitle, overflowPivot]() {
        const FluentElement::Theme next = FluentElement::currentTheme() == FluentElement::Light ? FluentElement::Dark : FluentElement::Light;
        FluentElement::setTheme(next);
        window->onThemeUpdated();
        emailTitle->onThemeUpdated();
        emailPivot->onThemeUpdated();
        emailHost->onThemeUpdated();
        compactPivot->onThemeUpdated();
        overflowTitle->onThemeUpdated();
        overflowPivot->onThemeUpdated();
    });
    addAnchored(layout, switchTheme);

    window->show();
    qApp->exec();
}
