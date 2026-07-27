#include <gtest/gtest.h>

#include <QImage>
#include <QSignalSpy>

#include "components/layout/Divider.h"
#include "components/windowing/WindowBackdrop.h"

using fluent::layout::Divider;

TEST(DividerTest, Contract_DefaultsAndOrientationGeometry)
{
    Divider divider;
    EXPECT_EQ(divider.orientation(), Qt::Horizontal);
    EXPECT_EQ(divider.leadingInset(), 0);
    EXPECT_EQ(divider.trailingInset(), 0);
    EXPECT_DOUBLE_EQ(divider.thickness(), 1.0);
    EXPECT_FALSE(divider.color().isValid());
    EXPECT_EQ(divider.sizeHint().height(), 1);

    divider.setOrientation(Qt::Vertical);
    EXPECT_EQ(divider.sizeHint().width(), 1);
}

TEST(DividerTest, Contract_NormalizesInputsAndSuppressesDuplicateSignals)
{
    Divider divider;
    QSignalSpy leadingSpy(&divider, &Divider::leadingInsetChanged);
    QSignalSpy trailingSpy(&divider, &Divider::trailingInsetChanged);
    QSignalSpy thicknessSpy(&divider, &Divider::thicknessChanged);

    divider.setLeadingInset(-10);
    divider.setTrailingInset(8);
    divider.setTrailingInset(8);
    divider.setThickness(-1.0);
    divider.setThickness(0.0);

    EXPECT_EQ(divider.leadingInset(), 0);
    EXPECT_EQ(leadingSpy.count(), 0);
    EXPECT_EQ(divider.trailingInset(), 8);
    EXPECT_EQ(trailingSpy.count(), 1);
    EXPECT_DOUBLE_EQ(divider.thickness(), 0.0);
    EXPECT_EQ(thicknessSpy.count(), 1);
}

TEST(DividerTest, Contract_CustomColorIsOptional)
{
    Divider divider;
    QSignalSpy colorSpy(&divider, &Divider::colorChanged);
    const QColor custom(12, 34, 56, 78);

    divider.setColor(custom);
    divider.setColor(custom);
    EXPECT_EQ(divider.color(), custom);
    EXPECT_EQ(colorSpy.count(), 1);

    divider.setColor(QColor());
    EXPECT_FALSE(divider.color().isValid());
    EXPECT_EQ(colorSpy.count(), 2);
}

TEST(DividerTest, Contract_CompositedBackdropReplacesInsteadOfAccumulating)
{
    QWidget host;
    host.setAttribute(Qt::WA_TranslucentBackground);
    fluent::windowing::BackdropState backdrop;
    backdrop.surfaceMode =
        fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    fluent::windowing::publishWindowBackdropState(&host, backdrop);

    Divider divider(&host);
    divider.resize(40, 3);
    divider.setColor(QColor(40, 100, 180, 64));

    QImage canvas(divider.size(), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    divider.render(&canvas);
    const int firstAlpha = canvas.pixelColor(20, 1).alpha();
    divider.render(&canvas);
    const int secondAlpha = canvas.pixelColor(20, 1).alpha();

    EXPECT_GT(firstAlpha, 0);
    EXPECT_EQ(secondAlpha, firstAlpha);
}

TEST(DividerTest, Contract_PublishedAncestorSurfaceIsNotCleared)
{
    QWidget host;
    host.setAttribute(Qt::WA_TranslucentBackground);
    fluent::windowing::BackdropState backdrop;
    backdrop.surfaceMode =
        fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    fluent::windowing::publishWindowBackdropState(&host, backdrop);

    QWidget surface(&host);
    const QColor surfaceColor(30, 40, 50);
    surface.setProperty("fluentSurfaceColor", surfaceColor);
    Divider divider(&surface);
    divider.resize(40, 3);
    divider.setColor(QColor(180, 190, 200, 64));

    QImage canvas(divider.size(), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(surfaceColor);
    divider.render(&canvas);

    EXPECT_EQ(canvas.pixelColor(20, 0).alpha(), 255);
    EXPECT_EQ(canvas.pixelColor(20, 2).alpha(), 255);
}
