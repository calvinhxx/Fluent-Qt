#include <gtest/gtest.h>

#include "components/foundation/overlay/OverlayGeometry.h"
#include "design/Elevation.h"

namespace {

TEST(FoundationContractsTest, Contract_NormalOverlayCardClampsInsideAvailableBounds)
{
    const QRect bounds(10, 20, 100, 80);
    const QSize cardSize(30, 20);

    const QPoint clamped = fluent::overlay::clampCardTopLeft(
        QPoint(-20, 500), cardSize, bounds, 8);

    EXPECT_EQ(clamped, QPoint(18, 72));
    EXPECT_GE(clamped.x(), bounds.left() + 8);
    EXPECT_GE(clamped.y(), bounds.top() + 8);
    EXPECT_LE(clamped.x() + cardSize.width() - 1, bounds.right() - 8);
    EXPECT_LE(clamped.y() + cardSize.height() - 1, bounds.bottom() - 8);
}

TEST(FoundationContractsTest, Contract_OversizedOverlayCardUsesStableAvailableOrigin)
{
    const QRect bounds(10, 20, 100, 80);
    const QSize oversizedCard(300, 200);

    const QPoint clamped = fluent::overlay::clampCardTopLeft(
        QPoint(70, 60), oversizedCard, bounds, 8);

    EXPECT_EQ(clamped, QPoint(18, 28));
}

TEST(FoundationContractsTest, Contract_ElevationNoneHasNoVisibleShadow)
{
    for (bool dark : {false, true}) {
        const Elevation::ShadowParams& shadow =
            Elevation::getShadow(Elevation::None, dark);

        EXPECT_EQ(shadow.offsetX, 0);
        EXPECT_EQ(shadow.offsetY, 0);
        EXPECT_EQ(shadow.blurRadius, 0);
        EXPECT_EQ(shadow.spreadRadius, 0);
        EXPECT_DOUBLE_EQ(shadow.opacity, 0.0);
    }
}

} // namespace
