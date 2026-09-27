#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <FluentQt/Spatial.h>
#include <QOpenGLWidget>
#include <QPainter>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>

#include "QtTestEnvironment.h"
#include "view/shell/GallerySpatialController.h"
#include "view/shell/GallerySpatialRenderPolicy.h"
#include "viewmodel/GallerySettings.h"

using namespace fluent;
using namespace fluent::gallery;

TEST(GallerySpatialBackdropTest, BudgetReservesNativeBackdropWithoutReducingPanelDensity)
{
    using namespace spatial_render;
    const std::array<QSizeF, 2> panels = {QSizeF(240, 900), QSizeF(1360, 900)};
    const auto previous = planCaches(panels, 2, 16384);
    const qint64 backdropBytes = 3200LL * 1800 * 4;
    const auto plan = planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, kPaintSamples,
                                 backdropBytes, kGlyphScratchBytes);
    ASSERT_TRUE(previous.valid());
    ASSERT_TRUE(plan.valid());
    EXPECT_EQ(plan.dpr, previous.dpr);
    EXPECT_EQ(plan.dpr, 4);
    EXPECT_EQ(plan.sizes, previous.sizes);
    EXPECT_LT(plan.paintSize.height(), previous.paintSize.height());
    EXPECT_EQ(plan.backdropBytes, backdropBytes);
    EXPECT_EQ(plan.glyphScratchBytes, kGlyphScratchBytes);
    const qint64 paintBytes =
        qint64(plan.paintSize.width()) * plan.paintSize.height() * (12 * kPaintSamples + 4);
    EXPECT_EQ(plan.estimatedBytes,
              backdropBytes + kGlyphScratchBytes + plan.pixels * 4 + paintBytes);
    EXPECT_LE(plan.estimatedBytes, kCacheBudgetBytes);
}

TEST(GallerySpatialBackdropTest, InvalidReservationDoesNotOverflowOrAllocate)
{
    using namespace spatial_render;
    const std::array<QSizeF, 2> panels = {QSizeF(240, 700), QSizeF(960, 700)};
    for (qint64 bytes : {-1LL, kCacheBudgetBytes, kCacheBudgetBytes + 1})
        EXPECT_FALSE(
            planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, kPaintSamples, bytes).valid());
    for (qint64 bytes : {-1LL, kCacheBudgetBytes - 1, kCacheBudgetBytes})
        EXPECT_FALSE(
            planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, kPaintSamples, 1, bytes).valid());
    const auto normal = planCaches(panels, 2, 16384);
    EXPECT_EQ(normal.backdropBytes, 0);
    EXPECT_EQ(normal.dpr, 4);
}

TEST(GallerySpatialBackdropTest, NativeBackdropIsReusedRefreshedAndReleased)
{
    if (tests::support::isHeadlessPlatform())
        GTEST_SKIP() << "Requires the native Gallery OpenGL surface";
    auto& settings = GallerySettings::instance();
    const auto oldTheme = settings.themeMode();
    const auto oldMotion = settings.motionMode();
    const bool oldSpatial = settings.spatialModeEnabled();
    const bool oldAvailable = settings.spatialAvailable();
    const bool oldPending = settings.spatialAvailabilityPending();
    const QString oldReason = settings.spatialUnavailableReason();
    const auto restore = qScopeGuard([&] {
        settings.setSpatialModeEnabled(oldSpatial);
        settings.setThemeMode(oldTheme);
        settings.setMotionMode(oldMotion);
        if (oldPending)
            settings.beginSpatialAvailabilityCheck();
        else
            settings.setSpatialAvailability(oldAvailable, oldReason);
    });
    settings.setSpatialModeEnabled(false);
    settings.setThemeMode(GallerySettings::ThemeMode::Light);
    settings.setMotionMode(GallerySettings::MotionMode::Full);
    class BackdropWindow final : public QWidget {
    public:
        QColor top = Qt::red;
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), Qt::blue);
            painter.fillRect(QRect(0, 0, width(), height() / 2), top);
        }
    } window;
    window.resize(1200, 800);
    auto* navigation = new navigation::NavigationView(&window);
    navigation->resize(window.size());
    GallerySpatialController controller(&window, navigation);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const auto* candidate = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            return candidate && candidate->property("presenting").toBool() &&
                   controller.renderingStatistics()["backdropUploads"].toLongLong() > 0;
        },
        5000));
    controller.cancelTransition();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    ASSERT_TRUE(surface->property("presenting").toBool());
    const auto frame = surface->grabFramebuffer();
    ASSERT_FALSE(frame.isNull());
    EXPECT_EQ(frame.pixelColor(0, 0), QColor(Qt::red));
    EXPECT_EQ(frame.pixelColor(0, frame.height() - 1), QColor(Qt::blue));
    const auto initial = controller.renderingStatistics();
    EXPECT_EQ(initial["backdropTextureBytes"].toLongLong(),
              qint64(frame.width()) * frame.height() * 4);
    EXPECT_LE(initial["cacheEstimatedBytes"].toLongLong(), spatial_render::kCacheBudgetBytes);
    const qint64 uploads = initial["backdropUploads"].toLongLong();
    QSignalSpy frames(surface, &QOpenGLWidget::frameSwapped);
    auto* pointer = controller.findChild<QVariantAnimation*>("galleryPointerAnimation");
    ASSERT_NE(pointer, nullptr);
    pointer->setStartValue(QPointF());
    pointer->setEndValue(QPointF(.5, -.3));
    pointer->start();
    ASSERT_TRUE(QTest::qWaitFor([&] { return pointer->state() == QAbstractAnimation::Stopped; }));
    EXPECT_GE(frames.count(), 2);
    EXPECT_EQ(controller.renderingStatistics()["backdropUploads"].toLongLong(), uploads);
    window.top = Qt::green;
    settings.setThemeMode(GallerySettings::ThemeMode::Dark);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return controller.renderingStatistics()["backdropUploads"].toLongLong() > uploads;
    }));
    EXPECT_EQ(surface->grabFramebuffer().pixelColor(0, 0), QColor(Qt::green));
    const qint64 themedUploads = controller.renderingStatistics()["backdropUploads"].toLongLong();
    window.resize(1000, 700);
    navigation->resize(window.size());
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return controller.renderingStatistics()["backdropUploads"].toLongLong() > themedUploads;
    }));
    const qint64 resizedUploads = controller.renderingStatistics()["backdropUploads"].toLongLong();
    settings.setSpatialModeEnabled(false);
    controller.cancelTransition();
    const auto cleared = controller.renderingStatistics();
    EXPECT_EQ(cleared["backdropTextureBytes"].toLongLong(), 0);
    EXPECT_EQ(cleared["cacheEstimatedBytes"].toLongLong(), 0);
    EXPECT_EQ(cleared["cachedPixels"].toLongLong(), 0);
    settings.setSpatialModeEnabled(true);
    controller.cancelTransition();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return controller.renderingStatistics()["backdropUploads"].toLongLong() > resizedUploads;
    }));
}

TEST(GallerySpatialBackdropTest, NativeWindowsMaterialHandoffPresentsOpaqueGpuBackground)
{
    if (QGuiApplication::platformName() != QStringLiteral("windows"))
        GTEST_SKIP() << "Requires the native Windows material compositor";
    auto& settings = GallerySettings::instance();
    const auto oldMotion = settings.motionMode();
    const bool oldSpatial = settings.spatialModeEnabled();
    const bool oldAvailable = settings.spatialAvailable();
    const bool oldPending = settings.spatialAvailabilityPending();
    const QString oldReason = settings.spatialUnavailableReason();
    const auto restore = qScopeGuard([&] {
        settings.setSpatialModeEnabled(oldSpatial);
        settings.setMotionMode(oldMotion);
        if (oldPending)
            settings.beginSpatialAvailabilityCheck();
        else
            settings.setSpatialAvailability(oldAvailable, oldReason);
    });
    settings.setSpatialModeEnabled(false);
    settings.setMotionMode(GallerySettings::MotionMode::Full);
    windowing::Window window;
    window.resize(900, 650);
    spatial::SpatialRuntime::prepareWindow(&window);
    auto* navigation = new navigation::NavigationView;
    window.setContentWidget(navigation);
    GallerySpatialController controller(&window, navigation);
    window.setBackdropEffect(windowing::BackdropEffect::Mica);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    if (window.backdropState().surfaceMode != windowing::BackdropSurfaceMode::CompositedTransparent)
        GTEST_SKIP() << "OS material unavailable; painted fallback is not DWM evidence";
    settings.setSpatialModeEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
            return surface && surface->property("presenting").toBool() && surface->isValid();
        },
        5000));
    controller.cancelTransition();
    auto* surface = window.findChild<QOpenGLWidget*>("gallerySpatialSurface");
    ASSERT_NE(surface, nullptr);
    for (const auto effect :
         {windowing::BackdropEffect::Mica, windowing::BackdropEffect::Acrylic}) {
        window.setBackdropEffect(effect);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return controller.renderingStatistics()["backdropTextureBytes"].toLongLong() == 0;
        }));
        int opaqueFrames = 0;
        const auto connection =
            QObject::connect(surface, &QOpenGLWidget::frameSwapped, &window, [&] {
                if (window.backdropEffect() == windowing::BackdropEffect::Solid &&
                    controller.renderingStatistics()["backdropTextureBytes"].toLongLong() > 0)
                    ++opaqueFrames;
            });
        // No GallerySettings change or event-loop wait is allowed to repair a stale frame.
        // The Window transaction must publish the actual state before native teardown.
        window.setBackdropEffect(windowing::BackdropEffect::Solid);
        EXPECT_GT(opaqueFrames, 0);
        EXPECT_GT(controller.renderingStatistics()["backdropTextureBytes"].toLongLong(), 0);
        QObject::disconnect(connection);
    }
}
