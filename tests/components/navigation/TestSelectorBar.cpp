#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include <QApplication>
#include <QImage>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>

#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"

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
    heading->setFluentTypography(QStringLiteral("BodyStrong"));
    heading->anchors()->top = {page, Edge::Top, 18};
    heading->anchors()->left = {page, Edge::Left, 18};
    heading->anchors()->right = {page, Edge::Right, -18};
    addAnchored(layout, heading);

    auto* summary = new Label(body.isEmpty() ? QStringLiteral("External page hosted by StackContentHost and driven by SelectorBar selection.") : body, page);
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
    selector.setItemFontRole(QStringLiteral("Caption"));
    selector.setItemFontRole(QStringLiteral("Caption"));
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

    SelectorBarItem rich(QStringLiteral("Sample code"), Typography::Icons::Document, true, false, QStringLiteral("code"), QStringLiteral("Sample code page"));
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
        selector->addItem(SelectorBarItem(QStringLiteral("Category %1 with long text").arg(index), Typography::Icons::Folder));
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
    QTRY_COMPARE(selector->property("animatedIndicatorRect").toRect(), activityIndicator);
    EXPECT_EQ(activationSpy.count(), 1);
    EXPECT_EQ(selectionSpy.count(), 1);
    EXPECT_TRUE(selector->accessibleName().contains(QStringLiteral("SelectorBar")));
    EXPECT_TRUE(selector->accessibleDescription().contains(QStringLiteral("Activity timeline")));
    EXPECT_TRUE(selector->accessibleDescription().contains(QStringLiteral("Visible item count: 3")));

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

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    selector->onThemeUpdated();
    EXPECT_EQ(selector->selectedIndex(), 1);
    EXPECT_TRUE(selector->itemAt(1).selected);
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    selector->onThemeUpdated();
    EXPECT_EQ(selector->selectedIndex(), 1);
}

TEST_F(SelectorBarTest, SelectionCanDriveExternalStackContentHostAndState)
{
    using Edge = AnchorLayout::Edge;
    auto* selector = new SelectorBar(window);
    selector->addItem(SelectorBarItem(QStringLiteral("Inbox"), Typography::Icons::Mail, true, true, QStringLiteral("inbox")));
    selector->addItem(SelectorBarItem(QStringLiteral("Calendar"), Typography::Icons::Calendar, true, true, QStringLiteral("calendar")));
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
    title->setFluentTypography(QStringLiteral("BodyStrong"));
    title->setFixedSize(180, 24);
    title->anchors()->top = {window, Edge::Top, 18};
    title->anchors()->left = {window, Edge::Left, 30};
    addAnchored(layout, title);

    auto* selector = new SelectorBar(window);
    selector->addItem(SelectorBarItem(QStringLiteral("Overview"), Typography::Icons::Home, true, true, QStringLiteral("overview")));
    selector->addItem(SelectorBarItem(QStringLiteral("Activity"), Typography::Icons::Calendar, true, true, QStringLiteral("activity")));
    selector->addItem(SelectorBarItem(QStringLiteral("Sample code"), Typography::Icons::Document, true, false, QStringLiteral("code")));
    selector->addItem(SelectorBarItem(QStringLiteral("Disabled"), Typography::Icons::Lock, false, true, QStringLiteral("disabled")));
    selector->addItem(SelectorBarItem(QStringLiteral("Settings"), Typography::Icons::Settings, true, true, QStringLiteral("settings")));
    selector->setFixedSize(920, 44);
    selector->anchors()->top = {title, Edge::Bottom, 4};
    selector->anchors()->left = {title, Edge::Left, 0};
    addAnchored(layout, selector);

    auto* host = new StackContentHost(window);
    host->setFixedSize(920, 216);
    host->insertPage(0, createPage(QStringLiteral("Overview"), QStringLiteral("SelectorBar owns the selector row; StackContentHost owns the visible page.")));
    host->insertPage(1, createPage(QStringLiteral("Activity"), QStringLiteral("SelectionChanged can also swap external data sources.")));
    host->insertPage(2, createPage(QStringLiteral("Sample code"), QStringLiteral("This hidden selector item mimics WinUI Gallery sample-code visibility.")));
    host->insertPage(3, createPage(QStringLiteral("Disabled"), QStringLiteral("Disabled items stay visible but cannot be selected.")));
    host->insertPage(4, createPage(QStringLiteral("Settings"), QStringLiteral("The page is switched externally through currentChanged.")));
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
    overflowTitle->setFluentTypography(QStringLiteral("BodyStrong"));
    overflowTitle->setFixedSize(180, 24);
    overflowTitle->anchors()->top = {compact, Edge::Top, 0};
    overflowTitle->anchors()->left = {compact, Edge::Right, 48};
    addAnchored(layout, overflowTitle);

    auto* overflow = new SelectorBar(window);
    for (int index = 0; index < 10; ++index)
        overflow->addItem(SelectorBarItem(QStringLiteral("Category %1").arg(index + 1), index % 2 == 0 ? Typography::Icons::Folder : Typography::Icons::Document));
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
    QObject::connect(switchTheme, &Button::clicked, window, [this, title, selector, host, compact, overflowTitle, overflow]() {
        const fluent::FluentElement::Theme next = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light;
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

// Headless render coverage for the brand-specific item treatments. Each case sets a
// (design language, theme) pair on the global ThemeRegistry / FluentElement, grabs the SelectorBar
// to a QImage, and asserts it renders without crashing and actually paints content. Globals are
// reset in TearDown so a brand/theme never leaks into the other suites.
// zh_CN: 品牌专属条目绘制的无头渲染覆盖。每个用例设置一对(设计语言, 主题)到全局 ThemeRegistry /
// FluentElement，将 SelectorBar 抓取为 QImage，断言无崩溃且确有绘制内容。TearDown 中重置全局，
// 避免品牌/主题泄漏到其它套件。
class SelectorBarDesignLanguageTest
    : public ::testing::TestWithParam<std::pair<fluent::FluentElement::DesignLanguage,
                                                fluent::FluentElement::Theme>> {
protected:
    void SetUp() override
    {
        const auto [lang, theme] = GetParam();
        fluent::ThemeRegistry::instance().setDesignLanguage(lang);
        fluent::FluentElement::setTheme(theme);
    }

    void TearDown() override
    {
        // CRITICAL: reset globals so other suites see Fluent + Light. zh_CN: 关键：重置全局，使其它套件回到 Fluent + Light。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static SelectorBar* makeSelector()
    {
        auto* selector = new SelectorBar();
        selector->addItem(QStringLiteral("All"));
        selector->addItem(QStringLiteral("Unread"));
        selector->addItem(QStringLiteral("Flagged"));
        selector->setSelectedIndex(1);
        selector->resize(360, 40);
        return selector;
    }

    // True if any pixel differs from the top-left background pixel (i.e. something was painted).
    // zh_CN: 若有任一像素不同于左上角背景像素则为真（即确有绘制）。
    static bool hasPaintedContent(const QImage& img)
    {
        if (img.isNull() || img.width() < 2 || img.height() < 2)
            return false;
        const QRgb background = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (img.pixel(x, y) != background)
                    return true;
            }
        }
        return false;
    }
};

TEST_P(SelectorBarDesignLanguageTest, RendersItemsWithoutCrashingAndPaintsContent)
{
    std::unique_ptr<SelectorBar> selector(makeSelector());

    QImage img = selector->grab().toImage();
    ASSERT_FALSE(img.isNull());
    EXPECT_EQ(img.width(), 360);
    EXPECT_EQ(img.height(), 40);
    EXPECT_TRUE(hasPaintedContent(img)) << "Expected the SelectorBar to paint labels/segment/indicator.";
}

TEST_P(SelectorBarDesignLanguageTest, ChangingSelectionRepaintsItems)
{
    std::unique_ptr<SelectorBar> selector(makeSelector());

    const QImage first = selector->grab().toImage();
    ASSERT_FALSE(first.isNull());

    selector->setSelectedIndex(2);
    const QImage second = selector->grab().toImage();
    ASSERT_FALSE(second.isNull());

    // Selection moved the accent label/indicator/segment, so the rendered image must change.
    // zh_CN: 选中项移动了强调文字/指示条/段背景，渲染图像必须随之变化。
    EXPECT_NE(first, second);
}

INSTANTIATE_TEST_SUITE_P(
    AllDesignLanguagesAndThemes,
    SelectorBarDesignLanguageTest,
    ::testing::Values(
        std::make_pair(fluent::FluentElement::DesignFluent, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignFluent, fluent::FluentElement::Dark),
        std::make_pair(fluent::FluentElement::DesignMaterial, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignMaterial, fluent::FluentElement::Dark),
        std::make_pair(fluent::FluentElement::DesignCupertino, fluent::FluentElement::Light),
        std::make_pair(fluent::FluentElement::DesignCupertino, fluent::FluentElement::Dark)));