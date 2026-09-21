#include <gtest/gtest.h>
#include <FluentQt/FluentQt.h>
#include <QAccessible>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGraphicsOpacityEffect>
#include <QOpenGLWidget>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <limits>

using fluent::spatial::SpatialItem;
using fluent::spatial::SpatialView;
using fluent::WidgetOwnership;
namespace {
class SpatialViewTest : public ::testing::Test {
protected:
    fluent::MotionPolicy::Mode previous;
    fluent::FluentElement::Theme theme;
    void SetUp() override
    {
        previous = fluent::MotionPolicy::instance().mode();
        theme = fluent::FluentElement::currentTheme();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
    void TearDown() override
    {
        fluent::MotionPolicy::instance().setMode(previous);
        fluent::FluentElement::setTheme(theme);
    }
};
TEST_F(SpatialViewTest, Contract_InheritsHostThemeAndRestoresCallerOverridesOnRelease)
{
    using fluent::FluentElement;
    SpatialView view;
    view.setProperty("fluentThemeOverride", int(FluentElement::Dark));
    auto* card = new fluent::layout::Card;
    auto* button = new fluent::basicinput::Button("Run", card);
    auto* item = view.addWidget(card);
    EXPECT_EQ(card->effectiveTheme(), FluentElement::Dark);
    EXPECT_EQ(button->effectiveTheme(), FluentElement::Dark);
    view.setSpatialEnabled(false);
    EXPECT_EQ(button->effectiveTheme(), FluentElement::Dark);
    view.setSpatialEnabled(true);
    view.setProperty("fluentThemeOverride", int(FluentElement::Light));
    view.onThemeUpdated();
    EXPECT_EQ(button->effectiveTheme(), FluentElement::Light);
    EXPECT_EQ(view.takeWidget(item), card);
    EXPECT_FALSE(card->property("fluentThemeOverride").isValid());
    card->setProperty("fluentThemeOverride", int(FluentElement::Dark));
    item = view.addWidget(card);
    EXPECT_EQ(card->effectiveTheme(), FluentElement::Dark);
    card->setProperty("fluentThemeOverride", int(FluentElement::Light));
    view.setProperty("fluentThemeOverride", int(FluentElement::Dark));
    view.onThemeUpdated();
    EXPECT_EQ(card->effectiveTheme(), FluentElement::Light);
    view.releaseItem(item);
    EXPECT_EQ(card->property("fluentThemeOverride").toInt(), int(FluentElement::Light));
    delete card;
}
TEST_F(SpatialViewTest, Contract_FlatCanvasFollowsHostTheme)
{
    using fluent::FluentElement;
    SpatialView view;
    view.setRenderMode(SpatialView::RenderMode::Raster);
    view.setSpatialEnabled(false);
    auto* card = new fluent::layout::Card;
    card->setFixedSize(160, 100);
    view.addWidget(card);
    view.resize(600, 360);
    view.show();
    for (const auto theme : {FluentElement::Dark, FluentElement::Light}) {
        view.setProperty("fluentThemeOverride", int(theme));
        view.onThemeUpdated();
        QApplication::processEvents();
        const QPixmap canvas = view.grab();
        const int inset = qRound(12 * canvas.devicePixelRatioF());
        EXPECT_EQ(canvas.toImage().pixelColor(inset, inset), view.themeColors().bgCanvas);
        EXPECT_EQ(card->effectiveTheme(), theme);
    }
}

TEST_F(SpatialViewTest, Contract_DefaultsAndNoOpSignals)
{
    static_assert(std::is_base_of<QWidget, SpatialView>::value, "Native widget host");
    static_assert(std::is_base_of<fluent::FluentElement, SpatialView>::value, "Fluent theme");
    static_assert(std::is_base_of<fluent::QMLPlus, SpatialView>::value, "Fluent layout");
    SpatialView view;
    EXPECT_EQ(view.cameraDistance(), 1000);
    EXPECT_EQ(view.zoom(), 1);
    EXPECT_TRUE(view.isSpatialEnabled());
    QSignalSpy distance(&view, &SpatialView::cameraDistanceChanged);
    QSignalSpy mode(&view, &SpatialView::spatialEnabledChanged);
    QSignalSpy backend(&view, &SpatialView::renderModeChanged);
    view.setCameraDistance(1000);
    view.setCameraDistance(std::numeric_limits<qreal>::quiet_NaN());
    EXPECT_EQ(distance.count(), 0);
    view.setCameraDistance(0);
    view.setCameraDistance(100);
    EXPECT_EQ(distance.count(), 1);
    EXPECT_EQ(view.cameraDistance(), 100);
    view.setSpatialEnabled(false);
    view.setSpatialEnabled(false);
    EXPECT_EQ(mode.count(), 1);
    view.setRenderMode(static_cast<SpatialView::RenderMode>(88));
    view.setRenderMode(SpatialView::RenderMode::Auto);
    EXPECT_EQ(backend.count(), 0);
}
TEST_F(SpatialViewTest, Contract_PositionScaleAndCameraHaveIndependentGeometry)
{
    SpatialView view;
    view.resize(600, 500);
    auto* widget = new fluent::layout::Card;
    widget->setFixedSize(200, 100);
    auto* item = view.addWidget(widget, WidgetOwnership::Owned);
    ASSERT_NE(item, nullptr);
    view.show();
    QTest::qWait(40);
    const auto original = item->projectedPolygon().boundingRect();
    EXPECT_NEAR(original.width(), 200, 2);
    item->setPosition({25, -10, 500});
    auto bounds = item->projectedPolygon().boundingRect();
    EXPECT_NEAR(bounds.width(), 400, 2);
    EXPECT_NEAR(bounds.center().x() - original.center().x(), 50, 2);
    EXPECT_NEAR(bounds.center().y() - original.center().y(), -20, 2);
    view.setCameraDistance(2000);
    EXPECT_NEAR(item->projectedPolygon().boundingRect().width(), 200.0 * 4 / 3, 2);
    item->setScale(.5);
    EXPECT_NEAR(item->projectedPolygon().boundingRect().width(), 100.0 * 4 / 3, 2);
    view.hide();
}
TEST_F(SpatialViewTest, Contract_ItemNormalizationVisibilityAndCameraClipping)
{
    SpatialView view;
    auto* card = new fluent::layout::Card;
    card->resize(200, 120);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    QSignalSpy position(item, &SpatialItem::positionChanged);
    QSignalSpy scale(item, &SpatialItem::scaleChanged);
    QSignalSpy rotation(item, &SpatialItem::rotationChanged);
    item->setPosition({});
    item->setPosition({std::numeric_limits<float>::infinity(), 0, 0});
    EXPECT_EQ(position.count(), 0);
    item->setScale(0);
    item->setScale(.05);
    EXPECT_EQ(scale.count(), 1);
    item->setRotation({0, 360, 0});
    EXPECT_EQ(rotation.count(), 0);
    item->setPivot({-1, 2});
    EXPECT_EQ(item->pivot(), QPointF(0, 1));
    item->setPosition({0, 0, 2000});
    EXPECT_TRUE(item->projectedPolygon().isEmpty());
    item->setPosition({});
    EXPECT_FALSE(item->projectedPolygon().isEmpty());
    item->setVisible(false);
    EXPECT_TRUE(item->projectedPolygon().isEmpty());
    view.setSpatialEnabled(false);
    EXPECT_TRUE(card->isHidden());
    item->setVisible(true);
    EXPECT_FALSE(card->isHidden());
}
TEST_F(SpatialViewTest, Contract_SurfacePropertiesNormalizeAndPreserveContent)
{
    SpatialView view;
    auto* card = new fluent::layout::Card;
    card->setFixedSize(200, 120);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    QSignalSpy intensity(item, &SpatialItem::surfaceIntensityChanged);
    QSignalSpy lift(item, &SpatialItem::hoverLiftChanged);
    EXPECT_EQ(item->surfaceIntensity(), 0);
    EXPECT_EQ(item->hoverLift(), 0);
    item->setSurfaceIntensity(-1);
    item->setHoverLift(-1);
    item->setSurfaceIntensity(std::numeric_limits<qreal>::quiet_NaN());
    item->setHoverLift(std::numeric_limits<qreal>::infinity());
    EXPECT_EQ(intensity.count(), 0);
    EXPECT_EQ(lift.count(), 0);
    item->setSurfaceIntensity(2);
    item->setSurfaceIntensity(1);
    item->setHoverLift(32);
    item->setHoverLift(16);
    EXPECT_EQ(intensity.count(), 1);
    EXPECT_EQ(lift.count(), 1);
    EXPECT_EQ(item->surfaceIntensity(), 1);
    EXPECT_EQ(item->hoverLift(), 16);
    for (bool spatial : {false, true, false}) {
        view.setSpatialEnabled(spatial);
        EXPECT_EQ(item->widget(), card);
        EXPECT_EQ(card->size(), QSize(200, 120));
        EXPECT_EQ(card->graphicsEffect(), nullptr);
        EXPECT_EQ(item->position(), QVector3D());
        EXPECT_EQ(item->surfaceIntensity(), 1);
        EXPECT_EQ(item->hoverLift(), 16);
    }
}

TEST_F(SpatialViewTest, Contract_SurfaceShadowPreservesContentAndRespondsToTheme)
{
    SpatialView view;
    view.setRenderMode(SpatialView::RenderMode::Raster);
    view.setPointerTrackingEnabled(false);
    view.resize(500, 360);
    auto* card = new fluent::layout::Card;
    card->setFixedSize(200, 120);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    view.show();
    QTest::qWait(40);
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        view.setProperty("fluentThemeOverride", int(theme));
        view.onThemeUpdated();
        item->setSurfaceIntensity(0);
        QApplication::processEvents();
        const QPixmap plain = view.grab();
        item->setSurfaceIntensity(.75);
        QApplication::processEvents();
        const QPixmap raised = view.grab();
        const QRectF bounds = item->projectedPolygon().boundingRect();
        const auto pixel = [](const QPixmap& pixmap, const QPointF& point) {
            return pixmap.toImage().pixelColor(qRound(point.x() * pixmap.devicePixelRatioF()),
                                               qRound(point.y() * pixmap.devicePixelRatioF()));
        };
        EXPECT_EQ(pixel(plain, bounds.center()), pixel(raised, bounds.center()));
        const QPointF below(bounds.center().x(), bounds.bottom() + 7);
        EXPECT_LT(pixel(raised, below).lightness(), pixel(plain, below).lightness());
        const QPointF rim(bounds.center().x(), bounds.top() + 1);
        if (theme == fluent::FluentElement::Dark)
            EXPECT_GT(pixel(raised, rim).lightness(), pixel(plain, rim).lightness());
    }
}

TEST_F(SpatialViewTest, Contract_ProjectedShadowNeverOverpaintsCardContents)
{
    SpatialView view;
    view.setRenderMode(SpatialView::RenderMode::Raster);
    view.setPointerTrackingEnabled(false);
    view.resize(500, 360);
    auto* card = new fluent::layout::Card;
    card->setFixedSize(220, 150);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    item->setRotation(QVector3D(-6, -16, 0));
    item->setPosition(QVector3D(24, 44, 160));
    view.show();
    QTest::qWait(40);
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        view.setProperty("fluentThemeOverride", int(theme));
        view.onThemeUpdated();
        for (bool cache : {true, false}) {
            SCOPED_TRACE(cache ? "cached" : "uncached");
            view.setCacheEnabled(cache);
            item->setSurfaceIntensity(0);
            QApplication::processEvents();
            const QPixmap plain = view.grab();
            item->setSurfaceIntensity(.75);
            QApplication::processEvents();
            const QPixmap raised = view.grab();
            const QPointF center = item->projectedPolygon().boundingRect().center();
            const QPoint pixel(qRound(center.x() * plain.devicePixelRatioF()),
                               qRound(center.y() * plain.devicePixelRatioF()));
            EXPECT_EQ(plain.toImage().pixelColor(pixel), raised.toImage().pixelColor(pixel));
        }
    }
}

TEST_F(SpatialViewTest, Contract_SurfaceHoverFreezesDuringPointerInputAndSettles)
{
    SpatialView view;
    view.setRenderMode(SpatialView::RenderMode::Raster);
    view.setPointerTrackingEnabled(false);
    view.resize(500, 360);
    auto* card = new fluent::layout::Card;
    card->setFixedSize(220, 160);
    auto* button = new fluent::basicinput::Button("Advance", card);
    button->setGeometry(25, 50, 170, 36);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    item->setSurfaceIntensity(.75);
    item->setHoverLift(8);
    view.show();
    QTest::qWait(40);
    auto* canvas = view.findChild<QGraphicsView*>();
    const QPointF rest = item->projectedPolygon().boundingRect().center();
    const auto hit = [&] {
        return canvas->mapFromScene(
            card->graphicsProxyWidget()->mapToScene(button->mapTo(card, button->rect().center())));
    };
    QTest::mouseMove(canvas->viewport(), QPoint(5, 5));
    QTest::mouseMove(canvas->viewport(), hit());
    QTRY_VERIFY_WITH_TIMEOUT(item->projectedPolygon().boundingRect().center().y() < rest.y() - 7,
                             1000);
    QSignalSpy clicked(button, &fluent::basicinput::Button::clicked);
    const QPoint down = hit();
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, down);
    const QPolygonF held = item->projectedPolygon();
    QTest::qWait(220);
    EXPECT_EQ(item->projectedPolygon(), held);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, down);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(item->position(), QVector3D());
    QTest::mouseMove(canvas->viewport(), QPoint(5, 5));
    QTRY_VERIFY_WITH_TIMEOUT(
        std::abs(item->projectedPolygon().boundingRect().center().y() - rest.y()) < 1, 1000);
    view.setSpatialEnabled(false);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_EQ(clicked.count(), 2);
}

TEST_F(SpatialViewTest, Contract_OwnershipReleaseAndTakeInBothModes)
{
    for (bool spatial : {true, false}) {
        QWidget original;
        auto view = std::make_unique<SpatialView>();
        view->setSpatialEnabled(spatial);
        QPointer<QWidget> borrowed = new QWidget;
        QPointer<QWidget> owned = new QWidget;
        auto* reparented = new QWidget(&original);
        view->addWidget(borrowed);
        view->addWidget(owned, WidgetOwnership::Owned);
        view->addWidget(reparented, WidgetOwnership::Reparented);
        view.reset();
        EXPECT_FALSE(borrowed.isNull());
        EXPECT_EQ(borrowed->parentWidget(), nullptr);
        EXPECT_TRUE(owned.isNull());
        EXPECT_EQ(reparented->parentWidget(), &original);
        delete borrowed.data();
        SpatialView taking;
        QPointer<QWidget> transferred = new QWidget;
        auto* item = taking.addWidget(transferred, WidgetOwnership::Owned);
        EXPECT_EQ(taking.takeWidget(item), transferred.data());
        EXPECT_EQ(taking.itemCount(), 0);
        EXPECT_FALSE(transferred.isNull());
        delete transferred.data();

        auto* button = new fluent::basicinput::Button;
        int clicks = 0;
        QObject::connect(button, &fluent::basicinput::Button::clicked, &taking, [&] { ++clicks; });
        item = taking.addWidget(button, WidgetOwnership::Owned);
        taking.takeWidget(item);
        button->click();
        EXPECT_EQ(clicks, 1);
        delete button;
    }
}
TEST_F(SpatialViewTest, Contract_ContentDestructionDuplicateAndUnsupportedInputs)
{
    SpatialView view;
    auto* widget = new QWidget;
    auto* item = view.addWidget(widget);
    EXPECT_EQ(view.addWidget(widget), item);
    view.setSpatialEnabled(false);
    EXPECT_EQ(view.addWidget(widget), item);
    EXPECT_EQ(view.itemCount(), 1);
    delete widget;
    EXPECT_EQ(view.itemCount(), 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(view.addWidget(nullptr), nullptr);
    EXPECT_EQ(view.addWidget(&view), nullptr);
    QOpenGLWidget unsupported;
    EXPECT_EQ(view.addWidget(&unsupported), nullptr);
    EXPECT_EQ(unsupported.parentWidget(), nullptr);
}
TEST_F(SpatialViewTest, Contract_AccessibilityFlatModePreservesControlsAndValues)
{
    SpatialView view;
    auto* slider = new fluent::basicinput::Slider(Qt::Horizontal);
    slider->setAccessibleName(QStringLiteral("Application-owned progress"));
    slider->setRange(0, 100);
    slider->setValue(73);
    auto* item = view.addWidget(slider, WidgetOwnership::Owned);
    view.setSpatialEnabled(false);
    EXPECT_EQ(item->widget(), slider);
    EXPECT_EQ(slider->value(), 73);
    EXPECT_TRUE(view.isAncestorOf(slider));
    auto* accessible = QAccessible::queryAccessibleInterface(slider);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->role(), QAccessible::Slider);
    EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("Application-owned progress"));
    view.setSpatialEnabled(true);
    EXPECT_EQ(slider->value(), 73);
    EXPECT_EQ(item->widget(), slider);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    EXPECT_FALSE(view.isSpatialEnabled());
    view.setSpatialEnabled(true);
    EXPECT_FALSE(view.isSpatialEnabled());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_FALSE(view.isSpatialEnabled());
    view.setSpatialEnabled(true);
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    view.onThemeUpdated();
    EXPECT_FALSE(view.isSpatialEnabled());
}
TEST_F(SpatialViewTest, Contract_ProjectedPointerInputAndTabReturnToNative)
{
    SpatialView view;
    view.resize(640, 440);
    view.setPointerTrackingEnabled(false);
    auto* button = new fluent::basicinput::Button(QStringLiteral("Run"));
    button->setFixedSize(160, 48);
    auto* item = view.addWidget(button, WidgetOwnership::Owned);
    item->setRotation({12, -24, 8});
    item->setPosition({20, 10, 60});
    QSignalSpy clicks(button, &fluent::basicinput::Button::clicked);
    view.show();
    QTest::qWait(50);
    auto* canvas = view.findChild<QGraphicsView*>();
    ASSERT_NE(canvas, nullptr);
    const QPoint hit = canvas->viewport()->mapFrom(
        &view, item->projectedPolygon().boundingRect().center().toPoint());
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, hit);
    EXPECT_EQ(clicks.count(), 1);
    QTest::keyClick(canvas, Qt::Key_Tab);
    EXPECT_FALSE(view.isSpatialEnabled());
    EXPECT_TRUE(view.isAncestorOf(button));
    view.hide();
}
TEST_F(SpatialViewTest, Contract_KeyboardExitBeforePointerInputPreservesExplicitCursor)
{
    SpatialView view;
    view.resize(640, 440);
    auto* button = new fluent::basicinput::Button(QStringLiteral("Open"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(180, 48);
    view.addWidget(button, WidgetOwnership::Owned);
    view.show();
    QApplication::processEvents();
    auto* canvas = view.findChild<QGraphicsView*>();
    QTest::keyClick(canvas, Qt::Key_Escape);
    EXPECT_FALSE(view.isSpatialEnabled());
    EXPECT_TRUE(view.isAncestorOf(button));
    EXPECT_EQ(button->cursor().shape(), Qt::PointingHandCursor);
    QSignalSpy clicks(button, &fluent::basicinput::Button::clicked);
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_EQ(clicks.count(), 1);
}
TEST_F(SpatialViewTest, Contract_PresentationSwitchPreservesGeometryAndNativeContentIsNotClipped)
{
    SpatialView view;
    view.resize(640, 480);
    view.setPointerTrackingEnabled(false);
    auto* card = new fluent::layout::Card;
    card->setFixedSize(320, 280);
    auto* item = view.addWidget(card, WidgetOwnership::Owned);
    item->setPosition({20, 15, 70});
    item->setRotation({12, -22, 0});
    view.show();
    QTest::qWait(50);
    const auto projection = item->projectedPolygon().boundingRect();
    view.setSpatialEnabled(false);
    QTest::qWait(50);
    auto* flat = view.findChild<fluent::scrolling::ScrollView*>();
    ASSERT_NE(flat, nullptr);
    ASSERT_NE(flat->contentWidget(), nullptr);
    EXPECT_GE(flat->contentWidget()->width(), card->width() + 48);
    EXPECT_GE(flat->contentWidget()->height(), card->height() + 48);
    const QRect rect(card->mapTo(flat->viewport(), QPoint()), card->size());
    EXPECT_TRUE(flat->viewport()->rect().contains(rect));
    EXPECT_TRUE(card->isVisible());
    view.setSpatialEnabled(true);
    QTest::qWait(50);
    const auto restored = item->projectedPolygon().boundingRect();
    EXPECT_NEAR(restored.center().x(), projection.center().x(), 1);
    EXPECT_NEAR(restored.center().y(), projection.center().y(), 1);
    EXPECT_NEAR(restored.width(), projection.width(), 1);
    EXPECT_NEAR(restored.height(), projection.height(), 1);
    view.hide();
}
TEST_F(SpatialViewTest, Contract_OpenGLOrExplainedFallbackAndBackendSwitch)
{
    SpatialView view;
    auto* slider = new fluent::basicinput::Slider(Qt::Horizontal);
    slider->setValue(42);
    view.addWidget(slider, WidgetOwnership::Owned);
    view.setRenderMode(SpatialView::RenderMode::OpenGL);
    view.show();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return view.activeBackend() == SpatialView::Backend::OpenGL ||
               !view.fallbackReason().isEmpty();
    }));
    if (view.activeBackend() == SpatialView::Backend::OpenGL)
        EXPECT_FALSE(view.rendererName().isEmpty());
    view.setRenderMode(SpatialView::RenderMode::Raster);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return view.activeBackend() == SpatialView::Backend::Raster &&
               view.fallbackReason().isEmpty();
    }));
    EXPECT_EQ(slider->value(), 42);
    EXPECT_EQ(view.itemCount(), 1);
    auto* motion = view.findChild<QTimer*>(QStringLiteral("spatialMotionTimer"));
    ASSERT_NE(motion, nullptr);
    view.hide();
    EXPECT_FALSE(motion->isActive());
}

TEST_F(SpatialViewTest, Contract_HiddenInitializationDefersOpenGLUntilShown)
{
    SpatialView view;
    view.resize(480, 320);
    EXPECT_EQ(view.findChild<QOpenGLWidget*>(), nullptr);
    view.setRenderMode(SpatialView::RenderMode::OpenGL);
    view.setSpatialEnabled(false);
    view.setSpatialEnabled(true);
    QTest::qWait(20);
    EXPECT_EQ(view.findChild<QOpenGLWidget*>(), nullptr);
    EXPECT_EQ(view.activeBackend(), SpatialView::Backend::Raster);

    const auto backendReady = [&] {
        return view.activeBackend() == SpatialView::Backend::OpenGL ||
               !view.fallbackReason().isEmpty();
    };
    view.show();
    ASSERT_TRUE(QTest::qWaitFor(backendReady));
    RecordProperty("backend",
                   view.activeBackend() == SpatialView::Backend::OpenGL ? "OpenGL" : "Raster");
    RecordProperty("renderer", view.rendererName().toStdString());
    RecordProperty("fallback", view.fallbackReason().toStdString());
    view.hide();
    view.setRenderMode(SpatialView::RenderMode::Raster);
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.findChild<QOpenGLWidget*>() == nullptr; }));
    view.setRenderMode(SpatialView::RenderMode::OpenGL);
    QTest::qWait(20);
    EXPECT_EQ(view.findChild<QOpenGLWidget*>(), nullptr);
    view.show();
    ASSERT_TRUE(QTest::qWaitFor(backendReady));
}

TEST_F(SpatialViewTest, Contract_AncestorCaptureAndClippingAdaptBackendWithoutChangingRequest)
{
    QWidget host;
    host.resize(600, 420);
    auto* view = new SpatialView(&host);
    view->setGeometry(20, 20, 480, 320);
    auto* slider = new fluent::basicinput::Slider(Qt::Horizontal);
    slider->setValue(42);
    auto* item = view->addWidget(slider, WidgetOwnership::Owned);
    item->setRotation(QVector3D(4, -10, 0));
    auto* effect = new QGraphicsOpacityEffect(&host);
    effect->setOpacity(.9);
    host.setGraphicsEffect(effect);
    host.show();
    QTest::qWait(30);
    EXPECT_EQ(view->renderMode(), SpatialView::RenderMode::Auto);
    EXPECT_EQ(view->activeBackend(), SpatialView::Backend::Raster);
    EXPECT_EQ(view->findChild<QOpenGLWidget*>(), nullptr);
    EXPECT_TRUE(view->isSpatialEnabled());

    const auto backendReady = [&] {
        return view->activeBackend() == SpatialView::Backend::OpenGL ||
               !view->fallbackReason().isEmpty();
    };
    effect->setEnabled(false);
    ASSERT_TRUE(QTest::qWaitFor(backendReady));
    const bool hasOpenGL = view->activeBackend() == SpatialView::Backend::OpenGL;
    for (int pass = 0; pass < 2; ++pass) {
        // Clipping can change through ancestor movement without a hide event.
        view->move(20, host.height() + 20);
        ASSERT_TRUE(QTest::qWaitFor([&] { return view->findChild<QOpenGLWidget*>() == nullptr; }));
        EXPECT_EQ(view->renderMode(), SpatialView::RenderMode::Auto);
        view->move(20, 20);
        ASSERT_TRUE(QTest::qWaitFor(backendReady));
        if (hasOpenGL)
            EXPECT_EQ(view->activeBackend(), SpatialView::Backend::OpenGL);
        effect->setEnabled(true);
        ASSERT_TRUE(QTest::qWaitFor([&] { return view->findChild<QOpenGLWidget*>() == nullptr; }));
        EXPECT_EQ(view->activeBackend(), SpatialView::Backend::Raster);
        effect->setEnabled(false);
        ASSERT_TRUE(QTest::qWaitFor(backendReady));
    }
    EXPECT_EQ(slider->value(), 42);
    EXPECT_EQ(view->items().first(), item);
    EXPECT_EQ(item->rotation(), QVector3D(4, -10, 0));
    EXPECT_TRUE(view->isSpatialEnabled());
}
} // namespace
