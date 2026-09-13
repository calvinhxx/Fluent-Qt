#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <limits>

#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/layout/ParticleBackdrop.h"
#include "QtTestEnvironment.h"

using fluent::layout::ParticleBackdrop;

class ParticleBackdropTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        oldMode = fluent::MotionPolicy::instance().mode();
        oldTheme = fluent::FluentElement::currentTheme();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    }
    void TearDown() override
    {
        fluent::MotionPolicy::instance().setMode(oldMode);
        fluent::FluentElement::setTheme(oldTheme);
    }
    fluent::MotionPolicy::Mode oldMode;
    fluent::FluentElement::Theme oldTheme;
};

class ParticleBackdropPresetTest : public ParticleBackdropTest,
                                   public ::testing::WithParamInterface<ParticleBackdrop::Effect> {
};

INSTANTIATE_TEST_SUITE_P(Presets, ParticleBackdropPresetTest,
                         ::testing::Values(ParticleBackdrop::FlowingRibbons,
                                           ParticleBackdrop::FloatingDots,
                                           ParticleBackdrop::Starfield));

TEST_F(ParticleBackdropTest, Contract_DefaultsAndNormalizedSetters)
{
    static_assert(std::is_base_of<fluent::QMLPlus, ParticleBackdrop>::value);
    ParticleBackdrop backdrop;
    EXPECT_FALSE(backdrop.isAnimating());
    EXPECT_TRUE(backdrop.isAnimationEnabled());
    EXPECT_FALSE(backdrop.isInteractive());
    EXPECT_TRUE(backdrop.isPauseWhenInactive());
    EXPECT_EQ(backdrop.backgroundMode(), ParticleBackdrop::Transparent);
    EXPECT_EQ(backdrop.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(backdrop.particleCount(), 240);
    EXPECT_EQ(backdrop.maximumFrameRate(), 30);
    EXPECT_EQ(backdrop.effect(), ParticleBackdrop::FlowingRibbons);

    QSignalSpy count(&backdrop, &ParticleBackdrop::particleCountChanged);
    backdrop.setParticleCount(10000);
    backdrop.setParticleCount(960);
    EXPECT_EQ(backdrop.particleCount(), 960);
    EXPECT_EQ(count.count(), 1);
    backdrop.setParticleCount(-1);
    EXPECT_EQ(backdrop.particleCount(), 0);

    QSignalSpy speed(&backdrop, &ParticleBackdrop::speedChanged);
    backdrop.setSpeed(9);
    backdrop.setSpeed(4);
    backdrop.setSpeed(std::numeric_limits<qreal>::quiet_NaN());
    EXPECT_EQ(backdrop.speed(), 4);
    EXPECT_EQ(speed.count(), 1);
    backdrop.setMaximumFrameRate(0);
    EXPECT_EQ(backdrop.maximumFrameRate(), 1);
    backdrop.setMaximumFrameRate(1000);
    EXPECT_EQ(backdrop.maximumFrameRate(), 60);
    backdrop.setFadeMargins(QMarginsF(-1, 20, -2, 40));
    EXPECT_EQ(backdrop.fadeMargins(), QMarginsF(0, 20, 0, 40));
}

TEST_P(ParticleBackdropPresetTest, Contract_DefaultFrameBudgetLimitsRepaints)
{
    class PaintProbe : public ParticleBackdrop {
    public:
        int paints = 0;
        void paintEvent(QPaintEvent* event) override
        {
            ++paints;
            ParticleBackdrop::paintEvent(event);
        }
    } backdrop;
    backdrop.setEffect(GetParam());
    backdrop.resize(240, 160);
    backdrop.setPauseWhenInactive(false);
    backdrop.setMaximumFrameRate(30); // An unchanged value must already be effective.
    backdrop.show();
    QTest::qWait(100);
    backdrop.paints = 0;
    QTest::qWait(400);
    EXPECT_GT(backdrop.paints, 0);
    EXPECT_LE(backdrop.paints, 16); // Allow extra exposure paints, but reject a 60 fps timer.
}

TEST_P(ParticleBackdropPresetTest, Contract_ClippedHiddenAndReparentedSurfacesStop)
{
    QWidget viewport;
    viewport.resize(240, 180);
    ParticleBackdrop backdrop(&viewport);
    backdrop.setEffect(GetParam());
    backdrop.resize(220, 160);
    backdrop.setPauseWhenInactive(false);
    viewport.show();
    QTest::qWait(25);
    EXPECT_TRUE(backdrop.isAnimating());

    backdrop.move(300, 0);
    QTest::qWait(25);
    EXPECT_FALSE(backdrop.isAnimating());
    backdrop.move(0, 0);
    QTest::qWait(25);
    EXPECT_TRUE(backdrop.isAnimating());

    QWidget clippedContent(&viewport);
    clippedContent.resize(400, 300);
    clippedContent.show();
    backdrop.setParent(&clippedContent);
    backdrop.show();
    QTest::qWait(25);
    EXPECT_TRUE(backdrop.isAnimating());
    clippedContent.move(0, -400);
    QTest::qWait(25);
    EXPECT_FALSE(backdrop.isAnimating());
    clippedContent.move(0, 0);
    QTest::qWait(25);
    EXPECT_TRUE(backdrop.isAnimating());
    viewport.hide();
    QTest::qWait(25);
    EXPECT_FALSE(backdrop.isAnimating());
    backdrop.setParent(&viewport);
}

TEST_P(ParticleBackdropPresetTest, Contract_MotionPolicyAndLocalSwitchRemainAuthoritative)
{
    ParticleBackdrop backdrop;
    backdrop.setEffect(GetParam());
    backdrop.resize(300, 200);
    backdrop.setPauseWhenInactive(false);
    backdrop.show();
    QTest::qWait(25);
    EXPECT_TRUE(backdrop.isAnimating());
    backdrop.setAnimationEnabled(false);
    EXPECT_FALSE(backdrop.isAnimating());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    backdrop.setAnimationEnabled(true);
    EXPECT_FALSE(backdrop.isAnimating());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    EXPECT_FALSE(backdrop.isAnimating());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_TRUE(backdrop.isAnimating());
    backdrop.setSpeed(0);
    EXPECT_FALSE(backdrop.isAnimating());
    backdrop.setSpeed(1);
    EXPECT_TRUE(backdrop.isAnimating());
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    EXPECT_FALSE(backdrop.isAnimating());
}

TEST_P(ParticleBackdropPresetTest, Contract_LocalPreviewThemeControlsMotionAndPixels)
{
    ParticleBackdrop backdrop;
    backdrop.setEffect(GetParam());
    backdrop.resize(300, 200);
    backdrop.setBackgroundMode(ParticleBackdrop::Solid);
    backdrop.setPauseWhenInactive(false);
    backdrop.show();
    QTest::qWait(25);
    ASSERT_TRUE(backdrop.isAnimating());
    backdrop.setProperty("fluentThemeOverride", int(fluent::FluentElement::HighContrast));
    backdrop.onThemeUpdated();
    EXPECT_FALSE(backdrop.isAnimating());
    EXPECT_EQ(backdrop.grab().toImage().pixelColor(0, 0), backdrop.themeColors().bgSolid);
    backdrop.setProperty("fluentThemeOverride", int(fluent::FluentElement::Light));
    backdrop.onThemeUpdated();
    EXPECT_TRUE(backdrop.isAnimating());
    EXPECT_FALSE(backdrop.effectiveThemeUsesDarkAppearance());
    EXPECT_EQ(backdrop.grab().toImage().pixelColor(0, 0), backdrop.themeColors().bgSolid);
}

TEST_P(ParticleBackdropPresetTest, Contract_TransparentFadePreservesParentAndPausedImage)
{
    QWidget parent;
    const QColor background(31, 43, 55);
    QPalette palette = parent.palette();
    palette.setColor(QPalette::Window, background);
    parent.setPalette(palette);
    parent.setAutoFillBackground(true);
    parent.resize(400, 240);
    ParticleBackdrop backdrop(&parent);
    backdrop.setEffect(GetParam());
    backdrop.resize(parent.size());
    backdrop.setAnimationEnabled(false);
    backdrop.setFadeMargins(QMarginsF(100, 0, 0, 130));
    parent.show();
    QTest::qWait(25);
    const QImage before = parent.grab().toImage();
    backdrop.triggerRipple(QPointF(250, 90));
    QTest::qWait(25);
    const QImage after = parent.grab().toImage();
    EXPECT_EQ(before, after);
    EXPECT_EQ(after.pixelColor(0, 0), background);
    const QColor bottom = after.pixelColor(after.width() / 2, after.height() - 1);
    EXPECT_NEAR(bottom.red(), background.red(), 1);
    EXPECT_NEAR(bottom.green(), background.green(), 1);
    EXPECT_NEAR(bottom.blue(), background.blue(), 1);
    EXPECT_EQ(bottom.alpha(), 255);
    backdrop.setParticleCount(0);
    EXPECT_NE(parent.grab().toImage(), after);
}

TEST_P(ParticleBackdropPresetTest, Contract_ChildButtonRetainsPointerInput)
{
    ParticleBackdrop backdrop;
    backdrop.setEffect(GetParam());
    backdrop.resize(320, 180);
    backdrop.setInteractive(true);
    backdrop.setAnimationEnabled(false);
    fluent::basicinput::Button button(QStringLiteral("Action"), &backdrop);
    button.setGeometry(20, 20, 120, 36);
    QSignalSpy clicked(&button, &QAbstractButton::clicked);
    backdrop.show();
    QTest::qWait(25);
    QTest::mouseClick(&button, Qt::LeftButton);
    EXPECT_EQ(clicked.count(), 1);
}

TEST_F(ParticleBackdropTest, Contract_EffectSwitchPreservesSettingsAndNotifiesOnce)
{
    ParticleBackdrop backdrop;
    backdrop.resize(320, 180);
    backdrop.setAnimationEnabled(false);
    backdrop.setParticleCount(96);
    backdrop.setMaximumFrameRate(24);
    backdrop.setSpeed(.5);
    backdrop.setInteractive(true);
    backdrop.setPauseWhenInactive(false);
    backdrop.setBackgroundMode(ParticleBackdrop::Solid);
    backdrop.setFadeMargins(QMarginsF(20, 0, 0, 10));
    fluent::basicinput::Button child(QStringLiteral("Action"), &backdrop);
    backdrop.show();
    QTest::qWait(25);
    QSignalSpy effects(&backdrop, &ParticleBackdrop::effectChanged);
    QSignalSpy motion(&backdrop, &ParticleBackdrop::animatingChanged);
    QSignalSpy counts(&backdrop, &ParticleBackdrop::particleCountChanged);
    for (auto effect : {ParticleBackdrop::FloatingDots, ParticleBackdrop::Starfield,
                        ParticleBackdrop::FlowingRibbons}) {
        ASSERT_TRUE(backdrop.setProperty("effect", QVariant::fromValue(effect)));
        backdrop.setEffect(effect);
        backdrop.setEffect(static_cast<ParticleBackdrop::Effect>(-1));
        EXPECT_EQ(backdrop.effect(), effect);
        EXPECT_EQ(backdrop.particleCount(), 96);
        EXPECT_EQ(backdrop.maximumFrameRate(), 24);
        EXPECT_EQ(backdrop.speed(), .5);
        EXPECT_TRUE(backdrop.isInteractive());
        EXPECT_FALSE(backdrop.isPauseWhenInactive());
        EXPECT_FALSE(backdrop.isAnimationEnabled());
        EXPECT_FALSE(backdrop.isAnimating());
        EXPECT_EQ(backdrop.backgroundMode(), ParticleBackdrop::Solid);
        EXPECT_EQ(backdrop.fadeMargins(), QMarginsF(20, 0, 0, 10));
        EXPECT_EQ(child.parentWidget(), &backdrop);
    }
    EXPECT_EQ(effects.count(), 3);
    EXPECT_EQ(motion.count(), 0);
    EXPECT_EQ(counts.count(), 0);
}

TEST_F(ParticleBackdropTest, Contract_PresetsAreDistinctAndResetDeterministically)
{
    ParticleBackdrop backdrop;
    backdrop.resize(420, 260);
    backdrop.setAnimationEnabled(false);
    backdrop.setBackgroundMode(ParticleBackdrop::Solid);
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        backdrop.setEffect(ParticleBackdrop::FlowingRibbons);
        const QImage ribbons = backdrop.grab().toImage();
        backdrop.setEffect(ParticleBackdrop::FloatingDots);
        const QImage dots = backdrop.grab().toImage();
        backdrop.setEffect(ParticleBackdrop::Starfield);
        const QImage stars = backdrop.grab().toImage();
        EXPECT_NE(ribbons, dots);
        EXPECT_NE(ribbons, stars);
        EXPECT_NE(dots, stars);
        backdrop.setEffect(ParticleBackdrop::FlowingRibbons);
        EXPECT_EQ(backdrop.grab().toImage(), ribbons);
        for (auto effect : {ParticleBackdrop::FloatingDots, ParticleBackdrop::Starfield}) {
            backdrop.setEffect(effect);
            const QImage populated = backdrop.grab().toImage();
            backdrop.setParticleCount(0);
            EXPECT_NE(backdrop.grab().toImage(), populated);
            backdrop.setParticleCount(240);
            EXPECT_EQ(backdrop.grab().toImage(), populated);
        }
    }
}

TEST_P(ParticleBackdropPresetTest, Contract_PresetsAnimateAndFreeze)
{
    ParticleBackdrop backdrop;
    backdrop.resize(420, 260);
    backdrop.setEffect(GetParam());
    backdrop.setPauseWhenInactive(false);
    backdrop.show();
    QTest::qWait(50);
    const QImage first = backdrop.grab().toImage();
    QTest::qWait(100);
    const QImage second = backdrop.grab().toImage();
    EXPECT_NE(first, second);
    backdrop.setAnimationEnabled(false);
    const QImage paused = backdrop.grab().toImage();
    QTest::qWait(100);
    EXPECT_EQ(backdrop.grab().toImage(), paused);
}

TEST_F(ParticleBackdropTest, Contract_EffectNotificationMayDestroySurface)
{
    QPointer<ParticleBackdrop> backdrop = new ParticleBackdrop;
    QObject::connect(backdrop, &ParticleBackdrop::effectChanged, backdrop,
                     [backdrop] { delete backdrop; });
    backdrop->setEffect(ParticleBackdrop::FloatingDots);
    EXPECT_TRUE(backdrop.isNull());
}

TEST_F(ParticleBackdropTest, Contract_AnimatingNotificationMayDestroySurface)
{
    QWidget host;
    host.resize(320, 180);
    auto* backdrop = new ParticleBackdrop(&host);
    backdrop->resize(host.size());
    backdrop->setPauseWhenInactive(false);
    host.show();
    QTest::qWait(25);
    ASSERT_TRUE(backdrop->isAnimating());
    QObject::connect(backdrop, &ParticleBackdrop::animatingChanged, &host,
                     [backdrop](bool running) {
                         if (!running)
                             delete backdrop;
                     });
    QPointer<ParticleBackdrop> guard(backdrop);
    backdrop->setAnimationEnabled(false);
    EXPECT_TRUE(guard.isNull());
}

TEST_F(ParticleBackdropTest, VisualCheck_ParticlesAndChildControls)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Interactive visual review";
    using Edge = fluent::AnchorLayout::Edge;
    QWidget window;
    window.resize(900, 540);
    auto* layout = new fluent::AnchorLayout(&window);
    auto* backdrop = new ParticleBackdrop(&window);
    backdrop->setBackgroundMode(ParticleBackdrop::Solid);
    backdrop->setInteractive(true);
    backdrop->anchors()->left = {&window, Edge::Left, 0};
    backdrop->anchors()->right = {&window, Edge::Right, 0};
    backdrop->anchors()->top = {&window, Edge::Top, 0};
    backdrop->anchors()->bottom = {&window, Edge::Bottom, 0};
    layout->addWidget(backdrop);
    auto* ripple = new fluent::basicinput::Button(QStringLiteral("Create a ripple"), backdrop);
    ripple->setGeometry(24, 24, 160, 36);
    QObject::connect(ripple, &fluent::basicinput::Button::clicked, backdrop,
                     [backdrop] { backdrop->triggerRipple(QPointF(580, 220)); });
    window.show();
    if (tests::support::isVisualSnapshotMode()) {
        tests::support::VisualSnapshotOptions options;
        options.windowSize = QSize(900, 540);
        ASSERT_TRUE(tests::support::captureVisualSnapshot(&window, options));
        return;
    }
    qApp->exec();
}
