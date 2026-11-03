#include <gtest/gtest.h>
#include <QApplication>
#include "components/textfields/LineEdit.h"
#include "components/textfields/Label.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMenu>
#include "components/basicinput/Button.h"
#include <QShortcut>
#include <QSignalSpy>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QDir>
#include <QPainter>
#include <QScopeGuard>
#include <QTest>
#include <QVBoxLayout>
#include <limits>
#include <type_traits>

#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/collections/DrawerView.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayCoordinator.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/MotionPolicy.h"

namespace {
using fluent::collections::DrawerView;
using fluent::basicinput::Button;
using fluent::textfields::LineEdit;
using fluent::dialogs_flyouts::Dialog;
using fluent::dialogs_flyouts::Popup;
using fluent::overlay::OverlayScrim;

class BackgroundProbe : public QWidget {
public:
    using QWidget::QWidget;
    int presses = 0;
    int releases = 0;

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        ++presses;
        e->accept();
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        ++releases;
        e->accept();
    }
};

class EscapeProbe : public QWidget {
public:
    using QWidget::QWidget;
    bool consumeNext = true;
    int presses = 0;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            ++presses;
            if (consumeNext) {
                consumeNext = false;
                event->accept();
                return;
            }
        }
        QWidget::keyPressEvent(event);
    }
};

class OverlayHost : public QWidget {
public:
    bool publishSurfaceOnResize = false;

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (publishSurfaceOnResize)
            setProperty(fluent::overlay::overlaySurfaceRectPropertyName(), rect());
    }
};

template <class Surface> class OverlayContractTest : public ::testing::Test {
protected:
    fluent::FluentElement::Theme previousTheme = fluent::FluentElement::currentTheme();
    OverlayHost host;
    BackgroundProbe background{&host};
    Surface surface{&host};
    LineEdit outside{&host};
    LineEdit* first = nullptr;
    Button* last = nullptr;
    void SetUp() override
    {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        host.resize(800, 600);
        host.setStyleSheet(QStringLiteral("background: rgb(160, 30, 30)"));
        background.setGeometry(0, 0, 120, 90);
        outside.setGeometry(150, 25, 200, 30);
        surface.setAnimationEnabled(false);
        QWidget* body = &surface;
        if constexpr (std::is_same_v<Surface, DrawerView>) {
            surface.setEdge(DrawerView::DrawerEdge::Right);
            surface.setDrawerLength(280);
            surface.setInteractive(false);
            body = new QWidget;
            surface.setContentWidget(body, fluent::WidgetOwnership::Owned);
        } else {
            surface.setFixedSize(320, 220);
        }
        auto* layout = new QVBoxLayout(body);
        first = new LineEdit(body);
        last = new Button(QStringLiteral("Inside"), body);
        layout->addWidget(first);
        layout->addWidget(last);
        host.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
        host.activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(&host));
    }
    void TearDown() override { fluent::FluentElement::setTheme(previousTheme); }
    void close()
    {
        surface.setIsOpen(false);
        QApplication::processEvents();
    }
};
using Surfaces = ::testing::Types<Popup, Dialog, DrawerView>;
TYPED_TEST_SUITE(OverlayContractTest, Surfaces);

TYPED_TEST(OverlayContractTest, HostSurfaceChangesUpdateOpenScrim)
{
    using namespace fluent::overlay;
    auto& host = this->host;
    auto& surface = this->surface;
    host.publishSurfaceOnResize = true;
    host.setProperty(overlaySurfaceRectPropertyName(), host.rect());
    surface.setModal(true);
    surface.setDim(true);
    surface.setBackdropBlurRadius(0.0);
    surface.open();
    auto* scrim = host.template findChild<OverlayScrim*>();
    auto* coordinator = surface.template findChild<OverlayCoordinator*>();
    ASSERT_NE(scrim, nullptr);
    ASSERT_NE(coordinator, nullptr);
    QSignalSpy geometryChanged(coordinator, &OverlayCoordinator::hostGeometryChanged);
    for (const QSize size : {QSize(640, 480), QSize(1000, 720), QSize(640, 480), QSize(1000, 720)}) {
        SCOPED_TRACE(::testing::Message() << "size=" << size.width() << 'x' << size.height());
        host.resize(size);
        // Native window managers can acknowledge an earlier resize asynchronously.
        // Like Window, the host publishes from each resizeEvent after event filters.
        QSize previousSize;
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                const QSize currentSize = host.size();
                const bool settled = currentSize == size && currentSize == previousSize;
                previousSize = currentSize;
                return settled;
            },
            2000));
        EXPECT_TRUE(surface.isOpen());
        EXPECT_EQ(scrim->geometry(), host.rect());
        if constexpr (std::is_same_v<TypeParam, Dialog>)
            EXPECT_LE((surface.geometry().center() - host.rect().center()).manhattanLength(), 2);
    }

    // A frame/inset change can update the usable surface without resizing the window.
    const QRect insetSurface = host.rect().adjusted(12, 20, -12, -20);
    host.setProperty(overlaySurfaceRectPropertyName(), insetSurface);
    EXPECT_EQ(scrim->geometry(), insetSurface);
    geometryChanged.clear();
    host.setProperty("unrelatedOverlayTestProperty", 1);
    EXPECT_EQ(geometryChanged.count(), 0);
    host.setProperty(overlaySurfaceRectPropertyName(), QVariant());
    EXPECT_EQ(scrim->geometry(), host.rect());
    this->close();
}

TYPED_TEST(OverlayContractTest, CachedSurfaceCapturesContentRaisedAfterConstruction)
{
    auto& surface = this->surface;
    surface.hide();
    surface.setModal(true);
    surface.setDim(false);
    QWidget content(&this->host);
    content.setGeometry(this->host.rect());
    content.setStyleSheet(QStringLiteral("background: rgb(30, 150, 40)"));
    content.show();
    for (int opening = 0; opening < 2; ++opening) {
        SCOPED_TRACE(opening);
        // A cached overlay can sit below a subsequently raised content/compositor layer.
        content.raise();
        surface.open();
        OverlayScrim* scrim = nullptr;
        for (auto* candidate : this->host.template findChildren<OverlayScrim*>()) {
            if (candidate->isVisible())
                scrim = candidate;
        }
        ASSERT_NE(scrim, nullptr);
        const auto image = scrim->grab().toImage();
        const auto color = image.pixelColor(QPoint(20, 140) * image.devicePixelRatio());
        EXPECT_GT(color.green(), color.red());
        this->close();
    }
}

TYPED_TEST(OverlayContractTest, AnimatedBackdropFollowsSurfaceThroughOpenAndClose)
{
    auto& motion = fluent::MotionPolicy::instance();
    const auto oldMode = motion.mode();
    const auto restore = qScopeGuard([&] { motion.setMode(oldMode); });
    motion.setMode(fluent::MotionPolicy::Mode::Full);
    auto& surface = this->surface;
    surface.hide();
    surface.setModal(true);
    surface.setDim(true);
    surface.setAnimationEnabled(true);
    surface.open();
    auto* scrim = this->host.template findChild<OverlayScrim*>();
    ASSERT_NE(scrim, nullptr);
    EXPECT_DOUBLE_EQ(scrim->progress(), 0.0);
    const QByteArray property = std::is_same_v<TypeParam, Popup> ? "popupProgress"
        : std::is_same_v<TypeParam, Dialog>
        ? "animationProgress"
        : "position";
    QPropertyAnimation* timeline = nullptr;
    for (auto* candidate : surface.template findChildren<QPropertyAnimation*>()) {
        if (candidate->targetObject() == &surface && candidate->propertyName() == property)
            timeline = candidate;
    }
    ASSERT_NE(timeline, nullptr);
    timeline->pause();
    timeline->setCurrentTime(timeline->duration() / 3);
    EXPECT_GT(scrim->progress(), 0.0);
    EXPECT_LT(scrim->progress(), 1.0);
    EXPECT_DOUBLE_EQ(scrim->progress(), surface.property(property.constData()).toDouble());
    surface.setDimOpacity(0.2);
    EXPECT_DOUBLE_EQ(scrim->progress(), surface.property(property.constData()).toDouble());

    surface.setIsOpen(false);
    timeline->pause();
    timeline->setCurrentTime(timeline->duration() / 2);
    ASSERT_TRUE(scrim->isVisible());
    EXPECT_GT(scrim->progress(), 0.0);
    EXPECT_DOUBLE_EQ(scrim->progress(), surface.property(property.constData()).toDouble());
    // Local motion opt-out must settle both layers and remove the input barrier.
    timeline->resume();
    surface.setAnimationEnabled(false);
    EXPECT_FALSE(surface.isVisible());
    for (auto* remaining : this->host.template findChildren<OverlayScrim*>())
        EXPECT_FALSE(remaining->isVisible());
}

TEST(OverlayScrimTest, InitialShowKeepsPreparedBackdropUntilExplicitRefresh)
{
    class PaintedHost : public QWidget {
    public:
        QColor color{180, 30, 30};

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), color);
        }
    } host;
    host.resize(300, 200);
    host.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
    QWidget surface(&host);
    OverlayScrim scrim(&host);
    scrim.resize(host.size());
    scrim.setModalAndDim(false, false);
    surface.raise();
    scrim.setBackdrop(&surface, 4);
    host.color = QColor(30, 150, 40);
    host.update();
    scrim.show();
    QApplication::processEvents();
    const auto sample = [&] { return scrim.grab().toImage().pixelColor(20, 20); };
    EXPECT_GT(sample().red(), sample().green());
    scrim.refreshBackdrop();
    EXPECT_GT(sample().green(), sample().red());
}

TEST(OverlayScrimTest, EffectSnapshotsPreservePositionOpacityAndWidgetState)
{
    class ColorBlock : public QWidget {
    public:
        ColorBlock(const QColor& color, QWidget* parent = nullptr) : QWidget(parent), color(color) {}
        QColor color;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), color);
        }
    };
    const auto oldTheme = fluent::FluentElement::currentTheme();
    const auto restoreTheme = qScopeGuard([&] { fluent::FluentElement::setTheme(oldTheme); });
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        const QColor background = theme == fluent::FluentElement::Light ? QColor(240, 240, 240) : QColor(30, 40, 50);
        for (bool nested : {false, true}) {
            for (qreal opacity : {1.0, 0.5}) {
                SCOPED_TRACE(
                    ::testing::Message() << "theme=" << theme << " nested=" << nested << " opacity=" << opacity);
                ColorBlock host(background);
                host.resize(680, 500);
                ColorBlock layer(nested ? background : QColor(200, 60, 40), &host);
                // Exercise clipping when a sibling extends beyond the scrim.
                layer.setGeometry(theme == fluent::FluentElement::Light ? 90 : -20, 70, 240, 160);
                layer.setMask(QRegion(0, 0, 220, 140));
                ColorBlock marker(QColor(200, 60, 40), &layer);
                marker.setGeometry(30, 20, 140, 100);
                auto* effect = new QGraphicsOpacityEffect;
                QWidget* faded = nested ? static_cast<QWidget*>(&marker) : &layer;
                faded->setGraphicsEffect(effect);
                effect->setOpacity(opacity);
                QSignalSpy enabled(effect, &QGraphicsEffect::enabledChanged);
                QSignalSpy changed(effect, &QGraphicsOpacityEffect::opacityChanged);
                QWidget surface(&host);
                surface.setGeometry(400, 300, 160, 100);
                OverlayScrim scrim(&host);
                scrim.setGeometry(20, 10, 620, 460);
                scrim.setModalAndDim(false, false);
                host.show();
                ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
                scrim.raise();
                surface.raise();
                const QRect original = layer.geometry();
                QWidget* focus = QApplication::focusWidget();
                scrim.setBackdrop(&surface, 2);
                const auto image = scrim.grab().toImage();
                const QPoint sample = marker.mapTo(&host, marker.rect().center()) - scrim.pos();
                const auto pixel = [&](const QPoint& position) {
                    return image.pixelColor(position * image.devicePixelRatio());
                };
                const QColor expected(qRound(200 * opacity + background.red() * (1 - opacity)),
                    qRound(60 * opacity + background.green() * (1 - opacity)),
                    qRound(40 * opacity + background.blue() * (1 - opacity)));
                const QColor masked = pixel(layer.mapTo(&host, QPoint(230, 80)) - scrim.pos());
                EXPECT_NEAR(masked.red(), background.red(), 3);
                EXPECT_NEAR(masked.green(), background.green(), 3);
                EXPECT_NEAR(masked.blue(), background.blue(), 3);
                const QColor actual = pixel(sample);
                EXPECT_NEAR(actual.red(), expected.red(), 3);
                EXPECT_NEAR(actual.green(), expected.green(), 3);
                EXPECT_NEAR(actual.blue(), expected.blue(), 3);
                // A duplicate translated copy must not appear in the lower right.
                const QColor empty = pixel(QPoint(400, 300));
                EXPECT_NEAR(empty.red(), background.red(), 3);
                EXPECT_NEAR(empty.green(), background.green(), 3);
                EXPECT_NEAR(empty.blue(), background.blue(), 3);
                EXPECT_EQ(layer.geometry(), original);
                EXPECT_TRUE(layer.isVisible());
                EXPECT_TRUE(effect->isEnabled());
                EXPECT_DOUBLE_EQ(effect->opacity(), opacity);
                EXPECT_EQ(enabled.count(), 0);
                EXPECT_EQ(changed.count(), 0);
                EXPECT_EQ(QApplication::focusWidget(), focus);
            }
        }
    }
}

TEST(OverlayScrimTest, ExternalShadowsSurviveIncludingWhenOnlyTheShadowIntersects)
{
    class ColorBlock : public QWidget {
    public:
        ColorBlock(const QColor& color, QWidget* parent = nullptr) : QWidget(parent), color(color) {}
        QColor color;

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), color);
        }
    };
    const auto oldTheme = fluent::FluentElement::currentTheme();
    const auto restoreTheme = qScopeGuard([&] { fluent::FluentElement::setTheme(oldTheme); });
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        const QColor background = theme == fluent::FluentElement::Light ? QColor(240, 240, 240) : QColor(30, 40, 50);
        for (const QPoint& offset : {QPoint(96, 0), QPoint(-96, 0), QPoint(0, 80), QPoint(0, -80)}) {
            for (bool shadowOnly : {false, true}) {
                SCOPED_TRACE(::testing::Message()
                    << "theme=" << theme << " offset=" << offset.x() << ',' << offset.y()
                    << " shadowOnly=" << shadowOnly);
                ColorBlock host(background);
                host.resize(900, 600);
                ColorBlock block(QColor(200, 50, 40), &host);
                block.setGeometry(400, 280, 80, 60);
                auto* effect = new QGraphicsDropShadowEffect(&block);
                effect->setOffset(offset);
                effect->setBlurRadius(4);
                effect->setColor(QColor(20, 80, 200));
                block.setGraphicsEffect(effect);
                host.show();
                ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));

                // Use the real parent rendering as the oracle, including the
                // platform's DPR treatment of effect offsets.
                // zh_CN: 用真实父控件的绘制作对照，包含平台对特效偏移的 DPR 处理。
                const QImage reference = host.grab().toImage();
                const QString captures = qEnvironmentVariable("FLUENT_QT_SHADOW_CAPTURE_DIR");
                const QString captureName =
                    QStringLiteral("%1-%2-%3-%4").arg(int(theme)).arg(offset.x()).arg(offset.y()).arg(shadowOnly);
                if (!captures.isEmpty()) {
                    QDir().mkpath(captures);
                    reference.save(captures + '/' + captureName + QStringLiteral("-reference.png"));
                }
                const qreal dpr = reference.devicePixelRatioF();
                const QRect bodyPixels(QRectF(QPointF(block.pos()) * dpr, QSizeF(block.size()) * dpr).toAlignedRect());
                QRect shadowPixels;
                for (int y = 0; y < reference.height(); ++y) {
                    for (int x = 0; x < reference.width(); ++x) {
                        if (bodyPixels.contains(x, y))
                            continue;
                        const QColor color = reference.pixelColor(x, y);
                        if (qAbs(color.red() - background.red()) > 40 || qAbs(color.green() - background.green()) > 40
                            || qAbs(color.blue() - background.blue()) > 40)
                            shadowPixels |= QRect(x, y, 1, 1);
                    }
                }
                ASSERT_FALSE(shadowPixels.isEmpty());
                const QPoint sample = (QPointF(shadowPixels.center()) / dpr).toPoint();
                const QRect shadowClip(sample - QPoint(8, 8), QSize(16, 16));
                ASSERT_FALSE(shadowClip.intersects(block.geometry()));
                const QColor expected = reference.pixelColor((QPointF(sample) * dpr).toPoint());
                // Capture with a dirty source instead of relying on the effect's cached image.
                // zh_CN: 使源图失效，避免测试只验证特效已有缓存时的抓图。
                block.update();

                QWidget surface(&host);
                surface.setGeometry(10, 10, 10, 10);
                OverlayScrim scrim(&host);
                scrim.setGeometry(shadowOnly ? shadowClip : host.rect().adjusted(20, 20, -20, -20));
                scrim.setModalAndDim(false, false);
                scrim.show();
                surface.show();
                scrim.raise();
                surface.raise();
                const QRect originalGeometry = block.geometry();
                QWidget* originalFocus = QApplication::focusWidget();
                QSignalSpy enabled(effect, &QGraphicsEffect::enabledChanged);
                QSignalSpy moved(effect, &QGraphicsDropShadowEffect::offsetChanged);
                scrim.setBackdrop(&surface, 1);
                const auto sampleBackdrop = [&]() {
                    const QImage image = scrim.grab().toImage();
                    if (!captures.isEmpty())
                        image.save(captures + '/' + captureName
                            + (effect->isEnabled()
                                    ? QStringLiteral("-backdrop.png")
                                    : QStringLiteral("-disabled.png")));
                    return image.pixelColor((QPointF(sample - scrim.pos()) * image.devicePixelRatioF()).toPoint());
                };
                const QColor actual = sampleBackdrop();
                EXPECT_NEAR(actual.red(), expected.red(), 3);
                EXPECT_NEAR(actual.green(), expected.green(), 3);
                EXPECT_NEAR(actual.blue(), expected.blue(), 3);
                EXPECT_EQ(block.geometry(), originalGeometry);
                EXPECT_TRUE(block.isVisible());
                EXPECT_EQ(QApplication::focusWidget(), originalFocus);
                EXPECT_TRUE(effect->isEnabled());
                EXPECT_EQ(enabled.count(), 0);
                EXPECT_EQ(moved.count(), 0);

                effect->setEnabled(false);
                scrim.refreshBackdrop();
                const QColor disabled = sampleBackdrop();
                EXPECT_NEAR(disabled.red(), background.red(), 3);
                EXPECT_NEAR(disabled.green(), background.green(), 3);
                EXPECT_NEAR(disabled.blue(), background.blue(), 3);
            }
        }
    }
}

TYPED_TEST(OverlayContractTest, ModalBlurDefaultsRespectExplicitChoices)
{
    auto& surface = this->surface;
    EXPECT_EQ(surface.backdropBlurRadius(), surface.isModal() ? 4.0 : 0.0);
    surface.setModal(false);
    QSignalSpy blur(&surface, &TypeParam::backdropBlurRadiusChanged);
    surface.setModal(true);
    surface.setModal(true);
    EXPECT_EQ(surface.backdropBlurRadius(), 4.0);
    ASSERT_EQ(blur.count(), 1);
    EXPECT_EQ(blur.at(0).at(0).toDouble(), 4.0);
    surface.setDim(false);
    EXPECT_EQ(surface.backdropBlurRadius(), 4.0);
    surface.setModal(false);
    EXPECT_EQ(surface.backdropBlurRadius(), 0.0);
    EXPECT_EQ(blur.count(), 2);

    surface.setBackdropBlurRadius(std::numeric_limits<double>::quiet_NaN());
    surface.setModal(true);
    EXPECT_EQ(surface.backdropBlurRadius(), 4.0);
    surface.setModal(false);
    // An explicit zero must opt out even when it is already the current value.
    surface.setBackdropBlurRadius(0);
    surface.setModal(true);
    EXPECT_EQ(surface.backdropBlurRadius(), 0.0);
    EXPECT_EQ(blur.count(), 4);
    surface.setBackdropBlurRadius(8);
    surface.setModal(false);
    EXPECT_EQ(surface.backdropBlurRadius(), 8.0);
}

TYPED_TEST(OverlayContractTest, ExplicitDefaultRadiusAndReentrantOverrideStayStable)
{
    auto& surface = this->surface;
    surface.setModal(true);
    QSignalSpy blur(&surface, &TypeParam::backdropBlurRadiusChanged);
    surface.setBackdropBlurRadius(4);
    surface.setModal(false);
    EXPECT_EQ(surface.backdropBlurRadius(), 4.0);
    EXPECT_EQ(blur.count(), 0);

    TypeParam other(&this->host);
    other.setModal(false);
    QSignalSpy reentrantBlur(&other, &TypeParam::backdropBlurRadiusChanged);
    QObject::connect(&other, &TypeParam::modalChanged, &other, [&other](bool modal) {
        if (modal)
            other.setBackdropBlurRadius(10);
    });
    other.setModal(true);
    EXPECT_EQ(other.backdropBlurRadius(), 10.0);
    ASSERT_EQ(reentrantBlur.count(), 1);
    EXPECT_EQ(reentrantBlur.at(0).at(0).toDouble(), 10.0);
}

TYPED_TEST(OverlayContractTest, ParametersNormalizeWithoutChangingModalOrDim)
{
    auto& surface = this->surface;
    surface.setModal(false);
    surface.setDim(false);
    QSignalSpy blur(&surface, &TypeParam::backdropBlurRadiusChanged);
    QSignalSpy dim(&surface, &TypeParam::dimOpacityChanged);
    EXPECT_EQ(surface.backdropBlurRadius(), 0.0);
    EXPECT_EQ(surface.dimOpacity(), -1.0);
    surface.setBackdropBlurRadius(8);
    surface.setBackdropBlurRadius(8);
    surface.setBackdropBlurRadius(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(blur.count(), 1);
    surface.setBackdropBlurRadius(80);
    EXPECT_EQ(surface.backdropBlurRadius(), 32.0);
    surface.setBackdropBlurRadius(-4);
    EXPECT_EQ(surface.backdropBlurRadius(), 0.0);
    surface.setDimOpacity(0.25);
    surface.setDimOpacity(0.25);
    surface.setDimOpacity(std::numeric_limits<double>::infinity());
    EXPECT_EQ(dim.count(), 1);
    surface.setDimOpacity(3);
    EXPECT_EQ(surface.dimOpacity(), 1.0);
    surface.setDimOpacity(-8);
    EXPECT_EQ(surface.dimOpacity(), -1.0);
    EXPECT_FALSE(surface.isModal());
    EXPECT_FALSE(surface.isDim());
}

TYPED_TEST(OverlayContractTest, ModalDismissConsumesPressAndReleaseWhileModelessContinues)
{
    for (bool modal : {true, false}) {
        this->surface.setModal(modal);
        this->surface.setDim(false);
        this->surface.setClosePolicy(TypeParam::CloseOnPressOutside);
        this->surface.open();
        QApplication::processEvents();
        QTest::mouseClick(this->host.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
        EXPECT_FALSE(this->surface.isOpen());
        EXPECT_EQ(this->background.presses, modal ? 0 : 1);
        EXPECT_EQ(this->background.releases, modal ? 0 : 1);
    }
}

TYPED_TEST(OverlayContractTest, OutsideDoubleClickDismissesAndConsumesItsRelease)
{
    auto& surface = this->surface;
    surface.setModal(true);
    surface.setClosePolicy(TypeParam::CloseOnPressOutside);
    surface.open();
    const QPoint local = this->background.rect().center();
    const QPoint global = this->background.mapToGlobal(local);
    const QPoint inWindow = this->host.mapFromGlobal(global);
    QMouseEvent press(QEvent::MouseButtonDblClick, QPointF(inWindow), QPointF(global), Qt::LeftButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(this->host.windowHandle(), &press);
    EXPECT_FALSE(surface.isOpen());
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(inWindow), QPointF(global), Qt::LeftButton, Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(this->host.windowHandle(), &release);
    EXPECT_EQ(this->background.presses, 0);
    EXPECT_EQ(this->background.releases, 0);
}

TYPED_TEST(OverlayContractTest, GrabbedMenuPressInAnotherWindowDoesNotDismissSurface)
{
    auto& surface = this->surface;
    surface.setModal(true);
    surface.setClosePolicy(TypeParam::CloseOnPressOutside);
    surface.open();
    QWidget other;
    other.resize(240, 160);
    other.move(this->host.frameGeometry().right() + 40, this->host.y());
    other.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&other));
    other.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&other));
    // Let the window manager finish its initial placement before moving the target.
    other.move(this->host.frameGeometry().right() + 40, this->host.y());
    QPoint global;
    ASSERT_TRUE(QTest::qWaitFor([&] {
        global = other.mapToGlobal(other.rect().center());
        return !this->host.rect().contains(this->host.mapFromGlobal(global));
    }));
    QMenu menu;
    menu.addAction(QStringLiteral("Choice"));
    menu.popup(this->host.mapToGlobal(QPoint(20, 20)));
    ASSERT_NE(menu.windowHandle(), nullptr);
    menu.windowHandle()->setTransientParent(this->host.windowHandle());
    global = other.mapToGlobal(other.rect().center());
    ASSERT_FALSE(this->host.rect().contains(this->host.mapFromGlobal(global)));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(menu.mapFromGlobal(global)), QPointF(global), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(menu.windowHandle(), &press);
    EXPECT_TRUE(surface.isOpen());
    menu.close();
    this->close();
}

TYPED_TEST(OverlayContractTest, GrabbedMenuPressInsideHostDismissesAndConsumesRelease)
{
    auto& surface = this->surface;
    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonDblClick}) {
        SCOPED_TRACE(int(type));
        surface.setModal(true);
        surface.setClosePolicy(TypeParam::CloseOnPressOutside);
        surface.open();
        const QPoint global = this->background.mapToGlobal(this->background.rect().center());
        ASSERT_TRUE(this->host.rect().contains(this->host.mapFromGlobal(global)));
        ASSERT_FALSE(surface.rect().contains(surface.mapFromGlobal(global)));
        QMenu menu;
        menu.addAction(QStringLiteral("Choice"));
        menu.popup(this->host.mapToGlobal(this->host.rect().center()));
        ASSERT_NE(menu.windowHandle(), nullptr);
        menu.windowHandle()->setTransientParent(this->host.windowHandle());
        ASSERT_FALSE(menu.frameGeometry().contains(global));
        QMouseEvent press(type, QPointF(menu.mapFromGlobal(global)), QPointF(global), Qt::LeftButton, Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(menu.windowHandle(), &press);
        EXPECT_FALSE(surface.isOpen());
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(this->host.mapFromGlobal(global)), QPointF(global),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(this->host.windowHandle(), &release);
        EXPECT_EQ(this->background.presses, 0);
        EXPECT_EQ(this->background.releases, 0);
        menu.close();
    }
}

TYPED_TEST(OverlayContractTest, GrabbedChildMenuPressInsideParentDoesNotDismissSurface)
{
    auto& surface = this->surface;
    surface.setModal(true);
    surface.setClosePolicy(TypeParam::CloseOnPressOutside);
    surface.open();
    QMenu parent;
    parent.addAction(QStringLiteral("Choice"));
    parent.popup(this->host.mapToGlobal(QPoint(10, 10)));
    ASSERT_NE(parent.windowHandle(), nullptr);
    parent.windowHandle()->setTransientParent(this->host.windowHandle());
    QMenu child(&parent);
    child.addAction(QStringLiteral("Nested choice"));
    child.popup(this->host.mapToGlobal(QPoint(300, 100)));
    ASSERT_NE(child.windowHandle(), nullptr);
    child.windowHandle()->setTransientParent(parent.windowHandle());
    ASSERT_TRUE(parent.isVisible());
    ASSERT_TRUE(child.isVisible());
    const QPoint global = parent.frameGeometry().center();
    ASSERT_TRUE(this->host.rect().contains(this->host.mapFromGlobal(global)));
    ASSERT_FALSE(surface.rect().contains(surface.mapFromGlobal(global)));
    ASSERT_FALSE(child.frameGeometry().contains(global));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(child.mapFromGlobal(global)), QPointF(global), Qt::LeftButton,
        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(child.windowHandle(), &press);
    EXPECT_TRUE(surface.isOpen());
    child.close();
    parent.close();
    this->close();
}

TYPED_TEST(OverlayContractTest, StickyModalContainsFocusAndBlocksOnlyItsOwnWindow)
{
    this->surface.setModal(true);
    this->surface.setDim(false);
    this->surface.setClosePolicy(TypeParam::NoAutoClose);
    this->surface.open();
    this->last->setFocus();
    QTest::keyClick(this->last, Qt::Key_Tab);
    EXPECT_TRUE(
        this->surface.isAncestorOf(QApplication::focusWidget()) || QApplication::focusWidget() == &this->surface);
    QTest::mouseClick(this->host.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
    EXPECT_EQ(this->background.presses, 0);
    EXPECT_TRUE(this->surface.isOpen());
    QShortcut shortcut(QKeySequence(QStringLiteral("Ctrl+K")), &this->host);
    QSignalSpy shortcutTriggered(&shortcut, &QShortcut::activated);
    this->first->setFocus();
    QTest::keyClick(this->first, Qt::Key_K, Qt::ControlModifier);
    EXPECT_EQ(shortcutTriggered.count(), 0);
    this->surface.setModal(false);
    QTest::keyClick(this->first, Qt::Key_K, Qt::ControlModifier);
    EXPECT_EQ(shortcutTriggered.count(), 1);
    this->outside.setFocus();
    EXPECT_EQ(QApplication::focusWidget(), &this->outside);
    this->surface.setModal(true);
    QApplication::processEvents();
    EXPECT_TRUE(this->surface.isAncestorOf(QApplication::focusWidget()));
    Button other(QStringLiteral("Other window"));
    other.resize(180, 60);
    other.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&other));
    QSignalSpy clicks(&other, &Button::clicked);
    QTest::mouseClick(other.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(30, 30));
    EXPECT_EQ(clicks.count(), 1);
    EXPECT_TRUE(this->surface.isOpen());
    QTest::keyClick(other.windowHandle(), Qt::Key_Escape);
    EXPECT_TRUE(this->surface.isOpen());
    this->close();
}

TYPED_TEST(OverlayContractTest, InnerEscapeHandlingPrecedesSurfaceDismissal)
{
    auto& surface = this->surface;
    auto* body = this->first->parentWidget();
    auto* editor = new EscapeProbe(body);
    editor->setFocusPolicy(Qt::StrongFocus);
    body->layout()->addWidget(editor);
    surface.setModal(true);
    surface.setClosePolicy(TypeParam::CloseOnEscape);
    for (const bool native : {false, true}) {
        SCOPED_TRACE(native);
        editor->consumeNext = true;
        editor->presses = 0;
        surface.open();
        QApplication::processEvents();
        editor->setFocus();
        ASSERT_EQ(QApplication::focusWidget(), editor);
        const auto escape = [&] {
            if (native)
                QTest::keyClick(this->host.windowHandle(), Qt::Key_Escape);
            else
                QTest::keyClick(editor, Qt::Key_Escape);
            QApplication::processEvents();
        };
        escape();
        EXPECT_EQ(editor->presses, 1);
        EXPECT_TRUE(surface.isOpen());
        escape();
        EXPECT_EQ(editor->presses, 2);
        EXPECT_FALSE(surface.isOpen());
    }
}

TYPED_TEST(OverlayContractTest, BlurRefreshesAndHighContrastKeepsInputPolicy)
{
    auto& surface = this->surface;
    surface.setModal(false);
    surface.setDim(false);
    surface.setBackdropBlurRadius(8);
    surface.open();
    QApplication::processEvents();
    auto* scrim = this->host.template findChild<OverlayScrim*>();
    ASSERT_NE(scrim, nullptr);
    EXPECT_TRUE(scrim->testAttribute(Qt::WA_TransparentForMouseEvents));
    const auto colorAt = [&] {
        const auto image = scrim->grab().toImage();
        return image.pixelColor(QPoint(20, 140) * image.devicePixelRatio());
    };
    const QColor before = colorAt();
    this->host.setStyleSheet(QStringLiteral("background: rgb(30, 140, 40)"));
    surface.refreshBackdrop();
    EXPECT_NE(colorAt(), before);
    surface.setModal(true);
    surface.setTheme(fluent::FluentElement::HighContrast);
    scrim->onThemeUpdated();
    QApplication::processEvents();
    EXPECT_FALSE(scrim->testAttribute(Qt::WA_TransparentForMouseEvents));
    surface.setClosePolicy(TypeParam::NoAutoClose);
    QTest::mouseClick(this->host.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
    EXPECT_EQ(this->background.presses, 0);
    this->close();
}

TYPED_TEST(OverlayContractTest, HigherPopupReceivesInputWithoutDismissingUnderlyingSurface)
{
    this->surface.setModal(true);
    this->surface.setClosePolicy(
        typename TypeParam::ClosePolicy(TypeParam::CloseOnPressOutside | TypeParam::CloseOnEscape));
    this->surface.open();
    Popup upper(&this->surface);
    upper.setAnimationEnabled(false);
    upper.setModal(true);
    upper.setFixedSize(200, 100);
    auto* layout = new QVBoxLayout(&upper);
    auto* button = new Button(QStringLiteral("Nested command"), &upper);
    layout->addWidget(button);
    QSignalSpy clicked(button, &Button::clicked);
    upper.open();
    QApplication::processEvents();
    const auto target = button->mapTo(&this->host, button->rect().center());
    QTest::mouseClick(this->host.windowHandle(), Qt::LeftButton, Qt::NoModifier, target);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_TRUE(this->surface.isOpen());
    QTest::keyClick(&upper, Qt::Key_Escape);
    EXPECT_FALSE(upper.isOpen());
    EXPECT_TRUE(this->surface.isOpen());
    this->close();
}

TYPED_TEST(OverlayContractTest, LocalDarkThemeRefreshesHostedText)
{
    auto* label = new fluent::textfields::Label(QStringLiteral("Hosted text"), this->first->parentWidget());
    this->first->parentWidget()->layout()->addWidget(label);
    this->host.setProperty("fluentThemeOverride", int(fluent::FluentElement::Dark));
    this->surface.open();
    QApplication::processEvents();
    EXPECT_EQ(label->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(label->palette().color(QPalette::WindowText), label->themeColors().textPrimary);
    this->close();
}

TYPED_TEST(OverlayContractTest, OutsideDismissMayDeleteTheSurfaceSynchronously)
{
    auto* transient = new TypeParam(&this->host);
    transient->setAnimationEnabled(false);
    transient->setModal(true);
    transient->setClosePolicy(TypeParam::CloseOnPressOutside);
    if constexpr (std::is_same_v<TypeParam, DrawerView>)
        transient->setEdge(DrawerView::DrawerEdge::Right);
    QPointer<TypeParam> guard(transient);
    QObject::connect(transient, &TypeParam::closed, transient, [transient] { delete transient; });
    transient->open();
    QApplication::processEvents();
    QTest::mouseClick(this->host.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(this->background.presses, 0);
    EXPECT_EQ(this->background.releases, 0);
}
} // namespace
