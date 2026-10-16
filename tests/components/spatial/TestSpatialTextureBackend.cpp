#include <gtest/gtest.h>
#include "compatibility/private/spatial/renderer/SpatialTextureBackend_p.h"
#include <QApplication>
#include <QWidget>

TEST(SpatialTextureBackendTest, Contract_UnavailableBackendKeepsTheCommonHostUsable)
{
    using compatibility::detail::SpatialTextureBackend;
    using compatibility::detail::SpatialTextureCallbacks;
    EXPECT_FALSE(SpatialTextureBackend::isSupported());
    EXPECT_FALSE(SpatialTextureBackend::isPreferred());
    EXPECT_FALSE(SpatialTextureBackend::preferredSurfaceType().has_value());
    int calls = 0;
    EXPECT_FALSE(SpatialTextureBackend::hasHardwareDevice([&](const QString&) {
        ++calls;
        return true;
    }));

    QWidget owner;
    SpatialTextureCallbacks callbacks;
    callbacks.synchronizeFrame = [&] { ++calls; };
    callbacks.paintLayer = [&](QPainter*, int, const QRegion&) { ++calls; };
    callbacks.initialized = [&] { ++calls; };
    callbacks.renderingFailed = [&] { ++calls; };
    callbacks.cacheUnavailable = [&] { ++calls; };
    callbacks.isHardwareRenderer = [&](const QString&) {
        ++calls;
        return true;
    };
    SpatialTextureBackend backend(&owner, callbacks);
    backend.resize(QRect(0, 0, 120, 80));
    backend.setLayerCount(1);
    backend.setLayer(0, QRectF(10, 10, 50, 40), QTransform(), 1, 0, Qt::transparent, Qt::transparent, 0, 0);
    backend.setSceneClip(QRectF(0, 0, 100, 60));
    backend.requestFrame();
    backend.clearFrameCaches();
    QApplication::processEvents();
    EXPECT_FALSE(backend.isReady());
    EXPECT_TRUE(backend.rendererName().isEmpty());
    EXPECT_TRUE(backend.grabFramebuffer().isNull());
    EXPECT_EQ(backend.statistics().value("paints").toInt(), 0);
    EXPECT_EQ(backend.statistics().value("uploads").toInt(), 0);
    EXPECT_TRUE(owner.findChildren<QWidget*>().isEmpty());
    EXPECT_EQ(calls, 0);
}
