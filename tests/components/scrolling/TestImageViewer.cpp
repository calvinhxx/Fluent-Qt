#include <QAccessible>
#include <QApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

#include <cmath>
#include <type_traits>

#include "compatibility/EventConstructionCompat.h"
#include "QtTestEnvironment.h"
#include "components/basicinput/Button.h"
#include "components/collections/FlipView.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/QMLPlus.h"
#include "components/scrolling/ImageViewer.h"
#include "components/scrolling/private/ImageViewerPixmapCache_p.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using fluent::basicinput::Button;
using fluent::collections::FlipView;
using fluent::scrolling::ImageViewer;
using FitMode = fluent::scrolling::ImageViewer::FitMode;

namespace {

constexpr double kEpsilon = 1e-6;

QPixmap solidPixmap(const QSize& size, const QColor& color = QColor(200, 40, 40))
{
    QPixmap pixmap(size);
    pixmap.fill(color);
    return pixmap;
}

QPixmap scenePixmap(const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    QLinearGradient sky(0, 0, 0, size.height());
    sky.setColorAt(0.0, QColor(32, 64, 110));
    sky.setColorAt(0.7, QColor(242, 195, 139));
    painter.fillRect(image.rect(), sky);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(255, 226, 174));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(size.width() * 0.7, size.height() * 0.55), size.height() * 0.08, size.height() * 0.08);
    painter.setBrush(QColor(22, 39, 63));
    painter.drawRect(QRectF(0, size.height() * 0.69, size.width(), size.height() * 0.31));
    painter.end();
    return QPixmap::fromImage(image);
}

Button* toolbarButton(ImageViewer& viewer, const QString& accessibleName)
{
    for (Button* button : viewer.findChildren<Button*>()) {
        if (button->accessibleName() == accessibleName)
            return button;
    }
    return nullptr;
}

QWidget* zoomBar(ImageViewer& viewer)
{
    return viewer.findChild<QWidget*>(QStringLiteral("fluentImageViewerZoomBar"));
}

QWidget* zoomIndicator(ImageViewer& viewer)
{
    return viewer.findChild<QWidget*>(QStringLiteral("fluentImageViewerZoomIndicator"));
}

void sendDiscreteWheel(QWidget& target,
    const QPoint& position,
    int angleY,
    Qt::KeyboardModifiers modifiers,
    bool* accepted = nullptr)
{
    FLUENT_MAKE_WHEEL_EVENT(event, position.x(), position.y(), angleY, modifiers);
    QApplication::sendEvent(&target, &event);
    if (accepted)
        *accepted = event.isAccepted();
}

bool sendPixelWheel(QWidget& target, const QPoint& position, const QPoint& pixelDelta, Qt::ScrollPhase phase)
{
    FLUENT_MAKE_WHEEL_EVENT_WITH_PHASE(event, position, target.mapToGlobal(position), pixelDelta, pixelDelta,
        Qt::NoButton, Qt::NoModifier, phase, false);
    QApplication::sendEvent(&target, &event);
    return event.isAccepted();
}

bool sendMouse(QWidget& target, QEvent::Type type, const QPoint& position, Qt::MouseButtons buttons)
{
    FLUENT_MAKE_MOUSE_EVENT(event, type, &target, position, Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
    return event.isAccepted();
}

class ImageViewerContractTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_previousMotion = fluent::MotionPolicy::instance().mode();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override { fluent::MotionPolicy::instance().setMode(m_previousMotion); }

    // A 400 x 300 viewer showing a 1600 x 1000 image fits at 25%.
    // zh_CN: 400 x 300 的查看器显示 1600 x 1000 图片时，适应比例为 25%。
    static void prepareLargeImage(ImageViewer& viewer)
    {
        viewer.resize(400, 300);
        viewer.setPixmap(scenePixmap(QSize(1600, 1000)));
    }

private:
    fluent::MotionPolicy::Mode m_previousMotion = fluent::MotionPolicy::Mode::Full;
};

} // namespace

TEST_F(ImageViewerContractTest, Contract_DefaultsAndInheritance)
{
    static_assert(std::is_base_of<QWidget, ImageViewer>::value, "ImageViewer must remain a QWidget");
    static_assert(std::is_base_of<fluent::FluentElement, ImageViewer>::value, "ImageViewer must be themed");
    static_assert(std::is_base_of<fluent::QMLPlus, ImageViewer>::value, "ImageViewer must support QMLPlus");

    ImageViewer viewer;
    EXPECT_TRUE(viewer.pixmap().isNull());
    EXPECT_TRUE(viewer.imageSize().isEmpty());
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_DOUBLE_EQ(viewer.minZoomFactor(), 0.1);
    EXPECT_DOUBLE_EQ(viewer.maxZoomFactor(), 10.0);
    EXPECT_EQ(viewer.viewCenter(), QPointF());
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);
    EXPECT_EQ(viewer.wheelAction(), ImageViewer::WheelAction::Zoom);
    EXPECT_EQ(viewer.controlsVisibility(), ImageViewer::ControlsVisibility::Auto);
    EXPECT_TRUE(viewer.isZoomIndicatorEnabled());
    EXPECT_TRUE(viewer.isFrameVisible());
    EXPECT_EQ(viewer.transformationMode(), Qt::SmoothTransformation);
    EXPECT_EQ(viewer.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(zoomBar(viewer)->isVisibleTo(&viewer)) << "no controls without an image";
}

TEST_F(ImageViewerContractTest, Contract_SetPixmapResetsToScaleDown)
{
    ImageViewer viewer;
    viewer.resize(400, 300);
    QSignalSpy pixmapSpy(&viewer, &ImageViewer::pixmapChanged);
    QSignalSpy zoomSpy(&viewer, &ImageViewer::zoomFactorChanged);
    QSignalSpy centerSpy(&viewer, &ImageViewer::viewCenterChanged);

    const QPixmap large = scenePixmap(QSize(1600, 1000));
    viewer.setPixmap(large);
    EXPECT_EQ(pixmapSpy.count(), 1);
    EXPECT_EQ(zoomSpy.count(), 1);
    EXPECT_EQ(centerSpy.count(), 1);
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);
    EXPECT_NEAR(viewer.fitZoomFactor(), 0.25, kEpsilon);
    EXPECT_EQ(viewer.viewCenter(), QPointF(800, 500));
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);

    viewer.setPixmap(large);
    EXPECT_EQ(pixmapSpy.count(), 1) << "the same pixmap is a no-op";

    viewer.zoomTo(2.0);
    ASSERT_EQ(viewer.fitMode(), FitMode::Manual);
    viewer.setPixmap(solidPixmap(QSize(40, 20)));
    EXPECT_EQ(pixmapSpy.count(), 2);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0) << "small images are not enlarged";
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);
    EXPECT_EQ(viewer.viewCenter(), QPointF(20, 10));

    viewer.setImage(QImage());
    EXPECT_TRUE(viewer.pixmap().isNull());
    EXPECT_TRUE(viewer.imageSize().isEmpty());
    EXPECT_EQ(viewer.viewCenter(), QPointF());
}

TEST_F(ImageViewerContractTest, Contract_SettersAreNoOpsForTheSameValue)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);

    QSignalSpy zoomSpy(&viewer, &ImageViewer::zoomFactorChanged);
    QSignalSpy minSpy(&viewer, &ImageViewer::minZoomFactorChanged);
    QSignalSpy maxSpy(&viewer, &ImageViewer::maxZoomFactorChanged);
    QSignalSpy centerSpy(&viewer, &ImageViewer::viewCenterChanged);
    QSignalSpy modeSpy(&viewer, &ImageViewer::fitModeChanged);
    QSignalSpy wheelSpy(&viewer, &ImageViewer::wheelActionChanged);
    QSignalSpy controlsSpy(&viewer, &ImageViewer::controlsVisibilityChanged);
    QSignalSpy indicatorSpy(&viewer, &ImageViewer::zoomIndicatorEnabledChanged);
    QSignalSpy frameSpy(&viewer, &ImageViewer::frameVisibleChanged);
    QSignalSpy transformSpy(&viewer, &ImageViewer::transformationModeChanged);

    viewer.setZoomFactor(0.25);
    // Without pan range the center clamps back to the image center.
    // zh_CN: 没有平移范围时，中心点会被夹回图片中心。
    viewer.setViewCenter(QPointF(10, 10));
    viewer.setFitMode(FitMode::ScaleDown);
    EXPECT_EQ(zoomSpy.count(), 0);
    EXPECT_EQ(centerSpy.count(), 0);
    EXPECT_EQ(modeSpy.count(), 0);
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown) << "a clamped no-op keeps the fit mode";

    viewer.setZoomFactor(1.0);
    viewer.setZoomFactor(1.0);
    EXPECT_EQ(zoomSpy.count(), 1);
    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(viewer.fitMode(), FitMode::Manual);

    viewer.setViewCenter(QPointF(600, 400));
    viewer.setViewCenter(QPointF(600, 400));
    EXPECT_EQ(centerSpy.count(), 1);
    EXPECT_EQ(viewer.viewCenter(), QPointF(600, 400));

    viewer.setMinZoomFactor(0.2);
    viewer.setMinZoomFactor(0.2);
    viewer.setMaxZoomFactor(8.0);
    viewer.setMaxZoomFactor(8.0);
    viewer.setWheelAction(ImageViewer::WheelAction::Pan);
    viewer.setWheelAction(ImageViewer::WheelAction::Pan);
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Visible);
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Visible);
    viewer.setZoomIndicatorEnabled(false);
    viewer.setZoomIndicatorEnabled(false);
    viewer.setFrameVisible(false);
    viewer.setFrameVisible(false);
    viewer.setTransformationMode(Qt::FastTransformation);
    viewer.setTransformationMode(Qt::FastTransformation);
    viewer.setFitMode(FitMode::Manual);
    EXPECT_EQ(minSpy.count(), 1);
    EXPECT_EQ(maxSpy.count(), 1);
    EXPECT_EQ(wheelSpy.count(), 1);
    EXPECT_EQ(controlsSpy.count(), 1);
    EXPECT_EQ(indicatorSpy.count(), 1);
    EXPECT_EQ(frameSpy.count(), 1);
    EXPECT_EQ(transformSpy.count(), 1);
    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(wheelSpy.takeFirst().at(0).value<ImageViewer::WheelAction>(), ImageViewer::WheelAction::Pan);
}

TEST_F(ImageViewerContractTest, Contract_ZoomClampsAndFitStaysReachable)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);

    viewer.setMinZoomFactor(0.5);
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon) << "the fit factor stays reachable below minZoomFactor";
    viewer.setZoomFactor(0.01);
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);
    viewer.setZoomFactor(100.0);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 10.0);
    viewer.setZoomFactor(std::nan(""));
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 10.0);

    viewer.setMinZoomFactor(12.0);
    EXPECT_DOUBLE_EQ(viewer.maxZoomFactor(), 12.0) << "raising the minimum lifts the maximum";
    viewer.setMaxZoomFactor(0.5);
    EXPECT_DOUBLE_EQ(viewer.minZoomFactor(), 0.5) << "lowering the maximum lowers the minimum";
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 0.5);
    viewer.setMinZoomFactor(-1.0);
    EXPECT_DOUBLE_EQ(viewer.minZoomFactor(), 0.01);

    ImageViewer small;
    small.resize(400, 300);
    small.setPixmap(solidPixmap(QSize(10, 10)));
    EXPECT_DOUBLE_EQ(small.zoomFactor(), 1.0);
    small.fitToView();
    EXPECT_NEAR(small.zoomFactor(), 30.0, kEpsilon) << "Contain enlarges beyond maxZoomFactor to fit";
    EXPECT_EQ(small.fitMode(), FitMode::Contain);
}

TEST_F(ImageViewerContractTest, Contract_ZoomAroundAnchorKeepsTheImagePointFixed)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    const QPointF anchor(100, 80);
    const QPointF before = viewer.mapToImage(anchor);
    EXPECT_EQ(before, QPointF(400, 220));

    viewer.zoomTo(1.0, anchor);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_EQ(viewer.fitMode(), FitMode::Manual);
    const QPointF after = viewer.mapToImage(anchor);
    EXPECT_NEAR(after.x(), before.x(), kEpsilon);
    EXPECT_NEAR(after.y(), before.y(), kEpsilon);
    EXPECT_EQ(viewer.viewCenter(), QPointF(500, 290));

    const QPointF imagePoint(321.5, 654.25);
    const QPointF mapped = viewer.mapToImage(viewer.mapFromImage(imagePoint));
    EXPECT_NEAR(mapped.x(), imagePoint.x(), kEpsilon);
    EXPECT_NEAR(mapped.y(), imagePoint.y(), kEpsilon);

    viewer.zoomToActualSize();
    EXPECT_EQ(viewer.viewCenter(), QPointF(500, 290)) << "100% keeps the viewport-center anchor";
}

TEST_F(ImageViewerContractTest, Contract_ViewCenterClampsAndFittedAxesStayCentered)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    viewer.setZoomFactor(1.0);

    viewer.setViewCenter(QPointF(0, 0));
    EXPECT_EQ(viewer.viewCenter(), QPointF(200, 150)) << "edges never enter the viewport";
    viewer.setViewCenter(QPointF(5000, 5000));
    EXPECT_EQ(viewer.viewCenter(), QPointF(1400, 850));

    viewer.setZoomFactor(0.3);
    EXPECT_DOUBLE_EQ(viewer.viewCenter().y(), 500.0) << "a fitted axis stays centered";
    viewer.panBy(0, 100);
    EXPECT_DOUBLE_EQ(viewer.viewCenter().y(), 500.0);
    const qreal x = viewer.viewCenter().x();
    viewer.panBy(-30, 0);
    EXPECT_NEAR(viewer.viewCenter().x(), x - 100.0, kEpsilon) << "pan distances are viewport pixels";
}

TEST_F(ImageViewerContractTest, Contract_FitModesFollowResizeAndManualKeepsTheView)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    viewer.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&viewer));

    viewer.fitToView();
    viewer.resize(800, 300);
    QApplication::processEvents();
    EXPECT_NEAR(viewer.zoomFactor(), 0.3, kEpsilon);
    EXPECT_EQ(viewer.fitMode(), FitMode::Contain);

    viewer.zoomTo(1.0);
    viewer.setViewCenter(QPointF(700, 400));
    viewer.resize(600, 400);
    QApplication::processEvents();
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_EQ(viewer.viewCenter(), QPointF(700, 400));
    EXPECT_EQ(viewer.fitMode(), FitMode::Manual);

    viewer.resetView();
    EXPECT_NEAR(viewer.zoomFactor(), 0.375, kEpsilon);
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);
}

TEST_F(ImageViewerContractTest, Contract_ZoomCommandsUsePresetSteps)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);

    viewer.zoomIn();
    EXPECT_NEAR(viewer.zoomFactor(), 1.0 / 3.0, kEpsilon);
    viewer.zoomIn(true);
    EXPECT_NEAR(viewer.zoomFactor(), 0.5, kEpsilon) << "animated steps settle when motion is disabled";
    viewer.zoomOut();
    viewer.zoomOut();
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);
    viewer.zoomOut();
    EXPECT_NEAR(viewer.zoomFactor(), 0.1, kEpsilon);

    viewer.setZoomFactor(10.0);
    QSignalSpy zoomSpy(&viewer, &ImageViewer::zoomFactorChanged);
    viewer.zoomIn();
    EXPECT_EQ(zoomSpy.count(), 0) << "zooming in at the maximum is a no-op";

    viewer.setMaxZoomFactor(2.5);
    viewer.setZoomFactor(2.0);
    viewer.zoomIn();
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 2.5) << "the last step stops at the maximum";
}

TEST_F(ImageViewerContractTest, Contract_WheelZoomsAroundPointerAndPanModeHandsOff)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    const QPoint pointer(100, 80);
    const QPointF before = viewer.mapToImage(pointer);

    bool accepted = false;
    sendDiscreteWheel(viewer, pointer, 120, Qt::NoModifier, &accepted);
    EXPECT_TRUE(accepted);
    EXPECT_NEAR(viewer.zoomFactor(), 0.3, kEpsilon);
    EXPECT_NEAR(viewer.mapToImage(pointer).x(), before.x(), kEpsilon) << "the pointer stays on the same image point";

    viewer.resetView();
    viewer.setWheelAction(ImageViewer::WheelAction::Pan);
    sendDiscreteWheel(viewer, pointer, -120, Qt::NoModifier, &accepted);
    EXPECT_FALSE(accepted) << "without pan range the wheel belongs to the parent";
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);

    sendDiscreteWheel(viewer, pointer, 120, Qt::ControlModifier, &accepted);
    EXPECT_TRUE(accepted) << "Ctrl/Command + wheel always zooms";
    EXPECT_NEAR(viewer.zoomFactor(), 0.3, kEpsilon);

    viewer.zoomTo(1.0);
    const QPointF center = viewer.viewCenter();
    sendDiscreteWheel(viewer, pointer, -120, Qt::NoModifier, &accepted);
    EXPECT_TRUE(accepted);
    EXPECT_NEAR(viewer.viewCenter().y(), center.y() + 48.0, kEpsilon) << "Pan wheels scroll down by one step";

    viewer.setZoomFactor(0.25);
    viewer.setZoomFactor(10.0);
    sendDiscreteWheel(viewer, pointer, 120, Qt::ControlModifier, &accepted);
    EXPECT_TRUE(accepted) << "zoom input is consumed at the maximum";
}

TEST_F(ImageViewerContractTest, Contract_TrackpadScrollPansOnlyAlongAxesWithRange)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    const QPoint pointer(200, 150);

    EXPECT_FALSE(sendPixelWheel(viewer, pointer, QPoint(-40, 0), Qt::ScrollBegin));
    EXPECT_FALSE(sendPixelWheel(viewer, pointer, QPoint(-40, 0), Qt::ScrollUpdate))
        << "a fitted image passes horizontal swipes to the parent";
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon) << "trackpad scrolling never zooms";

    viewer.setZoomFactor(1.0);
    const QPointF center = viewer.viewCenter();
    EXPECT_TRUE(sendPixelWheel(viewer, pointer, QPoint(0, -30), Qt::ScrollUpdate));
    EXPECT_NEAR(viewer.viewCenter().y(), center.y() + 30.0, kEpsilon);
    EXPECT_EQ(viewer.fitMode(), FitMode::Manual);
}

TEST_F(ImageViewerContractTest, Contract_NativePinchZoomsAroundTheGesture)
{
    if (!fluentCanConstructNativeGestureEvent())
        GTEST_SKIP() << fluentNativeGestureEventSkipReason();

    ImageViewer viewer;
    prepareLargeImage(viewer);
    const QPointF before = viewer.mapToImage(QPointF(100, 80));
    FLUENT_MAKE_NATIVE_GESTURE_EVENT(begin, &viewer, Qt::BeginNativeGesture, 100, 80, 0.0);
    QApplication::sendEvent(&viewer, &begin);
    FLUENT_MAKE_NATIVE_GESTURE_EVENT(zoom, &viewer, Qt::ZoomNativeGesture, 100, 80, 0.5);
    QApplication::sendEvent(&viewer, &zoom);
    FLUENT_MAKE_NATIVE_GESTURE_EVENT(end, &viewer, Qt::EndNativeGesture, 100, 80, 0.0);
    QApplication::sendEvent(&viewer, &end);

    EXPECT_NEAR(viewer.zoomFactor(), 0.375, kEpsilon);
    EXPECT_NEAR(viewer.mapToImage(QPointF(100, 80)).x(), before.x(), kEpsilon);

    bool accepted = false;
    sendDiscreteWheel(viewer, QPoint(100, 80), 120, Qt::NoModifier, &accepted);
    EXPECT_TRUE(accepted);
    EXPECT_NEAR(viewer.zoomFactor(), 0.375, kEpsilon) << "residual wheel events right after a pinch are ignored";
}

TEST_F(ImageViewerContractTest, Contract_DragPansOnlyWithPanRange)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);

    EXPECT_FALSE(sendMouse(viewer, QEvent::MouseButtonPress, QPoint(200, 150), Qt::LeftButton))
        << "a fitted image leaves the press to the parent";
    EXPECT_FALSE(viewer.testAttribute(Qt::WA_SetCursor));

    viewer.setZoomFactor(1.0);
    EXPECT_EQ(viewer.cursor().shape(), Qt::OpenHandCursor);
    const QPointF center = viewer.viewCenter();
    EXPECT_TRUE(sendMouse(viewer, QEvent::MouseButtonPress, QPoint(200, 150), Qt::LeftButton));
    EXPECT_EQ(viewer.cursor().shape(), Qt::ClosedHandCursor);
    EXPECT_TRUE(sendMouse(viewer, QEvent::MouseMove, QPoint(150, 120), Qt::LeftButton));
    EXPECT_TRUE(sendMouse(viewer, QEvent::MouseButtonRelease, QPoint(150, 120), Qt::NoButton));
    EXPECT_EQ(viewer.viewCenter(), center + QPointF(50, 30)) << "content follows the pointer";
    EXPECT_EQ(viewer.cursor().shape(), Qt::OpenHandCursor);
}

TEST_F(ImageViewerContractTest, Contract_KeyboardZoomsResetsAndPans)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);

    QTest::keyClick(&viewer, Qt::Key_Plus);
    EXPECT_NEAR(viewer.zoomFactor(), 1.0 / 3.0, kEpsilon);
    QTest::keyClick(&viewer, Qt::Key_Equal, Qt::ControlModifier);
    EXPECT_NEAR(viewer.zoomFactor(), 0.5, kEpsilon);
    QTest::keyClick(&viewer, Qt::Key_Minus);
    EXPECT_NEAR(viewer.zoomFactor(), 1.0 / 3.0, kEpsilon);
    QTest::keyClick(&viewer, Qt::Key_1);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);

    const QPointF center = viewer.viewCenter();
    QTest::keyClick(&viewer, Qt::Key_Right);
    QTest::keyClick(&viewer, Qt::Key_Down);
    EXPECT_EQ(viewer.viewCenter(), center + QPointF(48, 48));

    QTest::keyClick(&viewer, Qt::Key_0);
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);

    QKeyEvent left(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    QApplication::sendEvent(&viewer, &left);
    EXPECT_FALSE(left.isAccepted()) << "arrows without pan range belong to the parent";
}

TEST_F(ImageViewerContractTest, Contract_DoubleClickTogglesActualSizeAndReset)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    const QPoint pointer(100, 80);
    const QPointF before = viewer.mapToImage(pointer);

    EXPECT_TRUE(sendMouse(viewer, QEvent::MouseButtonDblClick, pointer, Qt::LeftButton));
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_NEAR(viewer.mapToImage(pointer).x(), before.x(), kEpsilon);

    EXPECT_TRUE(sendMouse(viewer, QEvent::MouseButtonDblClick, pointer, Qt::LeftButton));
    EXPECT_NEAR(viewer.zoomFactor(), 0.25, kEpsilon);
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);

    viewer.setPixmap(solidPixmap(QSize(40, 20)));
    sendMouse(viewer, QEvent::MouseButtonDblClick, QPoint(200, 150), Qt::LeftButton);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 2.0) << "a small image at 100% doubles";
}

TEST_F(ImageViewerContractTest, Contract_FlipViewReceivesInputWithoutPanRange)
{
    FlipView album;
    album.resize(400, 300);
    auto* first = new ImageViewer;
    auto* second = new ImageViewer;
    first->setFrameVisible(false);
    album.addPage(first);
    album.addPage(second);
    album.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&album));
    first->setPixmap(scenePixmap(QSize(1600, 1000)));
    second->setPixmap(scenePixmap(QSize(1600, 1000)));
    ASSERT_EQ(album.currentIndex(), 0);

    first->zoomTo(1.0);
    const QPointF center = first->viewCenter();
    QTest::keyClick(first, Qt::Key_Right);
    EXPECT_EQ(album.currentIndex(), 0) << "a zoomed page pans instead of flipping";
    EXPECT_EQ(first->viewCenter(), center + QPointF(48, 0));

    first->resetView();
    QTest::keyClick(first, Qt::Key_Right);
    EXPECT_EQ(album.currentIndex(), 1) << "a fitted page lets FlipView navigate";
}

TEST_F(ImageViewerContractTest, Contract_ControlsFollowVisibilityAndViewState)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    QWidget* bar = zoomBar(viewer);
    ASSERT_NE(bar, nullptr);
    EXPECT_FALSE(bar->isVisibleTo(&viewer)) << "Auto controls wait for hover or focus";

    FLUENT_MAKE_ENTER_EVENT(enter, 20, 20);
    QApplication::sendEvent(&viewer, &enter);
    EXPECT_TRUE(bar->isVisibleTo(&viewer));
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&viewer, &leave);
    EXPECT_FALSE(bar->isVisibleTo(&viewer));

    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Visible);
    EXPECT_TRUE(bar->isVisibleTo(&viewer));
    Button* fit = toolbarButton(viewer, QStringLiteral("Fit to view"));
    Button* actual = toolbarButton(viewer, QStringLiteral("Actual size"));
    Button* reset = toolbarButton(viewer, QStringLiteral("Reset view"));
    Button* zoomIn = toolbarButton(viewer, QStringLiteral("Zoom in"));
    ASSERT_TRUE(fit && actual && reset && zoomIn);
    EXPECT_EQ(fit->focusPolicy(), Qt::NoFocus);
    EXPECT_FALSE(fit->isEnabled()) << "the initial view of a large image already fits";
    EXPECT_FALSE(reset->isEnabled());
    EXPECT_TRUE(actual->isEnabled());

    auto* percent = viewer.findChild<fluent::textfields::Label*>(QStringLiteral("fluentImageViewerZoomPercent"));
    ASSERT_NE(percent, nullptr);
    EXPECT_EQ(percent->text(), QStringLiteral("25%"));
    zoomIn->click();
    EXPECT_NEAR(viewer.zoomFactor(), 1.0 / 3.0, kEpsilon);
    EXPECT_EQ(percent->text(), QStringLiteral("33%"));
    EXPECT_TRUE(reset->isEnabled());
    actual->click();
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_FALSE(actual->isEnabled());
    reset->click();
    EXPECT_EQ(viewer.fitMode(), FitMode::ScaleDown);

    viewer.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&viewer));
    viewer.resize(200, 300);
    QApplication::processEvents();
    EXPECT_FALSE(fit->isVisibleTo(&viewer)) << "narrow viewers keep only the zoom group";
    EXPECT_TRUE(zoomIn->isVisibleTo(&viewer));

    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Hidden);
    EXPECT_FALSE(bar->isVisibleTo(&viewer));
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Visible);
    viewer.setPixmap(QPixmap());
    EXPECT_FALSE(bar->isVisibleTo(&viewer)) << "no controls without an image";
}

TEST_F(ImageViewerContractTest, Contract_ZoomIndicatorFollowsDirectInputOnly)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Hidden);
    QWidget* indicator = zoomIndicator(viewer);
    ASSERT_NE(indicator, nullptr);

    viewer.zoomTo(0.5);
    EXPECT_FALSE(indicator->isVisibleTo(&viewer)) << "programmatic zoom stays silent";
    sendDiscreteWheel(viewer, QPoint(200, 150), 120, Qt::NoModifier);
    EXPECT_TRUE(indicator->isVisibleTo(&viewer));

    viewer.setZoomIndicatorEnabled(false);
    EXPECT_FALSE(indicator->isVisibleTo(&viewer));
    sendDiscreteWheel(viewer, QPoint(200, 150), 120, Qt::NoModifier);
    EXPECT_FALSE(indicator->isVisibleTo(&viewer));

    viewer.setZoomIndicatorEnabled(true);
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Visible);
    sendDiscreteWheel(viewer, QPoint(200, 150), 120, Qt::NoModifier);
    EXPECT_FALSE(indicator->isVisibleTo(&viewer)) << "visible controls already show the percentage";
}

TEST_F(ImageViewerContractTest, Contract_DevicePixelRatioDefinesActualSize)
{
    ImageViewer viewer;
    viewer.resize(400, 300);
    QPixmap retina = solidPixmap(QSize(800, 600));
    retina.setDevicePixelRatio(2.0);
    viewer.setPixmap(retina);
    EXPECT_EQ(viewer.imageSize(), QSizeF(400, 300));
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);
    EXPECT_DOUBLE_EQ(viewer.fitZoomFactor(), 1.0);
}

TEST_F(ImageViewerContractTest, Contract_PaintsImageFrameAndSamplingModes)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    viewer.setPixmap(solidPixmap(QSize(1600, 1000), QColor(200, 40, 40)));
    QImage frame = viewer.grab().toImage();
    const qreal dpr = frame.devicePixelRatio();
    EXPECT_EQ(frame.pixelColor(QPoint(200, 150) * dpr), QColor(200, 40, 40));
    EXPECT_EQ(frame.pixelColor(QPoint(200, 10) * dpr), viewer.themeColors().bgSolid)
        << "letterbox areas show the frame background";

    QImage split(2, 1, QImage::Format_RGB32);
    split.setPixel(0, 0, qRgb(0, 0, 0));
    split.setPixel(1, 0, qRgb(255, 255, 255));
    viewer.setFrameVisible(false);
    viewer.setImage(split);
    viewer.fitToView();
    ASSERT_NEAR(viewer.zoomFactor(), 200.0, kEpsilon);

    viewer.setTransformationMode(Qt::FastTransformation);
    frame = viewer.grab().toImage();
    EXPECT_EQ(frame.pixelColor(QPoint(195, 150) * dpr), QColor(0, 0, 0));
    EXPECT_EQ(frame.pixelColor(QPoint(205, 150) * dpr), QColor(255, 255, 255));

    viewer.setTransformationMode(Qt::SmoothTransformation);
    frame = viewer.grab().toImage();
    const int gray = frame.pixelColor(QPoint(200, 150) * dpr).red();
    EXPECT_GT(gray, 30);
    EXPECT_LT(gray, 225);
}

TEST(ImageViewerCacheTest, Contract_ReductionsReusePixelsAndInvalidateWithSource)
{
    fluent::scrolling::detail::ImageViewerPixmapCache cache;
    QPixmap source = solidPixmap(QSize(1024, 768), Qt::green);
    source.setDevicePixelRatio(2.0);
    const QPixmap half = cache.sourceForScale(source, 0.4);
    const QPixmap quarter = cache.sourceForScale(source, 0.2);
    ASSERT_EQ(half.size(), QSize(512, 384));
    ASSERT_EQ(quarter.size(), QSize(256, 192));
    EXPECT_DOUBLE_EQ(half.devicePixelRatio(), 1.0);
    for (int iteration = 0; iteration < 10; ++iteration) {
        EXPECT_EQ(cache.sourceForScale(source, 0.4).cacheKey(), half.cacheKey());
        EXPECT_EQ(cache.sourceForScale(source, 0.2).cacheKey(), quarter.cacheKey());
    }
    EXPECT_EQ(cache.retainedBytes(), (512 * 384 + 256 * 192) * 4);

    source.fill(Qt::blue);
    EXPECT_EQ(cache.sourceForScale(source, 1.0).cacheKey(), source.cacheKey());
    EXPECT_EQ(cache.retainedBytes(), 0) << "Replacing the source must immediately release old reductions";
    const QPixmap replacement = cache.sourceForScale(source, 0.2);
    EXPECT_NE(replacement.cacheKey(), quarter.cacheKey());
    EXPECT_EQ(replacement.toImage().pixelColor(30, 30), QColor(Qt::blue));
    cache.clear();
    EXPECT_EQ(cache.retainedBytes(), 0);
    EXPECT_NE(cache.sourceForScale(source, 0.2).cacheKey(), replacement.cacheKey());
}

TEST(ImageViewerCacheTest, Contract_ReductionRetentionHasAByteBudget)
{
    constexpr qint64 smallerBudget = 64 * 1024;
    fluent::scrolling::detail::ImageViewerPixmapCache cache(smallerBudget);
    const QPixmap source = solidPixmap(QSize(1024, 1024));
    const QPixmap half = cache.sourceForScale(source, 0.4);
    const QPixmap quarter = cache.sourceForScale(source, 0.2);
    EXPECT_EQ(cache.retainedBytes(), 256 * 256 * 4) << "Two oversized levels must not accumulate";

    const QPixmap eighth = cache.sourceForScale(source, 0.1);
    EXPECT_EQ(cache.retainedBytes(), 256 * 256 * 4 + smallerBudget);
    EXPECT_EQ(cache.sourceForScale(source, 0.2).cacheKey(), quarter.cacheKey());
    EXPECT_EQ(cache.sourceForScale(source, 0.1).cacheKey(), eighth.cacheKey());
    EXPECT_NE(cache.sourceForScale(source, 0.4).cacheKey(), half.cacheKey());
    EXPECT_LE(cache.retainedBytes(), 512 * 512 * 4 + smallerBudget);

    const QPixmap smallest = cache.sourceForScale(source, 0.001);
    EXPECT_EQ(smallest.size(), QSize(4, 4));
    EXPECT_LE(cache.retainedBytes(), 512 * 512 * 4 + smallerBudget);
}

TEST_F(ImageViewerContractTest, Contract_DeepReductionKeepsTheBottomOfOddSizedImages)
{
    ImageViewer viewer;
    viewer.resize(300, 300);
    viewer.setControlsVisibility(ImageViewer::ControlsVisibility::Hidden);
    QImage image(101, 2049, QImage::Format_RGB32);
    image.fill(Qt::red);
    {
        QPainter painter(&image);
        painter.fillRect(0, image.height() - 64, image.width(), 64, Qt::blue);
    }
    viewer.setImage(image);
    viewer.setZoomFactor(0.1);
    const QImage frame = viewer.grab().toImage();
    const QPoint sample = (viewer.mapFromImage(QPointF(50.5, 2029)) * frame.devicePixelRatio()).toPoint();
    const QColor color = frame.pixelColor(sample);
    EXPECT_GT(color.blue(), 240);
    EXPECT_LT(color.red(), 15) << "Rounded cache dimensions must not crop the bottom edge";
}

#if QT_CONFIG(accessibility)

namespace {

QList<QAccessible::Event> g_viewerEvents;
QObject* g_viewerTarget = nullptr;

void captureViewerEvent(QAccessibleEvent* event)
{
    if (event->object() == g_viewerTarget)
        g_viewerEvents.append(event->type());
}

} // namespace

TEST_F(ImageViewerContractTest, Contract_AccessibilityExposesZoomValueAndActions)
{
    ImageViewer viewer;
    prepareLargeImage(viewer);
    viewer.setAccessibleName(QStringLiteral("Harbor at dawn"));

    QAccessibleInterface* root = QAccessible::queryAccessibleInterface(&viewer);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Graphic);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Harbor at dawn"));
    EXPECT_EQ(root->text(QAccessible::Value), QStringLiteral("25%"));

    QAccessibleValueInterface* value = root->valueInterface();
    ASSERT_NE(value, nullptr);
    EXPECT_NEAR(value->currentValue().toDouble(), 25.0, kEpsilon);
    EXPECT_NEAR(value->minimumValue().toDouble(), 10.0, kEpsilon);
    EXPECT_NEAR(value->maximumValue().toDouble(), 1000.0, kEpsilon);
    value->setCurrentValue(100.0);
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.0);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->actionNames().contains(QAccessibleActionInterface::increaseAction()));
    EXPECT_TRUE(actions->actionNames().contains(QAccessibleActionInterface::decreaseAction()));
    actions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_DOUBLE_EQ(viewer.zoomFactor(), 1.5);
    viewer.setZoomFactor(10.0);
    EXPECT_FALSE(actions->actionNames().contains(QAccessibleActionInterface::increaseAction()));

    ASSERT_EQ(root->childCount(), 1);
    QAccessibleInterface* toolbar = root->child(0);
    ASSERT_NE(toolbar, nullptr);
    EXPECT_EQ(toolbar->role(), QAccessible::ToolBar);
    EXPECT_EQ(toolbar->text(QAccessible::Name), QStringLiteral("Zoom controls"));
    EXPECT_EQ(root->indexOfChild(toolbar), 0);

    viewer.setPixmap(QPixmap());
    EXPECT_FALSE(value->currentValue().isValid());
    EXPECT_TRUE(actions->actionNames().isEmpty()
        || !actions->actionNames().contains(QAccessibleActionInterface::increaseAction()));
}

TEST_F(ImageViewerContractTest, Contract_AccessibilityAnnouncesOnlyRealZoomChanges)
{
    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ImageViewer viewer;
    prepareLargeImage(viewer);
    QAccessible::queryAccessibleInterface(&viewer);

    g_viewerTarget = &viewer;
    QAccessible::UpdateHandler previous = QAccessible::installUpdateHandler(captureViewerEvent);
    g_viewerEvents.clear();
    viewer.setZoomFactor(0.25);
    viewer.setViewCenter(QPointF(1, 1));
    EXPECT_EQ(g_viewerEvents.count(QAccessible::ValueChanged), 0);
    viewer.setZoomFactor(1.0);
    EXPECT_EQ(g_viewerEvents.count(QAccessible::ValueChanged), 1);
    viewer.setZoomFactor(10.0);
    EXPECT_EQ(g_viewerEvents.count(QAccessible::ValueChanged), 2);
    EXPECT_GE(g_viewerEvents.count(QAccessible::ActionChanged), 1) << "zoom in is no longer available";
    QAccessible::installUpdateHandler(previous);
    g_viewerTarget = nullptr;
    g_viewerEvents.clear();
}

#endif // QT_CONFIG(accessibility)

TEST_F(ImageViewerContractTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);

    QWidget window;
    window.setWindowTitle(QStringLiteral("ImageViewer VisualCheck"));
    window.resize(820, 600);
    auto* layout = new fluent::AnchorLayout(&window);
    auto* title = new fluent::textfields::Label(QStringLiteral("ImageViewer"), &window);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    title->anchors()->top = {&window, fluent::AnchorLayout::Edge::Top, 24};
    title->anchors()->left = {&window, fluent::AnchorLayout::Edge::Left, 24};
    layout->addWidget(title);

    auto* theme = new Button(QStringLiteral("Toggle Light/Dark"), &window);
    theme->anchors()->top = {&window, fluent::AnchorLayout::Edge::Top, 20};
    theme->anchors()->right = {&window, fluent::AnchorLayout::Edge::Right, -24};
    layout->addWidget(theme);
    QObject::connect(theme, &Button::clicked, &window, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                ? fluent::FluentElement::Dark
                : fluent::FluentElement::Light);
    });

    auto* viewer = new ImageViewer(&window);
    viewer->setAccessibleName(QStringLiteral("Generated harbor scene"));
    viewer->setPixmap(scenePixmap(QSize(3200, 2000)));
    viewer->anchors()->top = {title, fluent::AnchorLayout::Edge::Bottom, 16};
    viewer->anchors()->left = {&window, fluent::AnchorLayout::Edge::Left, 24};
    viewer->anchors()->right = {&window, fluent::AnchorLayout::Edge::Right, -24};
    viewer->anchors()->bottom = {&window, fluent::AnchorLayout::Edge::Bottom, -24};
    layout->addWidget(viewer);

    window.show();
    qApp->exec();
}
