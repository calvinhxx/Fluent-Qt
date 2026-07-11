#include <gtest/gtest.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFrame>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/collections/TreeView.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/dialogs_flyouts/Dialog.h"  // SmokeOverlay
#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/navigation/NavigationView.h"
#include "components/scrolling/ScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/status_info/Shimmer.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/status_info/ToolTip.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/WindowBackdrop.h"
#include "design/Typography.h"
#include "view/pages/GalleryContentPage.h"
#include "view/shell/GalleryApplicationController.h"
#include "view/support/GalleryCloseBehaviorPrompt.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GalleryPageSkeleton.h"
#include "view/shell/GalleryWindowMetrics.h"
#include "view/widgets/GalleryEntryCard.h"
#include "view/widgets/samples/WindowingSamples.h"
#include "VisualGeometryTestUtils.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GalleryIntroTour.h"
#include "view/pages/SettingsPage.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"
#include "viewmodel/ThemeCatalog.h"
#include "components/foundation/ThemeRegistry.h"

using fluent::basicinput::Button;
using fluent::basicinput::ComboBox;
using fluent::collections::TreeView;
using fluent::dialogs_flyouts::ContentDialog;
using fluent::dialogs_flyouts::Popup;
using fluent::dialogs_flyouts::SmokeOverlay;
using fluent::gallery::CloseBehaviorPromptContent;
using fluent::gallery::GalleryApplicationController;
using fluent::gallery::GalleryContentPage;
using fluent::gallery::GalleryEntryCard;
using fluent::gallery::GalleryIntroTour;
using fluent::gallery::GalleryNavigationPane;
using fluent::gallery::GalleryNavigationViewModel;
using fluent::gallery::GalleryPageSkeleton;
using fluent::gallery::GallerySettings;
using fluent::gallery::GalleryWindow;
using fluent::gallery::SettingsPage;
using fluent::navigation::NavigationView;
using fluent::scrolling::ScrollView;
using fluent::status_info::Shimmer;
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
        , m_closeBehavior(settings.closeBehavior())
        , m_closeBehaviorConfirmed(settings.closeBehaviorConfirmed())
    {
    }

    ~GallerySettingsRestorer()
    {
        m_settings.setNavigationStyle(m_navigationStyle);
        m_settings.setThemeMode(m_themeMode);
        m_settings.setCloseBehavior(m_closeBehavior);
        m_settings.setCloseBehaviorConfirmed(m_closeBehaviorConfirmed);
    }

private:
    GallerySettings& m_settings;
    GallerySettings::ThemeMode m_themeMode;
    GallerySettings::NavigationStyle m_navigationStyle;
    GallerySettings::CloseBehavior m_closeBehavior;
    bool m_closeBehaviorConfirmed = false;
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

Popup* visiblePopupByName(QWidget* root, const QString& objectName)
{
    if (!root)
        return nullptr;
    const auto popups = root->findChildren<Popup*>(objectName);
    for (Popup* popup : popups) {
        if (popup && popup->isVisible())
            return popup;
    }
    return nullptr;
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
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Fluent-Qt Gallery"));
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

TEST_F(GalleryShellFrameworkTest, HomeHeroStartsWithDesignResourceCards)
{
    GalleryWindow window;

    auto* linkStrip = window.findChild<QAbstractItemView*>(
        QStringLiteral("galleryHomeHeroLinksView"));
    ASSERT_NE(linkStrip, nullptr);
    ASSERT_NE(linkStrip->model(), nullptr);
    ASSERT_GE(linkStrip->model()->rowCount(), 3);

    struct ExpectedLink {
        QString title;
        QUrl url;
        QString imagePath;
    };
    const QVector<ExpectedLink> expectedLinks{
        {QStringLiteral("Design"),
         QUrl(QStringLiteral("https://aka.ms/WinUI/3.0-figma-toolkit")),
         QStringLiteral(":/app/assets/home_header_tiles/Header-WindowsDesign.png")},
        {QStringLiteral("macOS 27 Community"),
         QUrl(QStringLiteral("https://www.figma.com/community/file/1651309434229735362")),
         QStringLiteral(":/app/assets/home_header_tiles/Header-macOS27.png")},
        {QStringLiteral("Material 3 Design Kit"),
         QUrl(QStringLiteral("https://www.figma.com/community/file/1035203688168086460")),
         QStringLiteral(":/app/assets/home_header_tiles/Header-Material3.png")},
    };
    constexpr int kHomeLinkUrlRole = Qt::UserRole + 3;
    constexpr int kHomeLinkImageRole = Qt::UserRole + 4;

    for (int row = 0; row < expectedLinks.size(); ++row) {
        const QModelIndex index = linkStrip->model()->index(row, 0);
        ASSERT_TRUE(index.isValid()) << row;
        EXPECT_EQ(index.data(Qt::DisplayRole).toString(), expectedLinks.at(row).title);
        EXPECT_EQ(index.data(kHomeLinkUrlRole).toUrl(), expectedLinks.at(row).url);
        EXPECT_EQ(index.data(kHomeLinkImageRole).toString(), expectedLinks.at(row).imagePath);
    }
}

TEST_F(GalleryShellFrameworkTest, IntroTourLocksAndRestoresWindowChrome)
{
    GalleryWindow window;
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();

    GalleryIntroTour tour(&window);
    GalleryIntroTour::Step step;
    step.title = QStringLiteral("Welcome");
    step.body = QStringLiteral("Intro content");
    step.centered = true;
    tour.setSteps({step});
    tour.start();

    EXPECT_FALSE(window.isChromeInteractive());
    // A centered (target-less) step dims uniformly — no spotlight cut-out.
    auto* scrim = window.findChild<SmokeOverlay*>(QStringLiteral("GalleryIntroTour.Scrim"));
    ASSERT_NE(scrim, nullptr);
    EXPECT_FALSE(scrim->spotlightEnabled());
    auto* coach = window.findChild<fluent::dialogs_flyouts::CoachMark*>();
    ASSERT_NE(coach, nullptr);
    EXPECT_EQ(coach->surfaceMode(), fluent::dialogs_flyouts::CoachMark::SameWindowSurface);
    EXPECT_EQ(coach->parentWidget(), &window);
    EXPECT_EQ(coach->windowType(), Qt::Widget);
    window.onThemeUpdated();
    QApplication::processEvents();
    EXPECT_FALSE(window.isChromeInteractive());
    auto* closeButton = window.findChild<Button*>(
        QStringLiteral("GalleryIntroTour.CloseButton"));
    ASSERT_NE(closeButton, nullptr);
    QTest::mouseClick(closeButton, Qt::LeftButton);
    EXPECT_TRUE(window.isChromeInteractive());

    window.close();
}

TEST_F(GalleryShellFrameworkTest, IntroTourSpotlightsAnchoredTarget)
{
    GalleryWindow window;
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();

    // A plain child with a known geometry stands in for a highlight target (search box, nav, etc.).
    auto* target = new QWidget(&window);
    target->setObjectName(QStringLiteral("introSpotlightTarget"));
    target->setGeometry(120, 90, 240, 44);
    target->show();

    GalleryIntroTour tour(&window);
    GalleryIntroTour::Step anchored;
    anchored.title = QStringLiteral("Search");
    anchored.target = target;
    tour.setSteps({anchored});
    tour.start();  // a single anchored step is applied immediately by start()

    auto* scrim = window.findChild<SmokeOverlay*>(QStringLiteral("GalleryIntroTour.Scrim"));
    ASSERT_NE(scrim, nullptr);
    EXPECT_TRUE(scrim->spotlightEnabled());

    // The cut-out covers the target with a little breathing room around it.
    const QRect targetInWindow(target->mapTo(&window, QPoint(0, 0)), target->size());
    const QRect targetInScrim = targetInWindow.translated(-scrim->geometry().topLeft());
    EXPECT_TRUE(scrim->spotlightRect().contains(targetInScrim));
    EXPECT_GT(scrim->spotlightRect().width(), targetInScrim.width());
    EXPECT_GT(scrim->spotlightRect().height(), targetInScrim.height());

    window.close();
}

TEST_F(GalleryShellFrameworkTest, IntroTourScrimHonorsWindowSurfaceRadius)
{
    QWidget window;
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();
    window.setProperty(::fluent::overlay::clientSideFrameRadiusPropertyName(), 18);

    GalleryIntroTour tour(&window);
    GalleryIntroTour::Step step;
    step.title = QStringLiteral("Welcome");
    step.body = QStringLiteral("Intro content");
    step.centered = true;
    tour.setSteps({step});
    tour.start();

    auto* scrim = window.findChild<SmokeOverlay*>(QStringLiteral("GalleryIntroTour.Scrim"));
    ASSERT_NE(scrim, nullptr);
    scrim->setProgress(1.0);

    QImage image(scrim->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scrim->render(&painter);
    painter.end();

    EXPECT_EQ(image.pixelColor(0, 0).alpha(), 0);
    EXPECT_GT(image.pixelColor(image.rect().center()).alpha(), 0);

    window.close();
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

    auto* featuredCards = window.findChild<QWidget*>(QStringLiteral("galleryHomeCards"));
    ASSERT_NE(featuredCards, nullptr);
    GalleryEntryCard* card = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (card = featuredCards->findChild<GalleryEntryCard*>()) != nullptr, 2000);
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

    const QPixmap iconPixmap = fluentLabelPixmapValue(appIcon);
    ASSERT_FALSE(iconPixmap.isNull());
    const QSize logicalPixmapSize = fluentPixmapLogicalSize(iconPixmap);
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
    // Full labels are revealed only once the widen animation settles at full width (otherwise they
    // clip mid-slide). The exact moment of that reveal is animation-timing dependent, so we assert
    // only the settled end state — icon-only collapse is immediate (checked above), label reveal is
    // deferred to settle. zh_CN: 完整标签仅在加宽动画稳定到全宽后才显示（否则滑动中会被裁剪）。该揭示的具体
    // 时刻取决于动画时序，故只断言稳定后的最终状态——折叠为仅图标是立即的（上方已校验），标签揭示延迟到稳定。
    settleNavigationViewAnimation();
    EXPECT_FALSE(mainPane->isCompact());
    EXPECT_FALSE(tree->property("galleryCompact").toBool());
    EXPECT_FALSE(tree->viewport()->property("galleryCompact").toBool());
    EXPECT_EQ(navigationView->chromeGeometry().width(), navigationView->expandedPaneWidth());
    EXPECT_EQ(navigationView->contentGeometry().left(), navigationView->expandedPaneWidth());
}

TEST_F(GalleryShellFrameworkTest, AutoNavigationCollapsesWhenNarrowedAndReexpandsWhenWidened)
{
    GallerySettings::instance().setNavigationStyle(GallerySettings::NavigationStyle::Auto);
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* navigationView = window.findChild<NavigationView*>(
        QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    using DisplayMode = NavigationView::DisplayMode;

    // Wide: Auto resolves to the expanded Left rail and the pane is presented inline-open.
    EXPECT_EQ(navigationView->effectiveDisplayMode(), DisplayMode::Left);
    EXPECT_TRUE(navigationView->isPaneOpen());

    // Narrowing past the compact threshold auto-collapses the pane — it must NOT be left open as a
    // stray flyout over the content (the bug this guards). zh_CN: 变窄越过紧凑阈值会自动收起窗格——
    // 不能把它作为残留浮层留在内容之上（此测试守护的 bug）。
    window.resize(520, 760);
    QApplication::processEvents();
    settleNavigationViewAnimation();
    EXPECT_NE(navigationView->effectiveDisplayMode(), DisplayMode::Left);
    EXPECT_FALSE(navigationView->isPaneOpen());

    // Widening back to the Left rail auto-expands it again, like WinUI.
    window.resize(1180, 760);
    QApplication::processEvents();
    settleNavigationViewAnimation();
    EXPECT_EQ(navigationView->effectiveDisplayMode(), DisplayMode::Left);
    EXPECT_TRUE(navigationView->isPaneOpen());
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

TEST_F(GalleryShellFrameworkTest, PreparesOneHiddenShimmerBeforeColdNavigation)
{
    GalleryWindow window;

    auto* skeleton = window.findChild<GalleryPageSkeleton*>();
    ASSERT_NE(skeleton, nullptr);
    const QList<Shimmer*> shimmers = skeleton->findChildren<Shimmer*>();
    ASSERT_EQ(shimmers.size(), 1);
    EXPECT_FALSE(skeleton->isVisible());
    EXPECT_FALSE(shimmers.constFirst()->isAnimationRunning());
}

TEST_F(GalleryShellFrameworkTest, ColdRouteSelectionKeepsPreparedSkeletonOffClickPath)
{
    GalleryWindow window;
    auto* preparedSkeleton = window.findChild<GalleryPageSkeleton*>();
    ASSERT_NE(preparedSkeleton, nullptr);

    QElapsedTimer clickTimer;
    clickTimer.start();
    ASSERT_TRUE(window.selectRoute(QStringLiteral("tab-view")));
    const qint64 clickMs = clickTimer.elapsed();

    EXPECT_EQ(window.findChild<GalleryPageSkeleton*>(), preparedSkeleton);
    EXPECT_EQ(preparedSkeleton->findChildren<Shimmer*>().size(), 1);
    EXPECT_LT(clickMs, 100)
        << "Cold navigation should only swap in the prepared skeleton; page construction is deferred";
}

TEST_F(GalleryShellFrameworkTest, StartupPrewarmPrioritizesHomeFeaturedTabView)
{
    GalleryWindow window;
    QTest::qWait(3200);
    QApplication::processEvents();

    ASSERT_TRUE(window.selectRoute(QStringLiteral("tab-view")));
    auto* page = window.currentContentPage();
    ASSERT_NE(page, nullptr)
        << "The Home-featured TabView route should be resident before the startup budget expires";
    EXPECT_EQ(page->routeId(), QStringLiteral("tab-view"));
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

TEST_F(GalleryShellFrameworkTest, NavigationArrowKeysActivateCurrentRoute)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QWidget*>(QStringLiteral("gallerySplashScreen")) == nullptr,
        2000);

    auto* mainPane = window.findChild<GalleryNavigationPane*>(
        QStringLiteral("galleryMainNavigationPane"));
    ASSERT_NE(mainPane, nullptr);
    TreeView* tree = navigationTree(mainPane);
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->currentIndex(), mainPane->indexForRouteId(QStringLiteral("home")));

    tree->setFocus(Qt::OtherFocusReason);
    QTest::keyClick(tree, Qt::Key_Down);

    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("foundation"), 1000);
    ASSERT_NE(window.currentContentPage(), nullptr);
    EXPECT_EQ(window.currentContentPage()->title(), QStringLiteral("Foundation"));
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
    auto* styleChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsStyleChoice"));
    auto* navigationChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsNavigationChoice"));
    auto* effectChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsEffectChoice"));
    auto* closeBehaviorChoice = page->findChild<ComboBox*>(
        QStringLiteral("gallerySettingsCloseBehaviorChoice"));
    auto* updateButton = page->findChild<Button*>(
        QStringLiteral("gallerySettingsCheckUpdatesButton"));
    ASSERT_NE(themeChoice, nullptr);
    ASSERT_NE(styleChoice, nullptr);
    ASSERT_NE(navigationChoice, nullptr);
    ASSERT_NE(effectChoice, nullptr);
    ASSERT_NE(closeBehaviorChoice, nullptr);
    ASSERT_NE(updateButton, nullptr);
    EXPECT_EQ(updateButton->text(), QStringLiteral("Check updates"));
    EXPECT_FALSE(page->autoFillBackground());
    auto* settingsScroll = page->findChild<ScrollView*>(
        QStringLiteral("gallerySettingsScrollArea"));
    ASSERT_NE(settingsScroll, nullptr);
    ASSERT_NE(settingsScroll->viewport(), nullptr);
    EXPECT_FALSE(settingsScroll->viewport()->autoFillBackground());
    auto* settingsViewport = page->findChild<QWidget*>(
        QStringLiteral("gallerySettingsViewport"));
    ASSERT_NE(settingsViewport, nullptr);
    EXPECT_FALSE(settingsViewport->autoFillBackground());
    EXPECT_EQ(themeChoice->count(), 3);
    EXPECT_EQ(themeChoice->currentText(), QStringLiteral("Light"));
    // Style theme offers the three brand presets (Fluent / Material 3 / macOS). zh_CN: 样式主题提供三套品牌预设。
    EXPECT_EQ(styleChoice->count(), 3);
    // Navigation style mirrors the native WinUI Gallery: only "Left" and "Top" are offered. "Left"
    // is the responsive Auto mode, so the Auto config above shows as "Left" (index 0).
    EXPECT_EQ(navigationChoice->count(), 2);
    EXPECT_EQ(navigationChoice->currentText(), QStringLiteral("Left"));
    EXPECT_EQ(effectChoice->count(), 3);
    EXPECT_EQ(closeBehaviorChoice->count(), 3);
    EXPECT_EQ(closeBehaviorChoice->currentIndex(), static_cast<int>(settings.closeBehavior()));
    // Appearance & behavior (4 rows) + App behavior (1 row) + Updates (1 row) = 6 rows;
    // Style theme + Accent color share one row.
    EXPECT_NE(page->findChild<QWidget*>(QStringLiteral("gallerySettingsAccentControl")), nullptr);
    EXPECT_EQ(page->findChildren<QFrame*>(QStringLiteral("gallerySettingsRow")).size(), 6);

    QStringList visibleText;
    for (auto* label : page->findChildren<fluent::textfields::Label*>())
        visibleText.append(label->text());
    EXPECT_FALSE(visibleText.contains(QStringLiteral("Sound")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("Manage samples")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("About")));
    EXPECT_FALSE(visibleText.contains(QStringLiteral("Fluent-Qt Gallery")));

    const auto iconLabels = page->findChildren<fluent::textfields::Label*>(
        QStringLiteral("gallerySettingsRowIcon"));
    ASSERT_EQ(iconLabels.size(), 6);
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
    window.resize(460, 760);
    QApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(page->width() < 640, 1000);
    for (auto* choice : {themeChoice, navigationChoice, effectChoice, closeBehaviorChoice}) {
        auto* row = qobject_cast<QFrame*>(choice->parentWidget());
        ASSERT_NE(row, nullptr);
        EXPECT_GE(row->height(), 120);
        const QRect choiceInPage(choice->mapTo(page, QPoint(0, 0)), choice->size());
        EXPECT_GE(choiceInPage.left(), page->rect().left());
        EXPECT_LE(choiceInPage.right(), page->rect().right());
    }

    window.resize(1180, 760);
    QApplication::processEvents();

    auto* navigationView = window.findChild<NavigationView*>(
        QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    // Index 1 = "Top". (Index 0 = "Left" → Auto is exercised at the end of this test.) The finer
    // Left/LeftCompact/LeftMinimal modes are no longer user-selectable — Auto resolves them by width.
    navigationChoice->setCurrentIndex(1);
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
    auto* topDialogsButton = navigationView->mainChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_dialogs-flyouts"));
    ASSERT_NE(topHomeButton, nullptr);
    ASSERT_NE(topFoundationButton, nullptr);
    ASSERT_NE(topDialogsButton, nullptr);
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

    QPointer<Popup> dismissedTopFlyout(topFlyout);
    QTest::mouseClick(topDialogsButton, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!dismissedTopFlyout || !dismissedTopFlyout->isVisible(), 1000);
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("foundation"));
    EXPECT_EQ(visiblePopupByName(&window, QStringLiteral("galleryTopNavigationFlyout")), nullptr);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QTest::mouseClick(topDialogsButton, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("dialogs-flyouts"), 1000);
    QApplication::processEvents();
    topFlyout = visiblePopupByName(&window, QStringLiteral("galleryTopNavigationFlyout"));
    ASSERT_NE(topFlyout, nullptr);

    auto* topSettingsButton = navigationView->footerChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_settings"));
    ASSERT_NE(topSettingsButton, nullptr);
    dismissedTopFlyout = topFlyout;
    QTest::mouseClick(topSettingsButton, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!dismissedTopFlyout || !dismissedTopFlyout->isVisible(), 1000);
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("dialogs-flyouts"));
    EXPECT_EQ(visiblePopupByName(&window, QStringLiteral("galleryTopNavigationFlyout")), nullptr);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(topSettingsButton->findChild<QPropertyAnimation*>(
                  QStringLiteral("galleryTopSettingsIconRotationAnimation"), Qt::FindDirectChildrenOnly),
              nullptr);

    QTest::mouseClick(topSettingsButton, Qt::LeftButton);
    auto* topSettingsAnimation = topSettingsButton->findChild<QPropertyAnimation*>(
        QStringLiteral("galleryTopSettingsIconRotationAnimation"), Qt::FindDirectChildrenOnly);
    ASSERT_NE(topSettingsAnimation, nullptr);
    EXPECT_EQ(topSettingsAnimation->state(), QAbstractAnimation::Running);
    QTRY_COMPARE_WITH_TIMEOUT(window.currentRouteId(), QStringLiteral("settings"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(topSettingsAnimation->state(), QAbstractAnimation::Stopped, 1000);
    EXPECT_NEAR(topSettingsButton->iconRotation(), 0.0, 0.001);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QTest::mouseClick(topFoundationButton, Qt::LeftButton);
    QApplication::processEvents();
    topFlyout = visiblePopupByName(&window, QStringLiteral("galleryTopNavigationFlyout"));
    ASSERT_NE(topFlyout, nullptr);
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

TEST_F(GalleryShellFrameworkTest, PaintedMicaHeroPreservesOpaqueWindowBacking)
{
    GalleryWindow window;
    auto* hero = window.findChild<QWidget*>(QStringLiteral("galleryHomeHero"));
    ASSERT_NE(hero, nullptr);
    ASSERT_GT(hero->width(), 2);
    ASSERT_GT(hero->height(), 2);

    fluent::windowing::BackdropState state;
    state.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    state.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::PaintedOpaque;
    state.platformApplied = true;
    fluent::windowing::publishWindowBackdropState(&window, state);

    QImage image(hero->size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(41, 53, 67, 255));
    QPainter painter(&image);
    hero->render(&painter);
    painter.end();

    // The hero fades to transparent artwork at the bottom-right. In a painted
    // material window that must reveal the already-opaque Mica backing, not
    // erase its alpha and expose the desktop.
    // zh_CN: hero 右下角渐隐的是美术图层；在应用侧绘制材质的窗口中必须露出
    // 已经不透明的 Mica 底层，而不能清除 alpha 后露出桌面。
    EXPECT_EQ(qAlpha(image.pixel(image.width() - 2, image.height() - 2)), 255);
}

TEST_F(GalleryShellFrameworkTest, CompositedMicaHeroDissolvesAtRetinaScale)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* hero = window.findChild<QWidget*>(QStringLiteral("galleryHomeHero"));
    ASSERT_NE(hero, nullptr);

    fluent::windowing::BackdropState state;
    state.requestedEffect = fluent::windowing::BackdropEffect::Mica;
    state.effectiveEffect = fluent::windowing::BackdropEffect::Mica;
    state.backend = fluent::windowing::BackdropBackend::MacVibrancy;
    state.fidelity = fluent::windowing::BackdropFidelity::Native;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    state.platformApplied = true;
    fluent::windowing::publishWindowBackdropState(&window, state);
    window.setAttribute(Qt::WA_TranslucentBackground, true);

    const qreal dpr = qMax<qreal>(1.0, hero->devicePixelRatioF());
    QImage image(qMax(1, qRound(hero->width() * dpr)),
                 qMax(1, qRound(hero->height() * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    hero->render(&painter);
    painter.end();

    const int centerX = image.width() / 2;
    EXPECT_GT(image.pixelColor(centerX, qMax(0, qRound(24 * dpr))).alpha(), 180);
    EXPECT_LT(image.pixelColor(centerX, image.height() - 2).alpha(), 16)
        << "The hero's device-scaled artwork must reach the transparent end of its bottom dissolve";
}

TEST_F(GalleryShellFrameworkTest, RapidRouteSwitchingKeepsCurrentPageScrollable)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.currentContentPage()
            && window.currentContentPage()->routeId() == QStringLiteral("button"),
        2000);
    ASSERT_TRUE(window.selectRoute(QStringLiteral("combobox")));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.currentContentPage()
            && window.currentContentPage()->routeId() == QStringLiteral("combobox"),
        2000);

    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(window.selectRoute(i % 2 == 0
                                          ? QStringLiteral("button")
                                          : QStringLiteral("combobox")));
    }
    ASSERT_TRUE(window.selectRoute(QStringLiteral("button")));
    QApplication::processEvents();

    GalleryContentPage* page = window.currentContentPage();
    ASSERT_NE(page, nullptr);
    ASSERT_EQ(page->routeId(), QStringLiteral("button"));
    auto* scrollView = page->findChild<ScrollView*>(QStringLiteral("galleryContentScrollArea"));
    ASSERT_NE(scrollView, nullptr);
    ASSERT_NE(scrollView->viewport(), nullptr);
    ASSERT_TRUE(scrollView->isVisible());
    ASSERT_GT(scrollView->verticalScrollBar()->maximum(), 0);

    scrollView->verticalScrollBar()->setValue(0);
    FLUENT_MAKE_WHEEL_EVENT(wheel, 96, 96, -120, Qt::NoModifier);
    wheel.setAccepted(false);
    QApplication::sendEvent(scrollView->viewport(), &wheel);

    EXPECT_TRUE(wheel.isAccepted());
    EXPECT_GT(scrollView->verticalScrollBar()->value(), 0)
        << "The current content viewport must still receive wheel input after rapid navigation";
}

TEST_F(GalleryShellFrameworkTest, SettingsThemeSwitchKeepsLabelsReadableInDarkMode)
{
    auto& settings = GallerySettings::instance();
    GallerySettingsRestorer restore(settings);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);

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
    ASSERT_NE(themeChoice, nullptr);

    themeChoice->setCurrentIndex(2);
    QTRY_COMPARE_WITH_TIMEOUT(settings.themeMode(), GallerySettings::ThemeMode::Dark, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(fluent::FluentElement::currentTheme(),
                              fluent::FluentElement::Dark,
                              1000);

    const auto darkColors = page->themeColors();
    const auto cssRgba = [](const QColor& color) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    };
    const auto labels = page->findChildren<fluent::textfields::Label*>();
    ASSERT_FALSE(labels.isEmpty());
    for (auto* label : labels) {
        const QColor expected =
            label->textColorRole() == fluent::textfields::Label::TextColorRole::Secondary
                ? darkColors.textSecondary
                : darkColors.textPrimary;
        QTRY_COMPARE_WITH_TIMEOUT(label->palette().color(QPalette::WindowText),
                                  expected,
                                  1000);
        QTRY_VERIFY_WITH_TIMEOUT(label->styleSheet().contains(cssRgba(expected)), 1000);
    }

    window.close();
}

TEST_F(GalleryShellFrameworkTest, FirstClosePromptsForBehaviorAndKeepsWindowOpenOnCancel)
{
    auto& settings = GallerySettings::instance();
    GallerySettingsRestorer restore(settings);
    settings.setCloseBehavior(GallerySettings::CloseBehavior::Tray);
    settings.setCloseBehaviorConfirmed(false);

    GalleryWindow window;
    GalleryApplicationController applicationController(&window);
    window.resize(900, 700);
    window.show();
    QApplication::processEvents();

    EXPECT_FALSE(window.close());

    ContentDialog* dialog = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (dialog = window.findChild<ContentDialog*>(
             QStringLiteral("galleryCloseBehaviorDialog"))) != nullptr,
        1000);
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(dialog->windowModality(), Qt::ApplicationModal);
    EXPECT_FALSE(window.isChromeInteractive());
    EXPECT_LE(dialog->width(), 380);
    EXPECT_LE(dialog->height(), 304);

    // Selection lives on the row itself (no separate radio control); the prompt
    // content exposes the chosen behavior. zh_CN: 选中态由整行承载（无独立单选控件），
    // 弹窗内容通过 selectedBehavior() 暴露当前选择。
    // The prompt content is a FluentElement mixin widget without Q_OBJECT, so
    // look it up by its unique object name and cast to the known concrete type.
    // zh_CN: 弹窗内容是不含 Q_OBJECT 的 FluentElement 混入控件，按唯一 objectName 查找后转为已知具体类型。
    auto* promptContent = static_cast<CloseBehaviorPromptContent*>(
        dialog->findChild<QWidget*>(QStringLiteral("galleryCloseBehaviorPromptContent")));
    ASSERT_NE(promptContent, nullptr);
    EXPECT_EQ(promptContent->selectedBehavior(), GallerySettings::CloseBehavior::Tray);

    auto* minimizeRow = dialog->findChild<QWidget*>(
        QStringLiteral("galleryCloseBehaviorRow0"));
    auto* quitRow = dialog->findChild<QWidget*>(
        QStringLiteral("galleryCloseBehaviorRow2"));
    ASSERT_NE(minimizeRow, nullptr);
    ASSERT_NE(quitRow, nullptr);
    EXPECT_LE(minimizeRow->height(), 42);
    EXPECT_TRUE(promptContent->rect().contains(quitRow->geometry()));

    QTest::mouseClick(minimizeRow, Qt::LeftButton);
    EXPECT_EQ(promptContent->selectedBehavior(), GallerySettings::CloseBehavior::Minimize);

    QPointer<ContentDialog> dialogGuard = dialog;
    dialog->done(ContentDialog::ResultNone);
    QTRY_VERIFY_WITH_TIMEOUT(dialogGuard.isNull() || !dialogGuard->isVisible(), 1000);
    EXPECT_TRUE(window.isVisible());
    EXPECT_TRUE(window.isChromeInteractive());
    EXPECT_FALSE(settings.closeBehaviorConfirmed());
}

// Regression: picking a custom accent must keep the whole accent FAMILY consistent. A full-dump theme
// template carries explicit accentSecondary/accentTertiary/textAccentPrimary; setUserAccent must drop
// them so applyColorSpec re-derives them from the NEW accentDefault. The original bug left them stale
// (a green accentDefault clashing with leftover blue variants). Relies on QStandardPaths test mode
// (set in QtTestEnvironment) so this writes to an isolated sandbox, never the developer's real themes.
// zh_CN: 回归——选自定义强调色后,整个强调色族必须一致。完整 dump 模板含显式 accentSecondary/Tertiary/
// textAccentPrimary;setUserAccent 必须清除它们,让 applyColorSpec 从新 accentDefault 重新派生。原 bug 会留下
// 陈旧变体(绿色 accentDefault 与残留蓝色变体冲突)。依赖测试模式沙盒,不会写到开发者真实 themes 目录。
TEST(ThemeCatalogAccentConsistencyTest, SetUserAccentReDerivesVariantsFromFullDump)
{
    using fluent::ThemeRegistry;
    namespace tc = fluent::gallery::ThemeCatalog;
    constexpr auto kTheme = GallerySettings::StyleTheme::MacOS;

    // Force a fresh FULL-DUMP template (explicit blue accentSecondary/textAccentPrimary) — the exact
    // condition under which the old setUserAccent left stale variants behind.
    QFile::remove(tc::userThemeFilePath(kTheme));
    tc::apply(kTheme);

    const QColor picked(0x4D, 0xA0, 0x4D);  // the green that originally clashed with stale blue variants
    tc::setUserAccent(kTheme, picked);
    tc::apply(kTheme);

    for (bool dark : {false, true}) {
        const auto colors = ThemeRegistry::instance().colors(dark);
        // QColor::rgb() drops alpha, so a derived variant (same hue, lower alpha) compares equal to the
        // picked accent — and unequal to the stale preset blue if the bug regressed.
        EXPECT_EQ(colors.accentDefault.rgb(), picked.rgb()) << "dark=" << dark;
        EXPECT_EQ(colors.accentSecondary.rgb(), picked.rgb()) << "accentSecondary stale; dark=" << dark;
        EXPECT_EQ(colors.accentTertiary.rgb(), picked.rgb()) << "accentTertiary stale; dark=" << dark;
        EXPECT_EQ(colors.textAccentPrimary.rgb(), picked.rgb()) << "textAccentPrimary stale; dark=" << dark;
    }

    // Reset reverts cleanly to the preset accent (no half-override left behind).
    tc::clearUserAccent(kTheme);
    tc::apply(kTheme);
    EXPECT_EQ(ThemeRegistry::instance().colors(false).accentDefault.rgb(),
              tc::presetAccent(kTheme, false).rgb());

    QFile::remove(tc::userThemeFilePath(kTheme));
    ThemeRegistry::instance().resetToDefaults();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
}

// --- Windowing TitleBar caption controls follow the active design language. ----------------------
// zh_CN: Windowing 标题栏 caption 控件跟随当前设计语言。
//
// The TitleBar sample renders Windows-style trailing caption buttons under Fluent/Material and
// leading macOS traffic lights under Cupertino, switching live on a Style-theme change — the real
// platform difference, rendered by design language rather than by the host OS.
// zh_CN: TitleBar 示例在 Fluent/Material 下渲染 Windows 风格尾部标题栏按钮,在 Cupertino 下渲染前导 macOS
// 红绿灯,并在样式主题切换时实时切换——真实的平台差异,按设计语言而非宿主系统渲染。
class WindowingSamplesTest : public ::testing::Test {
protected:
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(WindowingSamplesTest, CaptionStyleMapsCupertinoToTrafficLights) {
    using fluent::gallery::captionStyleForDesignLanguage;
    using fluent::gallery::TitleBarCaptionStyle;

    EXPECT_EQ(captionStyleForDesignLanguage(fluent::FluentElement::DesignFluent),
              TitleBarCaptionStyle::WindowsCaptionButtons);
    EXPECT_EQ(captionStyleForDesignLanguage(fluent::FluentElement::DesignMaterial),
              TitleBarCaptionStyle::WindowsCaptionButtons);
    EXPECT_EQ(captionStyleForDesignLanguage(fluent::FluentElement::DesignCupertino),
              TitleBarCaptionStyle::MacTrafficLights);
}

TEST_F(WindowingSamplesTest, TitleBarSampleReservesCaptionSpaceByDesignLanguage) {
    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignFluent);

    const auto samples = fluent::gallery::windowingSamples(QStringLiteral("title-bar"));
    ASSERT_FALSE(samples.isEmpty());
    ASSERT_TRUE(static_cast<bool>(samples.first().createPreview));

    QScopedPointer<QWidget> preview(samples.first().createPreview(nullptr));
    ASSERT_FALSE(preview.isNull());
    preview->show();
    QApplication::processEvents();

    auto* titleBar = preview->findChild<TitleBar*>();
    ASSERT_NE(titleBar, nullptr);

    // Fluent: trailing Windows caption buttons reserve the right, leading is free.
    // zh_CN: Fluent:尾部 Windows 标题栏按钮占右侧,前导留空。
    EXPECT_GT(titleBar->systemReservedTrailingWidth(), 0);
    EXPECT_EQ(titleBar->systemReservedLeadingWidth(), 0);

    // Switch to Cupertino: a live Style-theme change must flip the reservation to leading traffic
    // lights. refreshTheme() re-broadcasts onThemeUpdated to the visible preview, which rebuilds.
    // zh_CN: 切到 Cupertino:实时样式主题切换须把预留翻到前导红绿灯。refreshTheme() 向可见预览重广播 onThemeUpdated。
    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);
    fluent::FluentElement::refreshTheme();
    QApplication::processEvents();

    EXPECT_GT(titleBar->systemReservedLeadingWidth(), 0);
    EXPECT_EQ(titleBar->systemReservedTrailingWidth(), 0);

    // And back to Fluent restores the trailing caption buttons.
    // zh_CN: 切回 Fluent 恢复尾部标题栏按钮。
    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignFluent);
    fluent::FluentElement::refreshTheme();
    QApplication::processEvents();

    EXPECT_GT(titleBar->systemReservedTrailingWidth(), 0);
    EXPECT_EQ(titleBar->systemReservedLeadingWidth(), 0);
}

// Regression: a top-nav child flyout must still collapse on row click AFTER it was light-dismissed
// and reopened. The outgoing flyout's deferred deletion used to clear m_childFlyout out from under
// the freshly reopened flyout, so the next row click's closeChildFlyout() early-returned and the
// flyout stayed open over the navigated page. zh_CN: 顶部子浮窗在「轻关闭后再打开」时点击行仍须收起。
// 旧浮窗的延迟析构曾把 m_childFlyout 从刚重新打开的浮窗下清空,导致下次点击行时 closeChildFlyout() 提前返回,
// 浮窗滞留在已导航的页面上。
TEST_F(GalleryShellFrameworkTest, TopFlyoutRowClickDismissesAfterReopen)
{
    auto& settings = GallerySettings::instance();
    const auto previousStyle = settings.navigationStyle();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Top);
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    auto* navigationView = window.findChild<NavigationView*>(QStringLiteral("galleryNavigationView"));
    ASSERT_NE(navigationView, nullptr);
    ASSERT_NE(navigationView->mainChromeWidget(), nullptr);
    auto* collectionsButton = navigationView->mainChromeWidget()->findChild<Button*>(
        QStringLiteral("galleryTopNavigationButton_collections"));
    ASSERT_NE(collectionsButton, nullptr);

    auto openCollectionsFlyout = [&]() -> Popup* {
        QTest::mouseClick(collectionsButton, Qt::LeftButton);
        QApplication::processEvents();
        QTest::qWait(250);  // let the entrance settle
        QApplication::processEvents();
        return visiblePopupByName(&window, QStringLiteral("galleryTopNavigationFlyout"));
    };

    // 1) Open the flyout, then light-dismiss it the way an outside press does: close() leaves the
    //    pane still tracking this popup (it is only cleared on deletion), mirroring the real path.
    Popup* first = openCollectionsFlyout();
    ASSERT_NE(first, nullptr);
    first->close();
    QApplication::processEvents();

    // 2) Reopen the SAME category. This deleteLater()s the old popup and creates a new one; the old
    //    popup's destroyed signal then fires and previously nulled the new popup's tracking pointer.
    Popup* second = openCollectionsFlyout();
    ASSERT_NE(second, nullptr);
    QPointer<Popup> secondPtr(second);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    ASSERT_FALSE(secondPtr.isNull());
    ASSERT_TRUE(secondPtr->isVisible());

    // 3) Click a child row — the flyout must collapse and the route must change.
    auto* row = second->findChild<QWidget*>(
        QStringLiteral("galleryCompactNavigationFlyoutRow_tree-view"));
    ASSERT_NE(row, nullptr);
    QTest::mouseClick(row, Qt::LeftButton, Qt::NoModifier, row->rect().center());
    QApplication::processEvents();
    QTest::qWait(50);
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("tree-view"));
    EXPECT_TRUE(secondPtr.isNull() || !secondPtr->isVisible())
        << "flyout stayed open after row click following a light-dismiss + reopen";

    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settings.setNavigationStyle(previousStyle);
}
