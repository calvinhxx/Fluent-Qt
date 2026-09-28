#include <gtest/gtest.h>
#include "components/spatial/SpatialRuntime.h"
#include "components/windowing/Window.h"
#include <QApplication>
#include <QOpenGLContext>
#include <QWindow>

using fluent::spatial::SpatialRuntime;

TEST(SpatialRuntimeTest, Contract_RendererClassificationIsShared)
{
    for (const auto* software : {"", "llvmpipe", "softpipe", "SwiftShader", "Software",
                                 "Microsoft Basic Render Driver", "WARP", "GDI Generic"})
        EXPECT_FALSE(SpatialRuntime::isHardwareRenderer(QString::fromLatin1(software)));
    for (const auto* hardware : {"Apple M3", "AMD Radeon", "NVIDIA GeForce", "Intel Iris"})
        EXPECT_TRUE(SpatialRuntime::isHardwareRenderer(QString::fromLatin1(hardware)));
}

TEST(SpatialRuntimeTest, Contract_NoContextQueriesDoNotCreateOne)
{
    ASSERT_EQ(QOpenGLContext::currentContext(), nullptr);
    EXPECT_TRUE(SpatialRuntime::currentRendererName().isEmpty());
    EXPECT_EQ(SpatialRuntime::maximumTextureDimension(), 0);
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    EXPECT_FALSE(SpatialRuntime::prepareWindow(nullptr));
}

TEST(SpatialRuntimeTest, Contract_PreparationIsIdempotentAndNeverRecreatesVisibleWindow)
{
    fluent::windowing::Window window;
    const bool prepared = SpatialRuntime::prepareWindow(&window);
    const auto* handle = window.windowHandle();
    EXPECT_EQ(SpatialRuntime::prepareWindow(&window), prepared);
    EXPECT_EQ(window.windowHandle(), handle);
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    window.show();
    const WId id = window.winId();
    EXPECT_FALSE(SpatialRuntime::prepareWindow(&window));
    EXPECT_EQ(window.winId(), id);
}
