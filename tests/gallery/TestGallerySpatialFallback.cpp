#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <QDir>
#include <QGraphicsEffect>
#include <QHelpEvent>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QScrollBar>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>

#include "QtTestEnvironment.h"
#include "model/GalleryComponentCatalog.h"
#include "platform/GalleryPlatform.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/SettingsPage.h"
#include "view/shell/GalleryContentPresenter.h"
#include "view/shell/GalleryNavigationPane.h"
#include "view/shell/GallerySpatialController.h"
#include "view/shell/GalleryWindow.h"
#include "view/support/GalleryDepth.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "viewmodel/GallerySettings.h"
#include "viewmodel/GalleryNavigationViewModel.h"

using namespace fluent;
using namespace fluent::gallery;
namespace {
class GallerySpatialFallbackTest : public ::testing::Test {
protected:
    QByteArray oldDisabled = qgetenv("FLUENT_QT_GALLERY_DISABLE_3D");
    GallerySettings& settings = GallerySettings::instance();
    bool oldSpatial = settings.spatialModeEnabled();
    bool oldIntro = settings.introCompleted();
    bool oldAvailable = settings.spatialAvailable();
    bool oldPending = settings.spatialAvailabilityPending();
    QString oldReason = settings.spatialUnavailableReason();
    GallerySettings::MotionMode oldMotion = settings.motionMode();
    GallerySettings::ThemeMode oldTheme = settings.themeMode();

    void SetUp() override
    {
        settings.setIntroCompleted(true);
        settings.setMotionMode(MotionPolicy::Mode::Full);
        settings.setThemeMode(GallerySettings::ThemeMode::Light);
        settings.setSpatialModeEnabled(true);
    }
    void TearDown() override
    {
        if (oldDisabled.isNull())
            qunsetenv("FLUENT_QT_GALLERY_DISABLE_3D");
        else
            qputenv("FLUENT_QT_GALLERY_DISABLE_3D", oldDisabled);
        settings.setSpatialModeEnabled(oldSpatial);
        settings.setIntroCompleted(oldIntro);
        settings.setMotionMode(oldMotion);
        settings.setThemeMode(oldTheme);
        if (oldPending)
            settings.beginSpatialAvailabilityCheck();
        else
            settings.setSpatialAvailability(oldAvailable, oldReason);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void finishStartup(GalleryWindow& window)
    {
        auto* presenter = window.findChild<GalleryContentPresenter*>();
        presenter->setPrewarmPaused(true);
        presenter->prewarmFinished();
        window.resize(1050, 800);
        window.show();
    }
    void capture(QWidget& window, const QString& name)
    {
        const auto dir = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
        if (!dir.isEmpty() && !tests::support::isHeadlessPlatform()) {
            QCoreApplication::processEvents();
            // Include the system-composited window material, which QWidget::grab omits.
            QTest::qWait(300);
            const auto frame = window.screen()->grabWindow(window.winId());
            ASSERT_FALSE(frame.isNull());
            EXPECT_TRUE(frame.save(QDir(dir).filePath(name + ".png")));
        }
    }
};
} // namespace

TEST(GallerySpatialPreferenceTest, ColdLoadProbe)
{
    const QString scenario = qEnvironmentVariable("FLUENTQT_GALLERY_SPATIAL_COLD_LOAD");
    if (scenario.isEmpty())
        GTEST_SKIP() << "Only exercised by the isolated preference test";
    ASSERT_TRUE(QStandardPaths::isTestModeEnabled());
    QCoreApplication::setOrganizationName("Fluent-Qt");
    QCoreApplication::setApplicationName(platform::capabilities().applicationName);
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ASSERT_TRUE(QDir().mkpath(dataPath));
    QLockFile lock(QDir(dataPath).filePath("settings-cold-load.lock"));
    ASSERT_TRUE(lock.tryLock(10000));
    QSettings storage = platform::createSettings();
    const QStringList keys = {"settings/spatialModeEnabled", "settings/themeMode",
                              "settings/motionMode", "intro/completed", "home/particlesEnabled"};
    QMap<QString, QVariant> previous;
    for (const auto& key : keys) {
        if (storage.contains(key))
            previous.insert(key, storage.value(key));
        storage.remove(key);
    }
    const auto restore = qScopeGuard([&] {
        for (const auto& key : keys) {
            if (previous.contains(key))
                storage.setValue(key, previous.value(key));
            else
                storage.remove(key);
        }
        storage.sync();
    });
    if (scenario != "default")
        storage.setValue("settings/spatialModeEnabled", scenario != "saved-2d");
    storage.setValue("settings/themeMode", scenario == "high-contrast" ? 3 : 1);
    storage.setValue("settings/motionMode", scenario == "reduced" ? 1 : 0);
    storage.setValue("intro/completed", true);
    storage.setValue("home/particlesEnabled", false);
    storage.sync();

    auto& settings = GallerySettings::instance();
#ifdef FLUENT_QT_HAS_SPATIAL
    constexpr bool defaultEnabled = true;
#else
    constexpr bool defaultEnabled = false;
#endif
    const bool expected = scenario == "saved-3d" || (scenario == "default" && defaultEnabled);
    EXPECT_EQ(settings.spatialModeEnabled(), expected);
    EXPECT_EQ(storage.contains("settings/spatialModeEnabled"), scenario != "default");
#ifdef FLUENT_QT_HAS_SPATIAL
    if (scenario == "default" && !tests::support::isHeadlessPlatform()) {
        GalleryWindow window;
        auto* presenter = window.findChild<GalleryContentPresenter*>();
        presenter->setPrewarmPaused(true);
        presenter->prewarmFinished();
        window.resize(1200, 820);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget*>("gallerySplashScreen"), 6500);
        QTRY_VERIFY_WITH_TIMEOUT(!settings.spatialAvailabilityPending(), 6000);
        auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
        if (settings.spatialAvailable()) {
            ASSERT_NE(surface, nullptr);
            QTRY_VERIFY_WITH_TIMEOUT(surface->property("presenting").toBool(), 2000);
            EXPECT_TRUE(depth::enabled(&window));
        } else {
            EXPECT_FALSE(depth::enabled(&window));
        }
        EXPECT_TRUE(settings.spatialModeEnabled());
        EXPECT_FALSE(storage.contains("settings/spatialModeEnabled"));
        ASSERT_TRUE(window.selectRoute("spatial-view"));
        QTRY_VERIFY_WITH_TIMEOUT(window.currentContentPage() &&
                                     window.currentContentPage()->routeId() == "spatial-view",
                                 2000);
        auto* page = window.currentContentPage();
        charts::DonutChart* chart = nullptr;
        spatial::SpatialView* preview = nullptr;
        QWidget* card = nullptr;
        for (auto* candidate : page->findChildren<spatial::SpatialView*>()) {
            for (auto* item : candidate->items()) {
                auto* match =
                    item->widget()->findChild<charts::DonutChart*>("spatialAllocationChart");
                if (match) {
                    chart = match;
                    preview = candidate;
                    card = item->widget();
                }
            }
        }
        ASSERT_NE(chart, nullptr);
        auto* more = page->findChild<layout::Expander*>("galleryMoreSpatialExamples");
        ASSERT_NE(more, nullptr);
        EXPECT_FALSE(more->isExpanded());
        EXPECT_FALSE(more->isAncestorOf(preview));
        auto* view = card->findChild<basicinput::Slider*>("spatialAllocationSlider");
        ASSERT_NE(view, nullptr);
        view->setValue(72);
        EXPECT_EQ(chart->model()->pointAt(0).y(), 72);
        const QString directory = qEnvironmentVariable("FLUENT_QT_SPATIAL_EVIDENCE");
        if (!directory.isEmpty()) {
            auto* scroll = page->findChild<scrolling::ScrollView*>();
            ASSERT_NE(scroll, nullptr);
            for (auto theme :
                 {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
                settings.setThemeMode(theme);
                scroll->ensureWidgetVisible(preview, 0, 48);
                QTest::qWait(350);
                const QString name = theme == GallerySettings::ThemeMode::Light ? "light" : "dark";
                EXPECT_TRUE(window.screen()
                                ->grabWindow(window.winId())
                                .save(directory + "/default-3d-chart-" + name + ".png"));
            }
        }
    }
#endif
    if (expected) {
        settings.setSpatialModeEnabled(false);
        storage.sync();
        EXPECT_FALSE(storage.value("settings/spatialModeEnabled").toBool());
    }
}

TEST(GallerySpatialPreferenceTest, DefaultsAndSavedChoicesLoadInFreshProcesses)
{
    for (const QString& scenario :
         {"default", "saved-2d", "saved-3d", "reduced", "high-contrast"}) {
        QProcess probe;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("FLUENTQT_GALLERY_SPATIAL_COLD_LOAD", scenario);
        probe.setProcessEnvironment(environment);
        probe.start(QCoreApplication::applicationFilePath(),
                    {"--gtest_filter=GallerySpatialPreferenceTest.ColdLoadProbe"});
        ASSERT_TRUE(probe.waitForStarted(10000));
        ASSERT_TRUE(probe.waitForFinished(30000));
        EXPECT_EQ(probe.exitStatus(), QProcess::NormalExit);
        EXPECT_EQ(probe.exitCode(), 0) << scenario.toStdString() << "\n"
                                       << probe.readAllStandardOutput().toStdString()
                                       << probe.readAllStandardError().toStdString();
    }
}

TEST_F(GallerySpatialFallbackTest, SupportBadgesTrackCapabilityInsteadOfTheModePreference)
{
    GalleryNavigationViewModel model;
    GalleryNavigationPane pane(model.mainPaneItems());
    SettingsPage page(*model.itemById("settings"));
    auto* settingsBadge =
        page.findChild<status_info::InfoBadge*>("gallerySettingsSpatialSupportBadge");
    ASSERT_NE(settingsBadge, nullptr);
    auto badges = QList<status_info::InfoBadge*>{settingsBadge};
#ifdef FLUENT_QT_HAS_SPATIAL
    auto* navigationBadge =
        pane.findChild<status_info::InfoBadge*>("galleryNavigationSpatialSupportBadge");
    ASSERT_NE(navigationBadge, nullptr);
    badges.append(navigationBadge);
#endif
    auto* toggle = page.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    ASSERT_NE(toggle, nullptr);
    const auto expectNeutral = [&] {
        for (auto* badge : badges) {
            EXPECT_EQ(badge->status(), status_info::InfoBadge::InfoBadgeStatus::Informational);
            EXPECT_EQ(badge->effectiveForegroundColor(), badge->themeColorsRef().textSecondary);
            EXPECT_TRUE(badge->toolTip().contains("GPU"));
            EXPECT_EQ(badge->accessibleDescription(), badge->toolTip());
        }
    };
    settings.beginSpatialAvailabilityCheck();
    expectNeutral();
    settings.setSpatialAvailability(true);
    settings.setSpatialModeEnabled(false);
    expectNeutral();
    settings.setMotionMode(MotionPolicy::Mode::Reduced);
    expectNeutral();
    EXPECT_FALSE(toggle->isEnabled());
    settings.setMotionMode(MotionPolicy::Mode::Full);
    settings.setSpatialAvailability(false, "Hardware acceleration is unavailable. Using 2D.");
    EXPECT_FALSE(toggle->isEnabled());
    for (auto theme : {GallerySettings::ThemeMode::Light, GallerySettings::ThemeMode::Dark}) {
        settings.setThemeMode(theme);
        for (auto* badge : badges) {
            EXPECT_TRUE(badge->isEnabled());
            EXPECT_EQ(badge->status(), status_info::InfoBadge::InfoBadgeStatus::Critical);
            QTRY_COMPARE(badge->effectiveForegroundColor(), badge->themeColorsRef().systemCritical);
            EXPECT_TRUE(badge->toolTip().contains("unavailable"));
            EXPECT_TRUE(badge->toolTip().contains(settings.spatialUnavailableReason()));
        }
    }
    settings.setSpatialAvailability(true);
    expectNeutral();
}

#ifdef FLUENT_QT_HAS_SPATIAL
TEST_F(GallerySpatialFallbackTest, NavigationSupportBadgeFollowsTheRowWithoutActivatingIt)
{
    GalleryNavigationViewModel model;
    GalleryNavigationPane pane(model.mainPaneItems());
    pane.resize(260, 660);
    pane.show();
    auto* badge = pane.findChild<status_info::InfoBadge*>("galleryNavigationSpatialSupportBadge");
    auto* tree = pane.findChild<collections::TreeView*>("galleryMainNavigationTreeView");
    ASSERT_NE(badge, nullptr);
    ASSERT_NE(tree, nullptr);
    QTRY_VERIFY(badge->isVisible());
    const auto spatial = pane.indexForRouteId("spatial");
    const auto checkAligned = [&] {
        const auto row = tree->visualRect(spatial);
        EXPECT_LE(qAbs(row.center().y() - badge->geometry().center().y()), 1);
        EXPECT_TRUE(tree->viewport()->rect().contains(badge->geometry()));
    };
    checkAligned();
    QSignalSpy activated(&pane, &GalleryNavigationPane::routeActivated);
    QTest::mouseClick(badge, Qt::LeftButton);
    EXPECT_EQ(activated.count(), 0);
    EXPECT_FALSE(tree->isExpanded(spatial));
    const auto basicInput = pane.indexForRouteId("basic-input");
    ASSERT_TRUE(basicInput.isValid());
    tree->expand(basicInput);
    tree->scrollTo(spatial, QAbstractItemView::PositionAtCenter);
    QCoreApplication::processEvents();
    checkAligned();
    pane.setCompact(true);
    QTRY_VERIFY(badge->isHidden());
    pane.setCompact(false);
    tree->scrollTo(spatial, QAbstractItemView::PositionAtCenter);
    QTRY_VERIFY(badge->isVisible());
    checkAligned();
    settings.setSpatialAvailability(false, "Hardware acceleration is unavailable. Using 2D.");
    EXPECT_EQ(spatial.data(Qt::AccessibleDescriptionRole).toString(),
              badge->accessibleDescription());
    auto* tooltip = badge->findChild<status_info::ToolTip*>();
    ASSERT_NE(tooltip, nullptr);
    tooltip->setAnimationEnabled(false);
    QHelpEvent help(QEvent::ToolTip, badge->rect().center(),
                    badge->mapToGlobal(badge->rect().center()));
    QApplication::sendEvent(badge, &help);
    EXPECT_TRUE(tooltip->isVisible());
    EXPECT_EQ(tooltip->text(), badge->accessibleDescription());
}
#endif

TEST_F(GallerySpatialFallbackTest, UnavailableAccelerationKeepsNativeWidgetsAndPreference)
{
    qputenv("FLUENT_QT_GALLERY_DISABLE_3D", "1");
    GalleryWindow window;
    finishStartup(window);
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget*>("gallerySplashScreen"), 6500);
    auto* navigation = window.findChild<navigation::NavigationView*>();
    auto* controller = window.findChild<GallerySpatialController*>();
    ASSERT_NE(navigation, nullptr);
    ASSERT_NE(controller, nullptr);
    EXPECT_FALSE(settings.spatialAvailable());
    EXPECT_TRUE(settings.spatialModeEnabled());
    EXPECT_FALSE(depth::enabled(&window));
    EXPECT_EQ(window.findChild<QWidget*>("gallerySpatialSurface"), nullptr);
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->contentHost()->graphicsEffect(), nullptr);
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_EQ(controller->projectedPosition(navigation, QPoint(20, 20)),
              navigation->mapTo(&window, QPoint(20, 20)));
    ASSERT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage(), 2000);
    auto* toggle = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    ASSERT_NE(toggle, nullptr);
    EXPECT_FALSE(toggle->isOn());
    EXPECT_FALSE(toggle->isEnabled());
    EXPECT_FALSE(toggle->accessibleDescription().isEmpty());
    auto* badge = window.findChild<status_info::InfoBadge*>("gallerySettingsSpatialSupportBadge");
    ASSERT_NE(badge, nullptr);
    EXPECT_EQ(badge->status(), status_info::InfoBadge::InfoBadgeStatus::Critical);
    auto* tooltip = badge->findChild<status_info::ToolTip*>();
    ASSERT_NE(tooltip, nullptr);
    tooltip->setAnimationEnabled(false);
    QSignalSpy preferenceChanged(&settings, &GallerySettings::spatialModeEnabledChanged);
    QTest::mouseClick(toggle, Qt::LeftButton);
    EXPECT_EQ(preferenceChanged.count(), 0);
    EXPECT_TRUE(settings.spatialModeEnabled());
    QCoreApplication::processEvents();
    if (!tests::support::isHeadlessPlatform()) {
        window.raise();
        window.activateWindow();
        QTest::mouseMove(&window, QPoint(window.width() / 2, 25));
        QTest::mouseMove(&window, badge->mapTo(&window, badge->rect().center()));
        QTRY_VERIFY_WITH_TIMEOUT(tooltip->isVisible(), 3000);
    } else {
        QHelpEvent help(QEvent::ToolTip, badge->rect().center(),
                        badge->mapToGlobal(badge->rect().center()));
        QApplication::sendEvent(badge, &help);
        EXPECT_TRUE(tooltip->isVisible());
    }
    EXPECT_LT(qAbs(tooltip->geometry().bottom() - badge->mapToGlobal(badge->rect().center()).y()),
              60);
    capture(window, QStringLiteral("fallback-light"));
    tooltip->hide();
    settings.setThemeMode(GallerySettings::ThemeMode::Dark);
    window.resize(760, 740);
    capture(window, QStringLiteral("fallback-dark-narrow"));
    ASSERT_TRUE(window.selectRoute("button"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentContentPage(), 2000);
    EXPECT_TRUE(window.currentContentPage()->isVisible());
#ifndef FLUENT_QT_HAS_SPATIAL
    EXPECT_FALSE(window.selectRoute("spatial-view"));
    EXPECT_TRUE(gallerySamplesForRoute("spatial-view").isEmpty());
#else
    // Losing whole-window acceleration does not disable independent Spatial samples.
    EXPECT_FALSE(gallerySamplesForRoute("spatial-view").isEmpty());
#endif
}

TEST_F(GallerySpatialFallbackTest, RendererFailureRestoresTheSamePageAndStopsMotion)
{
#ifndef FLUENT_QT_HAS_SPATIAL
    GTEST_SKIP() << "Spatial module is disabled in this build";
#else
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires a native OpenGL surface";
    qunsetenv("FLUENT_QT_GALLERY_DISABLE_3D");
    GalleryWindow window;
    finishStartup(window);
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget*>("gallerySplashScreen"), 6500);
    ASSERT_TRUE(settings.spatialAvailable());
    auto* controller = window.findChild<GallerySpatialController*>();
    auto* navigation = window.findChild<navigation::NavigationView*>();
    ASSERT_TRUE(controller);
    QTRY_VERIFY_WITH_TIMEOUT(depth::enabled(&window), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->transitionRunning(), 2000);
    auto* page = window.currentContentPage();
    auto* surface = window.findChild<QWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    auto* marker = new basicinput::ToggleSwitch(page);
    marker->setIsOn(true);
    marker->hide();
    settings.setSpatialModeEnabled(false);
    EXPECT_TRUE(controller->transitionRunning());
    ASSERT_TRUE(
        QMetaObject::invokeMethod(controller, "disableSpatial", Qt::QueuedConnection,
                                  Q_ARG(QString, QStringLiteral("Simulated context loss"))));
    QTRY_VERIFY_WITH_TIMEOUT(!settings.spatialAvailable(), 1000);
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_EQ(navigation->graphicsEffect(), nullptr);
    EXPECT_EQ(navigation->contentHost()->graphicsEffect(), nullptr);
    EXPECT_FALSE(depth::enabled(page));
    EXPECT_EQ(window.currentContentPage(), page);
    EXPECT_TRUE(marker->isOn());
    QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget*>("gallerySpatialSurface"), 1000);
    settings.setSpatialModeEnabled(true);
    EXPECT_FALSE(depth::enabled(page));
    EXPECT_FALSE(controller->transitionRunning());
    EXPECT_TRUE(window.selectRoute("settings"));
    QTRY_VERIFY_WITH_TIMEOUT(window.currentSettingsPage(), 2000);
    auto* toggle = window.findChild<basicinput::ToggleSwitch*>("gallerySettingsSpatialModeToggle");
    ASSERT_NE(toggle, nullptr);
    EXPECT_FALSE(toggle->isEnabled());
    EXPECT_FALSE(toggle->isOn());
    capture(window, QStringLiteral("context-failure-2d"));
#endif
}
