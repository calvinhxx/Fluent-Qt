#include <gtest/gtest.h>

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

#include "compatibility/QtCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/collections/TreeView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/navigation/NavigationView.h"
#include "components/navigation/StackContentHost.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
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

TEST(HighDpiTest, CustomPaintStrokesSnapToFractionalDeviceGrid)
{
    constexpr std::array<qreal, 7> scales{{1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 3.0}};
    for (const qreal scale : scales) {
        QImage image(QSize(qCeil(120.0 * scale), qCeil(64.0 * scale)),
                     QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.scale(scale, scale);
        const fluent::painting::DpiPaintMetrics metrics(painter);
        const QRectF outer(3.2, 4.4, 80.0, 32.0);
        const QRectF alignedOuter = metrics.alignedOuterRect(outer);
        const auto stroke = metrics.alignedStroke(outer, 1.0);
        const QTransform device = painter.deviceTransform();
        const QRectF deviceOuter = device.mapRect(alignedOuter);
        const QRectF deviceStroke = device.mapRect(stroke.rect);
        const int expectedPhysicalWidth = qMax(1, qRound(scale));

        EXPECT_NEAR(deviceOuter.left(), qRound(deviceOuter.left()), 0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(deviceOuter.top(), qRound(deviceOuter.top()), 0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(deviceOuter.right(), qRound(deviceOuter.right()), 0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(deviceOuter.bottom(), qRound(deviceOuter.bottom()), 0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(stroke.width * scale, expectedPhysicalWidth, 0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(deviceStroke.left() - deviceOuter.left(),
                    expectedPhysicalWidth / 2.0,
                    0.0001)
            << "scale=" << scale;
        EXPECT_NEAR(deviceOuter.right() - deviceStroke.right(),
                    expectedPhysicalWidth / 2.0,
                    0.0001)
            << "scale=" << scale;
    }
}

TEST(HighDpiTest, RoundedSurfaceOwnsEveryStraightEdgePixel)
{
    constexpr std::array<qreal, 7> scales{{1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 3.0}};
    for (const qreal scale : scales) {
        QImage image(QSize(qCeil(100.0 * scale), qCeil(52.0 * scale)),
                     QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.scale(scale, scale);
        const QRectF outer(4.0, 4.0, 88.0, 40.0);
        fluent::painting::RoundedSurfacePaint surface;
        surface.fill = QColor(42, 42, 42);
        surface.border = QColor(90, 90, 90);
        surface.borderWidth = 1.0;
        surface.radius = 6.0;
        fluent::painting::paintRoundedSurface(painter, outer, surface);
        const QRectF deviceOuter = painter.deviceTransform().mapRect(
            fluent::painting::DpiPaintMetrics(painter).alignedOuterRect(outer));
        painter.end();

        const int centerX = qBound(0, qRound(deviceOuter.center().x()), image.width() - 1);
        const int centerY = qBound(0, qRound(deviceOuter.center().y()), image.height() - 1);
        const int left = qBound(0, qFloor(deviceOuter.left()), image.width() - 1);
        const int right = qBound(0, qCeil(deviceOuter.right()) - 1, image.width() - 1);
        const int top = qBound(0, qFloor(deviceOuter.top()), image.height() - 1);
        const int bottom = qBound(0, qCeil(deviceOuter.bottom()) - 1, image.height() - 1);

        EXPECT_EQ(QColor::fromRgba(image.pixel(centerX, top)).alpha(), 255)
            << "top scale=" << scale;
        EXPECT_EQ(QColor::fromRgba(image.pixel(centerX, bottom)).alpha(), 255)
            << "bottom scale=" << scale;
        EXPECT_EQ(QColor::fromRgba(image.pixel(left, centerY)).alpha(), 255)
            << "left scale=" << scale;
        EXPECT_EQ(QColor::fromRgba(image.pixel(right, centerY)).alpha(), 255)
            << "right scale=" << scale;
    }
}

TEST(HighDpiTest, FluentComponentCompositionKeepsAnOpaqueFrameInterior)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);

    fluent::windowing::Window window;
    window.setObjectName(QStringLiteral("highDpiFluentWindow"));
    window.setBackdropEffect(fluent::windowing::BackdropEffect::Solid);
    window.setCustomWindowChromeEnabled(true);
    window.resize(860, 600);

    auto* titleField = new fluent::textfields::LineEdit(window.titleBar());
    titleField->setPlaceholderText(QStringLiteral("Search"));
    titleField->setFixedWidth(280);
    window.titleBar()->setContentWidget(titleField);

    auto* navigation = new fluent::navigation::NavigationView;
    navigation->setAnimationEnabled(false);
    navigation->setDisplayMode(fluent::navigation::NavigationView::DisplayMode::Left);

    auto* tree = new fluent::collections::TreeView(navigation);
    auto* treeModel = new QStandardItemModel(tree);
    treeModel->appendRow(new QStandardItem(QStringLiteral("Home")));
    treeModel->appendRow(new QStandardItem(QStringLiteral("Settings")));
    tree->setModel(treeModel);
    navigation->setMainChromeWidget(tree);

    auto* footer = new fluent::basicinput::Button(QStringLiteral("Settings"), navigation);
    navigation->setFooterChromeWidget(footer);

    auto* page = new QWidget(navigation->contentHost());
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(32, 32, 32, 32);
    pageLayout->setSpacing(16);
    auto* title = new fluent::textfields::Label(QStringLiteral("High DPI surfaces"), page);
    auto* field = new fluent::textfields::LineEdit(page);
    field->setPlaceholderText(QStringLiteral("Text field"));
    auto* choice = new fluent::basicinput::ComboBox(page);
    choice->addItems({QStringLiteral("One"), QStringLiteral("Two")});
    auto* button = new fluent::basicinput::Button(QStringLiteral("Apply"), page);
    pageLayout->addWidget(title);
    pageLayout->addWidget(field);
    pageLayout->addWidget(choice);
    pageLayout->addWidget(button);
    pageLayout->addStretch(1);
    navigation->contentHost()->insertPage(0, page);
    navigation->contentHost()->setCurrentIndex(0, 0, false);
    window.setContentWidget(navigation);

    window.show();
    QApplication::processEvents();
    const QPixmap capture = window.grab();
    ASSERT_FALSE(capture.isNull());
    const QImage image = capture.toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal dpr = capture.devicePixelRatioF();
    // Linux deliberately reserves a transparent client-side shadow margin around
    // `chromeFrameRect()`. Inspect the opaque surface interior instead of treating
    // that top-level window shadow as a descendant alpha hole.
    // zh_CN: Linux 浼氬湪 `chromeFrameRect()` 澶栦繚鐣欓€忔槑鐨勫鎴风闃村奖杈硅窛锛涘洜姝ゅ彧妫€鏌?
    // 涓嶉€忔槑琛ㄩ潰鍐呴儴锛岄伩鍏嶆妸椤跺眰绐楀彛闃村奖璇垽涓哄瓙鎺т欢 alpha 鐮存礊銆?
    constexpr qreal CornerGuard = 12.0;
    const QRectF scanLogical = QRectF(window.chromeFrameRect()).adjusted(
        CornerGuard, CornerGuard, -CornerGuard, -CornerGuard);
    const int scanLeft = qBound(0, qCeil(scanLogical.left() * dpr), image.width());
    const int scanTop = qBound(0, qCeil(scanLogical.top() * dpr), image.height());
    const int scanRight = qBound(scanLeft,
                                 qFloor(scanLogical.right() * dpr),
                                 image.width());
    const int scanBottom = qBound(scanTop,
                                  qFloor(scanLogical.bottom() * dpr),
                                  image.height());

    int transparentPixels = 0;
    int longestBrightRun = 0;
    for (int y = scanTop; y < scanBottom; ++y) {
        int brightRun = 0;
        for (int x = scanLeft; x < scanRight; ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.alpha() != 255)
                ++transparentPixels;
            if (pixel.alpha() == 255 && pixel.red() >= 248
                && pixel.green() >= 248 && pixel.blue() >= 248) {
                ++brightRun;
                longestBrightRun = qMax(longestBrightRun, brightRun);
            } else {
                brightRun = 0;
            }
        }
    }

    EXPECT_EQ(transparentPixels, 0)
        << "UILib descendants must not punch alpha holes into an opaque window at DPR "
        << dpr;
    EXPECT_LT(longestBrightRun, qCeil(20.0 * dpr))
        << "A long near-white run indicates a leaked backing-store edge at DPR " << dpr;

    window.close();
    fluent::FluentElement::setTheme(previousTheme);
}
