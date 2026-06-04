#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QPixmap>
#include <QRect>
#include <QStringList>
#include <QtGlobal>

#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "design/Typography.h"
#include "VisualGeometryTestUtils.h"
#include "view/GalleryWindow.h"
#include "view/PlaceholderPage.h"
#include "view/SettingsPage.h"

using fluent::basicinput::Button;
using fluent::gallery::GalleryWindow;
using fluent::gallery::PlaceholderPage;
using fluent::gallery::SettingsPage;
using fluent::textfields::AutoSuggestBox;
using fluent::windowing::TitleBar;
namespace vg = fluent::testutils::visual_geometry;

namespace {

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

} // namespace

class GalleryShellFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(GalleryShellFrameworkTest, WindowConstructsInitialHomePlaceholder)
{
    GalleryWindow window;

    EXPECT_EQ(window.objectName(), QStringLiteral("galleryWindow"));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("WinUI 3 Gallery"));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("home"));
    EXPECT_NE(window.findChild<QWidget*>(QStringLiteral("galleryNavigationView")), nullptr);
    EXPECT_NE(window.findChild<QWidget*>(QStringLiteral("galleryMainNavigationPane")), nullptr);
    EXPECT_NE(window.findChild<QWidget*>(QStringLiteral("galleryFooterNavigationPane")), nullptr);

    auto* searchBox = window.findChild<AutoSuggestBox*>(QStringLiteral("GalleryTitleBar.SearchBox"));
    ASSERT_NE(searchBox, nullptr);
    EXPECT_EQ(searchBox->placeholderText(), QStringLiteral("Search controls and samples..."));

    PlaceholderPage* page = window.currentPlaceholderPage();
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page->routeId(), QStringLiteral("home"));
    ASSERT_NE(page->titleLabel(), nullptr);
    EXPECT_EQ(page->titleLabel()->text(), QStringLiteral("Home"));
}

TEST_F(GalleryShellFrameworkTest, TitleBarContentUsesAnchorsAndCentersControls)
{
    GalleryWindow window;
    window.resize(1180, 760);
    window.show();
    QApplication::processEvents();

    TitleBar* titleBar = window.titleBar();
    ASSERT_NE(titleBar, nullptr);
    EXPECT_EQ(titleBar->titleBarHeight(), TitleBar::defaultTitleBarHeight());

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
    EXPECT_GE(backButton->width(), 24);
    EXPECT_EQ(backButton->font().pixelSize(), Typography::FontSize::Caption);
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

TEST_F(GalleryShellFrameworkTest, NavigationEntriesExposeRequiredGroups)
{
    GalleryWindow window;

    const QStringList titles = window.visibleNavigationTitles();
    EXPECT_TRUE(containsAll(titles, {
        QStringLiteral("Home"),
        QStringLiteral("Fundamentals"),
        QStringLiteral("Design"),
        QStringLiteral("Accessibility"),
        QStringLiteral("Controls"),
        QStringLiteral("Settings")
    }));

    const QStringList routeIds = window.navigationEntryIds();
    EXPECT_TRUE(containsAll(routeIds, {
        QStringLiteral("all-controls"),
        QStringLiteral("basic-input"),
        QStringLiteral("collections"),
        QStringLiteral("date-time"),
        QStringLiteral("dialogs-flyouts"),
        QStringLiteral("layout"),
        QStringLiteral("media"),
        QStringLiteral("menus-toolbars"),
        QStringLiteral("motion"),
        QStringLiteral("navigation"),
        QStringLiteral("scrolling"),
        QStringLiteral("settings")
    }));
}

TEST_F(GalleryShellFrameworkTest, SelectRouteSwitchesPlaceholderContent)
{
    GalleryWindow window;
    PlaceholderPage* homePage = window.currentPlaceholderPage();
    ASSERT_NE(homePage, nullptr);
    const QColor homeColor = homePage->placeholderColor();

    ASSERT_TRUE(window.selectRoute(QStringLiteral("basic-input")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("basic-input"));

    PlaceholderPage* basicInputPage = window.currentPlaceholderPage();
    ASSERT_NE(basicInputPage, nullptr);
    EXPECT_EQ(basicInputPage->routeId(), QStringLiteral("basic-input"));
    EXPECT_EQ(basicInputPage->titleLabel()->text(), QStringLiteral("Basic input"));
    EXPECT_NE(basicInputPage->placeholderColor(), homeColor);

    EXPECT_FALSE(window.selectRoute(QStringLiteral("missing-route")));
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("basic-input"));
}

TEST_F(GalleryShellFrameworkTest, NavigationButtonActivationUpdatesRoute)
{
    GalleryWindow window;

    auto* navigationButton = window.findChild<Button*>(QStringLiteral("galleryNavigation_navigation"));
    ASSERT_NE(navigationButton, nullptr);
    navigationButton->click();
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("navigation"));
    ASSERT_NE(window.currentPlaceholderPage(), nullptr);
    EXPECT_EQ(window.currentPlaceholderPage()->titleLabel()->text(), QStringLiteral("Navigation"));

    auto* settingsButton = window.findChild<Button*>(QStringLiteral("galleryNavigation_settings"));
    ASSERT_NE(settingsButton, nullptr);
    settingsButton->click();
    QApplication::processEvents();

    EXPECT_EQ(window.currentRouteId(), QStringLiteral("settings"));
    ASSERT_NE(window.currentSettingsPage(), nullptr);
    EXPECT_EQ(window.currentSettingsPage()->titleLabel()->text(), QStringLiteral("Settings"));
}
