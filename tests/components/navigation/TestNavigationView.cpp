#include <gtest/gtest.h>

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>

#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/QMLPlus.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"
#include "components/windowing/WindowBackdrop.h"

using fluent::WidgetOwnership;
using fluent::navigation::NavigationView;
using fluent::navigation::StackContentHost;
using fluent::textfields::Label;

namespace {

void publishCompositedBackdrop(QWidget* window)
{
    fluent::windowing::BackdropState state;
    state.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    state.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    state.backend = fluent::windowing::BackdropBackend::DwmSystemBackdrop;
    state.fidelity = fluent::windowing::BackdropFidelity::Native;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    state.platformApplied = true;
    fluent::windowing::publishWindowBackdropState(window, state);
}

void publishPaintedOpaqueBackdrop(QWidget* window)
{
    fluent::windowing::BackdropState state;
    state.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    state.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    state.backend = fluent::windowing::BackdropBackend::PaintedMaterial;
    state.fidelity = fluent::windowing::BackdropFidelity::Emulated;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::PaintedOpaque;
    state.platformApplied = true;
    fluent::windowing::publishWindowBackdropState(window, state);
}

class NavigationViewTestWindow : public QWidget, public fluent::FluentElement {
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

class PaintTrackingStackContentHost final : public StackContentHost {
public:
    int paintCount() const { return m_paintCount; }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        ++m_paintCount;
        StackContentHost::paintEvent(event);
    }

private:
    int m_paintCount = 0;
};

void showAndProcess(QWidget& widget)
{
    widget.show();
    QApplication::processEvents();
}

QWidget* fixedWidget(const QSize& size)
{
    auto* widget = new QWidget();
    widget->setMinimumSize(size);
    widget->resize(size);
    return widget;
}

} // namespace

class NavigationViewTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::navigation::NavigationView::DisplayMode>(
            "fluent::navigation::NavigationView::DisplayMode");
        qRegisterMetaType<fluent::navigation::StackContentHost::TransitionEffect>(
            "fluent::navigation::StackContentHost::TransitionEffect");
    }

    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        window = new NavigationViewTestWindow();
        window->resize(920, 620);
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    NavigationViewTestWindow* window = nullptr;
};

TEST_F(NavigationViewTest, DefaultsAndPropertiesMatchShellContainer)
{
    NavigationView nav;

    EXPECT_EQ(nav.displayMode(), NavigationView::DisplayMode::Auto);
    EXPECT_TRUE(nav.isPaneOpen());
    EXPECT_EQ(nav.expandedPaneWidth(), 320);
    EXPECT_EQ(nav.compactPaneWidth(), 48);
    EXPECT_EQ(nav.topBarHeight(), 48);
    EXPECT_TRUE(nav.isAnimationEnabled());
    EXPECT_NE(nav.contentHost(), nullptr);
    EXPECT_TRUE(nav.accessibleName().isEmpty());
    EXPECT_FALSE(nav.sizeHint().isEmpty());
    EXPECT_FALSE(nav.minimumSizeHint().isEmpty());
    EXPECT_NE(dynamic_cast<QWidget*>(&nav), nullptr);
    EXPECT_NE(dynamic_cast<fluent::FluentElement*>(&nav), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(&nav), nullptr);

    QSignalSpy displaySpy(&nav, &NavigationView::displayModeChanged);
    QSignalSpy openSpy(&nav, &NavigationView::paneOpenChanged);
    QSignalSpy compactSpy(&nav, &NavigationView::compactModeThresholdWidthChanged);
    QSignalSpy expandedSpy(&nav, &NavigationView::expandedModeThresholdWidthChanged);
    QSignalSpy expandedPaneSpy(&nav, &NavigationView::expandedPaneWidthChanged);
    QSignalSpy compactPaneSpy(&nav, &NavigationView::compactPaneWidthChanged);
    QSignalSpy topBarSpy(&nav, &NavigationView::topBarHeightChanged);
    QSignalSpy animationSpy(&nav, &NavigationView::animationEnabledChanged);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setPaneOpen(false);
    nav.setPaneOpen(false);
    nav.setCompactModeThresholdWidth(500);
    nav.setCompactModeThresholdWidth(500);
    nav.setExpandedModeThresholdWidth(900);
    nav.setExpandedModeThresholdWidth(900);
    nav.setExpandedPaneWidth(280);
    nav.setExpandedPaneWidth(280);
    nav.setCompactPaneWidth(56);
    nav.setCompactPaneWidth(56);
    nav.setTopBarHeight(56);
    nav.setTopBarHeight(56);
    nav.setAnimationEnabled(false);
    nav.setAnimationEnabled(false);

    EXPECT_EQ(displaySpy.count(), 1);
    EXPECT_EQ(openSpy.count(), 1);
    EXPECT_EQ(compactSpy.count(), 1);
    EXPECT_EQ(expandedSpy.count(), 1);
    EXPECT_EQ(expandedPaneSpy.count(), 1);
    EXPECT_EQ(compactPaneSpy.count(), 1);
    EXPECT_EQ(topBarSpy.count(), 1);
    EXPECT_EQ(animationSpy.count(), 1);
}

TEST_F(NavigationViewTest, SideChromeSlotsUseHeaderMainFooterVerticalStructure)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(180, 72));
    auto* main = fixedWidget(QSize(180, 240));
    auto* footer = fixedWidget(QSize(180, 96));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);

    auto* firstPage = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, firstPage));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.resize(900, 600);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.chromeGeometry(), QRect(0, 0, 320, 600));
    EXPECT_EQ(nav.paneGeometry(), nav.chromeGeometry());
    EXPECT_EQ(nav.contentGeometry(), QRect(320, 0, 580, 600));
    EXPECT_EQ(nav.headerChromeGeometry(), QRect(0, 0, 320, 72));
    EXPECT_EQ(nav.mainChromeGeometry(), QRect(0, 72, 320, 432));
    EXPECT_EQ(nav.footerChromeGeometry(), QRect(0, 504, 320, 96));
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
    EXPECT_EQ(main->geometry(), nav.mainChromeGeometry());
    EXPECT_EQ(footer->geometry(), nav.footerChromeGeometry());
    EXPECT_EQ(nav.contentHost()->geometry(), nav.contentGeometry());
    EXPECT_EQ(firstPage->parentWidget(), nav.contentHost());
}

TEST_F(NavigationViewTest, TopChromeSlotsUseHorizontalStructureWithFooterAlignedRight)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(140, 40));
    auto* main = fixedWidget(QSize(260, 40));
    auto* footer = fixedWidget(QSize(120, 40));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.resize(900, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Top);
    EXPECT_EQ(nav.topBarGeometry(), QRect(0, 0, 900, 48));
    EXPECT_EQ(nav.contentGeometry(), QRect(0, 48, 900, 452));
    EXPECT_EQ(nav.headerChromeGeometry(), QRect(0, 0, 140, 48));
    EXPECT_EQ(nav.mainChromeGeometry(), QRect(140, 0, 260, 48));
    EXPECT_EQ(nav.footerChromeGeometry(), QRect(780, 0, 120, 48));
    EXPECT_EQ(nav.contentHost()->geometry(), nav.contentGeometry());
}

TEST_F(NavigationViewTest, AutoCompactAndMinimalModesOnlyAffectShellGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* main = fixedWidget(QSize(80, 200));
    nav.setMainChromeWidget(main);
    nav.resize(1100, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.chromeGeometry().width(), 320);
    EXPECT_EQ(nav.contentGeometry().left(), 320);

    nav.resize(700, 500);
    QApplication::processEvents();
    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 48);
    EXPECT_EQ(nav.contentGeometry().left(), 48);

    nav.resize(420, 500);
    QApplication::processEvents();
    EXPECT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::LeftMinimal);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_TRUE(nav.chromeGeometry().isEmpty());
    EXPECT_EQ(nav.contentGeometry().left(), 0);
    nav.setPaneOpen(true);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 320);
    EXPECT_EQ(nav.contentGeometry().left(), 0);
}

TEST_F(NavigationViewTest, SideModeTransitionsOnlyAnimateHorizontalGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(1, 72));
    auto* main = fixedWidget(QSize(1, 240));
    auto* footer = fixedWidget(QSize(1, 96));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    auto* page = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, page));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    nav.resize(900, 600);
    window->show();
    showAndProcess(nav);
    nav.setAnimationEnabled(true);

    const QRect compactHeader = header->geometry();
    const QRect compactMain = main->geometry();
    const QRect compactFooter = footer->geometry();
    const QRect compactContent = nav.contentHost()->geometry();

    nav.setDisplayMode(NavigationView::DisplayMode::LeftMinimal);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    const auto expectVerticalStable = [](const QRect& actual, const QRect& baseline) {
        EXPECT_EQ(actual.y(), baseline.y());
        EXPECT_EQ(actual.height(), baseline.height());
    };
    expectVerticalStable(header->geometry(), compactHeader);
    expectVerticalStable(main->geometry(), compactMain);
    expectVerticalStable(footer->geometry(), compactFooter);
    expectVerticalStable(nav.contentHost()->geometry(), compactContent);

    EXPECT_GT(header->geometry().width(), 0);
    EXPECT_LT(header->geometry().width(), compactHeader.width());
    EXPECT_LT(nav.contentHost()->geometry().left(), compactContent.left());
    EXPECT_EQ(nav.contentHost()->geometry().right(), compactContent.right());
}

TEST_F(NavigationViewTest, TopSideTransitionRevealsChromeWithoutMorphingIt)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(180, 40));
    auto* main = fixedWidget(QSize(240, 40));
    auto* footer = fixedWidget(QSize(96, 40));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.setFooterChromeWidget(footer);
    auto* page = new QWidget();
    ASSERT_TRUE(nav.contentHost()->insertPage(0, page));
    nav.contentHost()->setCurrentIndex(0, 0, false);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.resize(900, 600);
    window->show();
    showAndProcess(nav);
    nav.setAnimationEnabled(true);

    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    EXPECT_EQ(header->geometry(), QRect(-160, 0, 320, 40));
    EXPECT_EQ(main->geometry(), QRect(-160, 40, 320, 520));
    EXPECT_EQ(footer->geometry(), QRect(-160, 560, 320, 40));
    EXPECT_EQ(nav.contentHost()->geometry(), QRect(160, 24, 740, 576));

    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 1.0));
    QApplication::processEvents();
    nav.setAnimationEnabled(false);
    nav.setAnimationEnabled(true);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    QApplication::processEvents();
    ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 0.5));
    QApplication::processEvents();

    EXPECT_EQ(header->geometry(), QRect(0, -24, 180, 48));
    EXPECT_EQ(main->geometry(), QRect(180, -24, 240, 48));
    EXPECT_EQ(footer->geometry(), QRect(804, -24, 96, 48));
    EXPECT_EQ(nav.contentHost()->geometry(), QRect(160, 24, 740, 576));
}

TEST_F(NavigationViewTest, ConfigurableMetricsAndSlotReplacementUpdateGeometry)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(100, 40));
    auto* replacement = fixedWidget(QSize(100, 64));
    nav.setHeaderChromeWidget(header);
    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.setExpandedPaneWidth(280);
    nav.setCompactPaneWidth(64);
    nav.resize(900, 500);
    showAndProcess(nav);

    EXPECT_EQ(nav.chromeGeometry().width(), 280);
    EXPECT_EQ(nav.contentGeometry().left(), 280);
    EXPECT_EQ(nav.headerChromeGeometry().height(), 40);

    nav.setHeaderChromeWidget(replacement);
    QApplication::processEvents();
    EXPECT_EQ(nav.headerChromeWidget(), replacement);
    EXPECT_EQ(replacement->parentWidget(), &nav);
    EXPECT_EQ(header->parentWidget(), nullptr);
    EXPECT_EQ(nav.headerChromeGeometry().height(), 64);

    nav.setDisplayMode(NavigationView::DisplayMode::LeftCompact);
    nav.setPaneOpen(false);
    QApplication::processEvents();
    EXPECT_EQ(nav.chromeGeometry().width(), 64);
    EXPECT_EQ(nav.contentGeometry().left(), 64);

    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    nav.setTopBarHeight(56);
    QApplication::processEvents();
    EXPECT_EQ(nav.topBarGeometry().height(), 56);
    EXPECT_EQ(nav.contentGeometry().top(), 56);
}

TEST_F(NavigationViewTest, NavigationChromeExplicitReleaseAppliesConfiguredOwnership)
{
    NavigationView nav;

    QPointer<QWidget> owned = new QWidget;
    ASSERT_TRUE(nav.setHeaderChromeWidget(owned.data(), WidgetOwnership::Owned));
    EXPECT_EQ(nav.headerChromeWidgetOwnership(), WidgetOwnership::Owned);
    EXPECT_TRUE(nav.releaseHeaderChromeWidget());
    EXPECT_TRUE(owned.isNull());

    auto* borrowed = new QWidget;
    ASSERT_TRUE(nav.setMainChromeWidget(borrowed, WidgetOwnership::Borrowed));
    EXPECT_EQ(nav.mainChromeWidgetOwnership(), WidgetOwnership::Borrowed);
    EXPECT_TRUE(nav.releaseMainChromeWidget());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    delete borrowed;

    QWidget originalParent;
    auto* reparented = new QWidget(&originalParent);
    ASSERT_TRUE(nav.setFooterChromeWidget(reparented, WidgetOwnership::Reparented));
    EXPECT_EQ(reparented->parentWidget(), &nav);
    EXPECT_TRUE(nav.releaseFooterChromeWidget());
    EXPECT_EQ(reparented->parentWidget(), &originalParent);

    EXPECT_FALSE(nav.releaseHeaderChromeWidget());
    EXPECT_FALSE(nav.releaseMainChromeWidget());
    EXPECT_FALSE(nav.releaseFooterChromeWidget());
}

TEST_F(NavigationViewTest, NavigationChromeReplacementAndTakeHaveStablePolicies)
{
    QWidget originalParent;
    auto* original = new QWidget(&originalParent);
    auto* replacement = new QWidget;
    NavigationView nav;

    ASSERT_TRUE(nav.setHeaderChromeWidget(original, WidgetOwnership::Reparented));
    ASSERT_TRUE(nav.setHeaderChromeWidget(replacement, WidgetOwnership::Borrowed));
    EXPECT_EQ(original->parentWidget(), &originalParent);
    EXPECT_EQ(nav.headerChromeWidget(), replacement);
    EXPECT_EQ(nav.headerChromeWidgetOwnership(), WidgetOwnership::Borrowed);

    replacement->setProperty("fluentNavPaneFloating", true);
    EXPECT_EQ(nav.takeHeaderChromeWidget(), replacement);
    EXPECT_EQ(replacement->parentWidget(), nullptr);
    EXPECT_FALSE(replacement->property("fluentNavPaneFloating").toBool());
    delete replacement;

    auto* legacy = new QWidget;
    auto* legacyReplacement = new QWidget;
    nav.setMainChromeWidget(legacy);
    nav.setMainChromeWidget(legacyReplacement);
    EXPECT_EQ(legacy->parentWidget(), nullptr);
    EXPECT_EQ(nav.mainChromeWidget(), legacyReplacement);
    delete legacy;
    nav.setMainChromeWidget(nullptr);
    EXPECT_EQ(legacyReplacement->parentWidget(), nullptr);
    delete legacyReplacement;
}

TEST_F(NavigationViewTest, NavigationChromeDestructionHonorsExplicitOwnership)
{
    QWidget originalParent;
    QPointer<QWidget> owned = new QWidget;
    auto* borrowed = new QWidget;
    auto* reparented = new QWidget(&originalParent);
    auto* nav = new NavigationView;

    ASSERT_TRUE(nav->setHeaderChromeWidget(owned.data(), WidgetOwnership::Owned));
    ASSERT_TRUE(nav->setMainChromeWidget(borrowed, WidgetOwnership::Borrowed));
    ASSERT_TRUE(nav->setFooterChromeWidget(reparented, WidgetOwnership::Reparented));

    delete nav;

    EXPECT_TRUE(owned.isNull());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    EXPECT_EQ(reparented->parentWidget(), &originalParent);
    delete borrowed;
}

TEST_F(NavigationViewTest, NavigationChromeRejectsInvalidReuseAndTracksExternalDestruction)
{
    QWidget ancestor;
    auto* nav = new NavigationView(&ancestor);
    auto* header = new QWidget;
    ASSERT_TRUE(nav->setHeaderChromeWidget(header, WidgetOwnership::Borrowed));
    QSignalSpy headerSpy(nav, &NavigationView::headerChromeWidgetChanged);

    EXPECT_FALSE(nav->setMainChromeWidget(header, WidgetOwnership::Borrowed));
    EXPECT_FALSE(nav->setMainChromeWidget(nav, WidgetOwnership::Borrowed));
    EXPECT_FALSE(nav->setMainChromeWidget(&ancestor, WidgetOwnership::Borrowed));
    EXPECT_FALSE(nav->setMainChromeWidget(nav->contentHost(), WidgetOwnership::Borrowed));

    delete header;

    EXPECT_EQ(nav->headerChromeWidget(), nullptr);
    EXPECT_EQ(nav->headerChromeWidgetOwnership(), WidgetOwnership::Borrowed);
    EXPECT_EQ(headerSpy.count(), 1);

    delete nav;
}

TEST_F(NavigationViewTest, StackContentHostOwnsContentPagesAtApplicationLayer)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* first = new Label(QStringLiteral("First"));
    auto* second = new Label(QStringLiteral("Second"));
    ASSERT_TRUE(nav.contentHost()->insertPage(0, first));
    ASSERT_TRUE(nav.contentHost()->insertPage(1, second));
    nav.resize(760, 420);
    showAndProcess(nav);

    QSignalSpy currentSpy(nav.contentHost(), &StackContentHost::currentIndexChanged);
    nav.contentHost()->setCurrentIndex(0, 0, false);
    EXPECT_EQ(nav.contentHost()->currentIndex(), 0);
    EXPECT_EQ(first->parentWidget(), nav.contentHost());
    EXPECT_EQ(first->geometry(), QRect(QPoint(0, 0), nav.contentGeometry().size()));

    nav.contentHost()->setCurrentIndex(1, 1, false);
    EXPECT_EQ(nav.contentHost()->currentIndex(), 1);
    EXPECT_EQ(second->geometry(), QRect(QPoint(0, 0), nav.contentGeometry().size()));
    EXPECT_EQ(currentSpy.count(), 2);
}

TEST_F(NavigationViewTest, StackContentHostExplicitReleaseAppliesConfiguredOwnership)
{
    StackContentHost host;

    QPointer<QWidget> owned = new QWidget;
    ASSERT_TRUE(host.insertPage(0, owned.data(), WidgetOwnership::Owned));
    EXPECT_EQ(host.pageOwnershipAt(0), WidgetOwnership::Owned);
    EXPECT_TRUE(host.releasePage(0));
    EXPECT_TRUE(owned.isNull());

    auto* borrowed = new QWidget;
    ASSERT_TRUE(host.insertPage(0, borrowed, WidgetOwnership::Borrowed));
    EXPECT_EQ(host.pageOwnershipAt(0), WidgetOwnership::Borrowed);
    EXPECT_TRUE(host.releasePage(0));
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    delete borrowed;

    QWidget originalParent;
    auto* reparented = new QWidget(&originalParent);
    ASSERT_TRUE(host.insertPage(0, reparented, WidgetOwnership::Reparented));
    EXPECT_EQ(reparented->parentWidget(), &host);
    EXPECT_TRUE(host.releasePage(0));
    EXPECT_EQ(reparented->parentWidget(), &originalParent);

    EXPECT_FALSE(host.releasePage(-1));
    EXPECT_FALSE(host.releasePage(0));
    EXPECT_EQ(host.pageOwnershipAt(0), WidgetOwnership::Borrowed);
}

TEST_F(NavigationViewTest, StackContentHostReplacementAppliesOldOwnershipAndRecordsNewPolicy)
{
    QWidget originalParent;
    auto* original = new QWidget(&originalParent);
    auto* replacement = new QWidget;
    StackContentHost host;

    ASSERT_TRUE(host.insertPage(0, original, WidgetOwnership::Reparented));
    ASSERT_TRUE(host.replacePage(0, replacement, WidgetOwnership::Borrowed));

    EXPECT_EQ(original->parentWidget(), &originalParent);
    EXPECT_EQ(host.pageWidget(0), replacement);
    EXPECT_EQ(host.pageOwnershipAt(0), WidgetOwnership::Borrowed);
    EXPECT_TRUE(host.releasePage(0));
    EXPECT_EQ(replacement->parentWidget(), nullptr);
    delete replacement;
}

TEST_F(NavigationViewTest, StackContentHostDestructionHonorsExplicitOwnership)
{
    QWidget originalParent;
    QPointer<QWidget> owned = new QWidget;
    auto* borrowed = new QWidget;
    auto* reparented = new QWidget(&originalParent);
    auto* host = new StackContentHost;

    ASSERT_TRUE(host->insertPage(0, owned.data(), WidgetOwnership::Owned));
    ASSERT_TRUE(host->insertPage(1, borrowed, WidgetOwnership::Borrowed));
    ASSERT_TRUE(host->insertPage(2, reparented, WidgetOwnership::Reparented));

    delete host;

    EXPECT_TRUE(owned.isNull());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    EXPECT_EQ(reparented->parentWidget(), &originalParent);
    delete borrowed;
}

TEST_F(NavigationViewTest, StackContentHostTransferApisPreserveLegacyBehavior)
{
    QWidget originalParent;
    auto* taken = new QWidget(&originalParent);
    StackContentHost host;
    ASSERT_TRUE(host.insertPage(0, taken, WidgetOwnership::Reparented));

    EXPECT_EQ(host.takePage(0), taken);
    EXPECT_EQ(taken->parentWidget(), nullptr);
    delete taken;

    auto* replaced = new QWidget;
    auto* replacement = new QWidget;
    ASSERT_TRUE(host.insertPage(0, replaced));
    EXPECT_EQ(host.replacePage(0, replacement), replaced);
    EXPECT_EQ(replaced->parentWidget(), nullptr);
    delete replaced;

    host.clearPages();
    EXPECT_EQ(replacement->parentWidget(), nullptr);
    EXPECT_EQ(host.count(), 0);
    delete replacement;
}

TEST_F(NavigationViewTest, StackContentHostReleaseAllAppliesPerPagePolicies)
{
    QWidget originalParent;
    QPointer<QWidget> owned = new QWidget;
    auto* borrowed = new QWidget;
    auto* reparented = new QWidget(&originalParent);
    StackContentHost host;

    ASSERT_TRUE(host.insertPage(0, owned.data(), WidgetOwnership::Owned));
    ASSERT_TRUE(host.insertPage(1, borrowed, WidgetOwnership::Borrowed));
    ASSERT_TRUE(host.insertPage(2, reparented, WidgetOwnership::Reparented));
    host.setCurrentIndex(1, 0, false);
    QSignalSpy currentSpy(&host, &StackContentHost::currentIndexChanged);

    host.releaseAllPages();

    EXPECT_TRUE(owned.isNull());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    EXPECT_EQ(reparented->parentWidget(), &originalParent);
    EXPECT_EQ(host.count(), 0);
    EXPECT_EQ(host.currentIndex(), -1);
    EXPECT_EQ(currentSpy.count(), 1);
    delete borrowed;
}

TEST_F(NavigationViewTest, StackContentHostRejectsCyclesDuplicatesAndTracksExternalDestruction)
{
    QWidget ancestor;
    auto* host = new StackContentHost(&ancestor);
    auto* first = new QWidget;
    auto* second = new QWidget;

    EXPECT_FALSE(host->insertPage(0, host, WidgetOwnership::Borrowed));
    EXPECT_FALSE(host->insertPage(0, &ancestor, WidgetOwnership::Borrowed));
    ASSERT_TRUE(host->insertPage(0, first, WidgetOwnership::Borrowed));
    EXPECT_FALSE(host->insertPage(1, first, WidgetOwnership::Owned));
    ASSERT_TRUE(host->insertPage(1, second, WidgetOwnership::Borrowed));
    host->setCurrentIndex(0, 0, false);
    QSignalSpy currentSpy(host, &StackContentHost::currentIndexChanged);

    delete first;

    EXPECT_EQ(host->count(), 1);
    EXPECT_EQ(host->pageWidget(0), second);
    EXPECT_EQ(host->currentIndex(), 0);
    EXPECT_EQ(currentSpy.count(), 0);
    EXPECT_EQ(host->indexOf(second), 0);
    EXPECT_EQ(host->indexOf(nullptr), -1);

    delete host;
    EXPECT_EQ(second->parentWidget(), nullptr);
    delete second;
}

TEST_F(NavigationViewTest, StackContentHostClearsTranslucentBackdropPixels)
{
    StackContentHost host;
    host.setAttribute(Qt::WA_TranslucentBackground, true);
    publishCompositedBackdrop(&host);
    host.resize(240, 160);

    QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 255, 255));

    QPainter painter(&image);
    host.render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
    painter.end();

    EXPECT_EQ(image.pixelColor(host.rect().center()).alpha(), 0)
        << "A translucent content host must replace pixels left by the outgoing page";
}

TEST_F(NavigationViewTest, StackContentHostLeavesPaintedBackdropUncoveredWithoutSurface)
{
    StackContentHost host;
    publishPaintedOpaqueBackdrop(&host);
    host.resize(240, 160);

    QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    host.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    EXPECT_EQ(image.pixelColor(host.rect().center()).alpha(), 0)
        << "A host without an explicit surface must not cover the parent material";
}

TEST_F(NavigationViewTest, NestedStackContentHostPreservesLocallyThemedParentSurface)
{
    class ThemedSurface : public QWidget, public fluent::FluentElement {
    public:
        using QWidget::QWidget;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), themeColorsRef().bgCanvas);
        }
    };

    QWidget window;
    window.setAttribute(Qt::WA_TranslucentBackground, true);
    publishCompositedBackdrop(&window);
    window.resize(280, 200);
    ThemedSurface surface(&window);
    surface.setGeometry(10, 10, 260, 180);
    StackContentHost host(&surface);
    host.setGeometry(10, 10, 240, 160);
    ASSERT_TRUE(host.insertPage(0, new QWidget));
    ASSERT_TRUE(host.insertPage(1, new QWidget));
    host.setCurrentIndex(0, 0, false);

    for (const char* theme : {"Light", "Dark", "Light"}) {
        surface.setProperty("fluentThemeOverride", theme);
        for (int page : {0, 1}) {
            host.setCurrentIndex(page, 0, false);
            QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::magenta);
            window.render(&image);
            EXPECT_EQ(image.pixelColor(host.mapTo(&window, host.rect().center())), surface.themeColorsRef().bgCanvas)
                << "An embedded transparent host must preserve its parent's local " << theme
                << " surface when switching pages";
        }
    }
}

TEST_F(NavigationViewTest, StackContentHostUsesDefaultLayerOutsideMaterialBackdrops)
{
    StackContentHost host;
    host.resize(240, 160);

    QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    host.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    EXPECT_EQ(image.pixelColor(host.rect().center()), QColor("#FFFFFF"));
}

TEST_F(NavigationViewTest, NavigationViewConfiguresOverlayAcrossBackdropModes)
{
    NavigationView nav;
    nav.setAnimationEnabled(false);
    nav.setAttribute(Qt::WA_TranslucentBackground, true);
    nav.resize(240, 160);
    showAndProcess(nav);

    auto verifyOverlay = [&nav]() {
        StackContentHost* host = nav.contentHost();
        ASSERT_NE(host, nullptr);
        ASSERT_FALSE(host->size().isEmpty());

        QImage image(host->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        host->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
        painter.end();

        const QColor pixel = image.pixelColor(host->rect().center());
        EXPECT_EQ(pixel.alpha(), 128);
        EXPECT_NEAR(pixel.red(), 255, 1);
        EXPECT_NEAR(pixel.green(), 255, 1);
        EXPECT_NEAR(pixel.blue(), 255, 1);
    };

    publishCompositedBackdrop(&nav);
    verifyOverlay();

    publishPaintedOpaqueBackdrop(&nav);
    verifyOverlay();
}

TEST_F(NavigationViewTest, StackContentHostCoalescesBackdropClearAcrossRapidSwitches)
{
    PaintTrackingStackContentHost host;
    host.setAttribute(Qt::WA_TranslucentBackground, true);
    publishCompositedBackdrop(&host);
    host.resize(240, 160);

    auto* first = new QWidget;
    auto* second = new QWidget;
    ASSERT_TRUE(host.insertPage(0, first));
    ASSERT_TRUE(host.insertPage(1, second));
    host.setCurrentIndex(0, 0, false);
    showAndProcess(host);
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
    ASSERT_TRUE(QTest::qWaitFor([&host] { return host.paintCount() > 0; }));

    const int paintsBeforeSwitch = host.paintCount();
    for (int i = 0; i < 100; ++i)
        host.setCurrentIndex(i % 2, 0, false);

    EXPECT_EQ(host.paintCount(), paintsBeforeSwitch)
        << "Rapid navigation must not synchronously re-enter backing-store painting";
    EXPECT_FALSE(host.busy());
    EXPECT_TRUE(QTest::qWaitFor([&host, paintsBeforeSwitch] {
        return host.paintCount() > paintsBeforeSwitch;
    })) << "The coalesced frame must still replace the composited backdrop";
    EXPECT_EQ(host.currentIndex(), 1);
    EXPECT_TRUE(second->isVisible());
    EXPECT_FALSE(first->isVisible());
}

TEST_F(NavigationViewTest, StackContentHostTransitionEffectControlsIncomingOffset)
{
    StackContentHost host;
    host.resize(420, 320);

    auto* first = new Label(QStringLiteral("First"));
    auto* second = new Label(QStringLiteral("Second"));
    ASSERT_TRUE(host.insertPage(0, first));
    ASSERT_TRUE(host.insertPage(1, second));
    host.setCurrentIndex(0, 0, false);
    showAndProcess(host);

    EXPECT_EQ(host.transitionEffect(), StackContentHost::TransitionEffect::SlideFromLeft);

    QSignalSpy effectSpy(&host, &StackContentHost::transitionEffectChanged);
    host.setTransitionEffect(StackContentHost::TransitionEffect::SlideFromBottom);
    host.setTransitionEffect(StackContentHost::TransitionEffect::SlideFromBottom);
    EXPECT_EQ(effectSpy.count(), 1);

    host.setCurrentIndex(1, 0, true);
    EXPECT_EQ(host.currentIndex(), 1);
    EXPECT_TRUE(host.busy());
    EXPECT_TRUE(second->isVisible());
    EXPECT_EQ(second->geometry().size(), host.rect().size());
    EXPECT_EQ(second->geometry().left(), 0);
    EXPECT_GT(second->geometry().top(), 0);
    EXPECT_LT(second->geometry().top(), host.height());

    ASSERT_TRUE(QTest::qWaitFor([&] { return !host.busy(); }, 1000));
    EXPECT_EQ(second->geometry(), host.rect());
}

TEST_F(NavigationViewTest, StackContentHostLocalAndGlobalMotionPoliciesSettleTransitions)
{
    StackContentHost host;
    host.resize(420, 320);

    auto* first = new Label(QStringLiteral("First"));
    auto* second = new Label(QStringLiteral("Second"));
    ASSERT_TRUE(host.insertPage(0, first));
    ASSERT_TRUE(host.insertPage(1, second));
    host.setCurrentIndex(0, 0, false);
    showAndProcess(host);

    host.setCurrentIndex(1, 0, true);
    ASSERT_TRUE(host.busy());
    host.setTransitionAnimationEnabled(false);
    EXPECT_FALSE(host.busy());
    EXPECT_EQ(second->geometry(), host.rect());
    EXPECT_TRUE(second->isVisible());
    EXPECT_FALSE(first->isVisible());

    host.setTransitionAnimationEnabled(true);
    host.setCurrentIndex(0, 0, false);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    QSignalSpy busySpy(&host, &StackContentHost::busyChanged);

    host.setCurrentIndex(1, 0, true);

    EXPECT_FALSE(host.busy());
    EXPECT_EQ(second->geometry(), host.rect());
    EXPECT_TRUE(second->isVisible());
    EXPECT_FALSE(first->isVisible());
    ASSERT_EQ(busySpy.count(), 2);
    EXPECT_TRUE(busySpy.at(0).at(0).toBool());
    EXPECT_FALSE(busySpy.at(1).at(0).toBool());
}

TEST_F(NavigationViewTest, ThemeRefreshKeepsShellAndSlotsStable)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = fixedWidget(QSize(120, 48));
    auto* main = fixedWidget(QSize(120, 160));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(main);
    nav.resize(900, 500);
    showAndProcess(nav);

    const QRect chromeRect = nav.chromeGeometry();
    const QRect contentRect = nav.contentGeometry();

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    nav.onThemeUpdated();
    QApplication::processEvents();

    EXPECT_EQ(nav.chromeGeometry(), chromeRect);
    EXPECT_EQ(nav.contentGeometry(), contentRect);
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
    EXPECT_EQ(main->geometry(), nav.mainChromeGeometry());
}

TEST_F(NavigationViewTest, ChromeSizeHintChangesRelayoutAfterLeftToTopSwitch)
{
    NavigationView nav(window);
    nav.setAnimationEnabled(false);
    auto* header = new QWidget;
    auto* row = new QBoxLayout(QBoxLayout::TopToBottom, header);
    row->setSizeConstraint(QLayout::SetNoConstraint);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(fixedWidget(QSize(72, 44)));
    row->addWidget(fixedWidget(QSize(72, 44)));
    nav.setHeaderChromeWidget(header);
    nav.setMainChromeWidget(fixedWidget(QSize(80, 44)));
    nav.setFooterChromeWidget(fixedWidget(QSize(60, 48)));
    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    nav.resize(900, 520);
    showAndProcess(*window);

    ASSERT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_EQ(nav.headerChromeGeometry().size(), QSize(nav.expandedPaneWidth(), 88));
    nav.setDisplayMode(NavigationView::DisplayMode::Top);
    QApplication::processEvents();
    ASSERT_EQ(nav.effectiveDisplayMode(), NavigationView::DisplayMode::Top);
    EXPECT_EQ(nav.headerChromeGeometry().width(), 72);

    row->setDirection(QBoxLayout::LeftToRight);
    ASSERT_TRUE(QTest::qWaitFor([&] { return nav.headerChromeGeometry().width() == 144; }, 1000));
    EXPECT_EQ(nav.headerChromeGeometry().height(), nav.topBarHeight());
    EXPECT_EQ(header->geometry(), nav.headerChromeGeometry());
    EXPECT_EQ(nav.contentGeometry().topLeft(), QPoint(0, nav.topBarHeight()));
}

TEST(NavigationViewTransitionTest, ClosedModeChangesDoNotCreateFlyoutSurfaces)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    QWidget window;
    window.resize(800, 600);
    NavigationView nav(&window);
    nav.resize(window.size());
    QWidget pane;
    nav.setMainChromeWidget(&pane);
    window.show();
    QApplication::processEvents();

    using Mode = NavigationView::DisplayMode;
    for (auto from : {Mode::Left, Mode::LeftCompact, Mode::LeftMinimal, Mode::Top}) {
        for (auto to : {Mode::Left, Mode::LeftCompact, Mode::LeftMinimal, Mode::Top}) {
            if (from == to)
                continue;
            SCOPED_TRACE(::testing::Message() << "from=" << int(from) << " to=" << int(to));
            nav.setAnimationEnabled(false);
            nav.setDisplayMode(from);
            nav.setPaneOpen(from == Mode::Left || from == Mode::Top);
            QApplication::processEvents();
            nav.setAnimationEnabled(true);
            // Match the Gallery sample: set the pane state before changing mode.
            nav.setPaneOpen(to == Mode::Left || to == Mode::Top);
            nav.setDisplayMode(to);
            auto* animation = nav.findChild<QPropertyAnimation*>();
            ASSERT_NE(animation, nullptr);
            animation->pause();
            for (qreal progress : {0.0, 0.01, 0.25, 0.49, 0.5, 0.75, 0.99, 1.0}) {
                SCOPED_TRACE(progress);
                ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", progress));
                EXPECT_FALSE(pane.property("fluentNavPaneFloating").toBool());
            }
        }
    }
}

TEST(NavigationViewTransitionTest, RequestedFlyoutsRetainTheirSurfaceDuringOpeningAndClosing)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    QWidget window;
    window.resize(800, 600);
    NavigationView nav(&window);
    nav.resize(window.size());
    QWidget pane;
    nav.setMainChromeWidget(&pane);
    window.show();
    QApplication::processEvents();

    for (auto mode : {NavigationView::DisplayMode::LeftCompact, NavigationView::DisplayMode::LeftMinimal}) {
        nav.setAnimationEnabled(false);
        nav.setDisplayMode(mode);
        nav.setPaneOpen(false);
        QApplication::processEvents();
        nav.setAnimationEnabled(true);
        for (bool open : {true, false}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << int(mode) << " open=" << open);
            nav.setPaneOpen(open);
            nav.findChild<QPropertyAnimation*>()->pause();
            for (qreal progress : {0.1, 0.49, 0.5, 0.9}) {
                ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", progress));
                EXPECT_TRUE(pane.property("fluentNavPaneFloating").toBool());
            }
            ASSERT_TRUE(nav.setProperty("layoutTransitionProgress", 1.0));
            EXPECT_EQ(pane.property("fluentNavPaneFloating").toBool(), open);
            nav.setAnimationEnabled(false);
            QApplication::processEvents();
            nav.setAnimationEnabled(true);
        }
        nav.setAnimationEnabled(false);
        nav.setPaneOpen(true);
        QTest::keyClick(&pane, Qt::Key_Escape);
        EXPECT_FALSE(nav.isPaneOpen());
        EXPECT_FALSE(pane.property("fluentNavPaneFloating").toBool());
    }
}

TEST(NavigationViewTransitionTest, ReversingPaneMotionStartsFromVisibleGeometry)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    QWidget window;
    window.resize(800, 600);
    NavigationView nav(&window);
    nav.resize(window.size());
    QWidget pane;
    nav.setMainChromeWidget(&pane);
    window.show();
    QApplication::processEvents();
    auto* animation = nav.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);
    for (auto mode : {NavigationView::DisplayMode::Left, NavigationView::DisplayMode::LeftCompact,
             NavigationView::DisplayMode::LeftMinimal}) {
        for (bool initiallyOpen : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "mode=" << int(mode) << " initiallyOpen=" << initiallyOpen);
            nav.setAnimationEnabled(false);
            nav.setDisplayMode(mode);
            nav.setPaneOpen(initiallyOpen);
            QApplication::processEvents();
            const QRect initialPane = pane.geometry();
            nav.setAnimationEnabled(true);
            nav.setPaneOpen(!initiallyOpen);
            animation->setCurrentTime(animation->duration() * 2 / 5);
            const QRect visiblePane = pane.geometry();
            const QRect visibleContent = nav.contentHost()->geometry();
            nav.setPaneOpen(initiallyOpen);
            EXPECT_EQ(pane.geometry(), visiblePane);
            EXPECT_EQ(nav.contentHost()->geometry(), visibleContent);
            animation->setCurrentTime(1);
            EXPECT_NEAR(pane.width(), visiblePane.width(), 2);
            EXPECT_NEAR(nav.contentHost()->x(), visibleContent.x(), 2);
            animation->setCurrentTime(animation->duration());
            EXPECT_EQ(nav.isPaneOpen(), initiallyOpen);
            if (!pane.isHidden())
                EXPECT_EQ(pane.geometry(), initialPane);
        }
    }
}

TEST(NavigationViewTransitionTest, LayoutMotionUsesBalancedThemeEasing)
{
    auto& policy = fluent::MotionPolicy::instance();
    QWidget window;
    window.resize(800, 600);
    NavigationView nav(&window);
    nav.resize(window.size());
    nav.setAnimationEnabled(false);
    nav.setDisplayMode(NavigationView::DisplayMode::Left);
    window.show();
    QApplication::processEvents();
    auto* animation = nav.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);
    for (auto mode :
        {fluent::MotionPolicy::Mode::Full, fluent::MotionPolicy::Mode::Reduced, fluent::MotionPolicy::Mode::Disabled}) {
        nav.setAnimationEnabled(false);
        nav.setPaneOpen(true);
        policy.setMode(mode);
        nav.setAnimationEnabled(true);
        nav.setPaneOpen(false);
        if (mode == fluent::MotionPolicy::Mode::Disabled) {
            EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
            EXPECT_EQ(nav.contentHost()->x(), nav.compactPaneWidth());
        } else {
            EXPECT_EQ(animation->easingCurve(), nav.themeAnimation().standard);
            EXPECT_EQ(animation->duration(), policy.resolvedDuration(nav.themeAnimation().normal));
            animation->setCurrentTime(animation->duration());
        }
    }
    policy.setMode(fluent::MotionPolicy::Mode::Full);
}
