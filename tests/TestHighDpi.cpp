#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"

namespace {

QIcon highResolutionTestIcon()
{
    QPixmap source(256, 256);
    source.fill(Qt::transparent);
    QPainter painter(&source);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 120, 212));
    painter.drawEllipse(QRectF(16, 16, 224, 224));
    return QIcon(source);
}

qreal requestedTestScale(bool* ok)
{
    return qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(ok);
}

} // namespace

TEST(HighDpiTest, BootstrapEnablesScalingAndPassThroughRounding)
{
    EXPECT_TRUE(fluentHighDpiScalingIsEnabled());
    EXPECT_EQ(QGuiApplication::highDpiScaleFactorRoundingPolicy(),
              Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

TEST(HighDpiTest, WindowUsesRequestedLogicalScale)
{
    bool hasRequestedScale = false;
    const qreal expectedDpr = requestedTestScale(&hasRequestedScale);

    QWidget widget;
    widget.resize(320, 180);
    widget.show();
    QApplication::processEvents();

    EXPECT_EQ(widget.size(), QSize(320, 180));
    if (hasRequestedScale)
        EXPECT_NEAR(widget.devicePixelRatioF(), expectedDpr, 0.01);
    else
        EXPECT_GE(widget.devicePixelRatioF(), 1.0);
}

TEST(HighDpiTest, IconPixmapUsesTargetPhysicalResolution)
{
    QWidget widget;
    widget.resize(80, 48);
    widget.show();
    QApplication::processEvents();

    const qreal dpr = qMax<qreal>(1.0, widget.devicePixelRatioF());
    const QSize logicalExtent(24, 24);
    const QPixmap pixmap = fluentIconPixmapForLogicalExtent(
        highResolutionTestIcon(), logicalExtent, dpr, widget.windowHandle());

    ASSERT_FALSE(pixmap.isNull());
    EXPECT_NEAR(pixmap.devicePixelRatioF(), dpr, 0.01);
    EXPECT_EQ(fluentPixmapLogicalSize(pixmap), logicalExtent);
    EXPECT_EQ(pixmap.size(),
              QSize(qRound(logicalExtent.width() * dpr),
                    qRound(logicalExtent.height() * dpr)));
}

TEST(HighDpiTest, WidgetGrabKeepsLogicalGeometryAndPhysicalDensity)
{
    QWidget widget;
    widget.resize(96, 52);
    widget.setAutoFillBackground(true);
    widget.show();
    QApplication::processEvents();

    const QPixmap capture = widget.grab();
    ASSERT_FALSE(capture.isNull());
    EXPECT_EQ(fluentPixmapLogicalSize(capture), widget.size());
    EXPECT_NEAR(capture.devicePixelRatioF(), widget.devicePixelRatioF(), 0.01);
    EXPECT_EQ(Typography::Styles::Body.toQFont().pixelSize(),
              Typography::FontSize::Body);
}
