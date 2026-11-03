#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <utility>

#include <QAccessible>
#include <QApplication>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>

#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::navigation::SelectorBar;
using fluent::navigation::SelectorBarItem;
using fluent::navigation::StackContentHost;
using fluent::textfields::Label;

namespace {

class SelectorBarTestWindow : public QWidget, public fluent::FluentElement {
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

QWidget* createPage(const QString& title, const QString& body = QString(), QWidget* parent = nullptr)
{
    using Edge = AnchorLayout::Edge;
    auto* page = new QWidget(parent);
    auto* layout = new AnchorLayout(page);
    page->setLayout(layout);

    auto* heading = new Label(title, page);
    heading->setFluentTypography(Typography::FontRole::BodyStrong);
    heading->anchors()->top = {page, Edge::Top, 18};
    heading->anchors()->left = {page, Edge::Left, 18};
    heading->anchors()->right = {page, Edge::Right, -18};
    addAnchored(layout, heading);

    auto* summary = new Label(body.isEmpty()
            ? QStringLiteral("External page hosted by StackContentHost "
                             "and driven by SelectorBar selection.")
            : body,
        page);
    summary->setFluentTypography(Typography::FontRole::Caption);
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

class SelectorBarTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::SelectorBarItem>("fluent::navigation::SelectorBarItem");
        qRegisterMetaType<fluent::navigation::SelectorBar::OverflowBehavior>(
            "fluent::navigation::SelectorBar::OverflowBehavior");
        qRegisterMetaType<QVector<int>>("QVector<int>");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new SelectorBarTestWindow();
        window->resize(760, 420);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    SelectorBarTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(SelectorBarTest, DefaultsInheritanceAndMetatypesMatchComponentPattern)
{
    SelectorBar selector;

    EXPECT_EQ(selector.itemCount(), 0);
    EXPECT_EQ(selector.selectedIndex(), -1);
    EXPECT_EQ(selector.overflowBehavior(), SelectorBar::OverflowBehavior::ScrollButtons);
    EXPECT_TRUE(selector.isEnabled());
    EXPECT_EQ(selector.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(selector.sizeHint().isEmpty());
    EXPECT_FALSE(selector.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&selector), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&selector), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&selector), nullptr);
    EXPECT_GT(qMetaTypeId<SelectorBarItem>(), 0);
    EXPECT_GT(qMetaTypeId<SelectorBar::OverflowBehavior>(), 0);
}

TEST_F(SelectorBarTest, PropertiesAndSelectionSignalsSuppressDuplicates)
{
    SelectorBar selector;
    selector.addItem(QStringLiteral("Overview"));
    selector.addItem(QStringLiteral("Activity"));

    QSignalSpy selectedSpy(&selector, &SelectorBar::selectedIndexChanged);
    QSignalSpy currentSpy(&selector, &SelectorBar::currentChanged);
    QSignalSpy selectionSpy(&selector, &SelectorBar::selectionChanged);
    QSignalSpy activatedSpy(&selector, &SelectorBar::itemActivated);
    QSignalSpy overflowSpy(&selector, &SelectorBar::overflowBehaviorChanged);
    QSignalSpy fontSpy(&selector, &SelectorBar::itemFontRoleChanged);
    QSignalSpy iconSpy(&selector, &SelectorBar::iconFontFamilyChanged);

    selector.setSelectedIndex(1);
    selector.setSelectedIndex(1);
    EXPECT_EQ(selector.selectedIndex(), 1);
    EXPECT_EQ(selector.selectedItem().text, QStringLiteral("Activity"));
    EXPECT_TRUE(selector.itemAt(1).selected);
    EXPECT_FALSE(selector.itemAt(0).selected);
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(currentSpy.count(), 1);
    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_EQ(activatedSpy.count(), 0);

    selector.setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector.setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector.setItemFontRole(Typography::FontRole::Caption);
    selector.setItemFontRole(Typography::FontRole::Caption);
    selector.setIconFontFamily(QStringLiteral("Custom Icon Font"));
    selector.setIconFontFamily(QStringLiteral("Custom Icon Font"));
    EXPECT_EQ(overflowSpy.count(), 1);
    EXPECT_EQ(fontSpy.count(), 1);
    EXPECT_EQ(iconSpy.count(), 1);

    EXPECT_TRUE(selector.setItemSelected(1, false));
    EXPECT_EQ(selector.selectedIndex(), -1);
    EXPECT_EQ(selectedSpy.count(), 2);
    EXPECT_FALSE(selector.setItemSelected(1, false));
}

TEST_F(SelectorBarTest, ItemManagementPreservesMetadataAndClampsSelection)
{
    SelectorBar selector;
    QSignalSpy itemsSpy(&selector, &SelectorBar::itemsChanged);
    QSignalSpy countSpy(&selector, &SelectorBar::itemCountChanged);
    QSignalSpy selectedSpy(&selector, &SelectorBar::selectedIndexChanged);

    EXPECT_EQ(selector.addItem(QStringLiteral("Overview")), 0);
    EXPECT_EQ(selector.itemCount(), 1);
    EXPECT_EQ(selector.selectedIndex(), 0);
    EXPECT_TRUE(selector.itemAt(0).selected);

    SelectorBarItem rich(QStringLiteral("Sample code"), Typography::Icons::Document, true, false,
        QStringLiteral("code"), QStringLiteral("Sample code page"));
    EXPECT_EQ(selector.addItem(rich), 1);
    EXPECT_FALSE(selector.itemAt(1).visible);
    EXPECT_EQ(selector.itemAt(1).data.toString(), QStringLiteral("code"));
    EXPECT_EQ(selector.itemAt(1).accessibleName, QStringLiteral("Sample code page"));

    EXPECT_TRUE(selector.insertItem(1, QStringLiteral("Details")));
    EXPECT_EQ(selector.itemAt(1).text, QStringLiteral("Details"));
    EXPECT_EQ(selector.itemAt(2).text, QStringLiteral("Sample code"));
    EXPECT_FALSE(selector.insertItem(-1, QStringLiteral("Bad")));
    EXPECT_FALSE(selector.insertItem(99, QStringLiteral("Bad")));
    EXPECT_TRUE(selector.itemAt(99).text.isEmpty());

    EXPECT_TRUE(selector.setItemText(1, QStringLiteral("Details view")));
    EXPECT_TRUE(selector.setItemIconGlyph(1, Typography::Icons::View));
    EXPECT_TRUE(selector.setItemData(1, 42));
    EXPECT_TRUE(selector.setItemAccessibleName(1, QStringLiteral("Details view selector")));
    EXPECT_EQ(selector.itemAt(1).iconGlyph, Typography::Icons::View);
    EXPECT_EQ(selector.itemAt(1).data.toInt(), 42);
    EXPECT_FALSE(selector.setItemText(1, QStringLiteral("Details view")));
    EXPECT_FALSE(selector.setItemVisible(99, true));

    selector.setSelectedIndex(1);
    EXPECT_TRUE(selector.setItemEnabled(1, false));
    EXPECT_NE(selector.selectedIndex(), 1);
    EXPECT_FALSE(selector.itemAt(1).selected);
    EXPECT_FALSE(selector.setItemSelected(1, true));

    EXPECT_TRUE(selector.setItemEnabled(1, true));
    EXPECT_TRUE(selector.setItemVisible(2, true));
    EXPECT_TRUE(selector.setItemSelected(2, true));
    EXPECT_EQ(selector.selectedIndex(), 2);
    EXPECT_TRUE(selector.setItemVisible(2, false));
    EXPECT_NE(selector.selectedIndex(), 2);

    EXPECT_FALSE(selector.removeItem(-1));
    EXPECT_FALSE(selector.removeItem(99));
    EXPECT_TRUE(selector.removeItem(1));
    EXPECT_EQ(selector.itemCount(), 2);
    selector.clearItems();
    EXPECT_EQ(selector.itemCount(), 0);
    EXPECT_EQ(selector.selectedIndex(), -1);
    EXPECT_GE(itemsSpy.count(), 9);
    EXPECT_GE(countSpy.count(), 4);
    EXPECT_GE(selectedSpy.count(), 3);
}

TEST_F(SelectorBarTest, LayoutOverflowAndGeometryAreDeterministic)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    for (int index = 0; index < 9; ++index)
        selector->addItem(
            SelectorBarItem(QStringLiteral("Category %1 with long text").arg(index), Typography::Icons::Folder));
    selector->setItemVisible(2, false);
    selector->setFixedSize(280, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);
    showAndProcess(*window);

    EXPECT_FALSE(selector->selectorRowGeometry().isEmpty());
    EXPECT_FALSE(selector->itemGeometry(0).isEmpty());
    EXPECT_TRUE(selector->itemGeometry(2).isEmpty());
    const QRect initialIndicator = selector->selectedIndicatorGeometry(selector->selectedIndex());
    EXPECT_FALSE(initialIndicator.isEmpty());
    EXPECT_LT(initialIndicator.width(), selector->itemGeometry(selector->selectedIndex()).width() / 2);
    EXPECT_LE(initialIndicator.width(), 24);
    EXPECT_LT(selector->visibleItemIndexes().size(), selector->itemCount());
    EXPECT_TRUE(selector->hiddenItemIndexes().contains(2));
    EXPECT_FALSE(selector->overflowForwardGeometry().isEmpty());

    selector->clearSelection();
    const QVector<int> before = selector->visibleItemIndexes();
    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->overflowForwardGeometry().center());
    QApplication::processEvents();
    EXPECT_NE(selector->visibleItemIndexes(), before);

    selector->setSelectedIndex(8);
    QApplication::processEvents();
    EXPECT_TRUE(selector->visibleItemIndexes().contains(8));
    EXPECT_FALSE(selector->itemGeometry(8).isEmpty());

    selector->setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector->setFixedSize(250, 44);
    QApplication::processEvents();
    EXPECT_FALSE(selector->overflowGeometry().isEmpty());
    QSignalSpy overflowSpy(selector, &SelectorBar::overflowActivated);
    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->overflowGeometry().center());
    QApplication::processEvents();
    EXPECT_EQ(overflowSpy.count(), 1);

    selector->setFixedSize(2400, 44);
    QApplication::processEvents();
    EXPECT_TRUE(selector->hiddenItemIndexes().contains(2));
    EXPECT_TRUE(selector->overflowGeometry().isEmpty());
}

TEST_F(SelectorBarTest, BackgroundCanBlendIntoHostChrome)
{
    SelectorBar selector;
    selector.addItem(QStringLiteral("Overview"));
    selector.resize(240, 44);
    QSignalSpy backgroundSpy(&selector, &SelectorBar::backgroundVisibleChanged);

    const auto cornerAlpha = [&selector]() {
        QImage image(selector.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        selector.render(&image, QPoint(), QRegion(), QWidget::RenderFlags());
        return qAlpha(image.pixel(selector.width() - 2, 1));
    };

    EXPECT_TRUE(selector.backgroundVisible());
    EXPECT_TRUE(selector.isBackgroundVisible());
    EXPECT_GT(cornerAlpha(), 0);

    selector.setBackgroundVisible(false);
    selector.setBackgroundVisible(false);
    EXPECT_FALSE(selector.isBackgroundVisible());
    EXPECT_EQ(backgroundSpy.count(), 1);
    EXPECT_FALSE(backgroundSpy.first().first().toBool());
    EXPECT_EQ(cornerAlpha(), 0);

    selector.setBackgroundVisible(true);
    EXPECT_EQ(backgroundSpy.count(), 2);
    EXPECT_GT(cornerAlpha(), 0);
}

TEST_F(SelectorBarTest, InvocationIsReportedForTheSelectedItemToo)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    selector->addItem(SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home));
    selector->addItem(SelectorBarItem(QStringLiteral("Activity"), Typography::Icons::Calendar));
    selector->addItem(SelectorBarItem(QStringLiteral("Disabled"), Typography::Icons::Lock, false));
    selector->setFixedSize(520, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);
    showAndProcess(*window);

    QSignalSpy invokedSpy(selector, &SelectorBar::itemInvoked);
    QSignalSpy activatedSpy(selector, &SelectorBar::itemActivated);
    QSignalSpy selectedSpy(selector, &SelectorBar::selectedIndexChanged);
    QVector<QString> order;
    QObject::connect(selector, &SelectorBar::itemInvoked, selector, [&order]() {
        order.append(QStringLiteral("invoked"));
    });
    QObject::connect(selector, &SelectorBar::itemActivated, selector, [&order]() {
        order.append(QStringLiteral("activated"));
    });

    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(0).center());
    EXPECT_EQ(invokedSpy.count(), 1);
    EXPECT_EQ(invokedSpy.last().first().toInt(), 0);
    EXPECT_EQ(activatedSpy.count(), 0);
    EXPECT_EQ(selectedSpy.count(), 0);

    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(1).center());
    EXPECT_EQ(invokedSpy.count(), 2);
    EXPECT_EQ(activatedSpy.count(), 1);
    EXPECT_EQ(selector->selectedIndex(), 1);
    EXPECT_EQ(order.mid(1), (QVector<QString>{QStringLiteral("invoked"), QStringLiteral("activated")}));

    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(2).center());
    EXPECT_EQ(invokedSpy.count(), 2) << "Disabled items are not invoked";

    QTest::keyClick(selector, Qt::Key_Return);
    EXPECT_EQ(invokedSpy.count(), 3);
    EXPECT_EQ(invokedSpy.last().first().toInt(), 1);
    EXPECT_EQ(activatedSpy.count(), 1);

    QAccessibleInterface* accessible = QAccessible::queryAccessibleInterface(selector);
    ASSERT_NE(accessible, nullptr);
    QAccessibleInterface* selectedChild = accessible->child(1);
    ASSERT_NE(selectedChild, nullptr);
    ASSERT_NE(selectedChild->actionInterface(), nullptr);
    selectedChild->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(invokedSpy.count(), 4);
    EXPECT_EQ(activatedSpy.count(), 1);

    // A handler that selects the invoked item itself leaves no stale activation behind.
    // zh_CN: 处理器自行选中被调用条目时，不再补发过期的 activation。
    QObject::connect(selector, &SelectorBar::itemInvoked, selector, [selector](int index) {
        selector->setSelectedIndex(index);
    });
    selectedSpy.clear();
    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(0).center());
    EXPECT_EQ(selector->selectedIndex(), 0);
    EXPECT_EQ(selectedSpy.count(), 1);
    EXPECT_EQ(activatedSpy.count(), 1);
}

TEST_F(SelectorBarTest, MoreButtonOverflowKeepsLeadingAndSelectedItems)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    for (int index = 0; index < 8; ++index)
        selector->addItem(SelectorBarItem(QStringLiteral("Category %1").arg(index), Typography::Icons::Folder));
    selector->setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector->setFixedSize(360, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);
    showAndProcess(*window);

    const QVector<int> leading = selector->visibleItemIndexes();
    ASSERT_GE(leading.size(), 2);
    EXPECT_EQ(leading.first(), 0);
    EXPECT_FALSE(selector->overflowGeometry().isEmpty());
    EXPECT_FALSE(selector->hiddenItemIndexes().isEmpty());

    selector->setSelectedIndex(7);
    QApplication::processEvents();
    const QVector<int> pinned = selector->visibleItemIndexes();
    EXPECT_TRUE(pinned.contains(0)) << "Selecting a trailing item must not scroll the leading items away";
    EXPECT_TRUE(pinned.contains(7));
    EXPECT_TRUE(std::is_sorted(pinned.begin(), pinned.end()));
    EXPECT_TRUE(selector->hiddenItemIndexes().contains(leading.last()));
    EXPECT_FALSE(selector->itemGeometry(7).isEmpty());
    EXPECT_LT(selector->itemGeometry(0).right(), selector->itemGeometry(7).left());
    EXPECT_LE(selector->itemGeometry(7).right(), selector->overflowGeometry().left());
    EXPECT_EQ(selector->overflowGeometry().left(), selector->itemGeometry(7).right() + 1)
        << "More follows the last visible item";

    QSignalSpy overflowSpy(selector, &SelectorBar::overflowActivated);
    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->overflowGeometry().center());
    ASSERT_EQ(overflowSpy.count(), 1);
    const auto hidden = overflowSpy.first().first().value<QVector<int>>();
    EXPECT_FALSE(hidden.contains(7));
    EXPECT_TRUE(hidden.contains(1) || pinned.contains(1));

    selector->setFixedSize(2400, 44);
    QApplication::processEvents();
    EXPECT_TRUE(selector->hiddenItemIndexes().isEmpty());
    EXPECT_TRUE(selector->overflowGeometry().isEmpty());
}

TEST_F(SelectorBarTest, MoreButtonOverflowRevealsTheKeyboardFocusedItem)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    for (int index = 0; index < 8; ++index)
        selector->addItem(SelectorBarItem(QStringLiteral("Category %1").arg(index), Typography::Icons::Folder));
    selector->setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector->setFixedSize(360, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);
    showAndProcess(*window);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
    window->activateWindow();
    selector->setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return selector->hasFocus(); }, 2000));

    QTest::keyClick(selector, Qt::Key_End);
    QApplication::processEvents();
    EXPECT_EQ(selector->selectedIndex(), 0);
    EXPECT_TRUE(selector->visibleItemIndexes().contains(0));
    EXPECT_TRUE(selector->visibleItemIndexes().contains(7));
    EXPECT_FALSE(selector->itemGeometry(7).isEmpty());

    QSignalSpy invokedSpy(selector, &SelectorBar::itemInvoked);
    QTest::keyClick(selector, Qt::Key_Enter);
    ASSERT_EQ(invokedSpy.count(), 1);
    EXPECT_EQ(invokedSpy.first().first().toInt(), 7);
    EXPECT_EQ(selector->selectedIndex(), 7);

    selector->clearFocus();
    QApplication::processEvents();
    EXPECT_TRUE(selector->visibleItemIndexes().contains(7));
}

TEST_F(SelectorBarTest, Contract_AccessibilityMoreButtonKeepsNarrowFocusReachable)
{
    SelectorBar selector;
    for (int index = 0; index < 8; ++index)
        selector.addItem(
            SelectorBarItem(QStringLiteral("Category %1 with a long title").arg(index), Typography::Icons::Folder));
    selector.setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&selector));
    selector.activateWindow();

    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        selector.setLayoutDirection(direction);
        for (const int width : {selector.minimumSizeHint().width(), 180, 360, 1000}) {
            SCOPED_TRACE(::testing::Message() << "width=" << width << " direction=" << direction);
            selector.resize(width, 44);
            selector.setSelectedIndex(0);
            selector.setFocus();
            ASSERT_TRUE(QTest::qWaitFor([&] { return selector.hasFocus(); }, 2000));
            QTest::keyClick(&selector, Qt::Key_End);
            QApplication::processEvents();

            EXPECT_EQ(selector.selectedIndex(), 0);
            EXPECT_FALSE(selector.itemGeometry(7).isEmpty());
            EXPECT_TRUE(selector.visibleItemIndexes().contains(7));
            EXPECT_FALSE(selector.hiddenItemIndexes().contains(7));
            for (int index = 0; index < selector.itemCount(); ++index) {
                const QRect geometry = selector.itemGeometry(index);
                EXPECT_EQ(selector.visibleItemIndexes().contains(index), !geometry.isEmpty());
                EXPECT_EQ(selector.hiddenItemIndexes().contains(index), geometry.isEmpty());
                if (!geometry.isEmpty()) {
                    EXPECT_TRUE(selector.rect().contains(geometry));
                    EXPECT_FALSE(geometry.intersects(selector.overflowGeometry()));
                }
            }
            QAccessibleInterface* accessible = QAccessible::queryAccessibleInterface(&selector);
            ASSERT_NE(accessible, nullptr);
            ASSERT_NE(accessible->child(7), nullptr);
            EXPECT_TRUE(accessible->child(7)->state().focused);
            EXPECT_FALSE(accessible->child(7)->state().offscreen);

            selector.clearFocus();
            QApplication::processEvents();
            EXPECT_FALSE(selector.itemGeometry(0).isEmpty());
            selector.setFocus();
            QApplication::processEvents();
            EXPECT_FALSE(selector.itemGeometry(7).isEmpty()) << "Restoring focus must reveal its item again";

            QSignalSpy overflowSpy(&selector, &SelectorBar::overflowActivated);
            QTest::mouseClick(&selector, Qt::LeftButton, Qt::NoModifier, selector.overflowGeometry().center());
            ASSERT_EQ(overflowSpy.count(), 1);
            EXPECT_EQ(overflowSpy.first().first().value<QVector<int>>(), selector.hiddenItemIndexes());
            QTest::keyClick(&selector, Qt::Key_Return);
            EXPECT_EQ(selector.selectedIndex(), 7);
        }
    }
}

TEST_F(SelectorBarTest, Contract_KeyboardFocusCueDoesNotPersistForMouseInput)
{
    SelectorBar selector;
    selector.addItem(SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home));
    selector.addItem(SelectorBarItem(QStringLiteral("Activity"), Typography::Icons::Calendar));
    selector.resize(520, 44);
    selector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&selector));
    selector.activateWindow();
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        SCOPED_TRACE(::testing::Message() << "theme=" << theme);
        selector.setProperty("fluentThemeOverride", static_cast<int>(theme));
        selector.setSelectedIndex(0);
        selector.setFocus();
        ASSERT_TRUE(QTest::qWaitFor([&] { return selector.hasFocus(); }, 2000));
        QTest::keyClick(&selector, Qt::Key_End);
        const QRect item = selector.itemGeometry(1);
        const auto border = [&]() {
            const QImage image = selector.grab().toImage();
            const qreal dpr = image.devicePixelRatio();
            return image.copy(qRound((item.left() + 8) * dpr), qRound(item.top() * dpr),
                qRound((item.width() - 16) * dpr), qRound(4 * dpr));
        };
        const QImage keyboard = border();
        selector.clearFocus();
        const QImage unfocused = border();
        EXPECT_NE(keyboard, unfocused) << "Keyboard focus must have a visible outline";
        QTest::mouseClick(&selector, Qt::LeftButton, Qt::NoModifier, item.center());
        EXPECT_EQ(selector.selectedIndex(), 1);
        EXPECT_EQ(border(), unfocused) << "Mouse focus must not leave a keyboard outline";
    }
}

TEST_F(SelectorBarTest, Contract_MoreButtonMouseRefocusInvokesTheVisibleItem)
{
    SelectorBar selector;
    for (int index = 0; index < 8; ++index)
        selector.addItem(
            SelectorBarItem(QStringLiteral("Category %1 with a long title").arg(index), Typography::Icons::Folder));
    selector.setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector.resize(selector.minimumSizeHint());
    selector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&selector));
    selector.activateWindow();
    selector.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return selector.hasFocus(); }, 2000));
    QTest::keyClick(&selector, Qt::Key_End);
    selector.clearFocus();
    QApplication::processEvents();
    ASSERT_FALSE(selector.itemGeometry(0).isEmpty());
    QSignalSpy invoked(&selector, &SelectorBar::itemInvoked);
    QTest::mouseClick(&selector, Qt::LeftButton, Qt::NoModifier, selector.itemGeometry(0).center());
    ASSERT_EQ(invoked.count(), 1);
    EXPECT_EQ(invoked.first().first().toInt(), 0);
    EXPECT_EQ(selector.selectedIndex(), 0);
}

TEST_F(SelectorBarTest, Contract_MoreButtonAtForcedTinyWidthKeepsEveryItemInOverflow)
{
    SelectorBar selector;
    for (int index = 0; index < 4; ++index)
        selector.addItem(SelectorBarItem(QStringLiteral("Category %1").arg(index)));
    selector.setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    selector.resize(40, 44);
    EXPECT_TRUE(selector.visibleItemIndexes().isEmpty());
    EXPECT_EQ(selector.hiddenItemIndexes(), (QVector<int>{0, 1, 2, 3}));
    EXPECT_TRUE(selector.rect().contains(selector.overflowGeometry()));
}

TEST_F(SelectorBarTest, PointerKeyboardThemeAndAccessibilityBehaveAsSelector)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    selector->addItem(SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home));
    selector->addItem(SelectorBarItem(QStringLiteral("Activity"), Typography::Icons::Calendar));
    selector->addItem(SelectorBarItem(QStringLiteral("Disabled"), Typography::Icons::Lock, false));
    selector->addItem(SelectorBarItem(QStringLiteral("Hidden"), Typography::Icons::Hide, true, false));
    selector->setItemAccessibleName(1, QStringLiteral("Activity timeline"));
    selector->setFixedSize(560, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);
    showAndProcess(*window);

    QSignalSpy activationSpy(selector, &SelectorBar::itemActivated);
    QSignalSpy selectionSpy(selector, &SelectorBar::selectedIndexChanged);

    const QRect overviewIndicator = selector->selectedIndicatorGeometry(0);
    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(1).center());
    EXPECT_EQ(selector->selectedIndex(), 1);
    const QRect activityIndicator = selector->selectedIndicatorGeometry(1);
    EXPECT_FALSE(activityIndicator.isEmpty());
    EXPECT_NE(overviewIndicator, activityIndicator);
    EXPECT_LE(activityIndicator.width(), 24);
    const QRect revealStart = selector->property("animatedIndicatorRect").toRect();
    EXPECT_FALSE(revealStart.isEmpty());
    EXPECT_LT(revealStart.width(), activityIndicator.width());
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (selector->property("animatedIndicatorRect").toRect()) == (activityIndicator); }, 5000));
    EXPECT_EQ(activationSpy.count(), 1);
    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_TRUE(selector->accessibleName().isEmpty());
    EXPECT_EQ(selector->accessibleDescription(), QStringLiteral("Activity timeline"));

    QTest::mouseClick(selector, Qt::LeftButton, Qt::NoModifier, selector->itemGeometry(2).center());
    QApplication::processEvents();
    EXPECT_EQ(selector->selectedIndex(), 1);
    EXPECT_EQ(activationSpy.count(), 1);
    EXPECT_TRUE(selector->itemGeometry(3).isEmpty());

    selector->setSelectedIndex(0);
    selector->setFocus();
    QTest::keyClick(selector, Qt::Key_Right);
    QTest::keyClick(selector, Qt::Key_Space);
    EXPECT_EQ(selector->selectedIndex(), 1);

    QTest::keyClick(selector, Qt::Key_End);
    QTest::keyClick(selector, Qt::Key_Enter);
    EXPECT_EQ(selector->selectedIndex(), 1);

    selector->setSelectedIndex(0);
    selector->setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();
    EXPECT_GT(selector->itemGeometry(0).center().x(), selector->itemGeometry(1).center().x());
    QTest::keyClick(selector, Qt::Key_Left);
    QTest::keyClick(selector, Qt::Key_Space);
    EXPECT_EQ(selector->selectedIndex(), 1);

    selector->setAccessibleDescription(QStringLiteral("Primary section"));
    selector->setSelectedIndex(0);
    EXPECT_EQ(selector->accessibleDescription(), QStringLiteral("Primary section"));

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    selector->onThemeUpdated();
    EXPECT_EQ(selector->selectedIndex(), 0);
    EXPECT_TRUE(selector->itemAt(0).selected);
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    selector->onThemeUpdated();
    EXPECT_EQ(selector->selectedIndex(), 0);
}

TEST_F(SelectorBarTest, SelectionCanDriveExternalStackContentHostAndState)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    selector->addItem(
        SelectorBarItem(QStringLiteral("Inbox"), Typography::Icons::Mail, true, true, QStringLiteral("inbox")));
    selector->addItem(SelectorBarItem(QStringLiteral("Calendar"), Typography::Icons::Calendar, true, true,
        QStringLiteral("calendar")));
    selector->setFixedSize(520, 44);
    selector->anchors()->top = {window, Edge::Top, 20};
    selector->anchors()->left = {window, Edge::Left, 20};
    addAnchored(layout, selector);

    auto* host = new StackContentHost(window);
    auto* inboxPage = createPage(QStringLiteral("Inbox"));
    auto* calendarPage = createPage(QStringLiteral("Calendar"));
    host->setTransitionAnimationEnabled(false);
    host->setFixedSize(520, 180);
    host->insertPage(0, inboxPage);
    host->insertPage(1, calendarPage);
    host->anchors()->top = {selector, Edge::Bottom, 0};
    host->anchors()->left = {selector, Edge::Left, 0};
    addAnchored(layout, host);

    QString activeKey;
    QObject::connect(selector, &SelectorBar::currentChanged, host, [host](int index) {
        host->setCurrentIndex(index, 0, false);
    });
    QObject::connect(selector, &SelectorBar::selectionChanged, window, [&activeKey](int, const SelectorBarItem& item) {
        activeKey = item.data.toString();
    });
    host->setCurrentIndex(selector->selectedIndex(), 0, false);
    activeKey = selector->selectedItem().data.toString();
    showAndProcess(*window);

    EXPECT_EQ(host->parentWidget(), window);
    EXPECT_NE(inboxPage->parentWidget(), selector);
    EXPECT_EQ(host->currentIndex(), 0);
    EXPECT_EQ(activeKey, QStringLiteral("inbox"));

    selector->setSelectedIndex(1);
    QApplication::processEvents();
    EXPECT_EQ(host->currentIndex(), 1);
    EXPECT_EQ(activeKey, QStringLiteral("calendar"));
    EXPECT_FALSE(selector->findChild<StackContentHost*>());
}

TEST_F(SelectorBarTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->resize(980, 700);

    auto* title = new Label(QStringLiteral("SelectorBar"), window);
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    title->setFixedSize(180, 24);
    title->anchors()->top = {window, Edge::Top, 18};
    title->anchors()->left = {window, Edge::Left, 30};
    addAnchored(layout, title);

    auto* selector = new SelectorBar(window);
    selector->addItem(
        SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home, true, true, QStringLiteral("overview")));
    selector->addItem(SelectorBarItem(QStringLiteral("Activity"), Typography::Icons::Calendar, true, true,
        QStringLiteral("activity")));
    selector->addItem(SelectorBarItem(QStringLiteral("Sample code"), Typography::Icons::Document, true, false,
        QStringLiteral("code")));
    selector->addItem(
        SelectorBarItem(QStringLiteral("Disabled"), Typography::Icons::Lock, false, true, QStringLiteral("disabled")));
    selector->addItem(SelectorBarItem(QStringLiteral("Settings"), Typography::Icons::Settings, true, true,
        QStringLiteral("settings")));
    selector->setFixedSize(920, 44);
    selector->anchors()->top = {title, Edge::Bottom, 4};
    selector->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, selector);

    auto* host = new StackContentHost(window);
    host->setFixedSize(920, 216);
    host->insertPage(0,
        createPage(QStringLiteral("Overview"),
            QStringLiteral("SelectorBar owns the selector row; "
                           "StackContentHost owns the visible page.")));
    host->insertPage(1,
        createPage(QStringLiteral("Activity"),
            QStringLiteral("SelectionChanged can also swap external data sources.")));
    host->insertPage(2,
        createPage(QStringLiteral("Sample code"),
            QStringLiteral("This hidden selector item mimics WinUI "
                           "Gallery sample-code visibility.")));
    host->insertPage(3,
        createPage(QStringLiteral("Disabled"), QStringLiteral("Disabled items stay visible but cannot be selected.")));
    host->insertPage(4,
        createPage(QStringLiteral("Settings"),
            QStringLiteral("The page is switched externally through currentChanged.")));
    host->anchors()->top = {selector, Edge::Bottom, 0};
    host->anchors()->left = {selector, Edge::Left, 0};
    addAnchored(layout, host);
    QObject::connect(selector, &SelectorBar::currentChanged, host, [host](int index) {
        host->setCurrentIndex(index, 0, true);
    });
    host->setCurrentIndex(selector->selectedIndex(), 0, false);

    auto* compact = new SelectorBar(window);
    compact->addItem(QStringLiteral("Photos"));
    compact->addItem(QStringLiteral("Videos"));
    compact->addItem(QStringLiteral("Disabled"));
    compact->setItemEnabled(2, false);
    compact->setFixedSize(420, 44);
    compact->anchors()->top = {host, Edge::Bottom, 28};
    compact->anchors()->left = {selector, Edge::Left, 0};
    addAnchored(layout, compact);

    auto* overflowTitle = new Label(QStringLiteral("Overflow"), window);
    overflowTitle->setFluentTypography(Typography::FontRole::BodyStrong);
    overflowTitle->setFixedSize(180, 24);
    overflowTitle->anchors()->top = {compact, Edge::Top, 0};
    overflowTitle->anchors()->left = {compact, Edge::Right, 48};
    addAnchored(layout, overflowTitle);

    auto* overflow = new SelectorBar(window);
    for (int index = 0; index < 10; ++index)
        overflow->addItem(SelectorBarItem(QStringLiteral("Category %1").arg(index + 1),
            index % 2 == 0 ? Typography::Icons::Folder : Typography::Icons::Document));
    overflow->setItemVisible(3, false);
    overflow->setOverflowBehavior(SelectorBar::OverflowBehavior::MoreButton);
    overflow->setFixedSize(420, 44);
    overflow->anchors()->top = {overflowTitle, Edge::Bottom, 4};
    overflow->anchors()->left = {overflowTitle, Edge::Left, 0};
    addAnchored(layout, overflow);

    auto* switchTheme = new Button(QStringLiteral("Switch theme"), window);
    switchTheme->setFixedSize(140, 32);
    switchTheme->anchors()->top = {compact, Edge::Bottom, 28};
    switchTheme->anchors()->left = {compact, Edge::Left, 0};
    QObject::connect(switchTheme, &Button::clicked, window,
        [this, title, selector, host, compact, overflowTitle, overflow]() {
            const fluent::FluentElement::Theme next =
                fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light;
            fluent::FluentElement::setTheme(next);
            window->onThemeUpdated();
            title->onThemeUpdated();
            selector->onThemeUpdated();
            host->onThemeUpdated();
            compact->onThemeUpdated();
            overflowTitle->onThemeUpdated();
            overflow->onThemeUpdated();
        });
    addAnchored(layout, switchTheme);

    window->show();
    qApp->exec();
}
