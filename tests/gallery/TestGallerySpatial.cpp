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
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QHelpEvent>
#include <QGraphicsView>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QElapsedTimer>
#include <QDir>
#include <QPointer>
#include <QPlatformSurfaceEvent>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QStyleOption>
#include <QLineEdit>
#include <QTextEdit>
#include <QInputMethodEvent>
#include <QStandardItemModel>
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
#include "view/shell/GalleryGlyphPaintDevice.h"
#include "view/shell/GalleryPanelSampler.h"
#include "view/shell/GalleryIntroTour.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
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
void dragWithLeftButton(QWidget* target, const QPoint& from, const QPoint& to)
{
    QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, from);
    // Qt 5.15/6.2 mouseMove only relocates the system cursor; its synthetic press
    // does not hold an OS button. Deliver the same held-button event on every Qt line.
    QMouseEvent move(QEvent::MouseMove, to, target->mapToGlobal(to), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    QTest::mouseRelease(target, Qt::LeftButton, Qt::NoModifier, to);
}

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
    // Windows can also lose its restored geometry when Qt replaces a maximized
    // raster window on the first opt-in. Exercise a fresh window in both states.
    const bool nativeWindows = QGuiApplication::platformName() == QLatin1String("windows");
    for (const bool maximized : {false, true}) {
        if (maximized && !nativeWindows)
            break;
        SCOPED_TRACE(::testing::Message() << "maximized=" << maximized);
        settings.setSpatialModeEnabled(false);
        settings.setWindowEffect(windowing::BackdropEffect::Mica);
        GalleryWindow window;
        auto* presenter = window.findChild<GalleryContentPresenter*>();
        presenter->setPrewarmPaused(true);
        presenter->prewarmFinished();
        window.resize(1100, 800);
        window.show();
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!window.findChild<GallerySplashScreen*>()); },
                                    60000));
        ASSERT_TRUE(window.selectRoute("settings"));
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(window.currentSettingsPage()); }, 2000));
        settings.setThemeMode(GallerySettings::ThemeMode::Dark);
        window.resize(950, 750);
        // Qt's maximized normalGeometry can subtract native frame insets even
        // with custom NCCALCSIZE chrome. Compare restoration to the actual
        // unmaximized rectangle, not that differently expressed cached value.
        const QRect restoreGeometry = window.geometry();
        if (maximized)
            window.showMaximized();
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
            window.raise();
            window.activateWindow();
            ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
            toggle->setFocus(Qt::OtherFocusReason);
            ASSERT_TRUE(QTest::qWaitFor([&] { return toggle->hasFocus(); }, 1000));
            struct NativeWindowEvents final : QObject {
                int shows = 0;
                int hides = 0;
                int nativeIdChanges = 0;
                bool eventFilter(QObject*, QEvent* event) override
                {
                    if (event->type() == QEvent::Show)
                        ++shows;
                    else if (event->type() == QEvent::Hide)
                        ++hides;
                    else if (event->type() == QEvent::WinIdChange)
                        ++nativeIdChanges;
                    return false;
                }
            } nativeEvents;
            window.installEventFilter(&nativeEvents);
            const auto firstNativeId = window.winId();
            QPointer<QWindow> firstHandle = window.windowHandle();
            QSignalSpy visibilityChanges(firstHandle, &QWindow::visibleChanged);
            const auto firstGeometry = window.geometry();
            const auto firstNormalGeometry = window.normalGeometry();
            const auto firstWindowState = window.windowState();
            const auto firstBackdrop = windowing::windowBackdropState(&window);
            QPointer<QWidget> firstFocus = QApplication::focusWidget();
            QTest::mouseClick(toggle, Qt::LeftButton, Qt::NoModifier,
                              QPoint(20, toggle->height() / 2));
            ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
            auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            ASSERT_NE(surface, nullptr);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return bool(surface->property("presenting").toBool()); }, 5000));
            auto* controller = window.findChild<GallerySpatialController*>();
            ASSERT_TRUE(
                QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
            const QString windowPlatform = QGuiApplication::platformName();
            if (windowPlatform == QLatin1String("cocoa") || nativeWindows ||
                windowPlatform == QLatin1String("xcb") ||
                windowPlatform.startsWith(QLatin1String("wayland"))) {
                EXPECT_EQ(window.winId(), firstNativeId);
                EXPECT_EQ(window.windowHandle(), firstHandle);
                EXPECT_TRUE(visibilityChanges.isEmpty())
                    << "First activation must not hide/recreate the visible native window";
                EXPECT_EQ(nativeEvents.shows, 0);
                EXPECT_EQ(nativeEvents.hides, 0);
                EXPECT_EQ(nativeEvents.nativeIdChanges, 0);
                EXPECT_EQ(window.geometry(), firstGeometry);
                EXPECT_EQ(window.normalGeometry(), firstNormalGeometry);
                EXPECT_EQ(window.windowState(), firstWindowState);
                EXPECT_EQ(QApplication::focusWidget(), firstFocus);
                const auto afterBackdrop = windowing::windowBackdropState(&window);
                EXPECT_EQ(afterBackdrop.effectiveEffect, firstBackdrop.effectiveEffect);
                EXPECT_EQ(afterBackdrop.backend, firstBackdrop.backend);
                EXPECT_EQ(afterBackdrop.surfaceMode, firstBackdrop.surfaceMode);
            }
            const auto nativeId = window.winId();
            settings.setSpatialModeEnabled(false);
            ASSERT_TRUE(
                QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
            EXPECT_TRUE(surface->isHidden());
            ASSERT_NE(navigation->graphicsEffect(), nullptr);
            EXPECT_FALSE(navigation->graphicsEffect()->isEnabled());
            EXPECT_EQ(controller->renderingStatistics()["cachedPixels"].toLongLong(), 0);
            if (maximized) {
                window.showNormal();
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] { return !window.isMaximized() && window.geometry() == restoreGeometry; },
                    1500));
                EXPECT_EQ(window.winId(), firstNativeId);
            }
            window.resize(1000, 780);
            settings.setThemeMode(GallerySettings::ThemeMode::Light);
            settings.setSpatialModeEnabled(true);
            ASSERT_TRUE(
                QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
            EXPECT_EQ(window.winId(), nativeId);
            EXPECT_EQ(window.findChild<QOpenGLWidget*>("gallerySpatialSurface"), surface);
            EXPECT_TRUE(surface->isVisible());
        }
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
        ASSERT_NE(surface, nullptr);
        ASSERT_TRUE(
            QTest::qWaitFor([&] { return bool(surface->property("presenting").toBool()); }, 1500));
        EXPECT_TRUE(surface->isValid());
        settings.setSpatialModeEnabled(false);
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller.transitionRunning()); }, 1500));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(surface->property("presenting").toBool()); }, 3000));
    settings.setSpatialModeEnabled(false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(surface->isHidden()); }, 1500));
    window.setParent(&desktop, Qt::Widget);
    desktop.resize(800, 600);
    desktop.show();
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&desktop));
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(surface->property("presenting").toBool()); }, 3000));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(!settings.spatialAvailabilityPending()); }, 6500));
    EXPECT_FALSE(settings.spatialAvailable());
    EXPECT_FALSE(depth::enabled(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(surface.isNull()); }, 5000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(!window.findChild<QWidget*>("gallerySplashScreen")); }, 60000));
    ASSERT_TRUE(settings.spatialAvailable());
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(window.currentSettingsPage()); }, 2000));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
    const auto directory = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    for (auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
        settings.setThemeMode(theme);
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
        for (const auto* name :
             {"gallerySettingsSpatialSupportBadge", "galleryNavigationSpatialSupportBadge"}) {
            SCOPED_TRACE(::testing::Message() << "theme=" << int(theme) << " badge=" << name);
            auto* badge = window.findChild<status_info::InfoBadge*>(name);
            ASSERT_NE(badge, nullptr);
            ASSERT_TRUE(QTest::qWaitFor([&] { return bool(badge->isVisible()); }, 5000));
            auto* tooltip = badge->findChild<status_info::ToolTip*>();
            ASSERT_NE(tooltip, nullptr);
            tooltip->setAnimationEnabled(false);
            window.raise();
            window.activateWindow();
            QTest::mouseMove(&window, QPoint(window.width() / 2, 25));
            QTest::mouseMove(&window, controller->projectedPosition(badge, badge->rect().center()));
            ASSERT_TRUE(QTest::qWaitFor([&] { return bool(tooltip->isVisible()); }, 3000));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return (motion->state()) == (QAbstractAnimation::Stopped); }, 500));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
    auto* overlay = window.findChild<QWidget*>("gallerySpatialSurface");
    ASSERT_NE(overlay, nullptr);
    // Splash destruction queues the compositor attachment for the next event turn.
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(overlay->property("presenting").isValid()); }, 1000));
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    auto& settings = GallerySettings::instance();
    // The first opt-in can replace Qt's raster backing store. From then on, reuse
    // that native surface across mode, material and navigation changes.
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 1000));
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
        QImage backdrop(window.size() * window.devicePixelRatioF(),
                        QImage::Format_ARGB32_Premultiplied);
        backdrop.setDevicePixelRatio(window.devicePixelRatioF());
        backdrop.fill(Qt::transparent);
        window.render(&backdrop, QPoint(), QRegion(), QWidget::RenderFlags());
        save(backdrop, name + "-backdrop");
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
        // The corner stays outside both projected panels. Mid-edge pixels can be
        // partially covered by the animated panel's MSAA even before its final pose.
        // Compare Window-only paint: flat navigation can cover the native stroke,
        // and Linux has a transparent outer frame margin.
        auto* surface = qobject_cast<QOpenGLWidget*>(overlay);
        ASSERT_NE(surface, nullptr);
        const QPoint surfaceOrigin = surface->mapTo(&window, QPoint());
        const qreal dpr = window.devicePixelRatioF();
        const QPoint exposed(qRound(surfaceOrigin.x() * dpr), qRound(surfaceOrigin.y() * dpr));
        const QPoint surfacePixel;
        const QColor expectedBackdrop = backdrop.pixelColor(exposed);
        const QImage exposedSurface = surface->grabFramebuffer();
        ASSERT_FALSE(exposedSurface.isNull());
        EXPECT_EQ(exposedSurface.pixelColor(surfacePixel), expectedBackdrop);
        if (windowing::windowBackdropRequiresTransparentClear(&window)) {
            if (scenario.navigation == Navigation::Left)
                EXPECT_EQ(before.pixelColor(gap).alpha(), 0);
            EXPECT_EQ(exposedSurface.pixelColor(surfacePixel).alpha(), 0)
                << "The exposed background must still reach native Mica/Acrylic";
        }
        motion->setCurrentTime(motion->duration());
        controller->cancelTransition();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        const QImage after = frame(&window);
        save(after, name + "-after");
        // Compare against the same native frame, not the child-painted 2D navigation.
        EXPECT_EQ(surface->grabFramebuffer().pixelColor(surfacePixel), expectedBackdrop);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    auto* pane = window.findChild<GalleryNavigationPane*>("galleryMainNavigationPane");
    ASSERT_NE(pane, nullptr);
    auto* tree = pane->findChild<collections::TreeView*>();
    ASSERT_NE(tree, nullptr);
    auto* controller = window.findChild<GallerySpatialController*>();
    const QImage before = tree->viewport()->grab().toImage();
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return navigation->graphicsEffect() && navigation->graphicsEffect()->isEnabled(); },
        2000));
    controller->cancelTransition();
    ASSERT_NE(navigation->graphicsEffect(), nullptr);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(!navigation->contentHost()->graphicsEffect()->isEnabled()); }, 1500));
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!view->isSpatialEnabled()); }, 1000));
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
    dragWithLeftButton(zoom, start, finish);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (view->renderMode()) == (spatial::SpatialView::RenderMode::Auto); }, 1000));
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
            dragWithLeftButton(canvas->viewport(), from, to);
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
            ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!tree->isExpanded(root)); }, 1000));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return (views.first()->renderMode()) == (RenderMode::Auto); }, 1000));
    EXPECT_EQ(views.last()->renderMode(), RenderMode::Auto);
    EXPECT_EQ(views.last()->activeBackend(), spatial::SpatialView::Backend::Raster);
    EXPECT_EQ(views.last()->findChild<QOpenGLWidget*>(), nullptr);
    scroll.verticalScrollBar()->setValue(scroll.verticalScrollBar()->maximum());
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (views.first()->activeBackend()) == (spatial::SpatialView::Backend::Raster); },
        1000));
    EXPECT_EQ(views.first()->findChild<QOpenGLWidget*>(), nullptr);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return (views.last()->renderMode()) == (RenderMode::Auto); }, 1000));
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
        // Keep the reveal fixture stationary even when the real cursor is over a card.
        for (auto* item : view->items())
            item->setHoverLift(0);
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
        const auto foreground = [](const QImage& image, int half) {
            QVector<QPoint> pixels;
            for (int y = half * image.height() / 2; y < (half + 1) * image.height() / 2; ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) == 255 && qGray(image.pixel(x, y)) < 120)
                        pixels.append(QPoint(x, y));
            return pixels;
        };
        const auto save = [&id](const QImage& image, const QString& suffix) {
            if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
                !dir.isEmpty()) {
                QDir().mkpath(dir);
                image.save(dir + QStringLiteral("/%1-%2.png").arg(id, suffix));
            }
        };
        scroll.verticalScrollBar()->setValue(top - 40);
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return (view->activeBackend()) == (spatial::SpatialView::Backend::OpenGL); },
            2000));
        QTest::qWait(100);
        const QImage reference = frame();
        ASSERT_FALSE(reference.isNull());
        save(reference, QStringLiteral("reference"));
        const QVector<QPoint> referenceForeground[] = {foreground(reference, 0),
                                                       foreground(reference, 1)};
        // Both real samples contain text/control detail in each half. Compare those
        // same pixels, not a font/DPR-dependent absolute number on a sparse 2 px grid.
        ASSERT_FALSE(referenceForeground[0].isEmpty());
        ASSERT_FALSE(referenceForeground[1].isEmpty());
        for (int pass = 0; pass < 4; ++pass) {
            SCOPED_TRACE(pass);
            const bool fromAbove = pass % 2 == 0;
            scroll.verticalScrollBar()->setValue(fromAbove ? top + view->height() + 20 : 0);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return (view->activeBackend()) == (spatial::SpatialView::Backend::Raster); },
                1000));
            QTest::qWait(40);
            scroll.verticalScrollBar()->setValue(
                fromAbove ? top + view->height() - 30 : top - scroll.viewport()->height() + 30);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return (view->activeBackend()) == (spatial::SpatialView::Backend::OpenGL); },
                2000));
            QTest::qWait(60);
            scroll.verticalScrollBar()->setValue(top - 40);
            QTest::qWait(100);
            // No hover, content update, repaint or grabFramebuffer: those would hide the bug.
            const QImage actual = frame();
            ASSERT_EQ(actual.size(), reference.size());
            save(actual, QStringLiteral("reveal-%1").arg(pass));
            int transparentPixels = 0;
            int changedBackgroundPixels = 0;
            for (int y = 0; y < actual.height(); ++y)
                for (int x = 0; x < actual.width(); ++x) {
                    transparentPixels += qAlpha(actual.pixel(x, y)) != 255;
                    if (x == 0 || y == 0 || x == actual.width() - 1 || y == actual.height() - 1)
                        changedBackgroundPixels += actual.pixel(x, y) != reference.pixel(x, y);
                }
            EXPECT_EQ(transparentPixels, 0) << "The exposed Fluent canvas must be fully restored";
            EXPECT_EQ(changedBackgroundPixels, 0) << "Clear/black bands must not count as ink";
            for (int half = 0; half < 2; ++half) {
                int retained = 0;
                for (const QPoint& pixel : referenceForeground[half])
                    retained +=
                        qAlpha(actual.pixel(pixel)) == 255 && qGray(actual.pixel(pixel)) < 120;
                EXPECT_GE(retained, referenceForeground[half].size() * .95)
                    << "Newly exposed detail must remain at its original position; half=" << half;
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
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return (view->activeBackend()) == (spatial::SpatialView::Backend::OpenGL); },
            2000));
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
    dragWithLeftButton(canvas->viewport(), from, to);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    auto* home = window.currentContentPage();
    EXPECT_TRUE(home->findChildren<spatial::SpatialView*>().isEmpty());
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeHeroTitle"), nullptr);
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeHeroIcon"), nullptr);
    EXPECT_NE(home->findChild<QWidget*>("galleryHomeParticles"), nullptr);
    ASSERT_TRUE(window.selectRoute("spatial"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(qobject_cast<GalleryCategoryPage*>(window.currentContentPage())); },
        2000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
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
        ASSERT_TRUE(
            QTest::qWaitFor([&] { return bool(window.currentContentPage() != nullptr); }, 2000));
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return (window.currentContentPage()->routeId()) == (QString(route)); }, 2000));
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

    // Animated targets share the budget with panel pixels and the temporary
    // paint strip. Reserving them must shorten the strip before lowering density.
    constexpr qint64 particles = 90LL * 1024 * 1024;
    const auto joint = planCaches({QSizeF(240, 700), QSizeF(960, 700)}, 2, 16384, 2,
                                  kCacheBudgetBytes, kPaintSamples, 0, 0, particles);
    ASSERT_TRUE(joint.valid());
    EXPECT_EQ(joint.dpr, normal.dpr);
    EXPECT_EQ(joint.sizes, normal.sizes);
    EXPECT_LT(joint.paintSize.height(), normal.paintSize.height());
    EXPECT_LE(joint.estimatedBytes + particles, kCacheBudgetBytes);
    EXPECT_FALSE(
        planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, kPaintSamples, 0, 0, kCacheBudgetBytes)
            .valid());
    EXPECT_FALSE(
        planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, kPaintSamples, 0, 0, -1).valid());
}

TEST_F(GallerySpatialTest, CacheResourceFailureAllowsRetry)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL context";
    GalleryWindow window;
    window.resize(1100, 760);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 60000));
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
            gpuPaints += painter.paintEngine()->type() == QPaintEngine::OpenGL2 ||
                         spatial_render::GalleryGlyphPaintDevice::delegatesToOpenGL(painter);
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

TEST_F(GallerySpatialTest, NativeDonutSampleReadoutEscapesCardInBothGalleryModes)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native scene and Gallery composition";
    auto& settings = GallerySettings::instance();
    const auto samples = gallerySamplesForRoute("spatial-view");
    const auto sample = std::find_if(samples.cbegin(), samples.cend(), [](const auto& candidate) {
        return candidate.id == QStringLiteral("spatial-view-donut");
    });
    ASSERT_NE(sample, samples.cend());
    windowing::Window window;
    spatial::SpatialRuntime::prepareWindow(&window);
    window.resize(1100, 760);
    auto* navigation = new navigation::NavigationView;
    navigation->setDisplayMode(navigation::NavigationView::DisplayMode::Left);
    window.setContentWidget(navigation);
    auto* panel = sample->createPreview(nullptr);
    navigation->contentHost()->insertPage(0, panel);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* view = panel->findChild<spatial::SpatialView*>("spatialPreviewView");
    ASSERT_NE(view, nullptr);
    ASSERT_EQ(view->itemCount(), 1);
    auto* card = view->items().first()->widget();
    auto* chart = card->findChild<charts::DonutChart*>();
    auto* slider = card->findChild<basicinput::Slider*>();
    ASSERT_NE(chart, nullptr);
    ASSERT_NE(slider, nullptr);
    for (bool spatial : {true, false, true}) {
        SCOPED_TRACE(spatial);
        settings.setSpatialModeEnabled(spatial);
        if (spatial) {
            ASSERT_TRUE(QTest::qWaitFor(
                [&] {
                    const auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
                    return surface && surface->property("presenting").toBool();
                },
                5000));
        }
        controller.cancelTransition();
        slider->setValue(spatial ? 42 : 65);
        chart->setCurrentPoint(0, 0);
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                return window.findChild<dialogs_flyouts::Popup*>("FluentChartReadout") != nullptr;
            },
            1000))
            << "visible=" << chart->isVisible() << " size=" << chart->width() << "x"
            << chart->height();
        auto* readout = window.findChild<dialogs_flyouts::Popup*>("FluentChartReadout");
        ASSERT_NE(readout, nullptr);
        ASSERT_TRUE(QTest::qWaitFor([&] { return readout->isVisible(); }, 1000));
        EXPECT_EQ(readout->parentWidget(), &window);
        EXPECT_EQ(readout->graphicsProxyWidget(), nullptr);
        EXPECT_TRUE(window.rect().contains(overlay::visibleCardGeometry(readout->geometry())));
        chart->setCurrentPoint(0, 1);
        QTest::qWait(80);
        EXPECT_EQ(readout->parentWidget(), &window);
        EXPECT_TRUE(readout->isVisible());
        if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            window.grab().save(dir + (spatial ? "/donut-readout-3d.png" : "/donut-readout-2d.png"));
        }
        readout->close();
    }
}

TEST_F(GallerySpatialTest, NativeAutoSuggestPageTypingKeepsPopupAndGpuAlive)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires actual Gallery input and native GPU composition";
    auto& settings = GallerySettings::instance();
    settings.setHomeParticlesEnabled(false);
    settings.setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1200, 900);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 15000));
    ASSERT_TRUE(window.selectRoute("auto-suggest-box"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return window.currentContentPage() && !window.currentContentPage()
                                                       ->findChildren<textfields::AutoSuggestBox*>()
                                                       .isEmpty();
        },
        10000));
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(controller, nullptr);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 5000));
    controller->cancelTransition();
    auto* box = window.currentContentPage()->findChild<textfields::AutoSuggestBox*>();
    ASSERT_NE(box, nullptr);
    const QPoint target = controller->projectedPosition(box, box->rect().center());
    QTest::mouseMove(window.windowHandle(), target);
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, target);
    ASSERT_TRUE(QTest::qWaitFor([&] { return box->hasFocus(); }, 1000));
    QTest::keyClicks(box, "a", Qt::NoModifier, 20);
    ASSERT_TRUE(QTest::qWaitFor([&] { return box->isSuggestionListOpen(); }, 1000));
    ASSERT_FALSE(surface->grabFramebuffer().isNull());
    for (const auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
        SCOPED_TRACE(theme == GallerySettings::ThemeMode::Light ? "Light" : "Dark");
        settings.setThemeMode(theme);
        box->setText(QString::fromUtf8("Visible 输入 42"));
        box->deselect();
        QTest::qWait(100);
        const QImage image = surface->grabFramebuffer();
        const qreal dpr = surface->devicePixelRatioF();
        QPolygon polygon;
        const QRect input(12, box->height() - box->inputHeight() + 4, 170, box->inputHeight() - 8);
        for (const QPoint& corner :
             {input.topLeft(), input.topRight(), input.bottomRight(), input.bottomLeft()}) {
            const QPoint projected =
                surface->mapFrom(&window, controller->projectedPosition(box, corner));
            polygon << QPoint(qRound(projected.x() * dpr), qRound(projected.y() * dpr));
        }
        const QRect bounds = polygon.boundingRect().intersected(image.rect());
        int textPixels = 0;
        for (int y = bounds.top(); y <= bounds.bottom(); ++y)
            for (int x = bounds.left(); x <= bounds.right(); ++x) {
                if (!polygon.containsPoint(QPoint(x, y), Qt::OddEvenFill))
                    continue;
                const int gray = qGray(image.pixel(x, y));
                if (theme == GallerySettings::ThemeMode::Light ? gray < 100 : gray > 180)
                    ++textPixels;
            }
        EXPECT_GT(textPixels, 40 * dpr * dpr) << "Unselected input text must reach the GPU frame";
        if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            const QString mode = theme == GallerySettings::ThemeMode::Light ? "light" : "dark";
            image.save(dir + "/gallery-input-" + mode + ".png");
            image.copy(bounds).save(dir + "/gallery-input-" + mode + "-crop.png");
        }
    }
    QTest::keyClick(box, Qt::Key_Down);
    QTest::keyClick(box, Qt::Key_Return);
    EXPECT_FALSE(box->isSuggestionListOpen());
    EXPECT_FALSE(box->text().isEmpty());
    ASSERT_FALSE(surface->grabFramebuffer().isNull());
}

TEST_F(GallerySpatialTest, NativeNestedOpacityKeepsColorAndUnrelatedPixels)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires actual GPU effect composition";
    windowing::Window window;
    spatial::SpatialRuntime::prepareWindow(&window);
    window.resize(1200, 900);
    auto* nav = new navigation::NavigationView;
    nav->setDisplayMode(navigation::NavigationView::DisplayMode::Left);
    window.setContentWidget(nav);
    auto* page = new QWidget;
    page->setAutoFillBackground(true);
    QPalette palette = page->palette();
    palette.setColor(QPalette::Window, QColor(240, 240, 240));
    page->setPalette(palette);
    nav->contentHost()->insertPage(0, page);
    nav->contentHost()->setCurrentIndex(0, 0, false);
    auto* label = new textfields::Label("Static text must stay intact", page);
    label->setGeometry(40, 40, 400, 40);
    auto* tile = new QWidget(page);
    tile->setGeometry(80, 180, 430, 160);
    tile->setAutoFillBackground(true);
    palette.setColor(QPalette::Window, QColor(220, 40, 70));
    tile->setPalette(palette);
    auto* effect = new QGraphicsOpacityEffect(tile);
    tile->setGraphicsEffect(effect);
    effect->setOpacity(1.);
    GallerySpatialController controller(&window, nav);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    GallerySettings::instance().setSpatialModeEnabled(true);
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 5000));
    controller.cancelTransition();
    const qreal dpr = surface->devicePixelRatioF();
    const auto point = [&](QWidget* widget, QPoint local) {
        const QPoint p = surface->mapFrom(&window, controller.projectedPosition(widget, local));
        return QPoint(qRound(p.x() * dpr), qRound(p.y() * dpr));
    };
    const QRect labelRect(point(label, QPoint(0, 0)), point(label, label->rect().bottomRight()));
    const QImage baseline = surface->grabFramebuffer().copy(labelRect);
    for (const int y : {180, 520}) {
        SCOPED_TRACE(y);
        tile->move(80, y);
        for (const qreal alpha : {.25, .5, .75, 1.}) {
            SCOPED_TRACE(alpha);
            effect->setOpacity(alpha);
            QTest::qWait(100);
            const QImage image = surface->grabFramebuffer();
            const QColor actual = image.pixelColor(point(tile, tile->rect().center()));
            EXPECT_NEAR(actual.red(), 240 * (1 - alpha) + 220 * alpha, 6);
            EXPECT_NEAR(actual.green(), 240 * (1 - alpha) + 40 * alpha, 6);
            EXPECT_NEAR(actual.blue(), 240 * (1 - alpha) + 70 * alpha, 6);
            EXPECT_EQ(image.copy(labelRect), baseline);
            if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
                !dir.isEmpty()) {
                QDir().mkpath(dir);
                image.save(dir + QStringLiteral("/nested-opacity-%1-%2.png").arg(y).arg(alpha));
            }
        }
    }
}

TEST_F(GallerySpatialTest, NativeStackTransitionsRemainVisibleAcrossPaintStrips)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires actual GPU transition frames";
    auto& settings = GallerySettings::instance();
    settings.setHomeParticlesEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Dark);
    settings.setSpatialModeEnabled(true);
    GalleryWindow window;
    window.resize(1200, 900);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 15000));
    ASSERT_TRUE(window.selectRoute("stack-view"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return window.currentContentPage() &&
                   window.currentContentPage()->findChild<collections::StackView*>();
        },
        10000));
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 5000));
    controller->cancelTransition();
    auto* stack = window.currentContentPage()->findChild<collections::StackView*>();
    auto* scroll = window.currentContentPage()->findChild<scrolling::ScrollView*>();
    ASSERT_NE(stack, nullptr);
    ASSERT_NE(scroll, nullptr);
    scroll->verticalScrollBar()->setValue(0);
    QTest::qWait(100);
    basicinput::Button* push = nullptr;
    for (auto* b : stack->parentWidget()->findChildren<basicinput::Button*>())
        if (b->text() == "Push page")
            push = b;
    ASSERT_NE(push, nullptr);
    const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
    if (!dir.isEmpty())
        QDir().mkpath(dir);
    for (auto type : {collections::StackView::StackViewTransitionType::SlideFade,
                      collections::StackView::StackViewTransitionType::ScaleFade}) {
        stack->setTransitionType(type);
        stack->setTransitionDuration(1000);
        push->click();
        QParallelAnimationGroup* animation = nullptr;
        for (auto* group : stack->findChildren<QParallelAnimationGroup*>())
            if (group->state() == QAbstractAnimation::Running)
                animation = group;
        ASSERT_NE(animation, nullptr);
        animation->pause();
        for (bool spatial : {true, false}) {
            settings.setSpatialModeEnabled(spatial);
            controller->cancelTransition();
            QTest::qWait(30);
            for (int time : {0, 250, 500, 750, 999}) {
                animation->setCurrentTime(time);
                QTest::qWait(30);
                const auto name = QStringLiteral("/stack-%1-%2").arg(int(type)).arg(time);
                const QImage image = spatial ? surface->grabFramebuffer() : window.grab().toImage();
                const qreal dpr = window.devicePixelRatioF();
                int colored = 0, sampled = 0;
                for (int y = 10; y < stack->height() - 10; y += 8) {
                    for (int x = 10; x < stack->width() - 10; x += 8) {
                        QPoint target =
                            spatial ? surface->mapFrom(&window, controller->projectedPosition(
                                                                    stack, QPoint(x, y)))
                                    : stack->mapTo(&window, QPoint(x, y));
                        target = QPoint(qRound(target.x() * dpr), qRound(target.y() * dpr));
                        if (!image.rect().contains(target))
                            continue;
                        const auto color = image.pixelColor(target);
                        colored += std::max({color.red(), color.green(), color.blue()}) -
                                       std::min({color.red(), color.green(), color.blue()}) >
                                   20;
                        ++sampled;
                    }
                }
                EXPECT_GT(sampled, 100);
                EXPECT_GT(colored, sampled / 4)
                    << "mode=" << spatial << " type=" << int(type) << " time=" << time;
                if (!dir.isEmpty())
                    image.save(dir + name + (spatial ? "-gpu.png" : "-2d.png"));
            }
        }
        settings.setSpatialModeEnabled(true);
        controller->cancelTransition();
        animation->setCurrentTime(1000);
        QTest::qWait(30);
    }
}

TEST_F(GallerySpatialTest, GlyphAdapterReportsOnlyDelegateCapabilities)
{
    class RestrictedEngine final : public QPaintEngine {
    public:
        RestrictedEngine() : QPaintEngine(AlphaBlend | PorterDuff | PainterPaths) {}
        bool begin(QPaintDevice*) override { return true; }
        bool end() override { return true; }
        Type type() const override { return User; }
        void updateState(const QPaintEngineState&) override {}
        void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
    } engine;
    class Device final : public QPaintDevice {
    public:
        explicit Device(QPaintEngine* engine) : engine(engine) {}
        QPaintEngine* paintEngine() const override { return engine; }
        QPaintEngine* engine;
    } target(&engine);
    spatial_render::GalleryGlyphPaintDevice adapter(target, 1);
    for (quint32 bit = 1; bit != 0; bit <<= 1) {
        const auto feature = static_cast<QPaintEngine::PaintEngineFeature>(bit);
        EXPECT_EQ(adapter.paintEngine()->hasFeature(feature), engine.hasFeature(feature)) << bit;
    }
    EXPECT_FALSE(adapter.paintEngine()->hasFeature(QPaintEngine::RasterOpModes));
}

TEST_F(GallerySpatialTest, GlyphCoveragePreservesOpacityAndOpaqueBackground)
{
    const auto paint = [](QPaintDevice* device, int alpha, qreal opacity, int backgroundAlpha) {
        QPainter painter(device);
        QFont font(QStringLiteral("Arial"));
        font.setPixelSize(22);
        painter.setFont(font);
        painter.setPen(QColor(20, 30, 40, alpha));
        painter.setOpacity(opacity);
        if (backgroundAlpha >= 0) {
            painter.setBackground(QColor(20, 100, 80, backgroundAlpha));
            painter.setBackgroundMode(Qt::OpaqueMode);
        }
        painter.drawText(QPointF(13, 90),
                         QStringLiteral("abcdefghijklmnopqrstuvwxyz 0123456789 ").repeated(3));
    };
    for (int alpha : {96, 210, 255}) {
        for (qreal opacity : {.1, .65, 1.}) {
            for (int backgroundAlpha : {-1, 120, 255}) {
                SCOPED_TRACE(::testing::Message() << "alpha=" << alpha << " opacity=" << opacity);
                QImage expected(960, 120, QImage::Format_ARGB32_Premultiplied);
                expected.fill(Qt::transparent);
                QImage actual = expected.copy();
                paint(&expected, alpha, opacity, backgroundAlpha);
                spatial_render::GalleryGlyphPaintDevice device(actual, 1);
                paint(&device, alpha, opacity, backgroundAlpha);
                if (backgroundAlpha < 0)
                    EXPECT_GT(device.glyphItems(), 0);
                else
                    EXPECT_EQ(device.glyphItems(), 0);
                EXPECT_EQ(actual, expected);
            }
        }
    }
}

class GallerySpatialEditorTest : public GallerySpatialTest,
                                 public ::testing::WithParamInterface<const char*> {};

TEST_P(GallerySpatialEditorTest, NativeFocusTypingSelectionAndImePreserveGpuAndValues)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native focused editor and actual GPU composition";
    const QString kind = QString::fromLatin1(GetParam());
    windowing::Window window;
    spatial::SpatialRuntime::prepareWindow(&window);
    window.resize(1000, 720);
    auto* navigation = new navigation::NavigationView;
    // Exercise editors, not Auto mode's initially open compact-pane flyout.
    navigation->setDisplayMode(navigation::NavigationView::DisplayMode::Left);
    window.setContentWidget(navigation);
    auto* page = new QWidget;
    navigation->contentHost()->insertPage(0, page);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    QWidget* control = nullptr;
    QAbstractItemView* itemView = nullptr;
    QStandardItemModel model(3, 2);
    for (int row = 0; row < model.rowCount(); ++row)
        for (int column = 0; column < model.columnCount(); ++column)
            model.setData(model.index(row, column), QStringLiteral("Item %1").arg(row));
    if (kind == "LineEdit")
        control = new textfields::LineEdit(page);
    else if (kind == "AutoSuggestBox") {
        auto* box = new textfields::AutoSuggestBox(page);
        box->setSuggestions({"Alpha", "Beta", "Gamma"});
        control = box;
    } else if (kind == "PasswordBox")
        control = new textfields::PasswordBox(page);
    else if (kind == "NumberBox")
        control = new textfields::NumberBox(page);
    else if (kind == "EditableComboBox") {
        auto* box = new basicinput::ComboBox(page);
        box->addItems({"Alpha", "Beta"});
        box->setEditable(true);
        control = box;
    } else if (kind == "TextEdit")
        control = new textfields::TextEdit(page);
    else {
        if (kind == "DataGrid")
            itemView = new collections::DataGrid(page);
        else if (kind == "ListView")
            itemView = new collections::ListView(page);
        else if (kind == "GridView")
            itemView = new collections::GridView(page);
        else if (kind == "TreeView")
            itemView = new collections::TreeView(page);
        ASSERT_NE(itemView, nullptr);
        itemView->setModel(&model);
        itemView->setEditTriggers(QAbstractItemView::DoubleClicked |
                                  QAbstractItemView::EditKeyPressed);
        control = itemView;
    }
    ASSERT_NE(control, nullptr);
    control->setGeometry(55, 95, 390, itemView || kind == "TextEdit" ? 220 : 72);
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            return surface && surface->property("presenting").toBool();
        },
        5000));
    controller.cancelTransition();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    if (itemView) {
        itemView->setCurrentIndex(model.index(0, 0));
        itemView->edit(model.index(0, 0));
    }
    QPointer<QLineEdit> line = qobject_cast<QLineEdit*>(control);
    if (!line)
        line = control->findChild<QLineEdit*>();
    QPointer<QTextEdit> text = control->findChild<QTextEdit*>();
    QPointer<QWidget> editor = line ? static_cast<QWidget*>(line) : static_cast<QWidget*>(text);
    ASSERT_NE(editor, nullptr) << kind.toStdString();
    const QPoint point = controller.projectedPosition(editor, editor->rect().center());
    QTest::mouseMove(window.windowHandle(), point);
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
    ASSERT_TRUE(QTest::qWaitFor([&] { return editor && editor->hasFocus(); }, 1000));
    const auto value = [&]() -> QString {
        return line ? line->text() : text ? text->toPlainText() : QString();
    };
    QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(editor, kind == "NumberBox" ? "42" : "Audit42", Qt::NoModifier, 10);
    ASSERT_TRUE(QTest::qWaitFor([&] { return value().contains("42"); }, 1000));
    ASSERT_FALSE(surface->grabFramebuffer().isNull());
    // Selection and caret repaint must use backend-supported composition modes.
    QTest::keyClick(editor, Qt::Key_Left, Qt::ShiftModifier);
    QTest::keyClick(editor, Qt::Key_Backspace);
    if (kind != "NumberBox") {
        QInputMethodEvent commit;
        commit.setCommitString(QString::fromUtf8("中文"));
        QApplication::sendEvent(editor, &commit);
        EXPECT_TRUE(value().endsWith(QString::fromUtf8("中文")));
    }
    if (!itemView)
        QTest::keyClick(editor, Qt::Key_Escape);
    const QString edited = value();
    // Let the caret blink in the actual GL cache, not only in a forced snapshot.
    QTest::qWait(650);
    EXPECT_TRUE(surface->property("presenting").toBool());
    ASSERT_FALSE(surface->grabFramebuffer().isNull());
    surface->makeCurrent();
    EXPECT_EQ(surface->context()->functions()->glGetError(), GLenum(GL_NO_ERROR));
    surface->doneCurrent();
    if (itemView && line) {
        QTest::keyClick(line, Qt::Key_Return);
        // QStyledItemDelegate queues Return's commit/close until the key event unwinds.
        ASSERT_TRUE(QTest::qWaitFor(
            [&] { return model.data(model.index(0, 0)).toString() == edited; }, 1000));
    }
    GallerySettings::instance().setSpatialModeEnabled(false);
    controller.cancelTransition();
    if (!itemView)
        EXPECT_EQ(value(), edited);
    else
        EXPECT_EQ(model.data(model.index(0, 0)).toString(), edited);
}

INSTANTIATE_TEST_SUITE_P(EditableControls, GallerySpatialEditorTest,
                         ::testing::Values("LineEdit", "AutoSuggestBox", "PasswordBox", "NumberBox",
                                           "EditableComboBox", "TextEdit", "DataGrid", "ListView",
                                           "GridView", "TreeView"),
                         [](const ::testing::TestParamInfo<const char*>& info) {
                             return info.param;
                         });

TEST_F(GallerySpatialTest, ProjectedOverlayAnchorsTrackGeometryAndRestore2D)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL shell compositor";
    QWidget top;
    top.resize(1160, 820);
    top.winId();
    top.windowHandle()->setSurfaceType(QSurface::OpenGLSurface);
    QWidget host(&top);
    host.setGeometry(20, 30, 1100, 760);
    auto* navigation = new navigation::NavigationView(&host);
    navigation->resize(host.size());
    auto* page = new QWidget;
    navigation->contentHost()->insertPage(0, page);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    auto* scroll = new QScrollArea(page);
    scroll->setGeometry(10, 20, 550, 520);
    auto* inner = new QWidget;
    inner->resize(530, 1100);
    scroll->setWidget(inner);
    auto* anchor = new basicinput::ComboBox(inner);
    anchor->setGeometry(110, 200, 160, 32);
    anchor->addItems({"First", "Second", "Third"});
    GallerySpatialController controller(&host, navigation);
    top.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&top));
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* surface = host.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            return surface && surface->property("presenting").toBool();
        },
        5000))
        << settings.spatialUnavailableReason().toStdString();
    controller.cancelTransition();

    const auto validatePublishedMapping = [&] {
        for (const QPoint point :
             {QPoint(), anchor->rect().center(), anchor->rect().bottomRight()}) {
            const QPoint expected = controller.projectedPosition(anchor, point) + host.pos();
            EXPECT_LE(
                (overlay::presentedPointInTopLevel(anchor, point) - expected).manhattanLength(), 1);
        }
    };
    validatePublishedMapping();
    EXPECT_NE(overlay::presentedRectInTopLevel(anchor),
              QRect(anchor->mapTo(&top, QPoint()), anchor->size()));

    dialogs_flyouts::Popup popup(anchor);
    popup.setModal(false);
    popup.setDim(false);
    popup.setAnimationEnabled(false);
    popup.setClosePolicy(dialogs_flyouts::Popup::NoAutoClose);
    popup.resize(200, 112);
    const QPoint point(7, anchor->height() + 8);
    popup.setPosition(anchor, point);
    popup.open();
    dialogs_flyouts::Flyout flyout(anchor);
    flyout.setAnimationEnabled(false);
    flyout.setClosePolicy(dialogs_flyouts::Popup::NoAutoClose);
    flyout.setPlacement(dialogs_flyouts::Flyout::Bottom);
    flyout.resize(200, 112);
    flyout.showAt(anchor);
    dialogs_flyouts::TeachingTip tip(anchor);
    tip.setAnimationEnabled(false);
    tip.setModal(false);
    tip.setDim(false);
    tip.setTailVisible(false);
    tip.setCardSize(QSize(168, 80));
    tip.setPreferredPlacement(dialogs_flyouts::TeachingTip::BottomLeft);
    tip.showAt(anchor);
    dialogs_flyouts::CoachMark coach(anchor);
    coach.setCardSize(QSize(168, 80));
    coach.setTarget(anchor);
    coach.setPlacement(dialogs_flyouts::CoachMark::Bottom);
    coach.open();
    const auto positionsMatch = [&] {
        const QRect bounds = overlay::presentedRectInTopLevel(anchor);
        return overlay::visibleCardGeometry(popup.geometry()).topLeft() ==
                   overlay::presentedPointInTopLevel(anchor, point) &&
               overlay::visibleCardGeometry(flyout.geometry()).topLeft() ==
                   QPoint(bounds.center().x() - 84, bounds.bottom() + flyout.anchorOffset()) &&
               overlay::visibleCardGeometry(tip.geometry()).topLeft() ==
                   QPoint(bounds.left(), bounds.bottom() + tip.placementMargin()) &&
               coach.x() == bounds.center().x() - coach.width() / 2;
    };
    ASSERT_TRUE(QTest::qWaitFor(positionsMatch, 1000));
    for (QWidget* surface : QList<QWidget*>{&popup, &flyout, &tip, &coach}) {
        EXPECT_EQ(surface->parentWidget(), &top);
        EXPECT_FALSE(surface->isWindow());
    }
    scroll->verticalScrollBar()->setValue(65);
    ASSERT_TRUE(QTest::qWaitFor(positionsMatch, 1000));
    auto* pointer = controller.findChild<QVariantAnimation*>("galleryPointerAnimation");
    pointer->setStartValue(QPointF());
    pointer->setEndValue(QPointF(.6, -.4));
    pointer->start();
    ASSERT_TRUE(QTest::qWaitFor([&] { return pointer->state() == QAbstractAnimation::Stopped; }));
    ASSERT_TRUE(QTest::qWaitFor(positionsMatch, 1000));
    validatePublishedMapping();
    top.move(top.pos() + QPoint(25, 18));
    navigation->resize(navigation->width() - 40, navigation->height() - 30);
    ASSERT_TRUE(QTest::qWaitFor(positionsMatch, 1000));
    validatePublishedMapping();
    settings.setSpatialModeEnabled(false);
    controller.cancelTransition();
    ASSERT_TRUE(QTest::qWaitFor(positionsMatch, 1000));
    EXPECT_EQ(overlay::presentationRoot(anchor), nullptr);
    EXPECT_EQ(overlay::presentedRectInTopLevel(anchor),
              QRect(anchor->mapTo(&top, QPoint()), anchor->size()));
    coach.close();
    tip.close();
    flyout.close();
    popup.close();
}

TEST_F(GallerySpatialTest, NativeMenusMapOnceAndKeepSubmenusInNativeCoordinates)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL shell compositor";
    QWidget window;
    window.resize(1000, 720);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->setAnimationEnabled(false);
    navigation->setDisplayMode(navigation::NavigationView::DisplayMode::Left);
    navigation->resize(window.size());
    auto* page = new QWidget;
    navigation->contentHost()->insertPage(0, page);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    auto* button = new basicinput::DropDownButton("Menu", page);
    button->setGeometry(100, 140, 150, 32);
    QMenu menu(button);
    menu.addAction("First");
    auto* submenu = menu.addMenu("Children");
    submenu->addAction("Child");
    button->setMenu(&menu);
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            return surface && surface->property("presenting").toBool();
        },
        5000));
    controller.cancelTransition();
    const QPoint sourcePoint = button->rect().bottomLeft();
    const QPoint expected = overlay::presentedPointToGlobal(button, sourcePoint);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier,
                      controller.projectedPosition(button, button->rect().center()));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    QTest::mouseRelease(&menu, Qt::LeftButton, Qt::NoModifier, QPoint(-2, -2));
    EXPECT_LE((menu.pos() - expected).manhattanLength(), 1);
    QApplication::processEvents();
    const QPoint submenuPoint = menu.mapToGlobal(QPoint(menu.width(), 0));
    submenu->popup(submenuPoint);
    ASSERT_TRUE(QTest::qWaitFor([&] { return submenu->isVisible(); }));
    EXPECT_LE((submenu->pos() - submenuPoint).manhattanLength(), 1);
    EXPECT_EQ(overlay::presentationRoot(submenu), nullptr);
    submenu->hide();
    GallerySettings::instance().setSpatialModeEnabled(false);
    controller.cancelTransition();
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return menu.pos() == button->mapToGlobal(sourcePoint); }, 1000));
    menu.hide();
    GallerySettings::instance().setSpatialModeEnabled(true);
    controller.cancelTransition();
    const QRect available = window.screen()->availableGeometry();
    window.move(available.bottomRight() - QPoint(window.width() / 2, window.height() / 2));
    QApplication::processEvents();
    overlay::popupMenuAt(&menu, button, sourcePoint);
    QApplication::processEvents();
    ASSERT_TRUE(menu.isVisible());
    EXPECT_TRUE(available.contains(menu.frameGeometry()));
    window.move(window.pos() + QPoint(80, 80));
    QTest::qWait(60);
    EXPECT_TRUE(!menu.isVisible() || available.contains(menu.frameGeometry()));
    menu.hide();
    QMenu retainedMenu(&window);
    retainedMenu.addAction("Retained");
    auto* transientSource = new QWidget(page);
    transientSource->setGeometry(100, 100, 80, 32);
    transientSource->show();
    QPointer<QWidget> sourceGuard(transientSource);
    QObject::connect(&retainedMenu, &QMenu::aboutToShow, &retainedMenu,
                     [transientSource] { delete transientSource; });
    overlay::popupMenuAt(&retainedMenu, transientSource, QPoint(0, 32));
    EXPECT_TRUE(sourceGuard.isNull());
    EXPECT_FALSE(retainedMenu.property(overlay::presentedMenuSourcePropertyName()).isValid());
    retainedMenu.hide();
}

TEST_F(GallerySpatialTest, GlyphAdapterReusesWidgetClipAndPreservesPainterState)
{
    class Dots final : public QWidget {
    public:
        using QWidget::QWidget;
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            for (int number = 0; number < 160; ++number) {
                painter.setPen(QPen(QColor(number, 40, 200), 1));
                painter.setBrush(QColor(80, number, 40));
                painter.setOpacity(.4 + number / 400.);
                painter.drawEllipse(QRectF(number % 16 * 4 - 10, number / 16 * 4 - 8, 5, 5));
            }
            painter.setClipRect(QRect(0, 0, 35, 28));
            painter.fillRect(QRect(-5, -5, 70, 70), QColor(30, 120, 80, 90));
            painter.translate(3, 4);
            painter.setClipping(false);
            painter.setOpacity(1);
            painter.fillRect(QRect(20, 20, 80, 80), Qt::blue);
        }
    };
    QWidget parent;
    parent.resize(240, 140);
    auto* first = new Dots(&parent);
    auto* second = new Dots(&parent);
    first->setGeometry(10, 10, 48, 44);
    second->setGeometry(85, 55, 53, 42);
    for (qreal dpr : {1., 1.5, 2.}) {
        const auto draw = [&](QPaintDevice* target) {
            QPainter painter(target);
            painter.translate(13, 7);
            parent.render(&painter, QPoint(), QRegion(QRect(0, 0, 120, 110)),
                          QWidget::DrawChildren);
        };
        QImage expected(480, 280, QImage::Format_ARGB32_Premultiplied);
        expected.setDevicePixelRatio(dpr);
        expected.fill(Qt::transparent);
        QImage actual = expected.copy();
        draw(&expected);
        spatial_render::GalleryGlyphPaintDevice device(actual, 1);
        draw(&device);
        EXPECT_EQ(actual, expected)
            << "Device-space clips must survive user-clip and state changes";
        EXPECT_LT(device.systemClipApplications(), 20)
            << "Hundreds of primitives must not rebuild the same widget clip";
    }
}

TEST_F(GallerySpatialTest, TiltedPanelReconstructionRetainsContrastWithoutCoverageFlicker)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL sampler";
    QOpenGLWidget surface;
    surface.resize(640, 400);
    surface.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&surface));
    surface.makeCurrent();
    ASSERT_NE(surface.context(), nullptr);
    auto* gl = surface.context()->functions();
    {
        // Match the low-DPI adapter's native glyphs in a 2x cached panel. This
        // exercises real tilted sampling, rather than the flat endpoint oracle.
        QImage source(600, 360, QImage::Format_RGBA8888);
        source.fill(QColor(224, 224, 224));
        {
            QPainter painter(&source);
            painter.setPen(QColor(32, 32, 32));
            QFont font = qApp->font();
            for (int row = 0; row < 3; ++row) {
                font.setPixelSize(std::array<int, 3>{14, 18, 28}[row]);
                painter.setFont(font);
                painter.drawText(QPoint(30, 55 + row * 75),
                                 QString::fromUtf8("Gallery 设置 · Popup 0123456789"));
            }
            for (int x = 30; x < 450; x += 4)
                painter.fillRect(QRect(x, 260, 1, 50), QColor(32, 32, 32));
        }
        source = source.scaled(source.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QOpenGLTexture texture(source);
        texture.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture.setWrapMode(QOpenGLTexture::ClampToEdge);
        ASSERT_TRUE(texture.isCreated());
        QOpenGLFramebufferObject target(QSize(640, 400));
        ASSERT_TRUE(target.isValid());
        spatial_render::PanelSampler linear, monotone;
        ASSERT_TRUE(linear.create(spatial_render::PanelSampler::Reconstruction::LinearFootprint));
        ASSERT_TRUE(monotone.create());
        QMatrix4x4 projection, quad;
        projection.ortho(0.f, 640.f, 400.f, 0.f, -1.f, 1.f);
        quad.translate(300, 180);
        quad.scale(300, 180);
        const auto render = [&](spatial_render::PanelSampler& sampler, const QTransform& tilt) {
            target.bind();
            gl->glViewport(0, 0, 640, 400);
            gl->glDisable(GL_BLEND);
            gl->glDisable(GL_DEPTH_TEST);
            gl->glDisable(GL_SCISSOR_TEST);
            gl->glClearColor(224.f / 255, 224.f / 255, 224.f / 255, 1);
            gl->glClear(GL_COLOR_BUFFER_BIT);
            sampler.blit(texture.textureId(), source.size(), projection * QMatrix4x4(tilt) * quad);
        };
        const auto draw = [&](spatial_render::PanelSampler& sampler, const QTransform& tilt) {
            render(sampler, tilt);
            return target.toImage();
        };
        const auto measure = [](const QImage& image, QRect region) {
            region = region.intersected(image.rect().adjusted(1, 1, -1, -1));
            double ink = 0, energy = 0;
            for (int y = region.top(); y <= region.bottom(); ++y)
                for (int x = region.left(); x <= region.right(); ++x) {
                    const int value = qGray(image.pixel(x, y));
                    ink += 224 - value;
                    energy += qPow(value - qGray(image.pixel(x - 1, y)), 2) +
                              qPow(value - qGray(image.pixel(x, y - 1)), 2);
                }
            return std::array<double, 2>{ink, ink > 0 ? energy / ink : 0};
        };
        std::array<double, 3> minimumInk{1e20, 1e20, 1e20}, maximumInk{};
        double improvement = 0;
        for (int phase = 0; phase < 8; ++phase) {
            SCOPED_TRACE(::testing::Message() << "phase=" << phase);
            const QTransform tilt(.96, .006, .00006, -.012, .97, .00003, 20. + phase / 8.,
                                  16. + phase / 20., 1.);
            const QImage before = draw(linear, tilt), after = draw(monotone, tilt);
            for (int y = 0; y < after.height(); ++y)
                for (int x = 0; x < after.width(); ++x) {
                    const int value = qGray(after.pixel(x, y));
                    ASSERT_GE(value, 31) << "Reconstruction must not undershoot the source";
                    ASSERT_LE(value, 225) << "Reconstruction must not add a bright halo";
                }
            for (int row = 0; row < 3; ++row) {
                SCOPED_TRACE(::testing::Message() << "text row=" << row);
                const QRect region =
                    tilt.mapRect(QRectF(24, 20 + row * 75, 500, 43)).toAlignedRect();
                const auto old = measure(before, region), current = measure(after, region);
                ASSERT_GT(old[1], 10);
                EXPECT_GE(current[1], old[1] * .99);
                EXPECT_NEAR(current[0] / old[0], 1., .025)
                    << "Clarity must not be achieved by changing text weight";
                improvement += current[1] / old[1];
                minimumInk[row] = qMin(minimumInk[row], current[0]);
                maximumInk[row] = qMax(maximumInk[row], current[0]);
            }
            const QRect lines = tilt.mapRect(QRectF(35, 264, 400, 40)).toAlignedRect();
            int filtered = 0;
            for (int y = lines.top(); y <= lines.bottom(); ++y)
                for (int x = lines.left(); x <= lines.right(); ++x) {
                    const int value = qGray(after.pixel(x, y));
                    filtered += value > 48 && value < 208;
                }
            EXPECT_GT(filtered, lines.width() * lines.height() * .08)
                << "Projected thin strokes must retain continuous filtered coverage";
            const QString directory = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
            if (!directory.isEmpty() && phase == 0) {
                QDir().mkpath(directory);
                before.save(directory + "/tilted-linear-reference.png");
                after.save(directory + "/tilted-monotone.png");
            }
        }
        EXPECT_GT(improvement / 24., 1.01);
        for (int row = 0; row < 3; ++row)
            EXPECT_LT(maximumInk[row] / minimumInk[row], 1.03)
                << "Subpixel movement must not cause text coverage to flicker";
        EXPECT_EQ(gl->glGetError(), GLenum(GL_NO_ERROR));
    }
    surface.doneCurrent();
}

TEST_F(GallerySpatialTest, GpuCachePreservesHighDpiControlDetail)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL paint engine";
    for (const QSize windowSize : {QSize(900, 500), QSize(1500, 1000)}) {
        GallerySettings::instance().setSpatialModeEnabled(false);
        class DetailContent final : public QWidget {
        public:
            qreal glyphDpr = 0;
            void paintEvent(QPaintEvent*) override
            {
                QPainter painter(this);
                if (const auto* device =
                        spatial_render::GalleryGlyphPaintDevice::fromPainter(painter))
                    glyphDpr = device->nativeDpr();
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
        if (spatial_render::needsNativeGlyphCoverage(window.devicePixelRatioF()))
            EXPECT_EQ(content->glyphDpr, window.devicePixelRatioF())
                << "The production cache must rasterize shaped glyphs at native density";
        else
            EXPECT_EQ(content->glyphDpr, 0)
                << "Preserve Cocoa, WebAssembly and high-DPI glyph paths";
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
        // LCD text and transparent GPU glyphs have different edge colors. Keep
        // geometry's pixel oracle separate from each text size's contrast oracle.
        const QRect detail = QRect(30, 65, 380, 170).intersected(content->rect());
        const QRect pixels(detail.topLeft() * dpr, detail.size() * dpr);
        for (int y = pixels.top(); y < pixels.bottom(); ++y)
            for (int x = pixels.left(); x < pixels.right(); ++x) {
                const QColor a = actual.pixelColor(x, y), b = reference.pixelColor(x, y);
                different += qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) +
                                 qAbs(a.blue() - b.blue()) >
                             60;
            }
        // Fractional-DPR MSAA edges differ from the native aliased fillRect even
        // without the glyph adapter. Keep this exact-pixel oracle on integer grids.
        if (qFuzzyCompare(dpr, qreal(qRound(dpr))))
            EXPECT_LT(different, pixels.width() * pixels.height() * .02)
                << "The flat GPU endpoint must retain native control detail";
        // Large glyphs can use Qt's outline path instead of its raster glyph cache.
        // Compare stroke edge contrast; exact ink weight differs between those engines.
        for (const QRect region :
             {QRect(35, 20, 370, 35), QRect(35, 260, 370, 35), QRect(35, 306, 370, 30)}) {
            SCOPED_TRACE(::testing::Message() << "dpr=" << dpr << " text row=" << region.y());
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
            const double referenceContrast = contrast(reference);
            EXPECT_GT(referenceContrast, 20);
            EXPECT_GE(contrast(actual), referenceContrast * .9);
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 60000));
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
        QPolygon polygon;
        polygon << position(button->rect().topLeft()) << position(button->rect().topRight())
                << position(button->rect().bottomRight()) << position(button->rect().bottomLeft());
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(!window.findChild<GallerySplashScreen*>()); }, 60000));
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(controller->transitionRunning()); }, 5000));
    const qint64 startMs = activation.elapsed();
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 3000));
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
        QPolygon corners;
        corners << position(rect.topLeft()) << position(rect.topRight())
                << position(rect.bottomLeft()) << position(rect.bottomRight());
        const QRect bounds = corners.boundingRect().adjusted(-4, -4, 4, 4);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(preparation >= 0); }, 3000));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(!motion || motion->state() == QAbstractAnimation::Stopped); }, 2500));
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 60000));
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

TEST_F(GallerySpatialTest, StationaryParticleFramePacingProbe)
{
    if (qEnvironmentVariableIsEmpty("FLUENT_QT_SPATIAL_BENCHMARK") ||
        tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Set FLUENT_QT_SPATIAL_BENCHMARK=1 on a native GPU desktop.";
    auto& settings = GallerySettings::instance();
    settings.setNavigationStyle(GallerySettings::NavigationStyle::Left);
    settings.setHomeParticlesEnabled(true);
    GalleryWindow window;
    auto* presenter = window.findChild<GalleryContentPresenter*>();
    presenter->setPrewarmPaused(true);
    presenter->prewarmFinished();
    window.resize(1200, 850);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.findChild<GallerySplashScreen*>(); }, 60000));
    auto* particles = window.findChild<layout::ParticleBackdrop*>("galleryHomeParticles");
    ASSERT_NE(particles, nullptr);
    particles->setEffect(layout::ParticleBackdrop::Starfield);
    settings.setSpatialModeEnabled(true);
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    controller->cancelTransition();
    QTest::mouseMove(&window, QPoint(5, 5));
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&window, &leave);
    for (bool accelerated : {false, true}) {
        if (accelerated)
            qunsetenv("FLUENT_QT_SPATIAL_PARTICLES_CPU");
        else
            qputenv("FLUENT_QT_SPATIAL_PARTICLES_CPU", "1");
        QTest::qWait(800);
        const auto before = controller->renderingStatistics();
        QElapsedTimer elapsed, frame;
        QVector<double> intervals;
        int swaps = 0;
        QEventLoop loop;
        const auto connection = QObject::connect(surface, &QOpenGLWidget::frameSwapped, &loop, [&] {
            ++swaps;
            if (frame.isValid())
                intervals.append(frame.nsecsElapsed() / 1e6);
            frame.start();
        });
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        elapsed.start();
        loop.exec();
        QObject::disconnect(connection);
        auto result = controller->renderingStatistics();
        for (auto it = result.begin(); it != result.end(); ++it) {
            if (it.key().endsWith("Captures") || it.key().endsWith("CaptureMs") ||
                it.key() == "paints" || it.key() == "paintMs" || it.key() == "particleFrames" ||
                it.key() == "particleCompositions")
                it.value() = it.value().toDouble() - before.value(it.key()).toDouble();
        }
        std::sort(intervals.begin(), intervals.end());
        ASSERT_FALSE(intervals.isEmpty());
        result["mode"] = accelerated ? "automatic" : "cpu";
        result["route"] = "home-particles-only";
        result["fps"] = swaps * 1000.0 / elapsed.elapsed();
        result["p50Ms"] = intervals[intervals.size() / 2];
        result["p95Ms"] = intervals[qMin(int(intervals.size()) - 1, int(intervals.size() * .95))];
        result["dpr"] = window.devicePixelRatioF();
        if (accelerated && qFuzzyCompare(window.devicePixelRatioF(), 2.0)) {
            // Regression for the real 1200x850 Home, whose full hero does not fit
            // at cache DPR 4. A passing benchmark must actually use GPU particles.
            EXPECT_GT(result["particleLayers"].toInt(), 0);
            EXPECT_GT(result["particleFrames"].toLongLong(), 0);
            EXPECT_GE(result["cacheDpr"].toDouble(), window.devicePixelRatioF());
            EXPECT_LE(result["cacheEstimatedBytes"].toLongLong(),
                      result["cacheBudgetBytes"].toLongLong());
            EXPECT_EQ(result["particleBytes"], before["particleBytes"]);
            EXPECT_EQ(result["contentTextureId"], before["contentTextureId"]);
            EXPECT_EQ(result["allocationMs"], before["allocationMs"]);
            EXPECT_EQ(result["contentCaptures"].toLongLong(), 0);
            EXPECT_EQ(result["navigationCaptures"].toLongLong(), 0);
            EXPECT_EQ(result["particleForegroundCaptures"].toLongLong(), 0);
        }
        if (const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            const QString mode = accelerated ? "joint" : "reference";
            const auto frame = surface->grabFramebuffer();
            EXPECT_EQ(frame.size(), QSize(qRound(surface->width() * window.devicePixelRatioF()),
                                          qRound(surface->height() * window.devicePixelRatioF())));
            frame.save(dir + "/home-particles-" + mode + ".png");
            for (const auto& name : {"galleryHomeHeroTitle", "galleryHomeHeroTagline"}) {
                auto* label = window.findChild<QWidget*>(QLatin1String(name));
                ASSERT_NE(label, nullptr);
                QPolygon outline;
                for (const auto& point : {label->rect().topLeft(), label->rect().topRight(),
                                          label->rect().bottomRight(), label->rect().bottomLeft()})
                    outline << surface->mapFrom(&window,
                                                controller->projectedPosition(label, point));
                const auto crop = outline.boundingRect().adjusted(-2, -2, 2, 2);
                frame
                    .copy(QRect(crop.topLeft() * window.devicePixelRatioF(),
                                crop.size() * window.devicePixelRatioF()))
                    .save(dir + "/home-particles-" + mode + "-" + QLatin1String(name) + ".png");
            }
        }
        std::cout << "SPATIAL_PARTICLES "
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));

    // Match the browser runtime's embedded desktop, including context recreation.
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    window.setParent(&desktop, Qt::Widget);
    auto* layout = new QVBoxLayout(&desktop);
    layout->addWidget(&window);
    desktop.resize(1024, 768);
    desktop.show();
    window.show();
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(window.findChild<basicinput::ToggleSwitch*>(
                            "gallerySettingsSpatialModeToggle") != nullptr);
        },
        2000));
    auto* mode = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(mode, nullptr);
    ASSERT_NE(controller, nullptr);
    settings.setSpatialModeEnabled(true);
    surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    // Availability can retain the previous context's result, and a queued
    // presentation has not started its animation yet. Wait for the real surface.
    ASSERT_TRUE(QTest::qWaitFor([&] { return surface->property("presenting").toBool(); }, 3000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(window.findChild<basicinput::ToggleSwitch*>(
                            "gallerySettingsSpatialModeToggle") != nullptr);
        },
        2000));
    auto* controller = window.findChild<GallerySpatialController*>();
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(GallerySettings::instance().spatialAvailable()); }, 3000));
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(mode->knobPosition() >= .999); }, 700));
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
        ASSERT_NE(navigation->graphicsEffect(), nullptr);
        EXPECT_FALSE(navigation->graphicsEffect()->isEnabled());
        EXPECT_FALSE(surface->property("presenting").toBool());
        QTest::mouseClick(mode, Qt::LeftButton);
        EXPECT_TRUE(settings.spatialModeEnabled());
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(surface->isValid()); }, 2000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("splashLogoTransition")); }, 2500));
    if (!dir.isEmpty())
        window.grab().save(dir + "/startup-logo-handoff.png");
    EXPECT_FALSE(surface->property("presenting").toBool());
    ASSERT_NE(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->graphicsEffect()->objectName(), "galleryStartupContentEffect");
    ASSERT_TRUE(QTest::qWaitFor([&] { return (dismissed.count()) == (1); }, 1500));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(!splash && surface->property("presenting").toBool()); }, 1000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!controller->transitionRunning()); }, 1500));
    ASSERT_NE(navigation->graphicsEffect(), nullptr);
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(window.currentSettingsPage()); }, 2000));
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return bool(!window.findChild<GallerySplashScreen*>()); },
                                    60000));
        auto* controller = window.findChild<GallerySpatialController*>();
        ASSERT_NE(controller, nullptr);
        if (spatial) {
            auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
            ASSERT_NE(surface, nullptr);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return bool(surface->property("presenting").toBool()); }, 3000));
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
        ASSERT_TRUE(QTest::qWaitFor([&] { return (finished.count()) == (1); }, 1000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* target = window.findChild<QWidget*>("galleryFooterNavigationPane");
    ASSERT_NE(controller, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(surface->property("presenting").toBool()); }, 1000));
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
    QPolygon corners;
    corners << controller->projectedPosition(target, target->rect().topLeft())
            << controller->projectedPosition(target, target->rect().topRight())
            << controller->projectedPosition(target, target->rect().bottomLeft())
            << controller->projectedPosition(target, target->rect().bottomRight());
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
    ASSERT_TRUE(QTest::qWaitFor([&] { return (finished.count()) == (1); }, 500));
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
        ASSERT_TRUE(
            QTest::qWaitFor([&] { return bool(window.findChild<GalleryIntroTour*>()); }, 60000));
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
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] {
                        return bool(
                            scrim->spotlightRect().translated(scrim->pos()).contains(center));
                    },
                    700));
                EXPECT_EQ(card->placement(), top ? dialogs_flyouts::CoachMark::Bottom
                                                 : dialogs_flyouts::CoachMark::Right);
                const QRect anchor(card->target()->mapTo(&window, QPoint()),
                                   card->target()->size());
                if (top)
                    ASSERT_TRUE(QTest::qWaitFor(
                        [&] { return bool(qAbs(card->y() - anchor.bottom()) < 40); }, 700));
                else
                    ASSERT_TRUE(QTest::qWaitFor(
                        [&] { return bool(qAbs(card->x() - anchor.right()) < 40); }, 700));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(window.findChild<GalleryIntroTour*>()); }, 60000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(window.currentSettingsPage() != nullptr); }, 2000));
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* navigation = window.findChild<navigation::NavigationView*>();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(settings.spatialAvailable()); }, 3000));
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    auto* follow = controller->findChild<QVariantAnimation*>("galleryPointerAnimation");
    auto* menu = window.findChild<QWidget*>("GalleryTitleBar.MenuButton");
    auto* host = navigation->contentHost();
    ASSERT_NE(follow, nullptr);
    ASSERT_NE(menu, nullptr);
    EXPECT_TRUE(menu->isHidden());
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(
                QLineF(QPointF(), surface->property("galleryPointerTilt").toPointF()).length() >
                .1);
        },
        700));
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(follow->state() == QAbstractAnimation::Stopped); }, 700));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (surface->property("galleryPointerTilt").toPointF()) == (QPointF()); }, 700));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("spatial-view"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(window.currentContentPage() &&
                        window.currentContentPage()->findChild<spatial::SpatialView*>(
                            "spatialPreviewView"));
        },
        3000));
    auto* page = window.currentContentPage();
    auto* view = page->findChild<spatial::SpatialView*>("spatialPreviewView");
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* scroll = page->findChild<QScrollArea*>();
    EXPECT_EQ(page->findChild<QWidget*>("spatialPreviewMode"), nullptr);
    ASSERT_NE(scroll, nullptr);
    scroll->ensureWidgetVisible(view);
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
    controller->cancelTransition();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (view->activeBackend()) == (spatial::SpatialView::Backend::Raster); }, 2000));
    EXPECT_TRUE(view->isSpatialEnabled());
    EXPECT_EQ(view->renderMode(), spatial::SpatialView::RenderMode::Auto);
    GallerySettings::instance().setSpatialModeEnabled(false);
    controller->cancelTransition();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (view->renderMode()) == (spatial::SpatialView::RenderMode::Auto); }, 2000));
    EXPECT_FALSE(view->isSpatialEnabled());

    // A page first opened in 3D must lay out its canvas before projecting its cards.
    // zh_CN: 首次在 3D 模式打开页面时，先完成画布布局再投影卡片。
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
    controller->cancelTransition();
    ASSERT_TRUE(window.selectRoute("spatial-item"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(window.currentContentPage() &&
                        window.currentContentPage()->routeId() == "spatial-item");
        },
        3000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            return bool(window.currentContentPage() &&
                        window.currentContentPage()->routeId() == "slider" &&
                        window.currentContentPage()->findChild<basicinput::Slider*>());
        },
        3000));
    page = window.currentContentPage();
    scroll = page->findChild<QScrollArea*>();
    auto* slider = page->findChild<basicinput::Slider*>();
    scroll->ensureWidgetVisible(slider);
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
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
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(window.currentSettingsPage() != nullptr); }, 2000));
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* combo = window.currentSettingsPage()->findChild<basicinput::ComboBox*>(
        "gallerySettingsThemeChoice");
    auto* scroll = window.currentSettingsPage()->findChild<QScrollArea*>();
    ASSERT_NE(combo, nullptr);
    ASSERT_NE(scroll, nullptr);
    GallerySettings::instance().setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
    controller->cancelTransition();
    QApplication::processEvents();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier,
                      controller->projectedPosition(combo, combo->rect().center()));
    auto* popup = window.findChild<QWidget*>("ComboBoxPopup");
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(popup->isVisible()); }, 1000));
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
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(scroll->verticalScrollBar()->value() > 0); }, 1000));
    wheel(480);
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return (scroll->verticalScrollBar()->value()) == (0); }, 1000));
}

TEST_F(GallerySpatialTest, ProjectedWheelBubblesFromLabelsInBothLayouts)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires native GPU composition; headless fallback is covered separately";
    GalleryWindow window;
    window.resize(1100, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return bool(window.findChild<QWidget*>("gallerySplashScreen") == nullptr); }, 60000));
    ASSERT_TRUE(window.selectRoute("settings"));
    ASSERT_TRUE(
        QTest::qWaitFor([&] { return bool(window.currentSettingsPage() != nullptr); }, 2000));
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
    struct WheelObserver final : QObject {
        int count = 0;
        QPoint pixels;
        QPoint angles;
        Qt::ScrollPhase phase = Qt::NoScrollPhase;
        bool eventFilter(QObject*, QEvent* event) override
        {
            if (event->type() == QEvent::Wheel) {
                const auto* wheel = static_cast<QWheelEvent*>(event);
                ++count;
                pixels = wheel->pixelDelta();
                angles = wheel->angleDelta();
                phase = wheel->phase();
            }
            return false;
        }
    } observed;
    scroll->viewport()->installEventFilter(&observed);
    auto& settings = GallerySettings::instance();
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return bool(depth::enabled(&window)); }, 2000));
    controller->cancelTransition();
    for (const auto style :
         {GallerySettings::NavigationStyle::Left, GallerySettings::NavigationStyle::Top}) {
        settings.setNavigationStyle(style);
        QApplication::processEvents();
        for (const bool pixelInput : {false, true}) {
            scroll->verticalScrollBar()->setValue(0);
            QApplication::processEvents();
            const QPoint position = controller->projectedPosition(label, label->rect().center());
            const QPoint viewportPoint = label->mapTo(scroll->viewport(), label->rect().center());
            SCOPED_TRACE(::testing::Message()
                         << "style=" << int(style) << " pixelInput=" << pixelInput << " viewport="
                         << scroll->viewport()->width() << 'x' << scroll->viewport()->height()
                         << " labelCenter=" << viewportPoint.x() << ',' << viewportPoint.y()
                         << " projected=" << position.x() << ',' << position.y());
            const auto wheel = [&](int delta) {
                // Native wheel packets retain angleDelta even with pixel data.
                // Qt's native scrollbars can ignore pixel-only mouse packets.
                QWheelEvent event(position, window.mapToGlobal(position),
                                  pixelInput ? QPoint(0, delta) : QPoint(), QPoint(0, delta),
                                  Qt::NoButton, Qt::NoModifier,
                                  pixelInput ? Qt::ScrollUpdate : Qt::NoScrollPhase, false);
                QApplication::sendEvent(&window, &event);
            };
            const int previousWheelCount = observed.count;
            wheel(-120);
            ASSERT_GT(observed.count, previousWheelCount);
            EXPECT_EQ(observed.pixels, pixelInput ? QPoint(0, -120) : QPoint());
            EXPECT_EQ(observed.angles, QPoint(0, -120));
            EXPECT_EQ(observed.phase, pixelInput ? Qt::ScrollUpdate : Qt::NoScrollPhase);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return bool(scroll->verticalScrollBar()->value() > 0); }, 1000));
            wheel(120);
            ASSERT_TRUE(QTest::qWaitFor(
                [&] { return (scroll->verticalScrollBar()->value()) == (0); }, 1000));
        }
    }
}
