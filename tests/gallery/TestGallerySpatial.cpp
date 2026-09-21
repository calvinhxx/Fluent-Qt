#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QPainter>
#include <QPaintEngine>
#include <QJsonDocument>
#include <QtMath>
#include <iostream>
#include <QGraphicsProxyWidget>
#include <QGraphicsEffect>
#include <QHelpEvent>
#include <QGraphicsView>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QDir>
#include <QPointer>
#include <QPlatformSurfaceEvent>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QStyleOption>
#include <QLineEdit>
#include <QScopeGuard>
#include <QCheckBox>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QScrollBar>
#include <QScreen>
#include <QWindow>
#include "QtTestEnvironment.h"
#ifdef Q_OS_MAC
#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "view/pages/GalleryCategoryPage.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/GalleryPageFactory.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GalleryWindow.h"
#include "view/shell/GallerySpatialController.h"
#include "view/shell/GallerySpatialRenderPolicy.h"
#include "view/shell/GalleryIntroTour.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "view/shell/GallerySplashScreen.h"
#include "view/shell/GalleryContentPresenter.h"
#include "view/shell/GalleryNavigationPane.h"
#include "components/collections/TreeView.h"
#include "components/windowing/WindowBackdrop.h"
#include "view/support/GalleryDepth.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

using namespace fluent;
using namespace fluent::gallery;
namespace {
class GallerySpatialTest : public ::testing::Test {
protected:
    GallerySettings::ThemeMode oldTheme;
    GallerySettings::MotionMode oldMotion;
    GallerySettings::NavigationStyle oldNavigation;
    windowing::BackdropEffect oldEffect;
    bool oldSpatial, oldIntro, oldAvailable, oldPending, oldParticles;
    QString oldUnavailableReason;
    void SetUp() override
    {
        auto& s = GallerySettings::instance();
        oldAvailable = s.spatialAvailable();
        oldPending = s.spatialAvailabilityPending();
        oldUnavailableReason = s.spatialUnavailableReason();
        oldTheme = s.themeMode();
        oldMotion = s.motionMode();
        oldNavigation = s.navigationStyle();
        oldEffect = s.windowEffect();
        oldSpatial = s.spatialModeEnabled();
        oldIntro = s.introCompleted();
        oldParticles = s.homeParticlesEnabled();
        s.setIntroCompleted(true);
        s.setSpatialAvailability(true);
        s.setSpatialModeEnabled(false);
        s.setThemeMode(GallerySettings::ThemeMode::Light);
        s.setMotionMode(GallerySettings::MotionMode::Full);
    }
    void TearDown() override
    {
        auto& s = GallerySettings::instance();
        s.setSpatialModeEnabled(false);
        s.setThemeMode(oldTheme);
        s.setMotionMode(oldMotion);
        s.setNavigationStyle(oldNavigation);
        s.setWindowEffect(oldEffect);
        s.setSpatialModeEnabled(oldSpatial);
        s.setIntroCompleted(oldIntro);
        s.setHomeParticlesEnabled(oldParticles);
        if (oldPending)
            s.beginSpatialAvailabilityCheck();
        else
            s.setSpatialAvailability(oldAvailable, oldUnavailableReason);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
    }
};
} // namespace

TEST_F(GallerySpatialTest, CategoryOwnsBothComponentsAndPublicReferences)
{
    GalleryNavigationViewModel navigation;
    auto* category = navigation.itemById("spatial");
    ASSERT_NE(category, nullptr);
    EXPECT_EQ(category->kind, GalleryNavigationItem::Kind::CategoryRoute);
    EXPECT_EQ(category->parentId, "controls");
    EXPECT_FALSE(category->iconGlyph.isEmpty());
    EXPECT_NE(category->iconGlyph, navigation.itemById("windowing")->iconGlyph);
    EXPECT_EQ(navigation.itemById("foundation-spatial"), nullptr);
    for (const QString& id : {QStringLiteral("spatial-view"), QStringLiteral("spatial-item")}) {
        ASSERT_NE(navigation.itemById(id), nullptr);
        EXPECT_EQ(navigation.itemById(id)->parentId, "spatial");
        auto reference = galleryComponentReference(id);
        EXPECT_TRUE(reference.isValid());
        EXPECT_TRUE(reference.hasPythonReference());
        EXPECT_FALSE(gallerySamplesForRoute(id).isEmpty());
    }
}

TEST_F(GallerySpatialTest, Explicit2DDoesNotCreateOpenGLSurfaces)
{
    auto& settings = GallerySettings::instance();
    GalleryWindow window;
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.resize(1100, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<GallerySplashScreen*>(), 6500);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage(), 2000);
    settings.setThemeMode(GallerySettings::ThemeMode::Dark);
    window.resize(950, 750);
    QTest::qWait(100);
    EXPECT_TRUE(window.findChildren<QOpenGLWidget*>().isEmpty());
    auto* navigation = window.findChild<navigation::NavigationView*>();
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->contentHost()->graphicsEffect(), nullptr);
    if (!tests::support::isHeadlessPlatform()) {
        EXPECT_TRUE(settings.spatialAvailabilityPending());
        auto* toggle =
            window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
        ASSERT_NE(toggle, nullptr);
        EXPECT_TRUE(toggle->isEnabled());
        const auto firstNativeId = window.winId();
        QPointer<QWindow> firstHandle = window.windowHandle();
        QSignalSpy visibilityChanges(firstHandle, &QWindow::visibleChanged);
        const auto firstGeometry = window.geometry();
        QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(20, toggle->height() / 2));
        QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
        auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
        ASSERT_NE(surface, nullptr);
        QTRY_VERIFY(surface->property("presenting").toBool());
        auto* controller = window.findChild<GallerySpatialController*>();
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
            EXPECT_EQ(window.winId(), firstNativeId);
            EXPECT_EQ(window.windowHandle(), firstHandle);
            EXPECT_TRUE(visibilityChanges.isEmpty())
                << "First activation must not hide/recreate the visible native window";
            EXPECT_EQ(window.geometry(), firstGeometry);
        }
        const auto nativeId = window.winId();
        settings.setSpatialModeEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        EXPECT_TRUE(surface->isHidden());
        EXPECT_FALSE(navigation->graphicsEffect()->isEnabled());
        window.resize(1000, 780);
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        settings.setSpatialModeEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        EXPECT_EQ(window.winId(), nativeId);
        EXPECT_EQ(window.findChild<QOpenGLWidget*>("gallerySpatialSurface"), surface);
        EXPECT_TRUE(surface->isVisible());
    }
}

TEST_F(GallerySpatialTest, DeferredSurfaceInitializationWaitsForLayoutAndCanBeCancelled)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native deferred OpenGL initialization";
    auto& settings = GallerySettings::instance();
    for (const bool cancelWhilePending : {false, true}) {
        SCOPED_TRACE(cancelWhilePending);
        QWidget window;
        window.resize(800, 600);
        auto* navigation = new navigation::NavigationView(&window);
        navigation->setMinimumSize(0, 0);
        navigation->resize(0, 0);
        GallerySpatialController controller(&window, navigation);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        settings.setSpatialModeEnabled(true);
        QPointer<QOpenGLWidget> surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
        ASSERT_NE(surface, nullptr);
        EXPECT_FALSE(surface->isValid());
        QTest::qWait(100);
        ASSERT_NE(surface, nullptr);
        EXPECT_TRUE(settings.spatialAvailabilityPending());
        EXPECT_TRUE(settings.spatialModeEnabled());
        EXPECT_EQ(navigation->graphicsEffect(), nullptr);
        if (cancelWhilePending) {
            settings.setSpatialModeEnabled(false);
            EXPECT_TRUE(surface->isHidden());
        }
        navigation->resize(window.size());
        if (cancelWhilePending) {
            QTest::qWait(100);
            EXPECT_FALSE(surface->isValid());
            EXPECT_EQ(navigation->graphicsEffect(), nullptr);
            settings.setSpatialModeEnabled(true);
        }
        QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
        ASSERT_NE(surface, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 1500);
        EXPECT_TRUE(surface->isValid());
        settings.setSpatialModeEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.transitionRunning(), 1500);
    }
}

TEST_F(GallerySpatialTest, HiddenSurfaceRevalidatesAfterReparenting)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native OpenGL context recreation";
    QWidget desktop;
    QWidget window;
    window.resize(800, 600);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->resize(window.size());
    GallerySpatialController controller(&window, navigation);
    auto& settings = GallerySettings::instance();
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    settings.setSpatialModeEnabled(true);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 3000);
    settings.setSpatialModeEnabled(false);
    QTRY_VERIFY_WITH_TIMEOUT(surface->isHidden(), 1500);
    window.setParent(&desktop, Qt::Widget);
    desktop.resize(800, 600);
    desktop.show();
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&desktop));
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 3000);
    EXPECT_TRUE(surface->isValid());
    EXPECT_TRUE(settings.spatialAvailable());
    EXPECT_EQ(window.findChild<QOpenGLWidget*>("gallerySpatialSurface"), surface);
}

TEST_F(GallerySpatialTest, MissingInitializationCallbackFallsBackAfterWaiting)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires an exposed native window";
    // Model a platform that never services the new surface's initialization events.
    class BlockSurfaceInitialization final : public QObject {
        bool eventFilter(QObject* object, QEvent* event) override
        {
            return object->objectName() == QStringLiteral("gallerySpatialSurface") &&
                   (event->type() == QEvent::Show || event->type() == QEvent::Resize ||
                    event->type() == QEvent::Paint);
        }
    } blocker;
    auto& settings = GallerySettings::instance();
    QWidget window;
    window.resize(800, 600);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->resize(window.size());
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    qApp->installEventFilter(&blocker);
    settings.setSpatialModeEnabled(true);
    QPointer<QOpenGLWidget> surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_FALSE(surface->isValid());
    QTest::qWait(100);
    EXPECT_TRUE(settings.spatialAvailabilityPending());
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(!settings.spatialAvailabilityPending(), 6500);
    EXPECT_FALSE(settings.spatialAvailable());
    EXPECT_FALSE(depth::enabled(&window));
    QTRY_VERIFY(surface.isNull());
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->contentHost()->graphicsEffect(), nullptr);
}

TEST_F(GallerySpatialTest, SupportBadgesReceiveHoverAtTheirProjectedPositions)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1180, 820);
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget*>("gallerySplashScreen"), 6500);
    ASSERT_TRUE(settings.spatialAvailable());
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage(), 2000);
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
    const auto directory = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    for (auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
        settings.setThemeMode(theme);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        for (const auto* name :
             {"gallerySettingsSpatialSupportBadge", "galleryNavigationSpatialSupportBadge"}) {
            auto* badge = window.findChild<status_info::InfoBadge*>(name);
            ASSERT_NE(badge, nullptr);
            QTRY_VERIFY(badge->isVisible());
            auto* tooltip = badge->findChild<status_info::ToolTip*>();
            ASSERT_NE(tooltip, nullptr);
            tooltip->setAnimationEnabled(false);
            window.raise();
            window.activateWindow();
            QTest::mouseMove(&window, QPoint(window.width() / 2, 25));
            QTest::mouseMove(&window, controller->projectedPosition(badge, badge->rect().center()));
            QTRY_VERIFY_WITH_TIMEOUT(tooltip->isVisible(), 3000);
            const auto point = controller->projectedPosition(badge, badge->rect().center());
            QHelpEvent help(QEvent::ToolTip, point, window.mapToGlobal(point));
            QApplication::sendEvent(&window, &help);
            EXPECT_TRUE(tooltip->isVisible());
            EXPECT_TRUE(tooltip->text().contains("GPU"));
            EXPECT_LT(qAbs(tooltip->geometry().center().x() - window.mapToGlobal(point).x()), 40);
            EXPECT_LT(qAbs(tooltip->geometry().bottom() - window.mapToGlobal(point).y()), 60);
            if (!directory.isEmpty()) {
                QDir().mkpath(directory);
                QTest::qWait(100);
                const auto stem = QStringLiteral("%1-%2").arg(name).arg(int(theme));
                window.screen()->grabWindow(window.winId()).save(directory + "/" + stem + ".png");
                tooltip->grab().save(directory + "/" + stem + "-tooltip.png");
            }
            tooltip->hide();
        }
    }
}

TEST_F(GallerySpatialTest, IsolatedPreviewDoesNotChangePersistentGalleryMode)
{
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    QWidget host;
    host.setProperty("galleryPreviewRouteId", "spatial-view");
    const auto sample = gallerySamplesForRoute("spatial-view").first();
    auto* panel = sample.createPreview(&host);
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(panel->findChild<QWidget*>("spatialPreviewMode"), nullptr);
    EXPECT_FALSE(view->isSpatialEnabled());
    view->setSpatialEnabled(true);
    view->setSpatialEnabled(false);
    EXPECT_TRUE(settings.spatialModeEnabled());
}

TEST_F(GallerySpatialTest, SettingsSwitchSynchronizesWithoutDuplicateSignals)
{
    GallerySettings::instance().setSpatialAvailability(true);
    GalleryNavigationItem route;
    route.id = "settings";
    route.title = "Settings";
    SettingsPage page(route);
    page.resize(1000, 900);
    page.show();
    QApplication::processEvents();
    auto* toggle = page.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    ASSERT_NE(toggle, nullptr);
    auto& settings = GallerySettings::instance();
    QSignalSpy changed(&settings, &GallerySettings::spatialModeEnabledChanged);
    QTest::mouseClick(toggle, Qt::LeftButton);
    EXPECT_TRUE(settings.spatialModeEnabled());
    EXPECT_TRUE(toggle->isOn());
    EXPECT_EQ(changed.count(), 1);
    settings.setSpatialModeEnabled(true);
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(GallerySpatialTest, SettingsModeSynchronizesExistingHiddenAndNewPreviews)
{
    auto& settings = GallerySettings::instance();
    QWidget host;
    const auto samples = gallerySamplesForRoute("spatial-view");
    auto* first = samples.first().createPreview(&host);
    auto* second = samples.at(1).createPreview(&host);
    auto* firstView = first->findChild<spatial::SpatialView*>("spatialPreviewView");
    auto* secondView = second->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(firstView, nullptr);
    ASSERT_NE(secondView, nullptr);
    EXPECT_EQ(host.findChild<QWidget*>("spatialPreviewMode"), nullptr);
    EXPECT_FALSE(firstView->isSpatialEnabled());
    EXPECT_FALSE(secondView->isSpatialEnabled());
    firstView->setCameraDistance(1900);
    auto* chart = secondView->items().last()->widget()->findChild<charts::Sparkline*>();
    ASSERT_NE(chart, nullptr);
    const auto* model = chart->model();
    QSignalSpy globalChanges(&settings, &GallerySettings::spatialModeEnabledChanged);
    settings.setSpatialModeEnabled(true);
    EXPECT_TRUE(firstView->isSpatialEnabled());
    EXPECT_TRUE(secondView->isSpatialEnabled());
    auto* third = samples.at(2).createPreview(&host);
    auto* thirdView = third->findChild<spatial::SpatialView*>();
    EXPECT_TRUE(thirdView->isSpatialEnabled());
    settings.setSpatialAvailability(false, "Test unavailable renderer");
    for (auto* view : {firstView, secondView, thirdView})
        EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_TRUE(settings.spatialModeEnabled()); // Retain the saved preference.
    settings.setSpatialAvailability(true);
    for (auto* view : {firstView, secondView, thirdView})
        EXPECT_TRUE(view->isSpatialEnabled());
    settings.setSpatialModeEnabled(false);
    for (auto* view : {firstView, secondView, thirdView})
        EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_EQ(firstView->cameraDistance(), 1900);
    EXPECT_EQ(chart->model(), model);
    EXPECT_EQ(globalChanges.count(), 2);
    settings.setSpatialModeEnabled(true);
    // A native-input fallback from any view also restores the shell and other samples.
    QTest::keyClick(firstView->findChild<QGraphicsView*>(), Qt::Key_Escape);
    EXPECT_FALSE(settings.spatialModeEnabled());
    EXPECT_FALSE(secondView->isSpatialEnabled());
    EXPECT_FALSE(thirdView->isSpatialEnabled());
}

TEST_F(GallerySpatialTest, AdditionalCombinationsAreDisclosedAndShareTheGlobalMode)
{
    GalleryNavigationViewModel navigation;
    const auto* entry = galleryContentEntry("spatial-view");
    ASSERT_NE(entry, nullptr);
    GalleryComponentPage page(*entry, navigation);
    auto* more = page.findChild<layout::Expander*>("galleryMoreSpatialExamples");
    ASSERT_NE(more, nullptr);
    EXPECT_FALSE(more->isExpanded());
    EXPECT_EQ(more->findChildren<spatial::SpatialView*>().size(), 6);
    EXPECT_EQ(page.findChildren<spatial::SpatialView*>().size(), 10);
    auto* chart = page.findChild<charts::DonutChart*>("spatialAllocationChart");
    ASSERT_NE(chart, nullptr);
    EXPECT_FALSE(more->isAncestorOf(chart));
    auto* allocation = page.findChild<basicinput::Slider*>("spatialAllocationSlider");
    ASSERT_NE(allocation, nullptr);
    allocation->setValue(72);
    EXPECT_EQ(chart->centerText(), "72%");
    EXPECT_EQ(page.findChild<QWidget*>("spatialPreviewMode"), nullptr);
    GallerySettings::instance().setSpatialModeEnabled(true);
    for (auto* view : page.findChildren<spatial::SpatialView*>())
        EXPECT_TRUE(view->isSpatialEnabled());
    more->setExpandedAnimated(true, false);
    EXPECT_TRUE(more->isExpanded());
    GallerySettings::instance().setSpatialModeEnabled(false);
    for (auto* view : more->findChildren<spatial::SpatialView*>())
        EXPECT_FALSE(view->isSpatialEnabled());
    auto* link = page.findChild<basicinput::Button*>("gallerySpatialSettingsLink");
    ASSERT_NE(link, nullptr);
    QSignalSpy route(&page, &GalleryContentPage::routeActivated);
    link->click();
    ASSERT_EQ(route.count(), 1);
    EXPECT_EQ(route.first().first().toString(), "settings");
}

TEST_F(GallerySpatialTest, EntryGridDepthPreservesClickTargetsAndStopsWhenIdle)
{
    QWidget host;
    auto* column = new QVBoxLayout(&host);
    auto* grid = new GalleryEntryGrid(&host);
    grid->setEntries(
        {{"button", "Button", "Open the button examples", {}, Typography::Icons::Add},
         {"slider", "Slider", "Open the slider examples", {}, Typography::Icons::Settings}});
    column->addWidget(grid);
    host.resize(700, 180);
    host.show();
    QApplication::processEvents();
    const QImage flat = grid->grab().toImage();
    depth::setEnabled(&host, true);
    QApplication::processEvents();
    EXPECT_NE(flat, grid->grab().toImage());
    // Entries remain painted data, with no widget/proxy/GL view per card.
    EXPECT_TRUE(grid->findChildren<QWidget*>().isEmpty());
    auto* motion = grid->findChild<QVariantAnimation*>("galleryEntryDepthMotion");
    ASSERT_NE(motion, nullptr);
    QSignalSpy activated(grid, &GalleryEntryGrid::activated);
    QTest::mouseMove(grid, QPoint(130, 35));
    QTRY_COMPARE_WITH_TIMEOUT(motion->state(), QAbstractAnimation::Stopped, 500);
    QTest::mouseClick(grid, Qt::LeftButton, Qt::NoModifier, QPoint(130, 35));
    ASSERT_EQ(activated.count(), 1);
    EXPECT_EQ(activated.at(0).at(0).toString(), "button");
    QTest::mouseClick(grid, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    EXPECT_EQ(activated.count(), 1); // The projected card's transparent corner is not a target.
    depth::setEnabled(&host, false);
    EXPECT_EQ(motion->state(), QAbstractAnimation::Stopped);
    QTest::mouseClick(grid, Qt::LeftButton, Qt::NoModifier, QPoint(450, 35));
    ASSERT_EQ(activated.count(), 2);
    EXPECT_EQ(activated.at(1).at(0).toString(), "slider");
}

TEST_F(GallerySpatialTest, FlatPreviewKeepsDarkCanvasInsideStyledSampleCard)
{
    GallerySettings::instance().setThemeMode(GallerySettings::ThemeMode::Dark);
    GallerySampleCard card("spatial-view", gallerySamplesForRoute("spatial-view").first());
    card.resize(900, 900);
    card.show();
    auto* view = card.findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(view, nullptr);
    ASSERT_FALSE(view->isSpatialEnabled());
    for (const bool enabled : {false, true}) {
        depth::setEnabled(&card, enabled);
        QApplication::processEvents();
        const QPixmap canvas = view->grab();
        const int inset = qRound(12 * canvas.devicePixelRatioF());
        EXPECT_EQ(canvas.toImage().pixelColor(inset, inset), view->themeColors().bgCanvas)
            << "Gallery depth=" << enabled;
    }
}

TEST_F(GallerySpatialTest, GalleryAssemblyCancelsOnInputResizeAndAccessibilityChanges)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1100, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
    auto* overlay = window.findChild<QWidget*>("gallerySpatialSurface");
    ASSERT_NE(overlay, nullptr);
    // Splash destruction queues the compositor attachment for the next event turn.
    QTRY_VERIFY_WITH_TIMEOUT(overlay->property("presenting").isValid(), 1000);
    QWidget* home = window.currentContentPage();
    const auto nativeId = window.winId();
    settings.setSpatialModeEnabled(true);
    EXPECT_TRUE(depth::enabled(home));
    EXPECT_TRUE(controller->transitionRunning());
    EXPECT_EQ(overlay->parentWidget(), &window);
    EXPECT_EQ(overlay->geometry().size(), window.findChild<navigation::NavigationView*>()->size());
    QTest::keyClick(&window, Qt::Key_Escape);
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_TRUE(settings.spatialModeEnabled());
    EXPECT_EQ(window.currentContentPage(), home);
    EXPECT_EQ(window.winId(), nativeId);
    settings.setSpatialModeEnabled(false);
    EXPECT_TRUE(controller->transitionRunning());
    window.resize(900, 700);
    EXPECT_FALSE(controller->transitionRunning());
    settings.setSpatialModeEnabled(true);
    settings.setMotionMode(GallerySettings::MotionMode::Reduced);
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_FALSE(depth::enabled(home));
    settings.setMotionMode(GallerySettings::MotionMode::Full);
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_EQ(window.winId(), nativeId);
    settings.setThemeMode(GallerySettings::ThemeMode::HighContrast);
    EXPECT_FALSE(depth::enabled(home));
}

TEST_F(GallerySpatialTest, GalleryAssemblyPreservesMaterialAndNativeSurface)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1100, 860);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    auto& settings = GallerySettings::instance();
    // The first opt-in can replace Qt's raster backing store. From then on, reuse
    // that native surface across mode, material and navigation changes.
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 1000);
    controller->cancelTransition();
    settings.setSpatialModeEnabled(false);
    controller->cancelTransition();
    const auto nativeId = window.winId();
    const auto frame = [](QWidget* widget) {
        QImage image(widget->size() * widget->devicePixelRatioF(),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(widget->devicePixelRatioF());
        image.fill(Qt::transparent);
        widget->render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
        if (auto* gl = widget->findChild<QOpenGLWidget*>("gallerySpatialSurface")) {
            if (gl->property("presenting").toBool()) {
                QPainter painter(&image);
                painter.drawImage(QRect(gl->mapTo(widget, QPoint()), gl->size()),
                                  gl->grabFramebuffer());
            }
        }
        return image;
    };
    const auto save = [&](const QImage& image, const QString& name) {
        const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
        if (!dir.isEmpty()) {
            QDir().mkpath(dir);
            image.save(dir + '/' + name + ".png");
        }
    };
    using Theme = GallerySettings::ThemeMode;
    using Navigation = GallerySettings::NavigationStyle;
    using Effect = windowing::BackdropEffect;
    struct MaterialCase {
        Theme theme;
        Navigation navigation;
        Effect effect;
    };
    for (const MaterialCase scenario :
         {MaterialCase{Theme::Light, Navigation::Left, Effect::Solid},
          MaterialCase{Theme::Light, Navigation::Left, Effect::Mica},
          MaterialCase{Theme::Light, Navigation::Left, Effect::Acrylic},
          MaterialCase{Theme::Dark, Navigation::Left, Effect::Mica},
          MaterialCase{Theme::Light, Navigation::Top, Effect::Mica},
          MaterialCase{Theme::Dark, Navigation::Top, Effect::Acrylic}}) {
        const QString name = QStringLiteral("gallery-material-%1-%2-%3")
                                 .arg(int(scenario.theme))
                                 .arg(int(scenario.navigation))
                                 .arg(int(scenario.effect));
        SCOPED_TRACE(name.toStdString());
        settings.setSpatialModeEnabled(false);
        controller->cancelTransition();
        settings.setThemeMode(scenario.theme);
        settings.setNavigationStyle(scenario.navigation);
        settings.setWindowEffect(scenario.effect);
        QTest::qWait(250);
        QApplication::processEvents();
        const auto state = windowing::windowBackdropState(&window);
        const QImage before = frame(&window);
        save(before, name + "-before");
        // Empty chrome below the last navigation row, away from the new rim/shadows.
        const QPoint gap(160 * window.devicePixelRatioF(), 770 * window.devicePixelRatioF());
        settings.setSpatialModeEnabled(true);
        auto* overlay = window.findChild<QWidget*>("gallerySpatialSurface");
        ASSERT_NE(overlay, nullptr);
        auto* motion = controller->findChild<QVariantAnimation*>("galleryAssemblyAnimation");
        ASSERT_NE(motion, nullptr);
        motion->pause();
        motion->setCurrentTime(qRound(motion->duration() * 0.34));
        QApplication::processEvents();
        const QImage exploded = frame(&window);
        save(exploded, name + "-assembly");
        // In Top mode the old left gutter belongs to the content surface. Sample the
        // unpainted edge of the top navigation region instead, in both 2D and 3D.
        const int exposedY =
            scenario.navigation == Navigation::Top
                ? window.findChild<navigation::NavigationView*>()->mapTo(&window, QPoint(0, 1)).y()
                : 400;
        const QPoint exposed(0, exposedY * window.devicePixelRatioF());
        if (state.surfaceMode == windowing::BackdropSurfaceMode::CompositedTransparent) {
            if (scenario.navigation == Navigation::Left)
                EXPECT_EQ(before.pixelColor(gap).alpha(), 0);
            EXPECT_EQ(exploded.pixelColor(exposed).alpha(), 0)
                << "The exposed background must still reach native Mica/Acrylic";
        } else {
            EXPECT_EQ(exploded.pixelColor(exposed).alpha(), 255);
        }
        motion->setCurrentTime(motion->duration());
        controller->cancelTransition();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        const QImage after = frame(&window);
        save(after, name + "-after");
        // The permanent 3D pose also leaves a transparent outer gutter.
        EXPECT_EQ(before.pixelColor(exposed), after.pixelColor(exposed));
        EXPECT_GT(overlay->property("galleryNavigationRotation").toReal(), 0);
        EXPECT_EQ(overlay->property("galleryNavigationRotation").toReal(),
                  -overlay->property("galleryContentRotation").toReal());
        const auto afterState = windowing::windowBackdropState(&window);
        EXPECT_EQ(afterState.effectiveEffect, state.effectiveEffect);
        EXPECT_EQ(afterState.surfaceMode, state.surfaceMode);
        EXPECT_EQ(window.winId(), nativeId);
        EXPECT_FALSE(controller->transitionRunning());
    }
}

TEST_F(GallerySpatialTest, NavigationDepthRetainsRouteTargets)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    // Use real Gallery routes and the real delegate, including the compact rail.
    GalleryWindow window;
    window.resize(1100, 860);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto* pane = window.findChild<GalleryNavigationPane*>("galleryMainNavigationPane");
    ASSERT_NE(pane, nullptr);
    auto* tree = pane->findChild<collections::TreeView*>();
    ASSERT_NE(tree, nullptr);
    auto* controller = window.findChild<GallerySpatialController*>();
    const QImage before = tree->viewport()->grab().toImage();
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    QApplication::processEvents();
    EXPECT_NE(before, tree->viewport()->grab().toImage());
    for (const bool compact : {false, true}) {
        pane->setCompact(compact);
        const auto index = pane->indexForRouteId(compact ? "home" : "charts");
        ASSERT_TRUE(index.isValid());
        tree->scrollTo(index);
        QApplication::processEvents();
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                          controller->projectedPosition(
                              tree->viewport(), QPoint(26, tree->visualRect(index).center().y())));
        EXPECT_EQ(window.currentRouteId(), compact ? "home" : "charts");
    }
}

TEST_F(GallerySpatialTest, FloatingNavigationKeepsIconsAndLabelsOnOneSurface)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    GalleryWindow window;
    window.resize(800, 850);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto* navigation = window.findChild<navigation::NavigationView*>();
    auto* pane = window.findChild<GalleryNavigationPane*>("galleryMainNavigationPane");
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(navigation, nullptr);
    ASSERT_NE(pane, nullptr);
    ASSERT_NE(controller, nullptr);
    navigation->setDisplayMode(navigation::NavigationView::DisplayMode::LeftCompact);
    navigation->setAnimationEnabled(false);
    QApplication::processEvents();
    auto* tree = pane->findChild<collections::TreeView*>();
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(navigation->effectiveDisplayMode(),
              navigation::NavigationView::DisplayMode::LeftCompact);
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(navigation->graphicsEffect(), 2000);
    controller->cancelTransition();
    ASSERT_TRUE(navigation->graphicsEffect()->isEnabled());

    for (const int clickX : {26, 115}) {
        ASSERT_TRUE(window.selectRoute("settings"));
        navigation->setPaneOpen(true);
        QApplication::processEvents();
        ASSERT_GT(pane->width(), navigation->contentGeometry().left());
        ASSERT_FALSE(pane->isCompact());
        const auto home = pane->indexForRouteId("home");
        tree->scrollTo(home);
        QApplication::processEvents();
        const int y = tree->visualRect(home).center().y();
        const auto point = [&](int x) {
            return controller->projectedPosition(tree->viewport(), QPoint(x, y));
        };
        const QPoint a = point(26), b = point(90), c = point(180);
        const qreal distance =
            qAbs(qreal((b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x()))) /
            QLineF(a, c).length();
        EXPECT_LT(distance, 2.0) << "One navigation row must stay on one projected line";
        EXPECT_LT(QLineF(a, c).length(), 180.0) << "No split between the icon rail and labels";
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point(clickX));
        EXPECT_EQ(window.currentRouteId(), "home");
    }

    navigation->setPaneOpen(true);
    QApplication::processEvents();
    const QPoint outside = controller->projectedPosition(
        navigation->contentHost(), QPoint(navigation->contentHost()->width() - 30, 100));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, outside);
    EXPECT_FALSE(navigation->isPaneOpen());
    EXPECT_EQ(window.currentRouteId(), "home");
    navigation->setPaneOpen(true);
    QTest::keyClick(&window, Qt::Key_Escape);
    EXPECT_FALSE(navigation->isPaneOpen());
    settings.setSpatialModeEnabled(false);
    QTRY_VERIFY_WITH_TIMEOUT(!navigation->contentHost()->graphicsEffect()->isEnabled(), 1500);
}

TEST_F(GallerySpatialTest, ComposedControlsReceiveProjectedInputAndKeepStateIn2D)
{
    auto samples = gallerySamplesForRoute("spatial-view");
    const auto sample = std::find_if(samples.cbegin(), samples.cend(), [](const auto& value) {
        return value.id == "spatial-view-cards";
    });
    ASSERT_NE(sample, samples.cend());
    std::unique_ptr<QWidget> panel(sample->createPreview(nullptr));
    panel->resize(640, 440);
    panel->show();
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(view, nullptr);
    ASSERT_EQ(view->itemCount(), 2);
    view->setMaximumTilt(QPointF(0, 0));
    auto* card = view->items().first()->widget();
    auto* toggle = card->findChild<basicinput::ToggleSwitch*>();
    ASSERT_NE(toggle, nullptr);
    view->setSpatialEnabled(true);
    QTest::qWait(60);
    auto* canvas = view->findChild<QGraphicsView*>();
    auto* proxy = card->graphicsProxyWidget();
    ASSERT_NE(canvas, nullptr);
    ASSERT_NE(proxy, nullptr);
    QSignalSpy toggled(toggle, &basicinput::ToggleSwitch::toggled);
    const auto clickToggle = [&](const QPoint& local) {
        const QPointF point = toggle->mapTo(card, local);
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                          canvas->mapFromScene(proxy->mapToScene(point)));
    };
    clickToggle(QPoint(20, toggle->height() / 2));
    EXPECT_FALSE(toggle->isOn());
    EXPECT_EQ(toggled.count(), 1);
    clickToggle(QPoint(80, toggle->height() / 2));
    EXPECT_TRUE(toggle->isOn());
    EXPECT_EQ(toggled.count(), 2);
    clickToggle(QPoint(20, toggle->height() / 2));
    EXPECT_FALSE(toggle->isOn());
    EXPECT_EQ(toggled.count(), 3);
    QTest::keyClick(canvas, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!view->isSpatialEnabled(), 1000);
    EXPECT_EQ(view->items().first()->widget(), card);
    EXPECT_FALSE(toggle->isOn());
    QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(20, toggle->height() / 2));
    EXPECT_TRUE(toggle->isOn());
    EXPECT_EQ(toggled.count(), 4);
}

TEST_F(GallerySpatialTest, IndependentCardsKeepTheSameChartModelAcrossModeChanges)
{
    auto samples = gallerySamplesForRoute("spatial-view");
    const auto sample = std::find_if(samples.cbegin(), samples.cend(), [](const auto& value) {
        return value.id == "spatial-view-cards";
    });
    ASSERT_NE(sample, samples.cend());
    std::unique_ptr<QWidget> panel(sample->createPreview(nullptr));
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_EQ(view->itemCount(), 2);
    auto* chart = view->items().last()->widget()->findChild<charts::Sparkline*>("spatialDemoChart");
    ASSERT_NE(chart, nullptr);
    const auto* model = chart->model();
    ASSERT_NE(model, nullptr);
    EXPECT_GT(view->items().last()->position().z(), view->items().first()->position().z());
    view->setSpatialEnabled(true);
    view->setSpatialEnabled(false);
    EXPECT_EQ(chart->model(), model);
    EXPECT_EQ(model->rowCount(), 6);
}

TEST_F(GallerySpatialTest, ViewWorkbenchChangesProjectionAndPreservesSceneSettings)
{
    const auto sample = gallerySamplesForRoute("spatial-view").first();
    ASSERT_EQ(sample.id, "spatial-view-scene");
    std::unique_ptr<QWidget> panel(sample.createPreview(nullptr));
    panel->resize(900, 780);
    panel->show();
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    auto* controls = panel->findChild<QWidget*>("spatialViewControls");
    auto* distance = panel->findChild<basicinput::Slider*>("spatialViewDistance");
    auto* zoom = panel->findChild<basicinput::Slider*>("spatialViewZoom");
    auto* follow = panel->findChild<basicinput::ToggleSwitch*>("spatialViewFollow");
    ASSERT_NE(view, nullptr);
    ASSERT_NE(controls, nullptr);
    ASSERT_NE(follow, nullptr);
    for (auto* slider : {distance, zoom})
        ASSERT_NE(slider, nullptr);
    EXPECT_FALSE(controls->isEnabled());
    GallerySettings::instance().setSpatialModeEnabled(true);
    follow->setIsOn(false);
    QTest::qWait(60);
    EXPECT_TRUE(controls->isEnabled());
    EXPECT_FALSE(view->isAncestorOf(controls));
    ASSERT_EQ(view->itemCount(), 2);
    auto* back = view->items().first();
    auto* front = view->items().last();
    const auto backPose = back->position();
    const auto frontPose = front->position();
    const auto relativeWidth = [&] {
        return front->projectedPolygon().boundingRect().width() /
               back->projectedPolygon().boundingRect().width();
    };
    distance->setValue(2400);
    const qreal farRatio = relativeWidth();
    distance->setValue(650);
    EXPECT_GT(relativeWidth(), farRatio + 0.2);
    EXPECT_EQ(back->position(), backPose);
    EXPECT_EQ(front->position(), frontPose);

    zoom->setValue(50);
    const qreal smallWidth = front->projectedPolygon().boundingRect().width();
    const QPoint dragOrigin = zoom->mapTo(panel.get(), QPoint());
    const QPoint start(zoom->handleSize() / 2, zoom->height() / 2);
    const QPoint finish(zoom->width() * 3 / 4, zoom->height() / 2);
    QTest::mousePress(zoom, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(zoom, finish);
    QTest::mouseRelease(zoom, Qt::LeftButton, Qt::NoModifier, finish);
    EXPECT_GT(zoom->value(), 75);
    EXPECT_GT(front->projectedPolygon().boundingRect().width(), smallWidth * 1.4);
    EXPECT_EQ(zoom->mapTo(panel.get(), QPoint()), dragOrigin);
    EXPECT_FALSE(view->isPointerTrackingEnabled());
    const int zoomBeforeModeChange = zoom->value();
    GallerySettings::instance().setSpatialModeEnabled(false);
    EXPECT_FALSE(controls->isEnabled());
    EXPECT_EQ(distance->value(), 650);
    EXPECT_EQ(zoom->value(), zoomBeforeModeChange);
    EXPECT_EQ(view->items().first(), back);
    GallerySettings::instance().setSpatialModeEnabled(true);
    EXPECT_EQ(view->cameraDistance(), 650);
    EXPECT_DOUBLE_EQ(view->zoom(), zoomBeforeModeChange / 100.0);
    EXPECT_EQ(view->maximumTilt(), QPointF(6, 12));
    EXPECT_EQ(view->responseTime(), 140);
    EXPECT_EQ(view->maximumFrameRate(), 60);
    EXPECT_FALSE(view->isPointerTrackingEnabled());
    EXPECT_TRUE(view->isCacheEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(view->renderMode(), spatial::SpatialView::RenderMode::Auto, 1000);
    follow->setIsOn(false);
    distance->setValue(650);
    zoom->setValue(110);
    for (int width : {900, 520}) {
        panel->resize(width, 780);
        QTest::qWait(40);
        for (auto* item : view->items()) {
            const auto bounds = item->projectedPolygon().boundingRect();
            EXPECT_TRUE(QRectF(view->rect()).contains(bounds)) << width;
        }
        EXPECT_TRUE(panel->rect().contains(controls->geometry()));
    }
}

TEST_F(GallerySpatialTest, ComponentCompositionsHandleProjectedPointerInput)
{
    const auto samples = gallerySamplesForRoute("spatial-view");
    for (const QString& id :
         {QStringLiteral("spatial-view-inputs"), QStringLiteral("spatial-view-list"),
          QStringLiteral("spatial-view-calendar"), QStringLiteral("spatial-view-navigation"),
          QStringLiteral("spatial-view-rating"), QStringLiteral("spatial-view-tree"),
          QStringLiteral("spatial-view-donut"), QStringLiteral("spatial-view-hybrid")}) {
        SCOPED_TRACE(id.toStdString());
        const auto sample = std::find_if(samples.cbegin(), samples.cend(),
                                         [&id](const auto& value) { return value.id == id; });
        ASSERT_NE(sample, samples.cend());
        std::unique_ptr<QWidget> panel(sample->createPreview(nullptr));
        panel->resize(680, 560);
        panel->show();
        auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
        ASSERT_NE(view, nullptr);
        view->setPointerTrackingEnabled(false);
        view->setSpatialEnabled(true);
        QTest::qWait(40);
        auto* card = view->items().first()->widget();
        auto* canvas = view->findChild<QGraphicsView*>();
        ASSERT_NE(card->graphicsProxyWidget(), nullptr);
        const auto projected = [&](QWidget* widget, QPoint local) {
            return canvas->mapFromScene(
                card->graphicsProxyWidget()->mapToScene(widget->mapTo(card, local)));
        };
        const auto click = [&](QWidget* widget, QPoint local) {
            const QPoint point = projected(widget, local);
            ASSERT_TRUE(canvas->viewport()->rect().contains(point));
            QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        };
        if (id.endsWith("inputs")) {
            auto* level = card->findChild<basicinput::Slider*>("spatialLevelSlider");
            auto* mute = card->findChild<basicinput::CheckBox*>("spatialMuteCheckBox");
            auto* meter = card->findChild<status_info::ProgressBar*>("spatialLevelMeter");
            click(mute, QPoint(12, mute->height() / 2));
            EXPECT_TRUE(mute->isChecked());
            EXPECT_EQ(meter->value(), 0);
            const auto from = projected(level, QPoint(level->width() / 2, level->height() / 2));
            const auto to = projected(level, QPoint(level->width() * 3 / 4, level->height() / 2));
            QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, from);
            QTest::mouseMove(canvas->viewport(), to);
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, to);
            EXPECT_GT(level->value(), 60);
            EXPECT_EQ(meter->value(), 0);
            click(mute, QPoint(12, mute->height() / 2));
            EXPECT_EQ(meter->value(), level->value());
            view->setSpatialEnabled(false);
            EXPECT_EQ(meter->value(), level->value());
        } else if (id.endsWith("list")) {
            auto* list = card->findChild<collections::ListView*>("spatialTaskList");
            auto* done = card->findChild<basicinput::Button*>("spatialTaskDone");
            auto* count = card->findChild<status_info::InfoBadge*>("spatialTaskCount");
            const auto* model = list->model();
            QSignalSpy clicked(list, &QListView::clicked);
            click(list->viewport(),
                  static_cast<QListView*>(list)->visualRect(model->index(1, 0)).center());
            EXPECT_EQ(list->selectedIndex(), 1);
            EXPECT_EQ(clicked.count(), 1);
            click(done, done->rect().center());
            EXPECT_EQ(model->rowCount(), 2);
            EXPECT_EQ(count->value(), 2);
            EXPECT_EQ(model->index(1, 0).data().toString(), "Try dark mode");
            view->setSpatialEnabled(false);
            EXPECT_EQ(list->model(), model);
            QTest::mouseClick(done, Qt::LeftButton);
            QTest::mouseClick(done, Qt::LeftButton);
            EXPECT_EQ(model->rowCount(), 0);
            EXPECT_FALSE(done->isEnabled());
        } else if (id.endsWith("calendar")) {
            auto* calendar = card->findChild<date_time::CalendarView*>("spatialCalendar");
            auto* selected = card->findChild<textfields::Label*>("spatialSelectedDate");
            EXPECT_TRUE(calendar->rect().contains(calendar->gridRect()));
            EXPECT_GT(selected->geometry().top(), calendar->geometry().bottom());
            QSignalSpy changed(calendar, &date_time::CalendarView::selectedDateChanged);
            click(calendar, calendar->dateCellRect(QDate(2026, 9, 17)).center());
            EXPECT_EQ(calendar->selectedDate(), QDate(2026, 9, 17));
            EXPECT_EQ(changed.count(), 1);
            EXPECT_EQ(selected->text(), "Selected: 2026-09-17");
            view->setSpatialEnabled(false);
            EXPECT_EQ(calendar->selectedDate(), QDate(2026, 9, 17));
        } else if (id.endsWith("rating")) {
            auto* vivid = card->findChild<basicinput::RadioButton*>("spatialVividPreset");
            auto* rating = card->findChild<basicinput::RatingControl*>("spatialPresetRating");
            auto* result = card->findChild<textfields::Label*>("spatialPresetResult");
            click(vivid, QPoint(10, vivid->height() / 2));
            EXPECT_TRUE(vivid->isChecked());
            EXPECT_EQ(result->text(), "Vivid selected");
            click(rating, QPoint(8, 8));
            EXPECT_EQ(rating->value(), 1);
            EXPECT_EQ(rating->caption(), "1 / 5");
            view->setSpatialEnabled(false);
            EXPECT_EQ(rating->value(), 1);
        } else if (id.endsWith("tree")) {
            auto* tree = card->findChild<collections::TreeView*>("spatialFileTree");
            auto* selected = card->findChild<textfields::Label*>("spatialSelectedFile");
            auto* model = tree->model();
            const auto root = model->index(0, 0);
            const auto child = model->index(1, 0, root);
            click(tree->viewport(), tree->visualRect(child).center());
            EXPECT_EQ(tree->currentIndex(), child);
            EXPECT_EQ(selected->text(), "window.cpp");
            click(tree->viewport(), tree->visualRect(root).center());
            QTRY_VERIFY_WITH_TIMEOUT(!tree->isExpanded(root), 1000);
            view->setSpatialEnabled(false);
            EXPECT_EQ(tree->model(), model);
            EXPECT_FALSE(tree->isExpanded(root));
        } else if (id.endsWith("donut")) {
            auto* chart = card->findChild<charts::DonutChart*>("spatialAllocationChart");
            auto* slider = card->findChild<basicinput::Slider*>("spatialAllocationSlider");
            auto* model = chart->model();
            click(slider, QPoint(slider->width() / 4, slider->height() / 2));
            EXPECT_LT(slider->value(), 50);
            EXPECT_EQ(model->pointAt(0).y(), slider->value());
            EXPECT_EQ(chart->centerText(), QString("%1%").arg(slider->value()));
            view->setSpatialEnabled(false);
            EXPECT_EQ(chart->model(), model);
        } else if (id.endsWith("hybrid")) {
            auto* choice = panel->findChild<basicinput::ComboBox*>("spatialExportChoice");
            auto* format = card->findChild<textfields::Label*>("spatialExportFormat");
            auto* filename = card->findChild<textfields::Label*>("spatialExportFilename");
            ASSERT_NE(filename, nullptr);
            EXPECT_FALSE(view->isAncestorOf(choice));
            choice->setCurrentIndex(2);
            EXPECT_EQ(format->text(), "SVG");
            EXPECT_EQ(filename->text(), "gallery-preview.svg");
            view->setSpatialEnabled(false);
            EXPECT_EQ(choice->currentIndex(), 2);
        } else {
            auto* tabs = card->findChild<navigation::SelectorBar*>("spatialProfileTabs");
            auto* pages = card->findChild<navigation::StackContentHost*>("spatialProfilePages");
            auto* title = card->findChild<textfields::Label*>("spatialProfileTitle");
            auto* name = panel->findChild<textfields::LineEdit*>("spatialProfileName");
            EXPECT_EQ(pages->currentIndex(), 0);
            EXPECT_TRUE(pages->pageWidget(0)->isVisible());
            click(tabs, tabs->itemGeometry(1).center());
            EXPECT_EQ(tabs->selectedIndex(), 1);
            EXPECT_EQ(pages->currentIndex(), 1);
            EXPECT_FALSE(view->isAncestorOf(name));
            name->selectAll();
            QTest::keyClicks(name, "Design lab");
            EXPECT_EQ(title->text(), "Design lab");
            view->setSpatialEnabled(false);
            EXPECT_EQ(pages->currentIndex(), 1);
        }
    }
}

TEST_F(GallerySpatialTest, ScrolledOutPreviewsReleaseGpuViewports)
{
    scrolling::ScrollView scroll;
    auto* content = new QWidget;
    auto* column = new QVBoxLayout(content);
    const auto samples = gallerySamplesForRoute("spatial-view");
    column->addWidget(samples.first().createPreview(content));
    column->addSpacing(600);
    column->addWidget(samples.at(2).createPreview(content));
    scroll.setWidgetResizable(true);
    scroll.setWidget(content);
    scroll.resize(700, 500);
    scroll.show();
    const auto views = content->findChildren<spatial::SpatialView*>();
    ASSERT_EQ(views.size(), 2);
    for (auto* view : views)
        view->setSpatialEnabled(true);
    using RenderMode = spatial::SpatialView::RenderMode;
    QTRY_COMPARE_WITH_TIMEOUT(views.first()->renderMode(), RenderMode::Auto, 1000);
    EXPECT_EQ(views.last()->renderMode(), RenderMode::Auto);
    EXPECT_EQ(views.last()->activeBackend(), spatial::SpatialView::Backend::Raster);
    EXPECT_EQ(views.last()->findChild<QOpenGLWidget*>(), nullptr);
    scroll.verticalScrollBar()->setValue(scroll.verticalScrollBar()->maximum());
    QTRY_COMPARE_WITH_TIMEOUT(views.first()->activeBackend(), spatial::SpatialView::Backend::Raster,
                              1000);
    EXPECT_EQ(views.first()->findChild<QOpenGLWidget*>(), nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(views.last()->renderMode(), RenderMode::Auto, 1000);
}

TEST_F(GallerySpatialTest, NativeOpenGLPreviewsRevealWithoutHover)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        GTEST_SKIP() << "Requires a native OpenGL viewport";
    const auto samples = gallerySamplesForRoute("spatial-view");
    for (const QString& id :
         {QStringLiteral("spatial-view-cards"), QStringLiteral("spatial-view-navigation")}) {
        SCOPED_TRACE(id.toStdString());
        scrolling::ScrollView scroll;
        auto* content = new QWidget;
        auto* column = new QVBoxLayout(content);
        column->addSpacing(600);
        const auto sample = std::find_if(samples.cbegin(), samples.cend(),
                                         [&id](const auto& value) { return value.id == id; });
        ASSERT_NE(sample, samples.cend());
        auto* panel = sample->createPreview(content);
        column->addWidget(panel);
        column->addSpacing(600);
        scroll.setWidgetResizable(true);
        scroll.setWidget(content);
        scroll.resize(740, 540);
        scroll.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&scroll));
        auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
        view->setPointerTrackingEnabled(false);
        view->setSpatialEnabled(true);
        const int top = view->mapTo(content, QPoint()).y();
        const auto frame = [view] {
            auto* canvas = view->findChild<QGraphicsView*>();
            auto* gl = qobject_cast<QOpenGLWidget*>(canvas->viewport());
            if (!gl || !gl->isValid())
                return QImage();
            QImage image(gl->size() * gl->devicePixelRatioF(), QImage::Format_RGBA8888);
            gl->makeCurrent();
            gl->context()->functions()->glReadPixels(0, 0, image.width(), image.height(), GL_RGBA,
                                                     GL_UNSIGNED_BYTE, image.bits());
            gl->doneCurrent();
            return image.mirrored();
        };
        const auto ink = [](const QImage& image, int half) {
            int count = 0;
            for (int y = half * image.height() / 2; y < (half + 1) * image.height() / 2; y += 2)
                for (int x = 0; x < image.width(); x += 2)
                    if (qGray(image.pixel(x, y)) < 120)
                        ++count;
            return count;
        };
        scroll.verticalScrollBar()->setValue(top - 40);
        QTRY_COMPARE_WITH_TIMEOUT(view->activeBackend(), spatial::SpatialView::Backend::OpenGL,
                                  2000);
        QTest::qWait(100);
        const QImage reference = frame();
        ASSERT_FALSE(reference.isNull());
        const int referenceInk[] = {ink(reference, 0), ink(reference, 1)};
        ASSERT_GT(referenceInk[0], 100);
        ASSERT_GT(referenceInk[1], 100);
        for (int pass = 0; pass < 4; ++pass) {
            SCOPED_TRACE(pass);
            const bool fromAbove = pass % 2 == 0;
            scroll.verticalScrollBar()->setValue(fromAbove ? top + view->height() + 20 : 0);
            QTRY_COMPARE_WITH_TIMEOUT(view->activeBackend(), spatial::SpatialView::Backend::Raster,
                                      1000);
            QTest::qWait(40);
            scroll.verticalScrollBar()->setValue(
                fromAbove ? top + view->height() - 30 : top - scroll.viewport()->height() + 30);
            QTRY_COMPARE_WITH_TIMEOUT(view->activeBackend(), spatial::SpatialView::Backend::OpenGL,
                                      2000);
            QTest::qWait(60);
            scroll.verticalScrollBar()->setValue(top - 40);
            QTest::qWait(100);
            // No hover, content update, repaint or grabFramebuffer: those would hide the bug.
            const QImage actual = frame();
            ASSERT_EQ(actual.size(), reference.size());
            for (int half = 0; half < 2; ++half)
                EXPECT_GE(ink(actual, half), referenceInk[half] * .95)
                    << "Newly exposed content must repaint without pointer input; half=" << half;
            if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
                !dir.isEmpty()) {
                QDir().mkpath(dir);
                actual.save(dir + QStringLiteral("/%1-reveal-%2.png").arg(id).arg(pass));
            }
        }
    }
}

TEST_F(GallerySpatialTest, NativeOpenGLPreviewsSurviveClippingAndExternalUpdates)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        GTEST_SKIP() << "Requires a native OpenGL viewport";
    scrolling::ScrollView scroll;
    auto* content = new QWidget;
    auto* column = new QVBoxLayout(content);
    column->addSpacing(500);
    const auto samples = gallerySamplesForRoute("spatial-view");
    const auto sample = std::find_if(samples.cbegin(), samples.cend(), [](const auto& value) {
        return value.id == QStringLiteral("spatial-view-navigation");
    });
    ASSERT_NE(sample, samples.cend());
    auto* panel = sample->createPreview(content);
    column->addWidget(panel);
    column->addSpacing(500);
    scroll.setWidgetResizable(true);
    scroll.setWidget(content);
    scroll.resize(740, 540);
    scroll.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&scroll));
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    auto* name = panel->findChild<textfields::LineEdit*>("spatialProfileName");
    view->setPointerTrackingEnabled(false);
    view->setSpatialEnabled(true);
    const int top = view->mapTo(content, QPoint()).y();
    for (int pass = 0; pass < 3; ++pass) {
        scroll.verticalScrollBar()->setValue(0);
        QTest::qWait(40);
        // First expose only a strip, then reveal the whole viewport without pointer input.
        scroll.verticalScrollBar()->setValue(top - scroll.viewport()->height() + 30);
        QTRY_COMPARE_WITH_TIMEOUT(view->activeBackend(), spatial::SpatialView::Backend::OpenGL,
                                  2000);
        auto* canvas = view->findChild<QGraphicsView*>();
        auto* gl = qobject_cast<QOpenGLWidget*>(canvas->viewport());
        ASSERT_NE(gl, nullptr);
        RecordProperty("renderer", view->rendererName().toStdString());
        scroll.verticalScrollBar()->setValue(top - 40);
        name->setText(QString::fromUtf8("工作空间名称在三维预览中也应正确显示 %1").arg(pass));
        QTest::qWait(100);
        // Read the existing frame without asking QOpenGLWidget to render paintGL().
        // QGraphicsView owns the paint event for this viewport.
        QImage frame(gl->size() * gl->devicePixelRatioF(), QImage::Format_RGBA8888);
        gl->makeCurrent();
        gl->context()->functions()->glReadPixels(0, 0, frame.width(), frame.height(), GL_RGBA,
                                                 GL_UNSIGNED_BYTE, frame.bits());
        gl->doneCurrent();
        frame = frame.mirrored();
        ASSERT_FALSE(frame.isNull());
        int darkPixels = 0;
        for (int y = 0; y < frame.height(); y += 2)
            for (int x = 0; x < frame.width(); x += 2)
                if (qGray(frame.pixel(x, y)) < 120)
                    ++darkPixels;
        EXPECT_GT(darkPixels, 250) << "The projected text and controls must be painted";
        const QImage captured = gl->grabFramebuffer();
        int capturedText = 0;
        for (int y = 0; y < captured.height(); y += 2)
            for (int x = 0; x < captured.width(); x += 2)
                if (qGray(captured.pixel(x, y)) < 120)
                    ++capturedText;
        EXPECT_GT(capturedText, 250) << "Direct GL repaint must retain the scene, not clear it";
        if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            frame.save(dir + QStringLiteral("/native-scroll-%1.png").arg(pass));
            captured.save(dir + QStringLiteral("/native-capture-%1.png").arg(pass));
        }
        QTest::qWait(100);
        QSignalSpy idleFrames(gl, &QOpenGLWidget::frameSwapped);
        QTest::qWait(100);
        EXPECT_LE(idleFrames.count(), 1) << "An idle preview must not render continuously";
        scroll.hide();
        QTest::qWait(40);
        scroll.show();
    }
}

TEST_F(GallerySpatialTest, ItemParametersWorkInsideSceneAndPreserveTheirDragTargets)
{
    auto samples = gallerySamplesForRoute("spatial-item");
    std::unique_ptr<QWidget> panel(samples.first().createPreview(nullptr));
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(view, nullptr);
    ASSERT_EQ(view->itemCount(), 2);
    auto* item = view->items().first();
    auto* controlItem = view->items().last();
    auto* settings = controlItem->widget();
    auto* rotation = settings->findChild<basicinput::Slider*>("spatialItemRotation");
    auto* depth = settings->findChild<basicinput::Slider*>("spatialItemDepth");
    auto* finish = settings->findChild<basicinput::Slider*>("spatialItemSurfaceIntensity");
    ASSERT_NE(rotation, nullptr);
    ASSERT_NE(depth, nullptr);
    ASSERT_NE(finish, nullptr);
    panel->resize(900, 700);
    panel->show();
    view->setPointerTrackingEnabled(false);
    view->setSpatialEnabled(true);
    QTest::qWait(60);
    auto* canvas = view->findChild<QGraphicsView*>();
    ASSERT_NE(canvas, nullptr);
    ASSERT_NE(settings->graphicsProxyWidget(), nullptr);
    EXPECT_TRUE(settings->isAncestorOf(rotation));
    EXPECT_TRUE(settings->isAncestorOf(finish));
    const auto controlPose = controlItem->position();
    EXPECT_DOUBLE_EQ(item->surfaceIntensity(), .75);
    EXPECT_DOUBLE_EQ(item->hoverLift(), 5);
    EXPECT_DOUBLE_EQ(controlItem->hoverLift(), 0);
    finish->setValue(0);
    EXPECT_DOUBLE_EQ(item->surfaceIntensity(), 0);
    finish->setValue(100);
    EXPECT_DOUBLE_EQ(item->surfaceIntensity(), 1);
    const auto point = [canvas, settings](QWidget* widget, const QPoint& local) {
        return canvas->mapFromScene(
            settings->graphicsProxyWidget()->mapToScene(widget->mapTo(settings, local)));
    };
    const QPoint from = point(rotation, QPoint(rotation->width() / 4, rotation->height() / 2));
    const QPoint to = point(rotation, QPoint(rotation->width() * 3 / 4, rotation->height() / 2));
    ASSERT_TRUE(canvas->viewport()->rect().contains(from));
    ASSERT_TRUE(canvas->viewport()->rect().contains(to));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(canvas->viewport(), to);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, to);
    EXPECT_GT(rotation->value(), 10);
    EXPECT_EQ(item->rotation(), QVector3D(0, rotation->value(), 0));
    depth->setValue(100);
    EXPECT_EQ(item->position().z(), 100);
    EXPECT_EQ(controlItem->position(), controlPose);
    // Both surfaces fit at desktop and narrow widths, including the depth extremes.
    for (const int width : {900, 540}) {
        panel->resize(width, 800);
        QTest::qWait(40);
        for (const int z : {-120, 120}) {
            depth->setValue(z);
            for (auto* surface : view->items())
                EXPECT_TRUE(
                    QRectF(view->rect()).contains(surface->projectedPolygon().boundingRect()));
        }
    }
    view->setSpatialEnabled(false);
    EXPECT_TRUE(view->isAncestorOf(rotation));
    EXPECT_EQ(view->items().last()->widget(), settings);
    EXPECT_EQ(depth->value(), 120);
    view->setSpatialEnabled(true);
    EXPECT_EQ(item->position().z(), 120);
}

TEST_F(GallerySpatialTest, HiddenPreviewsReleaseGpuAndAccessibilityModesKeepNativeWidgets)
{
    auto samples = gallerySamplesForRoute("spatial-view");
    std::unique_ptr<QWidget> panel(samples.first().createPreview(nullptr));
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    panel->resize(640, 440);
    panel->show();
    GallerySettings::instance().setSpatialModeEnabled(true);
    panel->hide();
    QApplication::processEvents();
    EXPECT_EQ(view->renderMode(), spatial::SpatialView::RenderMode::Auto);
    EXPECT_EQ(view->activeBackend(), spatial::SpatialView::Backend::Raster);
    EXPECT_EQ(view->findChild<QOpenGLWidget*>(), nullptr);
    EXPECT_FALSE(view->findChild<QTimer*>("spatialMotionTimer")->isActive());
    panel->show();
    GallerySettings::instance().setMotionMode(GallerySettings::MotionMode::Reduced);
    EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_FALSE(GallerySettings::instance().spatialModeEnabled());
    EXPECT_TRUE(view->items().first()->widget()->isVisible());
    GallerySettings::instance().setMotionMode(GallerySettings::MotionMode::Full);
    GallerySettings::instance().setSpatialModeEnabled(true);
    GallerySettings::instance().setThemeMode(GallerySettings::ThemeMode::HighContrast);
    EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_FALSE(GallerySettings::instance().spatialModeEnabled());
}

TEST_F(GallerySpatialTest, SpatialCategoryNavigationPreservesOriginalHomeHero)
{
    GallerySettings::instance().setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1280, 900);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto* home = window.currentContentPage();
    EXPECT_TRUE(home->findChildren<spatial::SpatialView*>().isEmpty());
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeHeroTitle"), nullptr);
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeHeroIcon"), nullptr);
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeParticles"), nullptr);
    ASSERT_TRUE(window.selectRoute("spatial"));
    QTRY_VERIFY_WITH_TIMEOUT(qobject_cast<GalleryCategoryPage*>(window.currentContentPage()), 2000);
    auto* category = qobject_cast<GalleryCategoryPage*>(window.currentContentPage());
    EXPECT_EQ(category->componentRouteIds(), QStringList({"spatial-view", "spatial-item"}));
}

#ifdef Q_OS_MAC
TEST_F(GallerySpatialTest, MacStartupPrewarmKeepsNativeWindowSurface)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires native Cocoa window surfaces";
    GallerySettings::instance().setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1280, 800);
    window.show();

    class SurfaceObserver final : public QObject {
    public:
        int destroyed = 0;
        int hidden = 0;
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::PlatformSurface &&
                static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() ==
                    QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                ++destroyed;
            if (event->type() == QEvent::Hide)
                ++hidden;
            return QObject::eventFilter(watched, event);
        }
    } observer;
    ASSERT_NE(window.windowHandle(), nullptr);
    window.windowHandle()->installEventFilter(&observer);
    window.installEventFilter(&observer);

    // Exercise these pages even when a busy host exhausts the full catalog's prewarm budget.
    // zh_CN: 即使繁忙宿主耗尽全目录预热预算，也确保覆盖这两个隐藏页面的构建。
    QWidget prewarmHost(window.contentHost());
    prewarmHost.hide();
    GalleryNavigationViewModel navigation;
    GalleryPageFactory factory(navigation);
    ASSERT_NE(factory.createPage(QStringLiteral("spatial-view"), &prewarmHost), nullptr);
    ASSERT_NE(factory.createPage(QStringLiteral("spatial-item"), &prewarmHost), nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    const auto views = prewarmHost.findChildren<spatial::SpatialView*>();
    ASSERT_EQ(views.size(), gallerySamplesForRoute("spatial-view").size() +
                                gallerySamplesForRoute("spatial-item").size());
    EXPECT_EQ(window.currentRouteId(), QStringLiteral("home"));
    for (auto* view : views)
        EXPECT_FALSE(view->isVisible());
    EXPECT_EQ(observer.destroyed, 0) << "Hidden prewarm must not recreate the native surface";
    EXPECT_EQ(observer.hidden, 0) << "Startup must not hide and show the window again";
}

TEST_F(GallerySpatialTest, MacTrafficLightsRemainCenteredAcrossSpatialNavigation)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires native Cocoa chrome";
    GallerySettings::instance().setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1280, 800);
    window.show();
    window.raise();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    const auto get = [](id object, const char* name) {
        using Send = id (*)(id, SEL);
        return reinterpret_cast<Send>(objc_msgSend)(object, sel_registerName(name));
    };
    const auto rect = [](id object, const char* name) {
#if defined(__x86_64__)
        CGRect result{};
        using Send = void (*)(CGRect*, id, SEL);
        reinterpret_cast<Send>(objc_msgSend_stret)(&result, object, sel_registerName(name));
        return result;
#else
        using Send = CGRect (*)(id, SEL);
        return reinterpret_cast<Send>(objc_msgSend)(object, sel_registerName(name));
#endif
    };
    for (const auto* route : {"spatial-view", "home", "spatial-item", "home"}) {
        ASSERT_TRUE(window.selectRoute(route));
        QTRY_VERIFY_WITH_TIMEOUT(window.currentContentPage() != nullptr, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(window.currentContentPage()->routeId(), QString(route), 2000);
        QTest::qWait(200);
        id native = get(reinterpret_cast<id>(window.winId()), "window");
        ASSERT_NE(native, nil);
        for (unsigned long type : {0UL, 1UL, 2UL}) {
            using Button = id (*)(id, SEL, unsigned long);
            id button = reinterpret_cast<Button>(objc_msgSend)(
                native, sel_registerName("standardWindowButton:"), type);
            ASSERT_NE(button, nil);
            id host = get(button, "superview");
            id content = get(native, "contentView");
            CGRect converted{};
#if defined(__x86_64__)
            using Convert = void (*)(CGRect*, id, SEL, CGRect, id);
            reinterpret_cast<Convert>(objc_msgSend_stret)(&converted, host,
                                                          sel_registerName("convertRect:toView:"),
                                                          rect(button, "frame"), content);
#else
            using Convert = CGRect (*)(id, SEL, CGRect, id);
            converted = reinterpret_cast<Convert>(objc_msgSend)(
                host, sel_registerName("convertRect:toView:"), rect(button, "frame"), content);
#endif
            using Bool = BOOL (*)(id, SEL);
            const bool flipped =
                reinterpret_cast<Bool>(objc_msgSend)(content, sel_registerName("isFlipped"));
            const qreal actual =
                flipped ? CGRectGetMidY(converted)
                        : rect(content, "bounds").size.height - CGRectGetMidY(converted);
            auto* bar = window.titleBar();
            const qreal expected = bar->mapTo(&window, QPoint()).y() + bar->height() / 2.0;
            EXPECT_NEAR(actual, expected, 1.0) << route << " button=" << type;
        }
    }
}
#endif

TEST_F(GallerySpatialTest, CacheBudgetPreservesDensityAndRespectsGpuLimits)
{
    using namespace spatial_render;
    const std::array<QSizeF, 2> panels = {QSizeF(240, 900), QSizeF(1360, 900)};
    const auto normal = planCaches({QSizeF(240, 700), QSizeF(960, 700)}, 2, 16384);
    ASSERT_TRUE(normal.valid());
    EXPECT_EQ(normal.dpr, 4);
    const auto large = planCaches(panels, 2, 16384);
    ASSERT_TRUE(large.valid());
    EXPECT_GE(large.dpr, 2);
    EXPECT_EQ(large.dpr, 4);
    EXPECT_LT(large.paintSize.height(), large.sizes[1].height());
    EXPECT_LE(large.estimatedBytes, kCacheBudgetBytes);
    EXPECT_GT(large.estimatedBytes, large.pixels * kCacheBytesPerPixel);
    const auto limited = planCaches(panels, 2, 4096);
    ASSERT_TRUE(limited.valid());
    EXPECT_GE(limited.dpr, 2);
    for (const QSize& size : limited.sizes) {
        EXPECT_LE(size.width(), 4096);
        EXPECT_LE(size.height(), 4096);
    }
    EXPECT_FALSE(planCaches(panels, 2, 2048).valid());
    EXPECT_FALSE(planCaches(panels, 2, 16384, 2, 1024).valid());
    EXPECT_FALSE(planCaches(panels, 0, 16384).valid());
    EXPECT_FALSE(planCaches({QSizeF(1e20, 900), QSizeF()}, 2, 16384).valid());
    const auto retry = planCaches(panels, 1.25, 4096, 1.5);
    EXPECT_EQ(retry.dpr, 1.875);
}

TEST_F(GallerySpatialTest, CacheResourceFailureAllowsRetry)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL context";
    GalleryWindow window;
    window.resize(1100, 760);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    GallerySettings::instance().setSpatialModeEnabled(true);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 5000));
    controller->cancelTransition();
    ASSERT_TRUE(QMetaObject::invokeMethod(controller, "releaseOversizedPresentation"));
    EXPECT_FALSE(GallerySettings::instance().spatialModeEnabled());
    EXPECT_TRUE(GallerySettings::instance().spatialAvailable());
    EXPECT_FALSE(surface->property("presenting").toBool());
    EXPECT_EQ(controller->renderingStatistics()["cachedPixels"].toLongLong(), 0);
    GallerySettings::instance().setSpatialModeEnabled(true);
    controller->cancelTransition();
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }));
    EXPECT_EQ(surface, window.findChild<QOpenGLWidget*>("gallerySpatialSurface"));
}

TEST_F(GallerySpatialTest, GpuCachePaintsWidgetsDirectlyAndReusesStaticContent)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL paint engine";
    class PaintedContent final : public QWidget {
    public:
        int gpuPaints = 0;
        QColor color = Qt::red;
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            gpuPaints += painter.paintEngine()->type() == QPaintEngine::OpenGL2;
            painter.fillRect(rect(), color);
        }
    };
    QWidget window;
    window.resize(800, 600);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->resize(window.size());
    auto* content = new PaintedContent;
    navigation->contentHost()->insertPage(0, content);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->gpuPaints > 0; }, 3000));
    controller.cancelTransition();
    QTest::qWait(200);
    const int before = content->gpuPaints;
    auto* pointer = controller.findChild<QVariantAnimation*>("galleryPointerAnimation");
    pointer->setStartValue(QPointF());
    pointer->setEndValue(QPointF(.5, -.3));
    pointer->start();
    ASSERT_TRUE(QTest::qWaitFor([&] { return pointer->state() == QAbstractAnimation::Stopped; }));
    EXPECT_EQ(content->gpuPaints, before) << "Pointer motion must reuse the GPU texture";
    content->color = Qt::green;
    content->update();
    ASSERT_TRUE(QTest::qWaitFor([&] { return content->gpuPaints > before; }));
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    const QPoint projected = controller.projectedPosition(content, content->rect().center());
    const QPoint pixel = surface->mapFrom(&window, projected) * surface->devicePixelRatioF();
    const auto frame = surface->grabFramebuffer();
    ASSERT_TRUE(frame.rect().contains(pixel));
    EXPECT_GT(frame.pixelColor(pixel).green(), 240);
    EXPECT_LT(frame.pixelColor(pixel).red(), 15);
    EXPECT_GT(controller.renderingStatistics()["cachedPixels"].toLongLong(), 0);
    settings.setSpatialModeEnabled(false);
    controller.cancelTransition();
    EXPECT_EQ(controller.renderingStatistics()["cachedPixels"].toLongLong(), 0);
    const int after = content->gpuPaints;
    content->update();
    QTest::qWait(100);
    EXPECT_EQ(content->gpuPaints, after);
}

TEST_F(GallerySpatialTest, GpuCachePreservesHighDpiControlDetail)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL paint engine";
    for (const QSize windowSize : {QSize(900, 500), QSize(1500, 1000)}) {
        GallerySettings::instance().setSpatialModeEnabled(false);
        class DetailContent final : public QWidget {
        public:
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                painter.fillRect(rect(), Qt::white);
                painter.setPen(Qt::black);
                QFont text = font();
                text.setPixelSize(14);
                painter.setFont(text);
                painter.drawText(QPoint(40, 40), "High DPI text: Popup settings 0123456789");
                text.setPixelSize(28);
                painter.setFont(text);
                painter.drawText(QPoint(40, 290), QString::fromUtf8("Gallery 标题 · 清晰度"));
                text.setPixelSize(18);
                painter.setFont(text);
                painter.drawText(QPoint(40, 330),
                                 QString::fromUtf8("正文：设置与组件，0123456789"));
                for (int x = 40; x < 180; x += 4)
                    painter.fillRect(QRectF(x, 180, 1, 40), Qt::black);
            }
        };
        QWidget window;
        window.resize(windowSize);
        auto* navigation = new navigation::NavigationView(&window);
        navigation->resize(window.size());
        auto* content = new DetailContent;
        auto* button = new basicinput::Button("Show popup", content);
        button->setGeometry(40, 70, 140, 36);
        button->setFocusPolicy(Qt::NoFocus);
        auto* toggle = new basicinput::ToggleSwitch(content);
        toggle->setOnContent("Light dismiss");
        toggle->setIsOn(true);
        toggle->setGeometry(205, 70, 190, 36);
        toggle->setFocusPolicy(Qt::NoFocus);
        GallerySpatialController controller(&window, navigation);
        navigation->contentHost()->insertPage(0, content);
        navigation->contentHost()->setCurrentIndex(0, 0, false);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(200);
        toggle->setFocus(Qt::TabFocusReason);
        const QImage reference = content->grab().toImage();
        GallerySettings::instance().setSpatialModeEnabled(true);
        auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
        ASSERT_NE(surface, nullptr);
        ASSERT_TRUE(
            QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
        controller.cancelTransition();
        // Keep the GPU compositor active at the flat endpoint so this checks sampling
        // fidelity without conflating the expected perspective resampling with a regression.
        auto* motion = controller.findChild<QVariantAnimation*>("galleryAssemblyAnimation");
        ASSERT_NE(motion, nullptr);
        motion->setStartValue(0.0);
        motion->setEndValue(1.0);
        motion->setCurrentTime(motion->duration() / 2);
        motion->setCurrentTime(0);
        QTest::qWait(200);
        const QImage frame = surface->grabFramebuffer();
        const auto stats = controller.renderingStatistics();
        EXPECT_LE(stats["cacheEstimatedBytes"].toLongLong(), spatial_render::kCacheBudgetBytes);
        EXPECT_GE(stats["cacheDpr"].toDouble(), window.devicePixelRatioF());
        EXPECT_GT(stats["paintSamples"].toInt(), 1)
            << "Control curves need MSAA in the paint target, not just the window";
        if (windowSize.width() == 1500 && window.devicePixelRatioF() == 2) {
            EXPECT_EQ(stats["cacheDpr"].toDouble(), 4);
            EXPECT_LT(stats["paintTargetHeight"].toInt(), content->height() * 4);
        }
        const QString evidence = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE") +
                                 (windowSize.width() == 1500 ? "/large" : "");
        const qreal dpr = surface->devicePixelRatioF();
        const QPoint origin = surface->mapFrom(&window, controller.projectedPosition(content, {}));
        const QImage actual = frame.copy(QRect(origin * dpr, reference.size()));
        int seamPixels = 0;
        for (int y = qCeil(20 * dpr); y < actual.height() - qCeil(20 * dpr); ++y) {
            const auto pixel = actual.pixelColor(qRound(20 * dpr), y);
            seamPixels += pixel.alpha() < 250 || pixel.red() < 250;
        }
        EXPECT_EQ(seamPixels, 0)
            << "Shared MSAA strips must not leave transparent or dark seams: "
            << QJsonDocument::fromVariant(stats).toJson(QJsonDocument::Compact).constData();
        if (const auto dir = evidence; !qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_EVIDENCE")) {
            QDir().mkpath(dir);
            reference.save(dir + "/detail-2d.png");
            actual.save(dir + "/detail-gpu.png");
        }
        int different = 0;
        const QRect detail = QRect(30, 20, 380, 215).intersected(content->rect());
        const QRect pixels(detail.topLeft() * dpr, detail.size() * dpr);
        for (int y = pixels.top(); y < pixels.bottom(); ++y)
            for (int x = pixels.left(); x < pixels.right(); ++x) {
                const QColor a = actual.pixelColor(x, y), b = reference.pixelColor(x, y);
                different += qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) +
                                 qAbs(a.blue() - b.blue()) >
                             60;
            }
        EXPECT_LT(different, pixels.width() * pixels.height() * .02)
            << "The flat GPU endpoint must retain native text and control detail";
        // Large glyphs can use Qt's outline path instead of its raster glyph cache.
        // Compare stroke edge contrast; exact ink weight differs between those engines.
        for (const QRect region : {QRect(35, 260, 370, 35), QRect(35, 306, 370, 30)}) {
            const QRect area(region.topLeft() * dpr, region.size() * dpr);
            const auto contrast = [&](const QImage& image) {
                double ink = 0, energy = 0;
                for (int y = area.top() + 1; y < area.bottom(); ++y)
                    for (int x = area.left() + 1; x < area.right(); ++x) {
                        const int value = qGray(image.pixel(x, y));
                        ink += 255 - value;
                        energy += qPow(value - qGray(image.pixel(x - 1, y)), 2) +
                                  qPow(value - qGray(image.pixel(x, y - 1)), 2);
                    }
                return ink > 0 ? energy / ink : 0;
            };
            EXPECT_GT(contrast(reference), 20);
            EXPECT_GE(contrast(actual), contrast(reference) * .9);
        }
        // A perspective transform introduces fractional texture coordinates. The old
        // CPU/QPainter path filtered these; nearest-neighbour FBO sampling drops thin
        // strokes and makes their weight change as the pointer moves.
        controller.cancelTransition();
        QTest::qWait(100);
        const QImage projected = surface->grabFramebuffer();
        const QPoint a =
            surface->mapFrom(&window, controller.projectedPosition(content, {50, 190}));
        const QPoint b =
            surface->mapFrom(&window, controller.projectedPosition(content, {168, 208}));
        const QRect linePixels(a * dpr, b * dpr);
        int filtered = 0;
        for (int y = linePixels.top(); y < linePixels.bottom(); ++y)
            for (int x = linePixels.left(); x < linePixels.right(); ++x) {
                const int shade = projected.pixelColor(x, y).red();
                filtered += shade > 20 && shade < 235;
            }
        EXPECT_GT(filtered, linePixels.width() * linePixels.height() * .08)
            << "Projected one-pixel strokes need filtered coverage, not nearest-neighbour steps";
        if (const auto dir = evidence; !qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_EVIDENCE"))
            projected.save(dir + "/detail-projected.png");
    }
}

TEST_F(GallerySpatialTest, PopupControlsRetainDetailIn3D)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL paint engine";
    if (QGuiApplication::platformName() == "cocoa") {
        for (const auto& name : QStyleFactory::keys())
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
    }
    const auto restoreStyle = qScopeGuard([] { qApp->setStyle(QStringLiteral("Fusion")); });
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setHomeParticlesEnabled(false);
    GalleryWindow window;
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.resize(1200, 850);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 6500));
    ASSERT_TRUE(window.selectRoute("popup"));
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return window.currentContentPage() && window.currentContentPage()->routeId() == "popup";
    }));
    QTest::qWait(500);
    auto* controller = window.findChild<GallerySpatialController*>();
    basicinput::Button* button = nullptr;
    for (auto* candidate : window.currentContentPage()->findChildren<basicinput::Button*>())
        if (candidate->text() == "Show popup")
            button = candidate;
    ASSERT_NE(button, nullptr);
    auto* scroll = window.currentContentPage()->findChild<QScrollArea*>();
    ASSERT_NE(scroll, nullptr);
    scroll->ensureWidgetVisible(button, 0, 60);
    QTest::qWait(200);
    const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        window.screen()->grabWindow(window.winId()).save(dir + "/popup-2d.png");
        button->grab().save(dir + "/popup-button-2d.png");
    }
    settings.setSpatialModeEnabled(true);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->transitionRunning(); }, 2000));
    QTest::qWait(200);
    if (!dir.isEmpty()) {
        window.screen()->grabWindow(window.winId()).save(dir + "/popup-3d.png");
        const auto position = [&](const QPoint& local) {
            return surface->mapFrom(&window, controller->projectedPosition(button, local));
        };
        const qreal dpr = surface->devicePixelRatioF();
        const QPolygon polygon{
            position(button->rect().topLeft()), position(button->rect().topRight()),
            position(button->rect().bottomRight()), position(button->rect().bottomLeft())};
        const QRect bounds = polygon.boundingRect().adjusted(-2, -2, 2, 2);
        surface->grabFramebuffer()
            .copy(QRect(bounds.topLeft() * dpr, bounds.size() * dpr))
            .save(dir + "/popup-button-3d.png");
    }
    QSignalSpy clicked(button, &QAbstractButton::clicked);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      controller->projectedPosition(button, button->rect().center()));
    ASSERT_TRUE(QTest::qWaitFor([&] { return clicked.count() == 1; }));
}

TEST_F(GallerySpatialTest, MacNativeStyleControlsKeepTheirPixelsInGpuCache)
{
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        GTEST_SKIP() << "Requires native Cocoa control painting";
    QStyle* native = nullptr;
    for (const auto& name : QStyleFactory::keys()) {
        if (name.contains("mac", Qt::CaseInsensitive))
            native = QStyleFactory::create(name);
    }
    ASSERT_NE(native, nullptr);
    qApp->setStyle(native);
    const auto restoreStyle = qScopeGuard([] { qApp->setStyle(QStringLiteral("Fusion")); });
    QWidget window;
    window.resize(800, 600);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->resize(window.size());
    class NativeContent final : public QWidget {
    public:
        qint64 rasterPixels = 0;
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), Qt::white);
            QStyleOptionFrame option;
            option.initFrom(this);
            option.rect = QRect(460, 360, 160, 32);
            option.lineWidth = 1;
            style()->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, this);
            if (painter.paintEngine()->type() == QPaintEngine::OpenGL2)
                rasterPixels = style()->property("galleryLastNativeRasterPixels").toLongLong();
        }
    };
    auto* content = new NativeContent;
    auto* check = new QCheckBox("Native checkbox", content);
    auto* edit = new QLineEdit("Native text field", content);
    edit->setGeometry(40, 140, 240, 36);
    edit->setFocusPolicy(Qt::NoFocus);
    check->setChecked(true);
    check->setFocusPolicy(Qt::NoFocus);
    check->setGeometry(40, 60, 220, 48);
    navigation->contentHost()->insertPage(0, content);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(150);
    const qreal dpr = window.devicePixelRatioF();
    const QImage contentReference = content->grab().toImage();
    QImage reference(check->size() * dpr, QImage::Format_ARGB32_Premultiplied);
    reference.setDevicePixelRatio(dpr);
    reference.fill(Qt::transparent);
    {
        QPainter painter(&reference);
        check->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    GallerySettings::instance().setSpatialModeEnabled(true);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    controller.cancelTransition();
    QTest::qWait(150);
    EXPECT_TRUE(qApp->style()->property("galleryGpuCompatibleStyle").toBool());
    const auto frame = surface->grabFramebuffer();
    if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
        QDir().mkpath(dir);
        reference.save(dir + "/native-style-reference.png");
        frame.save(dir + "/native-style-frame.png");
    }
    auto* motion = controller.findChild<QVariantAnimation*>("galleryAssemblyAnimation");
    motion->setStartValue(0.0);
    motion->setEndValue(1.0);
    motion->setCurrentTime(motion->duration() / 2);
    motion->setCurrentTime(0);
    QTest::qWait(100);
    const QImage flat = surface->grabFramebuffer();
    for (const QRect region : {QRect(452, 352, 176, 48), QRect(36, 136, 248, 44)}) {
        int nativeInk = 0, gpuInk = 0;
        for (int y = region.top(); y < region.bottom(); ++y)
            for (int x = region.left(); x < region.right(); ++x) {
                nativeInk += contentReference.pixelColor(QPoint(x, y) * dpr).lightness() < 245;
                const auto target = controller.projectedPosition(content, QPoint(x, y));
                gpuInk +=
                    flat.pixelColor(surface->mapFrom(&window, target) * dpr).lightness() < 245;
            }
        EXPECT_GT(nativeInk, 40);
        EXPECT_GT(gpuInk, nativeInk * .7);
        // Supersampling can cover both sides of a thin gray border. Reject a filled
        // or displaced panel while allowing that expected antialiasing coverage.
        EXPECT_LT(gpuInk, nativeInk * 2);
    }
    if (qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK")) {
        EXPECT_GT(content->rasterPixels, 0);
        // QWidget::render retains the widget's native device density for native styles.
        EXPECT_EQ(content->rasterPixels, qCeil(168 * dpr) * qCeil(40 * dpr));
    }
    if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
        contentReference.save(dir + "/native-content-2d.png");
        flat.save(dir + "/native-content-flat.png");
    }
    controller.cancelTransition();
    int samples = 0, matches = 0;
    for (int y = 0; y < reference.height(); y += 2) {
        for (int x = 0; x < qMin(reference.width(), qRound(24 * dpr)); x += 2) {
            const auto expected = reference.pixelColor(x, y);
            if (expected.alpha() < 250)
                continue;
            const auto point = controller.projectedPosition(check, QPoint(x / dpr, y / dpr));
            const auto actual = frame.pixelColor(surface->mapFrom(&window, point) * dpr);
            ++samples;
            matches += qAbs(actual.red() - expected.red()) +
                           qAbs(actual.green() - expected.green()) +
                           qAbs(actual.blue() - expected.blue()) <
                       90;
        }
    }
    EXPECT_GT(samples, 30);
    EXPECT_GT(matches, samples * .75) << "Native control pixels must survive GPU composition";
}

// Opt-in, native frame pacing probe. No machine-dependent timing assertion in CI.
TEST_F(GallerySpatialTest, NativeColdActivationProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_BENCHMARK") ||
        tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires opt-in native cold activation measurement";
    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        for (const auto& name : QStyleFactory::keys())
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
    }
    const auto restoreStyle = qScopeGuard([] { qApp->setStyle(QStringLiteral("Fusion")); });
    GallerySettings::instance().setNavigationStyle(GallerySettings::NavigationStyle::Top);
    GalleryWindow window;
    window.resize(1209, 811);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<GallerySplashScreen*>(), 6500);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTest::qWait(300);
    auto* toggle = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(toggle, nullptr);
    ASSERT_NE(controller, nullptr);
    QElapsedTimer heartbeat, activation;
    heartbeat.start();
    qint64 longestGap = 0;
    QTimer pulse;
    pulse.setInterval(8);
    QObject::connect(&pulse, &QTimer::timeout, &window,
                     [&] { longestGap = qMax(longestGap, heartbeat.restart()); });
    pulse.start();
    auto* preparedStyle = qApp->style();
    activation.start();
    QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier, QPoint(20, toggle->height() / 2));
    const qint64 clickMs = activation.elapsed();
    EXPECT_EQ(qApp->style(), preparedStyle) << "First activation must not repolish every page";
    QTRY_VERIFY_WITH_TIMEOUT(controller->transitionRunning(), 5000);
    const qint64 startMs = activation.elapsed();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 3000);
    pulse.stop();
    toggle->setFocus(Qt::TabFocusReason);
    QTest::qWait(100);
    const QString directory = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    if (!directory.isEmpty()) {
        QDir().mkpath(directory);
        window.grab().save(directory + "/cold-settings-3d.png");
        auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
        const auto position = [&](const QPoint& point) {
            return surface->mapFrom(&window,
                                    controller->projectedPosition(toggle->parentWidget(), point));
        };
        const auto rect = toggle->parentWidget()->rect();
        const QRect bounds = QPolygon{position(rect.topLeft()), position(rect.topRight()),
                                      position(rect.bottomLeft()), position(rect.bottomRight())}
                                 .boundingRect()
                                 .adjusted(-4, -4, 4, 4);
        const qreal dpr = surface->devicePixelRatioF();
        surface->grabFramebuffer()
            .copy(QRect(bounds.topLeft() * dpr, bounds.size() * dpr))
            .save(directory + "/cold-toggle-3d.png");
    }
    std::cout << "SPATIAL_COLD click=" << clickMs << "ms start=" << startMs
              << "ms longestEventGap=" << longestGap << "ms stats="
              << QJsonDocument::fromVariant(controller->renderingStatistics())
                     .toJson(QJsonDocument::Compact)
                     .constData()
              << std::endl;
}

TEST_F(GallerySpatialTest, AssemblyFramePacingProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_BENCHMARK"))
        GTEST_SKIP() << "Set FLUENT_QT_SPATIAL_BENCHMARK=1 for native frame pacing.";
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU frame swaps";
    GalleryWindow window;
    window.resize(1200, 850);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTest::qWait(300);
    QPointer<QVariantAnimation> motion =
        window.findChild<QVariantAnimation*>("galleryAssemblyAnimation");
    ASSERT_NE(motion, nullptr);
    QElapsedTimer start;
    qint64 preparation = -1;
    QObject::connect(motion, &QVariantAnimation::stateChanged, &window,
                     [&](QAbstractAnimation::State state) {
                         if (state == QAbstractAnimation::Running)
                             preparation = start.elapsed();
                     });
    QVector<qint64> intervals;
    QElapsedTimer frame;
    int swaps = 0;
    start.start();
    GallerySettings::instance().setSpatialModeEnabled(true);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    QObject::connect(surface, &QOpenGLWidget::frameSwapped, &window, [&] {
        if (motion && motion->state() == QAbstractAnimation::Running) {
            ++swaps;
            if (frame.isValid())
                intervals.append(frame.restart());
            else
                frame.start();
        }
    });
    // Stopped before asynchronous initializeGL has run does not mean finished.
    // Measure submitted GL frames, rather than QVariantAnimation timer ticks.
    // zh_CN: 异步初始化前的 Stopped 不代表结束；统计实际 GL 提交帧，而非动画计时器回调。
    QTRY_VERIFY_WITH_TIMEOUT(preparation >= 0, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!motion || motion->state() == QAbstractAnimation::Stopped, 2500);
    std::sort(intervals.begin(), intervals.end());
    ASSERT_FALSE(intervals.isEmpty());
    std::cout << "SPATIAL_BENCH preparation=" << preparation << "ms swaps=" << swaps
              << " median=" << intervals[intervals.size() / 2] << "ms p95="
              << intervals[qMin(int(intervals.size()) - 1, int(intervals.size() * .95))]
              << "ms max=" << intervals.last() << "ms" << std::endl;
}

TEST_F(GallerySpatialTest, PointerFramePacingProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_BENCHMARK") ||
        tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Set FLUENT_QT_SPATIAL_BENCHMARK=1 on a native GPU desktop.";
    const bool nativeStyle = qEnvironmentVariableIsSet("FLUENT_QT_SPATIAL_NATIVE_STYLE");
    if (nativeStyle) {
        for (const auto& name : QStyleFactory::keys()) {
            if (name.contains("mac", Qt::CaseInsensitive))
                qApp->setStyle(QStyleFactory::create(name));
        }
    }
    const auto restoreStyle = qScopeGuard([=] {
        if (nativeStyle)
            qApp->setStyle(QStringLiteral("Fusion"));
    });
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setHomeParticlesEnabled(false);
    GalleryWindow window;
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.resize(1200, 850);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 6500));
    settings.setSpatialModeEnabled(true);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !controller->transitionRunning(); }, 2000));
    for (const QString& route :
         {QStringLiteral("settings"), QStringLiteral("home"), QStringLiteral("spatial-view")}) {
        settings.setHomeParticlesEnabled(route == "home");
        ASSERT_TRUE(window.selectRoute(route));
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                return route == "settings" ? window.currentSettingsPage() != nullptr
                                           : window.currentContentPage() &&
                                                 window.currentContentPage()->routeId() == route;
            },
            2000));
        if (auto* particles = window.findChild<layout::ParticleBackdrop*>("galleryHomeParticles"))
            particles->setEffect(layout::ParticleBackdrop::Starfield);
        if (route == "spatial-view") {
            auto* page = window.currentContentPage();
            auto* view = page->findChild<spatial::SpatialView*>("spatialPreviewView");
            ASSERT_NE(view, nullptr);
            auto* scroll = page->findChild<QScrollArea*>();
            ASSERT_NE(scroll, nullptr);
            scroll->ensureWidgetVisible(view, 0, 0);
        }
        QTest::qWait(800);
        if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            window.screen()->grabWindow(window.winId()).save(dir + "/pointer-" + route + ".png");
        }
        const auto before = controller->renderingStatistics();
        QVector<double> intervals;
        QElapsedTimer elapsed, frame;
        int swaps = 0;
        QEventLoop loop;
        const auto connection = QObject::connect(surface, &QOpenGLWidget::frameSwapped, &loop, [&] {
            ++swaps;
            if (frame.isValid())
                intervals.append(frame.nsecsElapsed() / 1e6);
            frame.start();
        });
        QTimer pointer;
        pointer.setTimerType(Qt::PreciseTimer);
        pointer.setInterval(16);
        QObject::connect(&pointer, &QTimer::timeout, &loop, [&] {
            const qreal angle = elapsed.elapsed() * .003;
            const QPointF point(window.width() * (.5 + .32 * qSin(angle)),
                                window.height() * (.55 + .25 * qCos(angle)));
            QMouseEvent move(QEvent::MouseMove, point, window.mapToGlobal(point.toPoint()),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &move);
            if (elapsed.elapsed() >= 4000)
                loop.quit();
        });
        elapsed.start();
        pointer.start();
        loop.exec();
        pointer.stop();
        QObject::disconnect(connection);
        auto result = controller->renderingStatistics();
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (it.key().endsWith("Captures") || it.key().endsWith("CaptureMs") ||
                it.key() == "paints" || it.key() == "paintMs")
                it.value() = it.value().toDouble() - before.value(it.key()).toDouble();
        }
        std::sort(intervals.begin(), intervals.end());
        ASSERT_FALSE(intervals.isEmpty());
        result["route"] = route;
        result["fps"] = swaps * 1000.0 / elapsed.elapsed();
        result["p50Ms"] = intervals[intervals.size() / 2];
        result["p95Ms"] = intervals[qMin(int(intervals.size()) - 1, int(intervals.size() * .95))];
        result["dpr"] = window.devicePixelRatioF();
        std::cout << "SPATIAL_POINTER "
                  << QJsonDocument::fromVariant(result).toJson(QJsonDocument::Compact).constData()
                  << std::endl;
    }
}

TEST_F(GallerySpatialTest, EmbeddedWindowRevalidatesContextAndRoutesProjectedInput)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL context";
    QWidget desktop;
    GalleryWindow window;
    window.resize(960, 720);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);

    // Match the browser runtime's embedded desktop, including context recreation.
    window.setParent(&desktop, Qt::Widget);
    auto* layout = new QVBoxLayout(&desktop);
    layout->addWidget(&window);
    desktop.resize(1024, 768);
    desktop.show();
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle") != nullptr,
        2000);
    auto* mode = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(mode, nullptr);
    ASSERT_NE(controller, nullptr);
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    EXPECT_TRUE(surface->isValid());
    EXPECT_TRUE(surface->property("presenting").toBool());
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&window, &leave);
    class HoverCounter final : public QObject {
    public:
        int enters = 0;
        int leaves = 0;
        bool eventFilter(QObject*, QEvent* event) override
        {
            enters += event->type() == QEvent::Enter;
            leaves += event->type() == QEvent::Leave;
            return false;
        }
    } hover;
    mode->installEventFilter(&hover);
    const QPoint point = controller->projectedPosition(mode, QPoint(20, mode->height() / 2));
    for (int index = 0; index < 4; ++index) {
        QMouseEvent move(QEvent::MouseMove, point, window.mapToGlobal(point), Qt::NoButton,
                         Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &move);
    }
    EXPECT_EQ(hover.enters, 1);
    EXPECT_EQ(hover.leaves, 0);
    mode->removeEventFilter(&hover);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                      controller->projectedPosition(mode, QPoint(20, mode->height() / 2)));
    EXPECT_FALSE(settings.spatialModeEnabled());
}

TEST_F(GallerySpatialTest, OpposedPanelsKeepLiveInputInBothNavigationLayouts)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1200, 850);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle") != nullptr,
        2000);
    auto* controller = window.findChild<GallerySpatialController*>();
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(GallerySettings::instance().spatialAvailable(), 3000);
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* navigation = window.findChild<navigation::NavigationView*>();
    auto* mode = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(surface, nullptr);
    ASSERT_NE(mode, nullptr);
    auto& settings = GallerySettings::instance();
    const auto oldStyle = settings.navigationStyle();
    const auto click = [&](QWidget* widget, QPoint point) {
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                          controller->projectedPosition(widget, point));
    };
    for (const auto style :
         {GallerySettings::NavigationStyle::Left, GallerySettings::NavigationStyle::Top}) {
        settings.setNavigationStyle(style);
        QTest::qWait(400);
        settings.setSpatialModeEnabled(true);
        controller->cancelTransition();
        QApplication::processEvents();
        EXPECT_EQ(surface->property("galleryRotationAxis").toString(),
                  style == GallerySettings::NavigationStyle::Top ? "X" : "Y");
        const qreal navigationAngle = surface->property("galleryNavigationRotation").toReal();
        const qreal contentAngle = surface->property("galleryContentRotation").toReal();
        EXPECT_GT(navigationAngle, 0);
        EXPECT_LT(contentAngle, 0);
        EXPECT_EQ(navigationAngle, -contentAngle);
        EXPECT_LE(navigationAngle, style == GallerySettings::NavigationStyle::Top ? 3 : 5);
        const QRect bounds = navigation->contentGeometry();
        const QPoint left = controller->projectedPosition(navigation, bounds.topLeft());
        const QPoint right = controller->projectedPosition(navigation, bounds.topRight());
        if (style == GallerySettings::NavigationStyle::Left)
            EXPECT_GT(left.y(), right.y());
        else {
            const QPoint bottom = controller->projectedPosition(navigation, bounds.bottomLeft());
            EXPECT_NE(left.x(), bottom.x());
        }
        // The rendered switch and inverse-mapped hit target must agree at native DPR.
        // A texture-brush origin expressed in logical pixels moves only the paint at DPR > 1.
        QTRY_VERIFY_WITH_TIMEOUT(mode->knobPosition() >= .999, 700);
        navigation->repaint();
        surface->repaint();
        auto* gl = qobject_cast<QOpenGLWidget*>(surface);
        const QImage pixels = gl ? gl->grabFramebuffer() : surface->grab().toImage();
        const QPoint onTrack(mode->width() / 5, mode->height() / 2);
        const QPoint track = (controller->projectedPosition(mode, onTrack) - surface->pos()) *
                             (qreal(pixels.width()) / surface->width());
        ASSERT_TRUE(pixels.rect().contains(track));
        const QColor color = pixels.pixelColor(track);
        EXPECT_GT(color.blue(), color.red() + 40)
            << "Painted switch must match its hit rectangle; style=" << int(style)
            << " track=" << track.x() << ',' << track.y() << " size=" << mode->width() << ','
            << mode->height();
        const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
        if (!dir.isEmpty()) {
            QDir().mkpath(dir);
            pixels.save(dir + QStringLiteral("/switch-%1.png").arg(int(style)));
        }
        // Hit the displayed switch, whose native rectangle no longer matches the surface.
        click(mode, mode->rect().center());
        EXPECT_FALSE(settings.spatialModeEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        EXPECT_FALSE(navigation->graphicsEffect()->isEnabled());
        EXPECT_FALSE(surface->property("presenting").toBool());
        QTest::mouseClick(mode, Qt::LeftButton);
        EXPECT_TRUE(settings.spatialModeEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
        EXPECT_TRUE(navigation->graphicsEffect()->isEnabled());
    }
    settings.setNavigationStyle(oldStyle);
}

TEST_F(GallerySpatialTest, PendingSpatialSurfacePreservesPaintedBackdrop)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    settings.setWindowEffect(windowing::BackdropEffect::Solid);
    for (const auto theme : {GallerySettings::ThemeMode::Dark, GallerySettings::ThemeMode::Light}) {
        settings.setThemeMode(theme);
        GalleryWindow window;
        window.resize(960, 720);
        window.findChild<GalleryContentPresenter*>()->setPrewarmPaused(true);
        auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
        ASSERT_NE(surface, nullptr);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY_WITH_TIMEOUT(surface->isValid(), 2000);
        ASSERT_NE(window.findChild<GallerySplashScreen*>(), nullptr);
        ASSERT_FALSE(surface->property("presenting").toBool());

        // A GL child replaces this part of Qt's raster backing store. Its pixels
        // must already contain the window material when the splash starts fading.
        const QImage frame = surface->grabFramebuffer();
        ASSERT_FALSE(frame.isNull());
        const QColor background = frame.pixelColor(frame.rect().center());
        EXPECT_EQ(background.alpha(), 255);
        if (theme == GallerySettings::ThemeMode::Dark)
            EXPECT_LT(background.lightness(), 80);
        else
            EXPECT_GT(background.lightness(), 220);
        EXPECT_EQ(window.findChild<navigation::NavigationView*>()->graphicsEffect(), nullptr);
    }
}

TEST_F(GallerySpatialTest, PersistedDepthWaitsForSplashAndConnectedLogoHandoff)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GallerySettings::instance().setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1100, 800);
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    ASSERT_NE(presenter, nullptr);
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    auto* navigation = window.findChild<navigation::NavigationView*>();
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* controller = window.findChild<GallerySpatialController*>();
    QPointer<GallerySplashScreen> splash = window.findChild<GallerySplashScreen*>();
    ASSERT_TRUE(splash);
    ASSERT_NE(surface, nullptr);
    QSignalSpy dismissed(splash, &GallerySplashScreen::dismissed);
    window.show();
    const WId nativeId = window.winId();
    QTest::qWait(350);
    EXPECT_TRUE(splash->isVisible());
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    EXPECT_FALSE(surface->property("presenting").toBool());
    const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        window.grab().save(dir + "/startup-splash.png");
    }
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("splashLogoTransition"), 2500);
    if (!dir.isEmpty())
        window.grab().save(dir + "/startup-logo-handoff.png");
    EXPECT_FALSE(surface->property("presenting").toBool());
    ASSERT_NE(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->graphicsEffect()->objectName(), "galleryStartupContentEffect");
    QTRY_COMPARE_WITH_TIMEOUT(dismissed.count(), 1, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(!splash && surface->property("presenting").toBool(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 1500);
    EXPECT_TRUE(navigation->graphicsEffect()->isEnabled());
    EXPECT_EQ(window.winId(), nativeId);
}

TEST_F(GallerySpatialTest, SettingsUpdateTextRemainsCompleteAfterSpatialResize)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Top);
    settings.setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1000, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage(), 2000);
    auto* status =
        window.currentSettingsPage()->findChild<textfields::Label*>("gallerySettingsUpdateStatus");
    auto* scroll = window.currentSettingsPage()->findChild<QScrollArea*>();
    ASSERT_NE(status, nullptr);
    ASSERT_NE(scroll, nullptr);
    window.findChild<GallerySpatialController*>()->cancelTransition();
    window.resize(556, 726);
    QApplication::processEvents();
    scroll->ensureWidgetVisible(status);
    QApplication::processEvents();
    const int textHeight = status->fontMetrics()
                               .boundingRect(QRect(0, 0, status->width(), 1000),
                                             Qt::TextWordWrap | Qt::AlignRight, status->text())
                               .height();
    EXPECT_TRUE(status->hasHeightForWidth());
    EXPECT_GE(status->height(), textHeight);
    EXPECT_TRUE(status->parentWidget()->rect().contains(status->geometry()));
    const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        if (auto* gl = window.findChild<QOpenGLWidget*>("gallerySpatialSurface"))
            gl->grabFramebuffer().save(dir + "/narrow-update.png");
    }
}

TEST_F(GallerySpatialTest, NativeOverlaysBlockProjectedHomeLinks)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native window hit testing and GPU composition";
    auto& settings = GallerySettings::instance();
    settings.setHomeParticlesEnabled(false);
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    for (bool spatial : {false, true}) {
        SCOPED_TRACE(spatial ? "3D" : "2D");
        settings.setSpatialModeEnabled(spatial);
        GalleryWindow window;
        auto* presenter = window.findChild<GalleryContentPresenter*>();
        presenter->setPrewarmPaused(true);
        presenter->prewarmFinished();
        window.resize(1200, 850);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<GallerySplashScreen*>(), 6500);
        auto* controller = window.findChild<GallerySpatialController*>();
        ASSERT_NE(controller, nullptr);
        if (spatial) {
            auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
            ASSERT_NE(surface, nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 3000);
            controller->cancelTransition();
        }
        auto snapshot = [&](const QString& name) {
            const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
            if (dir.isEmpty())
                return;
            QDir().mkpath(dir);
            window.raise();
            window.activateWindow();
            QTest::qWait(120);
            EXPECT_TRUE(window.screen()
                            ->grabWindow(window.winId())
                            .save(dir + (spatial ? "/3d-" : "/2d-") + name + ".png"));
        };
        auto* links = window.findChild<collections::ListView*>("galleryHomeHeroLinksView");
        ASSERT_NE(links, nullptr);
        // Observe the real activation without launching an external browser.
        QObject::disconnect(links, &collections::ListView::itemClicked, nullptr, nullptr);
        QSignalSpy activated(links, &collections::ListView::itemClicked);
        const QPoint source =
            static_cast<QListView*>(links)->visualRect(links->model()->index(0, 0)).topLeft() +
            QPoint(24, 36);
        const QPoint point = controller->projectedPosition(links->viewport(), source);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        ASSERT_EQ(activated.count(), 1);
        activated.clear();

        dialogs_flyouts::ContentDialog dialog(&window);
        dialog.setAnimationEnabled(false);
        dialog.setTitle("Close behavior");
        dialog.setContent(new textfields::Label("Choose how to close Gallery."));
        dialog.setCloseButtonText("Cancel");
        dialog.open();
        auto* scrim = window.findChild<overlay::OverlayScrim*>("DialogSmokeScrim");
        ASSERT_NE(scrim, nullptr);
        ASSERT_TRUE(scrim->isVisible());
        ASSERT_FALSE(scrim->testAttribute(Qt::WA_TransparentForMouseEvents));
        ASSERT_FALSE(dialog.geometry().contains(point));
        snapshot("modal-scrim");
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 0) << "The modal scrim must block native hit testing";
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 0) << "Host-delivered input must also respect the scrim";
        activated.clear();

        dialog.setModal(false);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 1) << "A dim-only scrim must retain modeless input";
        dialog.done(dialogs_flyouts::ContentDialog::ResultNone);
        activated.clear();

        auto* target = window.findChild<QWidget*>("galleryMainNavigationPane");
        ASSERT_NE(target, nullptr);
        GalleryIntroTour tour(&window);
        tour.setSteps({{target,
                        {},
                        "Browse by category",
                        "Explore the controls.",
                        dialogs_flyouts::CoachMark::Right}});
        tour.start();
        auto* card = window.findChild<dialogs_flyouts::CoachMark*>();
        ASSERT_NE(card, nullptr);
        QTest::qWait(350);
        textfields::Label* title = nullptr;
        for (auto* label : card->findChildren<textfields::Label*>())
            if (label->text() == "Browse by category")
                title = label;
        ASSERT_NE(title, nullptr);
        // Put ignored label input exactly over a live link, as in the reported tour.
        card->move(card->pos() + point - title->mapTo(&window, title->rect().center()));
        snapshot("intro-card");
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 0)
            << "Ignored card/label events must not reach projected links";
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 0);
        auto* next = window.findChild<basicinput::Button*>("GalleryIntroTour.NextButton");
        ASSERT_NE(next, nullptr);
        QSignalSpy finished(&tour, &GalleryIntroTour::finished);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier,
                          next->mapTo(&window, next->rect().center()));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
        QTest::qWait(350);
        activated.clear();
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        EXPECT_EQ(activated.count(), 1) << "Closing the overlay must restore ordinary links";
    }
}

TEST_F(GallerySpatialTest, IntroStaysAboveSpatialPanelsAndUsesPresentedTargets)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Auto);
    settings.setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1200, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* target = window.findChild<QWidget*>("galleryFooterNavigationPane");
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(target, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 1000);
    controller->cancelTransition();

    GalleryIntroTour tour(&window);
    tour.setSteps(
        {{target, {}, "Settings", "Choose your preferences.", dialogs_flyouts::CoachMark::Right}});
    tour.start();
    auto* scrim = window.findChild<overlay::OverlayScrim*>("GalleryIntroTour.Scrim");
    auto* card = window.findChild<dialogs_flyouts::CoachMark*>();
    ASSERT_NE(scrim, nullptr);
    ASSERT_NE(card, nullptr);
    // Completing a transition or changing the backdrop must not cover the modal layer.
    controller->cancelTransition();
    QApplication::processEvents();
    const auto siblings = window.children();
    EXPECT_LT(siblings.indexOf(surface), siblings.indexOf(scrim));
    EXPECT_LT(siblings.indexOf(scrim), siblings.indexOf(card));
    const QPolygon corners{controller->projectedPosition(target, target->rect().topLeft()),
                           controller->projectedPosition(target, target->rect().topRight()),
                           controller->projectedPosition(target, target->rect().bottomLeft()),
                           controller->projectedPosition(target, target->rect().bottomRight())};
    const QRect projected = corners.boundingRect();
    EXPECT_EQ(
        scrim->spotlightRect(),
        projected.translated(-scrim->pos()).adjusted(-1, -1, 1, 1).intersected(scrim->rect()));
    ASSERT_NE(card->target(), nullptr);
    EXPECT_EQ(QRect(card->target()->mapTo(&window, QPoint()), card->target()->size()), projected);
    EXPECT_FALSE(window.isChromeInteractive());
    auto* next = window.findChild<basicinput::Button*>("GalleryIntroTour.NextButton");
    QSignalSpy finished(&tour, &GalleryIntroTour::finished);
    QTest::mouseClick(next, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 500);
    EXPECT_TRUE(window.isChromeInteractive());
}

TEST_F(GallerySpatialTest, StartupIntroTargetsTheActiveNavigationLayout)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    auto& settings = GallerySettings::instance();
    for (bool top : {false, true}) {
        settings.setIntroCompleted(false);
        settings.setSpatialModeEnabled(true);
        settings.setNavigationStyle(top ? GallerySettings::NavigationStyle::Top
                                        : GallerySettings::NavigationStyle::Auto);
        GalleryWindow window;
        window.resize(1200, 800);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<GalleryIntroTour*>(), 6500);
        auto* scrim = window.findChild<overlay::OverlayScrim*>("GalleryIntroTour.Scrim");
        auto* card = window.findChild<dialogs_flyouts::CoachMark*>();
        auto* next = window.findChild<basicinput::Button*>("GalleryIntroTour.NextButton");
        auto* nav = window.findChild<navigation::NavigationView*>();
        auto* controller = window.findChild<GallerySpatialController*>();
        auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
        ASSERT_NE(scrim, nullptr);
        ASSERT_NE(card, nullptr);
        ASSERT_NE(next, nullptr);
        for (int step = 0; step != 4; ++step) {
            SCOPED_TRACE(::testing::Message() << "top=" << top << " step=" << step);
            EXPECT_LT(window.children().indexOf(surface), window.children().indexOf(scrim));
            if (step >= 2) {
                auto* target = step == 2 ? nav->mainChromeWidget() : nav->footerChromeWidget();
                ASSERT_TRUE(target->isVisible());
                const QPoint center =
                    controller->projectedPosition(target, target->rect().center());
                QTRY_VERIFY_WITH_TIMEOUT(
                    scrim->spotlightRect().translated(scrim->pos()).contains(center), 700);
                EXPECT_EQ(card->placement(), top ? dialogs_flyouts::CoachMark::Bottom
                                                 : dialogs_flyouts::CoachMark::Right);
                const QRect anchor(card->target()->mapTo(&window, QPoint()),
                                   card->target()->size());
                if (top)
                    QTRY_VERIFY_WITH_TIMEOUT(qAbs(card->y() - anchor.bottom()) < 40, 700);
                else
                    QTRY_VERIFY_WITH_TIMEOUT(qAbs(card->x() - anchor.right()) < 40, 700);
            }
            QTest::mouseClick(next, Qt::LeftButton);
        }
        EXPECT_TRUE(settings.introCompleted());
        EXPECT_TRUE(window.isChromeInteractive());
    }
}

TEST_F(GallerySpatialTest, IntroVisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Intro visual review requires a desktop platform";
    auto& settings = GallerySettings::instance();
    settings.setIntroCompleted(false);
    settings.setSpatialModeEnabled(true);
    settings.setWindowEffect(windowing::BackdropEffect::Mica);
    settings.setNavigationStyle(qEnvironmentVariableIsSet("FLUENT_QT_VISUAL_TOP")
                                    ? GallerySettings::NavigationStyle::Top
                                    : GallerySettings::NavigationStyle::Auto);
    GalleryWindow window;
    window.resize(1200, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<GalleryIntroTour*>(), 6500);
    if (tests::support::shouldCaptureVisualSnapshot()) {
        auto* next = window.findChild<basicinput::Button*>("GalleryIntroTour.NextButton");
        for (int step = 0; step != 4; ++step) {
            QTest::qWait(400);
            tests::support::VisualSnapshotOptions options;
            options.variant = QStringLiteral("intro-step-%1").arg(step + 1);
            ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));
            QTest::mouseClick(next, Qt::LeftButton);
        }
        return;
    }
    qApp->exec();
}

TEST_F(GallerySpatialTest, TopRailUsesWindowWidthAndPointerMotionFreezesDuringInput)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Top);
    GalleryWindow window;
    window.resize(1200, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage() != nullptr, 2000);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* navigation = window.findChild<navigation::NavigationView*>();
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(settings.spatialAvailable(), 3000);
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* follow = controller->findChild<QVariantAnimation*>("galleryPointerAnimation");
    auto* menu = window.findChild<QWidget*>("GalleryTitleBar.MenuButton");
    auto* host = navigation->contentHost();
    ASSERT_NE(follow, nullptr);
    ASSERT_NE(menu, nullptr);
    EXPECT_TRUE(menu->isHidden());
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    QApplication::processEvents();
    const QPoint left = controller->projectedPosition(navigation, QPoint(0, 20));
    const QPoint right = controller->projectedPosition(navigation, QPoint(navigation->width(), 20));
    EXPECT_GT(right.x() - left.x(), navigation->width() * .95);
    const QPoint sample(host->width() / 3, 70);
    const QPoint before = controller->projectedPosition(host, sample);
    const QPoint pointer = navigation->mapTo(&window, QPoint(30, navigation->height() - 30));
    QMouseEvent move(QEvent::MouseMove, pointer, window.mapToGlobal(pointer), Qt::NoButton,
                     Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &move);
    QTRY_VERIFY_WITH_TIMEOUT(
        QLineF(QPointF(), surface->property("galleryPointerTilt").toPointF()).length() > .1, 700);
    QTRY_VERIFY_WITH_TIMEOUT(follow->state() == QAbstractAnimation::Stopped, 700);
    const QPointF tilt = surface->property("galleryPointerTilt").toPointF();
    EXPECT_GT(QLineF(QPointF(), tilt).length(), .1);
    EXPECT_LE(qAbs(tilt.x()), .65);
    EXPECT_LE(qAbs(tilt.y()), .45);
    EXPECT_NE(controller->projectedPosition(host, sample), before);
    const QPoint press = controller->projectedPosition(host, sample);
    QMouseEvent down(QEvent::MouseButtonPress, press, window.mapToGlobal(press), Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &down);
    const QPoint frozen = controller->projectedPosition(host, sample);
    QMouseEvent drag(QEvent::MouseMove, press + QPoint(30, 20), window.mapToGlobal(press),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &drag);
    QTest::qWait(220);
    EXPECT_EQ(controller->projectedPosition(host, sample), frozen);
    EXPECT_EQ(follow->state(), QAbstractAnimation::Stopped);
    QMouseEvent up(QEvent::MouseButtonRelease, press, window.mapToGlobal(press), Qt::LeftButton,
                   Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &up);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&window, &leave);
    QTRY_COMPARE_WITH_TIMEOUT(surface->property("galleryPointerTilt").toPointF(), QPointF(), 700);
    EXPECT_EQ(follow->state(), QAbstractAnimation::Stopped);
    settings.setMotionMode(GallerySettings::MotionMode::Reduced);
    QTest::mouseMove(&window, pointer);
    EXPECT_FALSE(surface->property("presenting").toBool());
    EXPECT_EQ(follow->state(), QAbstractAnimation::Stopped);
}

TEST_F(GallerySpatialTest, HiddenSampleStylesRefreshWhenRevealed)
{
    QWidget host;
    auto* stack = new QStackedLayout(&host);
    stack->addWidget(new QWidget);
    auto* card = new GallerySampleCard(gallerySamplesForRoute("button").first());
    stack->addWidget(card);
    host.resize(800, 600);
    host.show();
    QApplication::processEvents();
    const auto flat = card->styleSheet();
    depth::setEnabled(&host, true);
    stack->setCurrentWidget(card);
    QApplication::processEvents();
    EXPECT_NE(card->styleSheet(), flat);
    stack->setCurrentIndex(0);
    depth::setEnabled(&host, false);
    stack->setCurrentWidget(card);
    QApplication::processEvents();
    EXPECT_EQ(card->styleSheet(), flat);
}

TEST_F(GallerySpatialTest, ShellAndPreviewsShareModeAndKeepSliderDrag)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1200, 900);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("spatial-view"));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.currentContentPage() &&
            window.currentContentPage()->findChild<spatial::SpatialView*>("spatialPreviewView"),
        3000);
    auto* page = window.currentContentPage();
    auto* view = page->findChild<spatial::SpatialView*>("spatialPreviewView");
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* scroll = page->findChild<QScrollArea*>();
    EXPECT_EQ(page->findChild<QWidget*>("spatialPreviewMode"), nullptr);
    ASSERT_NE(scroll, nullptr);
    scroll->ensureWidgetVisible(view);
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    QTRY_COMPARE_WITH_TIMEOUT(view->activeBackend(), spatial::SpatialView::Backend::Raster, 2000);
    EXPECT_TRUE(view->isSpatialEnabled());
    EXPECT_EQ(view->renderMode(), spatial::SpatialView::RenderMode::Auto);
    GallerySettings::instance().setSpatialModeEnabled(false);
    controller->cancelTransition();
    QTRY_COMPARE_WITH_TIMEOUT(view->renderMode(), spatial::SpatialView::RenderMode::Auto, 2000);
    EXPECT_FALSE(view->isSpatialEnabled());

    // A page first opened in 3D must lay out its canvas before projecting its cards.
    // zh_CN: 首次在 3D 模式打开页面时，先完成画布布局再投影卡片。
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    ASSERT_TRUE(window.selectRoute("spatial-item"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentContentPage() &&
                                 window.currentContentPage()->routeId() == "spatial-item",
                             3000);
    page = window.currentContentPage();
    view = page->findChild<spatial::SpatialView*>("spatialPreviewView");
    scroll = page->findChild<QScrollArea*>();
    ASSERT_NE(view, nullptr);
    ASSERT_NE(scroll, nullptr);
    scroll->ensureWidgetVisible(view);
    QTest::qWait(100);
    auto* canvas = view->findChild<QGraphicsView*>();
    ASSERT_NE(canvas, nullptr);
    EXPECT_EQ(canvas->size(), view->size());
    EXPECT_NEAR(canvas->mapFromScene(QPointF()).y(), canvas->viewport()->height() / 2.0, 1);
    for (auto* item : view->items())
        EXPECT_TRUE(QRectF(view->rect()).contains(item->projectedPolygon().boundingRect()));

    ASSERT_TRUE(window.selectRoute("slider"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentContentPage() &&
                                 window.currentContentPage()->routeId() == "slider" &&
                                 window.currentContentPage()->findChild<basicinput::Slider*>(),
                             3000);
    page = window.currentContentPage();
    scroll = page->findChild<QScrollArea*>();
    auto* slider = page->findChild<basicinput::Slider*>();
    scroll->ensureWidgetVisible(slider);
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    QApplication::processEvents();
    slider->setValue(slider->minimum());
    const QPoint start = controller->projectedPosition(slider, QPoint(14, slider->height() / 2));
    const QPoint end =
        controller->projectedPosition(slider, QPoint(slider->width() - 15, slider->height() / 2));
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, end, window.mapToGlobal(end), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&window, &move);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
    EXPECT_GT(slider->value(), slider->minimum() + (slider->maximum() - slider->minimum()) * .8);
    EXPECT_FALSE(slider->isSliderDown());
}

TEST_F(GallerySpatialTest, ProjectedWheelWorksAfterAnOverlayReceivesMouseRelease)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1100, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage() != nullptr, 2000);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* combo = window.currentSettingsPage()->findChild<basicinput::ComboBox*>(
        "gallerySettingsThemeChoice");
    auto* scroll = window.currentSettingsPage()->findChild<QScrollArea*>();
    ASSERT_NE(combo, nullptr);
    ASSERT_NE(scroll, nullptr);
    GallerySettings::instance().setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    QApplication::processEvents();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier,
                      controller->projectedPosition(combo, combo->rect().center()));
    auto* popup = window.findChild<QWidget*>("ComboBoxPopup");
    ASSERT_NE(popup, nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(popup->isVisible(), 1000);
    QTest::mouseRelease(popup, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QTest::keyClick(popup, Qt::Key_Escape);
    const auto wheel = [&](int delta) {
        const QPoint position =
            controller->projectedPosition(scroll->viewport(), scroll->viewport()->rect().center());
        QWheelEvent event(position, window.mapToGlobal(position), QPoint(), QPoint(0, delta),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&window, &event);
    };
    EXPECT_EQ(scroll->verticalScrollBar()->value(), 0);
    wheel(-480);
    QTRY_VERIFY_WITH_TIMEOUT(scroll->verticalScrollBar()->value() > 0, 1000);
    wheel(480);
    QTRY_COMPARE_WITH_TIMEOUT(scroll->verticalScrollBar()->value(), 0, 1000);
}

TEST_F(GallerySpatialTest, ProjectedWheelBubblesFromLabelsInBothLayouts)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1100, 800);
    window.show();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget*>("gallerySplashScreen") == nullptr, 6000);
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage() != nullptr, 2000);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* page = window.currentSettingsPage();
    auto* scroll = page->findChild<QScrollArea*>();
    QLabel* label = nullptr;
    for (auto* candidate : page->findChildren<QLabel*>()) {
        if (candidate->text() == QStringLiteral("3D Gallery"))
            label = candidate;
    }
    ASSERT_NE(label, nullptr);
    ASSERT_NE(scroll, nullptr);
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 2000);
    controller->cancelTransition();
    for (const auto style :
         {GallerySettings::NavigationStyle::Left, GallerySettings::NavigationStyle::Top}) {
        settings.setNavigationStyle(style);
        QApplication::processEvents();
        for (const bool pixelInput : {false, true}) {
            scroll->verticalScrollBar()->setValue(0);
            QApplication::processEvents();
            const QPoint position = controller->projectedPosition(label, label->rect().center());
            const auto wheel = [&](int delta) {
                QWheelEvent event(position, window.mapToGlobal(position),
                                  pixelInput ? QPoint(0, delta) : QPoint(),
                                  pixelInput ? QPoint() : QPoint(0, delta), Qt::NoButton,
                                  Qt::NoModifier, pixelInput ? Qt::ScrollUpdate : Qt::NoScrollPhase,
                                  false);
                QApplication::sendEvent(&window, &event);
            };
            wheel(-120);
            QTRY_VERIFY_WITH_TIMEOUT(scroll->verticalScrollBar()->value() > 0, 1000);
            wheel(120);
            QTRY_COMPARE_WITH_TIMEOUT(scroll->verticalScrollBar()->value(), 0, 1000);
        }
    }
}
