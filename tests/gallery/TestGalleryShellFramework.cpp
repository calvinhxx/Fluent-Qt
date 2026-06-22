#include <gtest/gtest.h>

#include <QApplication>
#include <QEvent>
#include <QElapsedTimer>
#include <QFrame>
#include <QHelpEvent>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTest>
#include <QtGlobal>

#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/collections/TreeView.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/navigation/NavigationView.h"
#include "components/scrolling/ScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/status_info/ToolTip.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "view/pages/GalleryContentPage.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "view/widgets/GalleryEntryCard.h"
#include "VisualGeometryTestUtils.h"
#include "view/shell/GalleryWindow.h"
#include "view/pages/SettingsPage.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

using fluent::basicinput::Button;
using fluent::basicinput::ComboBox;
using fluent::collections::TreeView;
using fluent::dialogs_flyouts::Popup;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryEntryCard;
using fluent::gallery::GalleryNavigationPane;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GallerySettings;
using fluent::gallery::GalleryWindow;
using fluent::gallery::SettingsPage;
using fluent::navigation::NavigationView;
using fluent::scrolling::ScrollView;
using fluent::status_info::ToolTip;
using fluent::textfields::AutoSuggestBox;
using fluent::windowing::TitleBar;
namespace vg = fluent::testutils::visual_geometry;

namespace {

class GallerySettingsRestorer {
public:
    explicit GallerySettingsRestorer(GallerySettings& settings)
        : m_settings(settings)
        , m_themeMode(settings.themeMode())
        , m_navigationStyle(settings.navigationStyle())
    {
    }

    ~GallerySettingsRestorer()
    {
        m_settings.setNavigationStyle(m_navigationStyle);
        m_settings.setThemeMode(m_themeMode);
    }

private:
    GallerySettings& m_settings;
    GallerySettings::ThemeMode m_themeMode;
    GallerySettings::NavigationStyle m_navigationStyle;
};

bool containsAll(const QStringList& values, const QStringList& expectedValues)
{
    for (const QString& expectedValue : expectedValues) {
        if (!values.contains(expectedValue))
            return false;
    }
    return true;
}

QRect mappedGeometry(const QWidget* widget, const QWidget* ancestor)
{
    return QRect(widget->mapTo(const_cast<QWidget*>(ancestor), QPoint(0, 0)), widget->size());
}

::testing::AssertionResult centerYWithinAncestor(const QWidget* widget,
                                                 const QWidget* ancestor,
                                                 int tolerance = 1)
{
    if (!widget || !ancestor) {
        return ::testing::AssertionFailure()
            << "Cannot compare mapped centerY with null widget";
    }

    const int actual = mappedGeometry(widget, ancestor).center().y();
    const int expected = ancestor->rect().center().y();
    if (qAbs(actual - expected) <= qMax(0, tolerance))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Expected mapped centerY within " << tolerance
        << " px. actual=" << actual
        << " expected=" << expected;
}

::testing::AssertionResult visibleWithinAncestor(const QWidget* widget, const QWidget* ancestor)
{
    if (!widget || !ancestor)
        return ::testing::AssertionFailure() << "Cannot compare visible area with null widget";
    if (!widget->isVisibleTo(const_cast<QWidget*>(ancestor)))
        return ::testing::AssertionFailure() << widget->objectName().toStdString() << " is not visible";

    const QRect mappedRect = mappedGeometry(widget, ancestor);
    const QRect visibleRect = mappedRect.intersected(ancestor->rect());
    if (!visibleRect.isEmpty() && visibleRect.height() >= widget->height() / 2)
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << widget->objectName().toStdString()
        << " is clipped outside ancestor. mapped="
        << mappedRect.x() << "," << mappedRect.y() << " "
        << mappedRect.width() << "x" << mappedRect.height()
        << " ancestor=" << ancestor->rect().width() << "x" << ancestor->rect().height();
}

TreeView* navigationTree(GalleryNavigationPane* pane)
{
    return pane ? pane->findChild<TreeView*>() : nullptr;
}

void settleTreeAnimations()
{
    QApplication::processEvents();
    QTest::qWait(360);
    QApplication::processEvents();
}

void settleNavigationViewAnimation()
{
    QApplication::processEvents();
    QTest::qWait(320);
    QApplication::processEvents();
}

::testing::AssertionResult routeVisibleInTree(GalleryNavigationPane* pane, const QString& routeId)
{
    TreeView* tree = navigationTree(pane);
    if (!tree)
        return ::testing::AssertionFailure() << "Missing navigation TreeView";

    const QModelIndex index = pane->indexForRouteId(routeId);
    if (!index.isValid())
        return ::testing::AssertionFailure() << "Missing route index: " << routeId.toStdString();

    const QRect visualRect = tree->visualRect(index);
    if (!visualRect.isEmpty() && tree->viewport()->rect().intersects(visualRect))
        return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
        << "Route is not visible: " << routeId.toStdString()
        << " visual=" << visualRect.x() << "," << visualRect.y()
        << " " << visualRect.width() << "x" << visualRect.height();
}

void clickNavigationRoute(GalleryNavigationPane* pane, const QString& routeId)
{
    TreeView* tree = navigationTree(pane);
    ASSERT_NE(tree, nullptr);
    const QModelIndex index = pane->indexForRouteId(routeId);
    ASSERT_TRUE(index.isValid()) << routeId.toStdString();
    QModelIndex parentIndex = index.parent();
    while (parentIndex.isValid()) {
        tree->expand(parentIndex);
        parentIndex = parentIndex.parent();
    }
    settleTreeAnimations();
    const QRect visualRect = tree->visualRect(index);
    ASSERT_FALSE(visualRect.isEmpty()) << routeId.toStdString();
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, visualRect.center());
    settleTreeAnimations();
}

} // namespace

class GalleryShellFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto& settings = GallerySettings::instance();
        m_themeMode = settings.themeMode();
        m_navigationStyle = settings.navigationStyle();
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        settings.setNavigationStyle(GallerySettings::NavigationStyle::Auto);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        auto& settings = GallerySettings::instance();
        settings.setNavigationStyle(m_navigationStyle);
        settings.setThemeMode(m_themeMode);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

private:
    GallerySettings::ThemeMode m_themeMode = GallerySettings::ThemeMode::System;
    GallerySettings::NavigationStyle m_navigationStyle = GallerySettings::NavigationStyle::Auto;
};

TEST_F(GalleryShellFrameworkTest, WindowConstructsInitialHomeContentPage)
{
    GalleryWindow window;

    EXPECT_EQ(window.objectName(), QStringLiteral("galleryWindow"));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("WinUI 3 Gallery"));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("home"));
    EXPECT_NE(window.findChild<QWidget*>(QStringLiteral("galleryNavigationView")), nullptr);
    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    auto* footerPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryFooterNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    ASSERT_NE(footerPane, nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(mainPane), nullptr);
    EXPECT_EQ(mainPane->selectedRouteId(), QStringLiteral("home"));
    EXPECT_EQ(footerPane->selectedRouteId(), QStringLiteral("home"));

    auto* searchBox = window.findChild<AutoSuggestBox*>(QStringLiteral("GalleryTitleBar.SearchBox"));
    ASSERT_NE(searchBox, nullptr);
    EXPECT_EQ(searchBox->placeholderText(), QStringLiteral("Search controls and samples..."));

    // Home now resolves to a real content page rather than a placeholder.
    GalleryContentPage* page = window.currentContentPage();
    ASSERT_NE(page, nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(page), nullptr);
    EXPECT_EQ(page->routeId(), QStringLiteral("home"));
    ASSERT_NE(page->titleLabel(), nullptr);
    EXPECT_EQ(page->titleLabel()->text(), QStringLiteral("Home"));
    EXPECT_EQ(window.currentPlaceholderPage(), nullptr);
}

TEST_F(GalleryShellFrameworkTest, ContentPageUsesFloatingVisibleVerticalScrollbar)
{
    GalleryWindow window;
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));

    auto* scrollView = window.findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    ASSERT_NE(scrollView, nullptr);
    EXPECT_EQ(scrollView->verticalScrollBarVisibility(), ScrollView::ScrollBarVisibility::Visible);
    EXPECT_EQ(scrollView->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);

    auto* floatingBar = scrollView->viewport()->findChild<fluent::scrolling::ScrollBar*>(
        QStringLiteral("fluentScrollViewFloatingVerticalBar"));
    ASSERT_NE(floatingBar, nullptr);
    EXPECT_EQ(floatingBar->orientation(), Qt::Vertical);
    EXPECT_EQ(floatingBar->parentWidget(), scrollView->viewport());
}

TEST_F(GalleryShellFrameworkTest, ClickingHomeFeaturedCardNavigatesWithoutUseAfterFree)
{
    // Regression: a featured card triggers navigation from inside its own
    // mouseReleaseEvent. Navigation replaces and frees the home page, which is the
    // card's ancestor and is still dispatching the event. Freeing it synchronously was a
    // use-after-free crash (SIGSEGV in QApplication::notify); the page must be deferred.
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")) == nullptr,
        2000);

    GalleryEntryCard* card = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((card = window.findChild<GalleryEntryCard*>()) != nullptr, 2000);
    const QString targetRouteId = card->targetRouteId();
    ASSERT_FALSE(targetRouteId.isEmpty());

    QPointer<GalleryContentPage> homePage = window.currentContentPage();
    ASSERT_NE(homePage.data(), nullptr);
    QPointer<GalleryEntryCard> cardGuard = card;

    GalleryContentPage* homeRaw = homePage.data();

    // Deliver a real click: this synchronously re-enters navigation and replaces the page
    // that owns `card`. Surviving to the next statement is itself the crash assertion.
    QTest::mouseClick(card, Qt::LeftButton, Qt::NoModifier, card->rect().center());
    EXPECT_EQ(window.currentRouteId(), targetRouteId);

    // The previous page (and its card) must still be alive immediately after the event...
    EXPECT_FALSE(homePage.isNull());
    EXPECT_FALSE(cardGuard.isNull());

    // ...and, because pages are now cached for reuse (built once, swapped as the model drives
    // navigation) rather than rebuilt per click, the home page is retained — not deleted —
    // even after the event loop drains deferred deletes.
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_FALSE(homePage.isNull());
    EXPECT_FALSE(cardGuard.isNull());

    GalleryContentPage* newPage = window.currentContentPage();
    ASSERT_NE(newPage, nullptr);
    EXPECT_EQ(newPage->routeId(), targetRouteId);

    // Navigating back to home reuses the cached page instance instead of constructing a new one.
    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    clickNavigationRoute(mainPane, QStringLiteral("home"));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("home"));
    EXPECT_EQ(window.currentContentPage(), homeRaw);
}

TEST_F(GalleryShellFrameworkTest, TitleBarContentUsesAnchorsAndCentersControls)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")) == nullptr,
        2000);

    TitleBar* titleBar = window.titleBar();
    ASSERT_NE(titleBar, nullptr);
    EXPECT_EQ(titleBar->titleBarHeight(), fluent::gallery::metrics::TitleBar::Height);

    EXPECT_NE(qobject_cast<fluent::AnchorLayout*>(titleBar->layout()), nullptr);

    const QStringList centeredWidgetNames{
        QStringLiteral("GalleryTitleBar.BackButton"),
        QStringLiteral("GalleryTitleBar.MenuButton"),
        QStringLiteral("GalleryTitleBar.AppIcon"),
        QStringLiteral("GalleryTitleBar.Title"),
        QStringLiteral("GalleryTitleBar.SearchBox")
    };
    vg::maybeDumpNamedWidgets(titleBar, centeredWidgetNames);

    auto* backButton = vg::findRequiredChild<Button>(titleBar, QStringLiteral("GalleryTitleBar.BackButton"));
    auto* menuButton = vg::findRequiredChild<Button>(titleBar, QStringLiteral("GalleryTitleBar.MenuButton"));
    auto* appIcon = vg::findRequiredChild<QLabel>(titleBar, QStringLiteral("GalleryTitleBar.AppIcon"));
    auto* title = vg::findRequiredChild<QWidget>(titleBar, QStringLiteral("GalleryTitleBar.Title"));
    auto* searchBox = vg::findRequiredChild<AutoSuggestBox>(titleBar, QStringLiteral("GalleryTitleBar.SearchBox"));
    ASSERT_NE(backButton, nullptr);
    ASSERT_NE(menuButton, nullptr);
    ASSERT_NE(appIcon, nullptr);
    ASSERT_NE(title, nullptr);
    ASSERT_NE(searchBox, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(searchBox->isVisible(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(menuButton->isEnabled(), 1000);

    EXPECT_EQ(backButton->parentWidget(), titleBar);
    EXPECT_EQ(menuButton->parentWidget(), titleBar);
    EXPECT_EQ(appIcon->parentWidget(), titleBar);
    EXPECT_EQ(title->parentWidget(), titleBar);

    EXPECT_TRUE(centerYWithinAncestor(backButton, titleBar, 1));
    EXPECT_TRUE(centerYWithinAncestor(menuButton, titleBar, 1));
    EXPECT_TRUE(centerYWithinAncestor(appIcon, titleBar, 1));
    EXPECT_TRUE(centerYWithinAncestor(title, titleBar, 1));
    EXPECT_TRUE(vg::centerYWithin(searchBox, titleBar, 1));

    EXPECT_EQ(backButton->height(), 24);
    EXPECT_EQ(backButton->width(), 0);
    EXPECT_EQ(backButton->font().pixelSize(), Typography::FontSize::Caption);
    EXPECT_EQ(backButton->iconOffset(), QPoint(0, 0));
    EXPECT_EQ(menuButton->iconOffset(), QPoint(0, 0));
    EXPECT_FALSE(backButton->isEnabled());
    EXPECT_TRUE(menuButton->isEnabled());
    QEvent menuEnterEvent(QEvent::Enter);
    QApplication::sendEvent(menuButton, &menuEnterEvent);
    QApplication::processEvents();
    EXPECT_EQ(menuButton->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryTitleBarButtonPressAnimation")),
              nullptr);
    QTest::mousePress(menuButton, Qt::LeftButton, Qt::NoModifier, menuButton->rect().center());
    QApplication::processEvents();
    auto* menuJitterAnimation = menuButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTitleBarButtonPressAnimation"));
    ASSERT_NE(menuJitterAnimation, nullptr);
    EXPECT_EQ(menuJitterAnimation->state(), QAbstractAnimation::Running);

    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    QApplication::processEvents();
    ASSERT_TRUE(backButton->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(backButton->width(), 24, 1000);
    QEvent backEnterEvent(QEvent::Enter);
    QApplication::sendEvent(backButton, &backEnterEvent);
    QApplication::processEvents();
    EXPECT_EQ(backButton->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryTitleBarButtonPressAnimation")),
              nullptr);
    QTest::mousePress(backButton, Qt::LeftButton, Qt::NoModifier, backButton->rect().center());
    QApplication::processEvents();
    auto* backJitterAnimation = backButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTitleBarButtonPressAnimation"));
    ASSERT_NE(backJitterAnimation, nullptr);
    EXPECT_EQ(backJitterAnimation->state(), QAbstractAnimation::Running);
    QTest::qWait(220);
    QApplication::processEvents();
    EXPECT_EQ(backButton->iconOffset(), QPoint(0, 0));
    EXPECT_EQ(menuButton->iconOffset(), QPoint(0, 0));
    EXPECT_TRUE(vg::sizeIs(menuButton, QSize(24, 24)));
    EXPECT_TRUE(vg::sizeIs(appIcon, QSize(18, 18)));
    EXPECT_EQ(mappedGeometry(appIcon, titleBar).center().y(),
              mappedGeometry(menuButton, titleBar).center().y());
    EXPECT_EQ(title->height(), 24);
    EXPECT_TRUE(vg::sizeIs(searchBox, QSize(360, 28)));
    EXPECT_TRUE(vg::spacingXIs(backButton, menuButton, 8));
    EXPECT_TRUE(vg::spacingXIs(menuButton, appIcon, 8));
    EXPECT_TRUE(vg::spacingXIs(appIcon, title, 8));
    EXPECT_GE(mappedGeometry(backButton, titleBar).left(), titleBar->systemReservedLeadingWidth());
    EXPECT_TRUE(vg::containedIn(searchBox, titleBar, 0));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPixmap iconPixmap = appIcon->pixmap(Qt::ReturnByValue);
    ASSERT_FALSE(iconPixmap.isNull());
    const QSize logicalPixmapSize = iconPixmap.deviceIndependentSize().toSize();
#else
    const QPixmap* iconPixmap = appIcon->pixmap();
    ASSERT_NE(iconPixmap, nullptr);
    ASSERT_FALSE(iconPixmap->isNull());
    const QSize logicalPixmapSize(qRound(iconPixmap->width() / iconPixmap->devicePixelRatioF()),
                                  qRound(iconPixmap->height() / iconPixmap->devicePixelRatioF()));
#endif
    EXPECT_EQ(logicalPixmapSize, QSize(18, 18));
}

TEST_F(GalleryShellFrameworkTest, MenuButtonTogglesLeftCompactNavigationMode)
{
    GallerySettings::instance().setNavigationStyle(GallerySettings::NavigationStyle::Left);
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* navigationView = window.findChild<NavigationView*>(QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(navigationView->expandedPaneWidth(), 260);
    EXPECT_EQ(navigationView->compactPaneWidth(), 48);
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Left);
    EXPECT_TRUE(navigationView->isAnimationEnabled());
    EXPECT_TRUE(navigationView->isPaneOpen());
    EXPECT_EQ(navigationView->chromeGeometry().width(), 260);

    auto* menuButton = vg::findRequiredChild<Button>(window.titleBar(),
                                                     QStringLiteral("GalleryTitleBar.MenuButton"));
    ASSERT_NE(menuButton, nullptr);
    QTest::mouseClick(menuButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Left);
    EXPECT_FALSE(navigationView->isPaneOpen());
    EXPECT_LT(navigationView->property("layoutTransitionProgress").toDouble(), 1.0);
    EXPECT_TRUE(mainPane->isCompact());
    EXPECT_TRUE(tree->property("galleryCompact").toBool());
    EXPECT_TRUE(tree->viewport()->property("galleryCompact").toBool());
    settleNavigationViewAnimation();
    EXPECT_EQ(navigationView->chromeGeometry().width(), navigationView->compactPaneWidth());
    EXPECT_EQ(navigationView->contentGeometry().left(), navigationView->compactPaneWidth());

    QTest::mouseClick(menuButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Left);
    EXPECT_TRUE(navigationView->isPaneOpen());
    EXPECT_LT(navigationView->property("layoutTransitionProgress").toDouble(), 1.0);
    EXPECT_FALSE(mainPane->isCompact());
    EXPECT_FALSE(tree->property("galleryCompact").toBool());
    EXPECT_FALSE(tree->viewport()->property("galleryCompact").toBool());
    settleNavigationViewAnimation();
    EXPECT_FALSE(mainPane->isCompact());
    EXPECT_FALSE(tree->property("galleryCompact").toBool());
    EXPECT_FALSE(tree->viewport()->property("galleryCompact").toBool());
    EXPECT_EQ(navigationView->chromeGeometry().width(), navigationView->expandedPaneWidth());
    EXPECT_EQ(navigationView->contentGeometry().left(), navigationView->expandedPaneWidth());
}

TEST_F(GalleryShellFrameworkTest, LeftCompactNavigationHidesHeadersAndInlineChildren)
{
    GallerySettings::instance().setNavigationStyle(GallerySettings::NavigationStyle::Left);
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    auto* navigationView = window.findChild<NavigationView*>(QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    auto* menuButton = vg::findRequiredChild<Button>(window.titleBar(),
                                                     QStringLiteral("GalleryTitleBar.MenuButton"));
    ASSERT_NE(menuButton, nullptr);

    QTest::mouseClick(menuButton, Qt::LeftButton);
    settleNavigationViewAnimation();

    EXPECT_TRUE(mainPane->isCompact());
    QModelIndex controlsHeader;
    for (int row = 0; row < tree->model()->rowCount(); ++row) {
        const QModelIndex candidate = tree->model()->index(row, 0);
        if (candidate.data(Qt::DisplayRole).toString() == QStringLiteral("Controls")) {
            controlsHeader = candidate;
            break;
        }
    }
    ASSERT_TRUE(controlsHeader.isValid());
    EXPECT_TRUE(tree->visualRect(controlsHeader).isEmpty());

    const QModelIndex categoryIndex = mainPane->indexForRouteId(QStringLiteral("basic-input"));
    const QModelIndex childIndex = mainPane->indexForRouteId(QStringLiteral("button"));
    ASSERT_TRUE(categoryIndex.isValid());
    ASSERT_TRUE(childIndex.isValid());
    EXPECT_FALSE(tree->isExpanded(categoryIndex));
    EXPECT_FALSE(tree->visualRect(categoryIndex).isEmpty());
    EXPECT_TRUE(tree->visualRect(childIndex).isEmpty());
}

TEST_F(GalleryShellFrameworkTest, LeftCompactNavigationShowsFluentToolTips)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(
        QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    const QModelIndex homeIndex = mainPane->indexForRouteId(QStringLiteral("home"));
    ASSERT_TRUE(homeIndex.isValid());
    const QRect expandedRect = tree->visualRect(homeIndex);
    ASSERT_FALSE(expandedRect.isEmpty());

    QHelpEvent expandedHelp(QEvent::ToolTip,
                            expandedRect.center(),
                            tree->viewport()->mapToGlobal(expandedRect.center()));
    QApplication::sendEvent(tree->viewport(), &expandedHelp);
    EXPECT_EQ(mainPane->findChild<ToolTip*>(
                  QStringLiteral("galleryCompactNavigationToolTip")),
              nullptr);

    auto* menuButton = vg::findRequiredChild<Button>(
        window.titleBar(), QStringLiteral("GalleryTitleBar.MenuButton"));
    ASSERT_NE(menuButton, nullptr);
    QTest::mouseClick(menuButton, Qt::LeftButton);
    settleNavigationViewAnimation();

    const QRect compactRect = tree->visualRect(homeIndex);
    ASSERT_FALSE(compactRect.isEmpty());
    QHelpEvent compactHelp(QEvent::ToolTip,
                           compactRect.center(),
                           tree->viewport()->mapToGlobal(compactRect.center()));
    QApplication::sendEvent(tree->viewport(), &compactHelp);
    QApplication::processEvents();

    auto* toolTip = mainPane->findChild<ToolTip*>(
        QStringLiteral("galleryCompactNavigationToolTip"));
    ASSERT_NE(toolTip, nullptr);
    EXPECT_EQ(toolTip->text(), QStringLiteral("Home"));
    EXPECT_TRUE(toolTip->isVisible());

    const QRect rowGlobal(tree->viewport()->mapToGlobal(compactRect.topLeft()), compactRect.size());
    const QRect bubbleGlobal = toolTip->geometry().adjusted(toolTip->shadowMargin(),
                                                            toolTip->shadowMargin(),
                                                            -toolTip->shadowMargin(),
                                                            -toolTip->shadowMargin());
    EXPECT_NEAR(bubbleGlobal.center().x(), rowGlobal.center().x(), 1);
    EXPECT_EQ(rowGlobal.top() - bubbleGlobal.bottom() - 1, 4);

    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(tree->viewport(), &leaveEvent);
    QTRY_VERIFY_WITH_TIMEOUT(!toolTip->isVisible(), 1000);
}

TEST_F(GalleryShellFrameworkTest, LeftCompactNavigationShowsChildrenInFlyout)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    auto* navigationView = window.findChild<NavigationView*>(QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    auto* menuButton = vg::findRequiredChild<Button>(window.titleBar(),
                                                     QStringLiteral("GalleryTitleBar.MenuButton"));
    ASSERT_NE(menuButton, nullptr);

    QTest::mouseClick(menuButton, Qt::LeftButton);
    settleNavigationViewAnimation();

    const QModelIndex categoryIndex = mainPane->indexForRouteId(QStringLiteral("status-info"));
    ASSERT_TRUE(categoryIndex.isValid());
    tree->scrollTo(categoryIndex, QAbstractItemView::EnsureVisible);
    QApplication::processEvents();
    const QRect categoryRect = tree->visualRect(categoryIndex);
    ASSERT_FALSE(categoryRect.isEmpty());
    ASSERT_TRUE(tree->viewport()->rect().intersects(categoryRect));

    const QPoint categoryPoint(tree->viewport()->rect().center().x(), categoryRect.center().y());
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, categoryPoint);
    QApplication::processEvents();

    auto* flyout = window.findChild<Popup*>(QStringLiteral("galleryCompactNavigationFlyout"));
    ASSERT_NE(flyout, nullptr);
    QPointer<Popup> flyoutPointer(flyout);
    EXPECT_TRUE(flyout->isVisible());
    auto* anchor = mainPane->findChild<QWidget*>(QStringLiteral("galleryCompactNavigationFlyoutAnchor"));
    ASSERT_NE(anchor, nullptr);
    EXPECT_EQ(anchor->geometry().left(), 0);
    EXPECT_EQ(anchor->geometry().width(), navigationView->compactPaneWidth());
    const QRect anchorInWindow = mappedGeometry(anchor, &window);
    const QRect flyoutCard = fluent::overlay::visibleCardGeometry(flyout->geometry());
    EXPECT_GE(flyoutCard.top(), mappedGeometry(tree, &window).top());
    EXPECT_LT(flyoutCard.top(), anchorInWindow.center().y());
    EXPECT_EQ(flyout->findChild<QScrollArea*>(), nullptr);
    for (const QString& childRouteId : {QStringLiteral("info-badge"),
                                        QStringLiteral("info-bar"),
                                        QStringLiteral("progress-bar"),
                                        QStringLiteral("progress-ring"),
                                        QStringLiteral("shimmer"),
                                        QStringLiteral("tooltip")}) {
        auto* childRow = flyout->findChild<QWidget*>(
            QStringLiteral("galleryCompactNavigationFlyoutRow_%1").arg(childRouteId));
        ASSERT_NE(childRow, nullptr);
        EXPECT_TRUE(childRow->isVisibleTo(flyout));
    }
    auto* infoBadgeRow = flyout->findChild<QWidget*>(QStringLiteral("galleryCompactNavigationFlyoutRow_info-badge"));
    ASSERT_NE(infoBadgeRow, nullptr);
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("status-info"));

    QTest::mouseClick(infoBadgeRow, Qt::LeftButton, Qt::NoModifier, infoBadgeRow->rect().center());
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("info-badge"));
    QTest::qWait(320);
    QApplication::processEvents();
    EXPECT_TRUE(flyoutPointer.isNull() || !flyoutPointer->isVisible());
}

TEST_F(GalleryShellFrameworkTest, NavigationEntriesExposeRequiredGroups)
{
    GalleryWindow window;
    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    auto* footerPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryFooterNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    ASSERT_NE(footerPane, nullptr);

    const QStringList titles = mainPane->visibleTitles();
    EXPECT_TRUE(containsAll(titles, {
        QStringLiteral("Home"),
        QStringLiteral("Controls"),
        QStringLiteral("Basic input"),
        QStringLiteral("Collections"),
        QStringLiteral("Date & time"),
        QStringLiteral("Dialogs & flyouts"),
        QStringLiteral("Menus & toolbars"),
        QStringLiteral("Navigation"),
        QStringLiteral("Scrolling"),
        QStringLiteral("Status & info"),
        QStringLiteral("Text fields"),
        QStringLiteral("Windowing")
    }));
    EXPECT_TRUE(titles.contains(QStringLiteral("Foundation")));
    EXPECT_FALSE(titles.contains(QStringLiteral("Settings")));
    EXPECT_EQ(Typography::Icons::Message, QString::fromUtf16(u"\uE8BD"));

    const QStringList routeIds = mainPane->routeIds();
    EXPECT_TRUE(containsAll(routeIds, {
        QStringLiteral("all-controls"),
        QStringLiteral("basic-input"),
        QStringLiteral("collections"),
        QStringLiteral("date-time"),
        QStringLiteral("dialogs-flyouts"),
        QStringLiteral("menus-toolbars"),
        QStringLiteral("navigation"),
        QStringLiteral("scrolling"),
        QStringLiteral("status-info"),
        QStringLiteral("text-fields"),
        QStringLiteral("windowing")
    }));
    EXPECT_TRUE(routeIds.contains(QStringLiteral("foundation")));
    EXPECT_FALSE(routeIds.contains(QStringLiteral("settings")));
    EXPECT_EQ(footerPane->routeIds(), QStringList{QStringLiteral("settings")});
}

TEST_F(GalleryShellFrameworkTest, MainNavigationRowsAreVisibleInPane)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);

    const QStringList visibleRouteIds{
        QStringLiteral("home"),
        QStringLiteral("all-controls"),
        QStringLiteral("basic-input")
    };
    for (const QString& routeId : visibleRouteIds) {
        EXPECT_TRUE(routeVisibleInTree(mainPane, routeId)) << routeId.toStdString();
    }

    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    tree->expand(mainPane->indexForRouteId(QStringLiteral("basic-input")));
    settleTreeAnimations();
    EXPECT_TRUE(routeVisibleInTree(mainPane, QStringLiteral("button")));
}

TEST_F(GalleryShellFrameworkTest, MainNavigationUsesWinUIGalleryRowMetrics)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    EXPECT_EQ(tree->indentation(), 0);
    EXPECT_FALSE(tree->isAnimated());

    const QModelIndex homeIndex = mainPane->indexForRouteId(QStringLiteral("home"));
    const QModelIndex categoryIndex = mainPane->indexForRouteId(QStringLiteral("basic-input"));
    ASSERT_TRUE(homeIndex.isValid());
    ASSERT_TRUE(categoryIndex.isValid());
    EXPECT_EQ(tree->visualRect(homeIndex).height(), 36);
    EXPECT_EQ(tree->visualRect(categoryIndex).height(), 36);

    tree->expand(categoryIndex);
    const QModelIndex childIndex = mainPane->indexForRouteId(QStringLiteral("button"));
    ASSERT_TRUE(childIndex.isValid());
    EXPECT_FALSE(tree->visualRect(childIndex).isEmpty());
    settleTreeAnimations();
    EXPECT_EQ(tree->visualRect(childIndex).height(), 36);
}

TEST_F(GalleryShellFrameworkTest, MainNavigationChildIndicatorAnchorsToTextColumn)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    tree->setIndicatorMotionAnimationEnabled(false);
    tree->setSelectionIndicatorVisible(true);

    const QModelIndex categoryIndex = mainPane->indexForRouteId(QStringLiteral("basic-input"));
    const QModelIndex childIndex = mainPane->indexForRouteId(QStringLiteral("button"));
    ASSERT_TRUE(categoryIndex.isValid());
    ASSERT_TRUE(childIndex.isValid());

    tree->expand(categoryIndex);
    settleTreeAnimations();

    tree->setSelectedItem(categoryIndex);
    QApplication::processEvents();
    const QRectF categoryIndicator = tree->selectedIndicatorRect(1.0);

    tree->setSelectedItem(childIndex);
    QApplication::processEvents();
    const QRectF childIndicator = tree->selectedIndicatorRect(1.0);

    EXPECT_FALSE(categoryIndicator.isEmpty());
    EXPECT_FALSE(childIndicator.isEmpty());
    EXPECT_GT(childIndicator.left(), categoryIndicator.left() + 24.0);
    EXPECT_NEAR(childIndicator.left(),
                tree->visualRect(childIndex).left() + 36.0,
                0.01);
    EXPECT_NEAR(tree->visualRect(childIndex).left() + 47.0 - childIndicator.right(),
                8.0,
                0.01);
    EXPECT_NEAR(childIndicator.height(), 14.0, 0.01);
}

TEST_F(GalleryShellFrameworkTest, MainNavigationRowClickTogglesCategory)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);

    const QModelIndex categoryIndex = mainPane->indexForRouteId(QStringLiteral("basic-input"));
    ASSERT_TRUE(categoryIndex.isValid());
    ASSERT_FALSE(tree->isExpanded(categoryIndex));

    const QRect rowRect = tree->visualRect(categoryIndex);
    ASSERT_FALSE(rowRect.isEmpty());
    const QPoint rowPoint(rowRect.left() + 100, rowRect.center().y());
    QTest::mousePress(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rowPoint);
    QApplication::processEvents();
    EXPECT_TRUE(tree->isExpanded(categoryIndex));
    QTest::mouseRelease(tree->viewport(), Qt::LeftButton, Qt::NoModifier, rowPoint);
    settleTreeAnimations();
    EXPECT_TRUE(tree->isExpanded(categoryIndex));

    const QRect expandedRowRect = tree->visualRect(categoryIndex);
    const QPoint expandedRowPoint(expandedRowRect.left() + 100, expandedRowRect.center().y());
    QTest::mousePress(tree->viewport(), Qt::LeftButton, Qt::NoModifier, expandedRowPoint);
    QApplication::processEvents();
    EXPECT_TRUE(tree->isExpanded(categoryIndex));
    QTest::mouseRelease(tree->viewport(), Qt::LeftButton, Qt::NoModifier, expandedRowPoint);
    settleTreeAnimations();
    EXPECT_FALSE(tree->isExpanded(categoryIndex));
}

TEST_F(GalleryShellFrameworkTest, FooterNavigationHasTopDivider)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* footerPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryFooterNavigationPane"));
    ASSERT_NE(footerPane, nullptr);

    auto* divider = footerPane->findChild<QWidget*>(QStringLiteral("galleryFooterNavigationDivider"));
    ASSERT_NE(divider, nullptr);
    EXPECT_EQ(divider->height(), 1);
    EXPECT_TRUE(divider->isVisibleTo(footerPane));
}

TEST_F(GalleryShellFrameworkTest, MainNavigationScrollbarUsesInsetOverlay)
{
    GalleryWindow window;
    window.resize(1180, 500);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    tree->expandAll();
    settleTreeAnimations();
    tree->refreshFluentScrollChrome();

    auto* scrollBar = tree->verticalFluentScrollBar();
    ASSERT_NE(scrollBar, nullptr);
    ASSERT_GT(tree->verticalScrollBar()->maximum(), tree->verticalScrollBar()->minimum());
    ASSERT_TRUE(scrollBar->isVisible());
    EXPECT_EQ(scrollBar->thickness(), 5);
    EXPECT_FALSE(tree->isHorizontalFluentScrollBarEnabled());
    ASSERT_NE(tree->horizontalFluentScrollBar(), nullptr);
    EXPECT_FALSE(tree->horizontalFluentScrollBar()->isVisible());

    const QRect barGeometry = scrollBar->geometry();
    EXPECT_GE(barGeometry.left(), tree->rect().left());
    EXPECT_LT(barGeometry.right(), tree->rect().right());
    EXPECT_GE(tree->rect().right() - barGeometry.right(), 4);
    EXPECT_GE(barGeometry.top() - tree->rect().top(), 4);
    EXPECT_GE(tree->rect().bottom() - barGeometry.bottom(), 4);
}

TEST_F(GalleryShellFrameworkTest, NavigationAutoScrollDoesNotRevealPaneScrollbar)
{
    GalleryWindow window;
    window.resize(1180, 500);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    tree->expandAll();
    settleTreeAnimations();
    tree->refreshFluentScrollChrome();

    auto* scrollBar = tree->verticalFluentScrollBar();
    ASSERT_NE(scrollBar, nullptr);
    ASSERT_GT(tree->verticalScrollBar()->maximum(), tree->verticalScrollBar()->minimum());
    const bool scrollBarSignalsBlocked = scrollBar->signalsBlocked();
    scrollBar->blockSignals(true);
    tree->verticalScrollBar()->setValue(tree->verticalScrollBar()->minimum());
    scrollBar->blockSignals(scrollBarSignalsBlocked);
    scrollBar->setOpacity(0.0);

    const int previousOffset = tree->verticalScrollBar()->value();
    ASSERT_TRUE(window.selectRoute(QStringLiteral("tooltip")));
    QApplication::processEvents();

    EXPECT_GT(tree->verticalScrollBar()->value(), previousOffset);
    EXPECT_DOUBLE_EQ(scrollBar->opacity(), 0.0);
}

TEST_F(GalleryShellFrameworkTest, CurrentContentScrollbarStaysAtRightEdgeAfterNavigationClick)
{
    GalleryWindow window;
    window.resize(1180, 500);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    clickNavigationRoute(mainPane, QStringLiteral("combobox"));
    QApplication::processEvents();

    auto* page = window.currentContentPage();
    ASSERT_NE(page, nullptr);
    ASSERT_EQ(page->routeId(), QStringLiteral("combobox"));

    auto* scrollView = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    ASSERT_NE(scrollView, nullptr);
    auto* floatingBar = scrollView->viewport()->findChild<fluent::scrolling::ScrollBar*>(
        QStringLiteral("fluentScrollViewFloatingVerticalBar"));
    ASSERT_NE(floatingBar, nullptr);
    ASSERT_TRUE(floatingBar->isVisible());

    const QRect barGeometry = floatingBar->geometry();
    EXPECT_EQ(barGeometry.right(), scrollView->viewport()->rect().right());
    EXPECT_EQ(barGeometry.left(), qMax(0, scrollView->viewport()->width() - floatingBar->thickness()));
}

TEST_F(GalleryShellFrameworkTest, ComponentRoutesRetainParentCategories)
{
    GalleryNavigationViewModel model;

    struct ExpectedRoute {
        QString id;
        QString parentId;
    };

    const QVector<ExpectedRoute> expectedRoutes{
        {QStringLiteral("button"), QStringLiteral("basic-input")},
        {QStringLiteral("grid-view"), QStringLiteral("collections")},
        {QStringLiteral("date-picker"), QStringLiteral("date-time")},
        {QStringLiteral("flyout"), QStringLiteral("dialogs-flyouts")},
        {QStringLiteral("menu-bar"), QStringLiteral("menus-toolbars")},
        {QStringLiteral("navigation-view"), QStringLiteral("navigation")},
        {QStringLiteral("scroll-view"), QStringLiteral("scrolling")},
        {QStringLiteral("info-bar"), QStringLiteral("status-info")},
        {QStringLiteral("auto-suggest-box"), QStringLiteral("text-fields")},
        {QStringLiteral("title-bar"), QStringLiteral("windowing")}
    };

    for (const ExpectedRoute& expectedRoute : expectedRoutes) {
        const auto* item = model.itemById(expectedRoute.id);
        ASSERT_NE(item, nullptr) << expectedRoute.id.toStdString();
        EXPECT_EQ(item->kind, fluent::gallery::GalleryNavigationItem::Kind::ComponentRoute)
            << expectedRoute.id.toStdString();
        EXPECT_EQ(model.parentRouteId(expectedRoute.id), expectedRoute.parentId)
            << expectedRoute.id.toStdString();
    }

    const auto* settingsItem = model.itemById(QStringLiteral("settings"));
    ASSERT_NE(settingsItem, nullptr);
    EXPECT_EQ(settingsItem->kind, fluent::gallery::GalleryNavigationItem::Kind::FooterRoute);
    EXPECT_TRUE(model.parentRouteId(QStringLiteral("settings")).isEmpty());
}

TEST_F(GalleryShellFrameworkTest, SelectRouteSwitchesContentPages)
{
    GalleryWindow window;
    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    auto* footerPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryFooterNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    ASSERT_NE(footerPane, nullptr);
    // Home is a content page, so no placeholder is present initially.
    ASSERT_NE(window.currentContentPage(), nullptr);
    EXPECT_EQ(window.currentPlaceholderPage(), nullptr);

    // Every catalog route now resolves to a real content page; placeholders are
    // reserved for hypothetical routes without content metadata.
    // zh_CN: 目录路由现在全部解析为真实内容页；占位页只留给没有内容元数据的假想路由。
    ASSERT_TRUE(window.selectRoute(QStringLiteral("checkbox")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("checkbox"));
    EXPECT_EQ(mainPane->selectedRouteId(), QStringLiteral("checkbox"));
    EXPECT_EQ(footerPane->selectedRouteId(), QStringLiteral("checkbox"));
    EXPECT_EQ(window.currentPlaceholderPage(), nullptr);
    GalleryContentPage* checkboxPage = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (checkboxPage = window.currentContentPage())
            && checkboxPage->routeId() == QStringLiteral("checkbox"),
        2000);
    EXPECT_EQ(checkboxPage->routeId(), QStringLiteral("checkbox"));
    EXPECT_EQ(checkboxPage->title(), QStringLiteral("CheckBox"));

    ASSERT_TRUE(window.selectRoute(QStringLiteral("combobox")));
    GalleryContentPage* comboboxPage = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (comboboxPage = window.currentContentPage())
            && comboboxPage->routeId() == QStringLiteral("combobox"),
        2000);
    EXPECT_EQ(comboboxPage->routeId(), QStringLiteral("combobox"));

    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("button"));
    EXPECT_EQ(window.currentPlaceholderPage(), nullptr);
    GalleryContentPage* buttonPage = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (buttonPage = window.currentContentPage())
            && buttonPage->routeId() == QStringLiteral("button"),
        2000);
    EXPECT_EQ(buttonPage->routeId(), QStringLiteral("button"));

    EXPECT_FALSE(window.selectRoute(QStringLiteral("missing-route")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("button"));
}

TEST_F(GalleryShellFrameworkTest, SearchBoxNavigatesToMatchingRoute)
{
    GalleryWindow window;

    // Exact (case-insensitive) title match wins.
    EXPECT_TRUE(window.navigateToSearchResult(QStringLiteral("checkbox")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("checkbox"));

    // Partial matches fall back to the first containing title.
    EXPECT_TRUE(window.navigateToSearchResult(QStringLiteral("Progress")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("progress-bar"));

    // Category titles navigate too; unknown text changes nothing.
    EXPECT_TRUE(window.navigateToSearchResult(QStringLiteral("Date & time")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("date-time"));
    EXPECT_FALSE(window.navigateToSearchResult(QStringLiteral("no-such-control")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("date-time"));

    // The title-bar search box suggests every navigable route title.
    auto* searchBox = window.findChild<AutoSuggestBox*>(QStringLiteral("GalleryTitleBar.SearchBox"));
    ASSERT_NE(searchBox, nullptr);
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("CheckBox")));
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("Scrolling")));
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("Settings")));

    // Typing narrows the suggestions to containing titles (owner-side filtering).
    window.show();
    QApplication::processEvents();
    searchBox->setFocus();
    QTest::keyClicks(searchBox, QStringLiteral("progress"));
    QApplication::processEvents();
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("ProgressBar")));
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("ProgressRing")));
    EXPECT_FALSE(searchBox->suggestions().contains(QStringLiteral("CheckBox")));

    // Retyping re-filters against the full title list, not the narrowed one.
    searchBox->clear();
    QTest::keyClicks(searchBox, QStringLiteral("date"));
    QApplication::processEvents();
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("Date & time")));
    EXPECT_TRUE(searchBox->suggestions().contains(QStringLiteral("DatePicker")));
    EXPECT_FALSE(searchBox->suggestions().contains(QStringLiteral("ProgressBar")));
}

TEST_F(GalleryShellFrameworkTest, BackButtonReturnsThroughNavigationHistory)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")) == nullptr,
        2000);

    auto* backButton = vg::findRequiredChild<Button>(window.titleBar(),
                                                     QStringLiteral("GalleryTitleBar.BackButton"));
    ASSERT_NE(backButton, nullptr);
    EXPECT_FALSE(backButton->isEnabled());

    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    QApplication::processEvents();
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("button"));
    EXPECT_TRUE(backButton->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(backButton->width(), 24, 1000);

    ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
    QApplication::processEvents();
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("settings"));
    EXPECT_TRUE(backButton->isEnabled());

    QTest::mouseClick(backButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("button"), 1000);
    EXPECT_TRUE(backButton->isEnabled());

    QTest::mouseClick(backButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("home"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!backButton->isEnabled(), 1000);
}

TEST_F(GalleryShellFrameworkTest, NavigationButtonActivationUpdatesRoute)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* mainPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    clickNavigationRoute(mainPane, QStringLiteral("button"));
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("button"));
    ASSERT_NE(window.currentContentPage(), nullptr);
    EXPECT_EQ(window.currentContentPage()->title(), QStringLiteral("Button"));
    EXPECT_EQ(window.currentPlaceholderPage(), nullptr);

    auto* footerPane = window.findChild<GalleryNavigationPane*>(QStringLiteral("galleryFooterNavigationPane"));
    ASSERT_NE(footerPane, nullptr);
    auto* settingsRotationAnimation = footerPane->findChild<QPropertyAnimation*>(
        QStringLiteral("gallerySettingsIconRotationAnimation"));
    ASSERT_NE(settingsRotationAnimation, nullptr);
    EXPECT_EQ(settingsRotationAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_NEAR(footerPane->settingsIconRotation(), 0.0, 0.001);

    TreeView* footerTree = navigationTree(footerPane);
    ASSERT_NE(footerTree, nullptr);
    const QModelIndex settingsIndex = footerPane->indexForRouteId(QStringLiteral("settings"));
    ASSERT_TRUE(settingsIndex.isValid());
    const QRect settingsRect = footerTree->visualRect(settingsIndex);
    ASSERT_FALSE(settingsRect.isEmpty());
    const QPoint settingsPoint = settingsRect.center();
    QTest::mousePress(footerTree->viewport(), Qt::LeftButton, Qt::NoModifier, settingsPoint);
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("settings"));
    EXPECT_EQ(settingsRotationAnimation->state(), QAbstractAnimation::Running);
    QTest::qWait(80);
    QApplication::processEvents();
    EXPECT_GT(footerPane->settingsIconRotation(), 0.0);
    QTest::mouseRelease(footerTree->viewport(), Qt::LeftButton, Qt::NoModifier, settingsPoint);
    QTest::qWait(360);
    QApplication::processEvents();
    EXPECT_EQ(settingsRotationAnimation->state(), QAbstractAnimation::Stopped);
    EXPECT_NEAR(footerPane->settingsIconRotation(), 0.0, 0.001);
    ASSERT_NE(window.currentSettingsPage(), nullptr);
    EXPECT_NE(dynamic_cast<fluent::QMLPlus*>(window.currentSettingsPage()), nullptr);
    EXPECT_EQ(window.currentSettingsPage()->titleLabel()->text(), QStringLiteral("Settings"));
}

TEST_F(GalleryShellFrameworkTest, SettingsChoicesApplyAndDeferredRowsAreOmitted)
{
    auto& settings = GallerySettings::instance();
    GallerySettingsRestorer restore(settings);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Auto);

    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    ASSERT_TRUE(window.selectRoute(QStringLiteral("settings")));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage() != nullptr, 2000);

    SettingsPage* page = window.currentSettingsPage();
    ASSERT_NE(page, nullptr);
    auto* themeChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsThemeChoice"));
    auto* navigationChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsNavigationChoice"));
    ASSERT_NE(themeChoice, nullptr);
    ASSERT_NE(navigationChoice, nullptr);
    EXPECT_EQ(themeChoice->count(), 3);
    EXPECT_EQ(themeChoice->currentText(), QStringLiteral("Light"));
    EXPECT_EQ(navigationChoice->count(), 5);
    EXPECT_EQ(navigationChoice->currentText(), QStringLiteral("Auto"));
    EXPECT_EQ(page->findChildren<QFrame*>(QStringLiteral("gallerySettingsRow")).size(), 2);

    QStringList visibleText;
    for (auto* label : page->findChildren<fluent::textfields::Label*>())
        visibleText.append(label->text());
    EXPECT_FALSE(visibleText.contains(QStringLiteral("Sound")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("Manage samples")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("About")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("WinUI 3 Gallery")));

    const auto iconLabels = page->findChildren<fluent::textfields::Label*>(
        QStringLiteral("gallerySettingsRowIcon"));
    ASSERT_EQ(iconLabels.size(), 2);
    for (auto* iconLabel : iconLabels)
        EXPECT_EQ(iconLabel->font().family(), Typography::FontFamily::SegoeFluentIcons);

    QElapsedTimer themeRequestTimer;
    themeRequestTimer.start();
    themeChoice->setCurrentIndex(2);
    EXPECT_LT(themeRequestTimer.elapsed(), 100);
    QTRY_COMPARE_WITH_TIMEOUT(settings.themeMode(), GallerySettings::ThemeMode::Dark, 1000);
    EXPECT_EQ(settings.themeMode(), GallerySettings::ThemeMode::Dark);
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Dark);
    for (auto* iconLabel : iconLabels)
        EXPECT_EQ(iconLabel->font().family(), Typography::FontFamily::SegoeFluentIcons);
    const auto darkColors = page->themeColors();
    for (auto* label : page->findChildren<fluent::textfields::Label*>()) {
        if (label->objectName() == QStringLiteral("gallerySettingsSubtitle"))
            QTRY_COMPARE_WITH_TIMEOUT(label->palette().color(QPalette::WindowText),
                                      darkColors.textSecondary,
                                      1000);
        else
            QTRY_COMPARE_WITH_TIMEOUT(label->palette().color(QPalette::WindowText),
                                      darkColors.textPrimary,
                                      1000);
    }

    window.resize(460, 760);
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(page->width() < 640, 1000);
    for (auto* choice : {themeChoice, navigationChoice}) {
        auto* row = qobject_cast<QFrame*>(choice->parentWidget());
        ASSERT_NE(row, nullptr);
        EXPECT_GE(row->height(), 120);
        const QRect choiceInPage(choice->mapTo(page, QPoint(0, 0)), choice->size());
        EXPECT_TRUE(page->rect().contains(choiceInPage));
    }

    window.resize(1180, 760);
    QApplication::processEvents();

    auto* navigationView = window.findChild<NavigationView*>(
        QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    navigationChoice->setCurrentIndex(1);
    QApplication::processEvents();
    EXPECT_EQ(settings.navigationStyle(), GallerySettings::NavigationStyle::Left);
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Left);
    EXPECT_TRUE(navigationView->isPaneOpen());

    navigationChoice->setCurrentIndex(2);
    QApplication::processEvents();
    EXPECT_EQ(settings.navigationStyle(), GallerySettings::NavigationStyle::LeftCompact);
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::LeftCompact);
    EXPECT_FALSE(navigationView->isPaneOpen());

    navigationChoice->setCurrentIndex(3);
    QApplication::processEvents();
    EXPECT_EQ(settings.navigationStyle(), GallerySettings::NavigationStyle::LeftMinimal);
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::LeftMinimal);

    navigationChoice->setCurrentIndex(4);
    QApplication::processEvents();
    EXPECT_EQ(settings.navigationStyle(), GallerySettings::NavigationStyle::Top);
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Top);
    ASSERT_NE(navigationView->mainChromeWidget(), nullptr);
    EXPECT_EQ(navigationView->mainChromeWidget()->objectName(),
              QStringLiteral("galleryTopMainNavigationPane"));
    auto* topHomeButton = navigationView->mainChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_home"));
    auto* topFoundationButton = navigationView->mainChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_foundation"));
    ASSERT_NE(topHomeButton, nullptr);
    ASSERT_NE(topFoundationButton, nullptr);
    EXPECT_GE(topFoundationButton->geometry().left() - topHomeButton->geometry().right() - 1, 4);

    auto* topToolTip = topHomeButton->findChild<ToolTip*>(
        QStringLiteral("FluentAttachedToolTip"), Qt::FindDirectChildrenOnly);
    ASSERT_NE(topToolTip, nullptr);
    QHelpEvent topHelp(QEvent::ToolTip,
                       topHomeButton->rect().center(),
                       topHomeButton->mapToGlobal(topHomeButton->rect().center()));
    QApplication::sendEvent(topHomeButton, &topHelp);
    QApplication::processEvents();
    EXPECT_TRUE(topToolTip->isVisible());

    QTest::mouseClick(topFoundationButton, Qt::LeftButton);
    QApplication::processEvents();
    auto* topFlyout = window.findChild<Popup*>(QStringLiteral("galleryTopNavigationFlyout"));
    ASSERT_NE(topFlyout, nullptr);
    ASSERT_TRUE(topFlyout->isVisible());
    const QRect foundationButtonInWindow(topFoundationButton->mapTo(&window, QPoint(0, 0)),
                                         topFoundationButton->size());
    QTRY_VERIFY_WITH_TIMEOUT(
        fluent::overlay::visibleCardRect(topFlyout->geometry()).top()
            > foundationButtonInWindow.bottom(),
        1000);

    auto* topSettingsButton = navigationView->footerChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_settings"));
    ASSERT_NE(topSettingsButton, nullptr);
    QPointer<Popup> dismissedTopFlyout(topFlyout);
    QTest::mouseClick(topSettingsButton, Qt::LeftButton);
    auto* topSettingsAnimation = topSettingsButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopSettingsIconRotationAnimation"), Qt::FindDirectChildrenOnly);
    ASSERT_NE(topSettingsAnimation, nullptr);
    EXPECT_EQ(topSettingsAnimation->state(), QAbstractAnimation::Running);
    QTRY_VERIFY_WITH_TIMEOUT(!dismissedTopFlyout || !dismissedTopFlyout->isVisible(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("settings"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(topSettingsAnimation->state(), QAbstractAnimation::Stopped, 1000);
    EXPECT_NEAR(topSettingsButton->iconRotation(), 0.0, 0.001);

    QTest::mouseClick(topFoundationButton, Qt::LeftButton);
    QApplication::processEvents();
    topFlyout = window.findChild<Popup*>(QStringLiteral("galleryTopNavigationFlyout"));
    ASSERT_NE(topFlyout, nullptr);
    ASSERT_TRUE(topFlyout->isVisible());
    auto* foundationChild = topFlyout->findChild<QWidget*>(
        QStringLiteral("galleryCompactNavigationFlyoutRow_foundation-qmlplus"));
    ASSERT_NE(foundationChild, nullptr);
    QTest::mouseClick(foundationChild, Qt::LeftButton);
    EXPECT_FALSE(topFlyout->isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("foundation-qmlplus"), 1000);

    QTest::mouseClick(topHomeButton, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("home"));

    navigationChoice->setCurrentIndex(0);
    QApplication::processEvents();
    EXPECT_EQ(navigationView->displayMode(), NavigationView::DisplayMode::Auto);
    EXPECT_EQ(navigationView->effectiveDisplayMode(), NavigationView::DisplayMode::Left);
    EXPECT_TRUE(navigationView->isPaneOpen());

    themeChoice->setCurrentIndex(0);
    QTRY_COMPARE_WITH_TIMEOUT(settings.themeMode(), GallerySettings::ThemeMode::System, 1000);
}
