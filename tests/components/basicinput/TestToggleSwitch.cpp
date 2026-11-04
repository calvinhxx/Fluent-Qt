#include "components/basicinput/Button.h"
#include "QtTestEnvironment.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QApplication>
#include <QImage>
#include <QLayout>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>
#include <limits>

using namespace fluent::basicinput;

namespace {
QImage renderSwitch(ToggleSwitch& toggle)
{
    toggle.ensurePolished();
    QImage image(toggle.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    toggle.render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
    return image;
}

QRect paintedBounds(const QImage& image)
{
    QRect result;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)))
                result = result.united(QRect(x, y, 1, 1));
        }
    }
    return result;
}
} // namespace

class ToggleSwitchTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::ThemeRegistry::instance().resetToDefaults();
    }

    void TearDown() override
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::ThemeRegistry::instance().resetToDefaults();
    }
};

// ── 默认属性 ─────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, DefaultPropertyValues)
{
    ToggleSwitch ts;
    EXPECT_FALSE(ts.isOn());
    EXPECT_TRUE(ts.onContent().isEmpty());
    EXPECT_TRUE(ts.offContent().isEmpty());
    EXPECT_TRUE(ts.accessibleDescription().isEmpty());
    EXPECT_EQ(ts.fontRole(), Typography::FontRole::Body);
}

// ── IsOn 属性 ────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, ToggledEmitsOncePerStateChange)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.takeFirst().first().toBool());
    EXPECT_TRUE(ts.isOn());
    ts.setIsOn(true);
    EXPECT_EQ(spy.count(), 0);

    ts.setIsOn(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.first().first().toBool());
}

// ── OnContent / OffContent 属性 ──────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetOnContentEmitsSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::onContentChanged);
    ts.setOnContent(QString());
    EXPECT_EQ(spy.count(), 0);
    ts.setOnContent("Working");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.onContent(), "Working");
}

TEST_F(ToggleSwitchTest, SetOffContentEmitsSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::offContentChanged);
    ts.setOffContent(QString());
    EXPECT_EQ(spy.count(), 0);
    ts.setOffContent("Do work");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.offContent(), "Do work");
}

TEST_F(ToggleSwitchTest, ApplicationTextDrivesAccessibilityWithoutOverwritingOverrides)
{
    ToggleSwitch ts;
    ts.setOnContent(QStringLiteral("Connected"));
    ts.setOffContent(QStringLiteral("Disconnected"));
    EXPECT_EQ(ts.accessibleDescription(), QStringLiteral("Disconnected"));

    ts.setIsOn(true);
    EXPECT_EQ(ts.accessibleDescription(), QStringLiteral("Connected"));

    ts.setAccessibleDescription(QStringLiteral("Wi-Fi state"));
    ts.setIsOn(false);
    ts.setOffContent(QStringLiteral("Offline"));
    EXPECT_EQ(ts.accessibleDescription(), QStringLiteral("Wi-Fi state"));
}

// ── FontRole 属性 ────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetFontRoleEmitsSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::fontRoleChanged);
    ts.setFontRole(Typography::FontRole::Body);
    EXPECT_EQ(spy.count(), 0);
    ts.setFontRole(Typography::FontRole::Caption);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.fontRole(), Typography::FontRole::Caption);
}

TEST_F(ToggleSwitchTest, RoleFontTracksThemeAndExplicitFontTakesPrecedence)
{
    ToggleSwitch ts;
    ts.setAttribute(Qt::WA_DontShowOnScreen);
    ts.setOnContent(QStringLiteral("On"));
    ts.show();
    ASSERT_TRUE(ts.isVisible());

    auto& registry = fluent::ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontFamilyOverride = QStringLiteral("Issue 50 Toggle Theme Font");
    themed.fontScale = 1.5;
    ASSERT_TRUE(registry.applySnapshot(themed));

    const QFont roleFont = ts.themeFont(Typography::FontRole::Body).toQFont();
    EXPECT_EQ(ts.font().family(), roleFont.family());
    EXPECT_EQ(ts.font().pixelSize(), roleFont.pixelSize());
    EXPECT_EQ(ts.font().weight(), roleFont.weight());

    QFont explicitFont(QStringLiteral("Issue 50 Toggle Explicit Font"));
    explicitFont.setPixelSize(23);
    ts.setFont(explicitFont);
    const QFont appliedExplicitFont = ts.font();

    themed.fontScale = 1.75;
    ASSERT_TRUE(registry.applySnapshot(themed));
    EXPECT_EQ(ts.font(), appliedExplicitFont);

    QSignalSpy spy(&ts, &ToggleSwitch::fontRoleChanged);
    ts.setFontRole(Typography::FontRole::Body);
    EXPECT_EQ(spy.count(), 0);
    const QFont restoredRoleFont = ts.themeFont(Typography::FontRole::Body).toQFont();
    EXPECT_EQ(ts.font().family(), restoredRoleFont.family());
    EXPECT_EQ(ts.font().pixelSize(), restoredRoleFont.pixelSize());
    EXPECT_EQ(ts.font().weight(), restoredRoleFont.weight());
}

// ── KnobPosition 属性 ────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, KnobPositionClamped)
{
    ToggleSwitch ts;
    ts.setKnobPosition(2.0);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 1.0);
    ts.setKnobPosition(-1.0);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 0.0);
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SizeHintIncludesTrackAndContent)
{
    ToggleSwitch ts;
    QSize hint = ts.sizeHint();
    EXPECT_GE(hint.width(), 40); // at least track width
    EXPECT_GE(hint.height(), Spacing::ControlHeight::Small);
}

TEST_F(ToggleSwitchTest, SizeHintReflectsContentWidth)
{
    ToggleSwitch ts;
    QSize shortHint = ts.sizeHint();
    ts.setOnContent("A much longer switch state");
    QSize longHint = ts.sizeHint();
    EXPECT_GT(longHint.width(), shortHint.width());
    EXPECT_EQ(longHint.height(), shortHint.height());
}

TEST_F(ToggleSwitchTest, MinimumSizeHintPreservesTrackAndHitHeight)
{
    ToggleSwitch ts;
    QSize minHint = ts.minimumSizeHint();
    EXPECT_EQ(minHint.width(), 40);
    EXPECT_EQ(minHint.height(), Spacing::ControlHeight::Small);
}

TEST_F(ToggleSwitchTest, VisualScaleNormalizesAndNotifiesWithoutChangingFontOrFixedSize)
{
    ToggleSwitch ts;
    EXPECT_DOUBLE_EQ(ts.visualScale(), 1.0);
    ts.setFixedSize(300, 100);
    const QFont originalFont = ts.font();
    QSignalSpy scales(&ts, &ToggleSwitch::visualScaleChanged);
    QSignalSpy toggles(&ts, &ToggleSwitch::toggled);
    ASSERT_TRUE(ts.setProperty("visualScale", 2.5));
    EXPECT_DOUBLE_EQ(ts.visualScale(), 2.5);
    ASSERT_EQ(scales.count(), 1);
    EXPECT_DOUBLE_EQ(scales.first().first().toDouble(), 2.5);
    ts.setVisualScale(2.5);
    ts.setVisualScale(std::numeric_limits<qreal>::quiet_NaN());
    ts.setVisualScale(std::numeric_limits<qreal>::infinity());
    ts.setVisualScale(-std::numeric_limits<qreal>::infinity());
    EXPECT_EQ(scales.count(), 1);
    ts.setVisualScale(-1.0);
    EXPECT_DOUBLE_EQ(ts.visualScale(), 0.5);
    ts.setVisualScale(0.0);
    EXPECT_EQ(scales.count(), 2);
    ts.setVisualScale(100.0);
    EXPECT_DOUBLE_EQ(ts.visualScale(), 10.0);
    ts.setVisualScale(std::numeric_limits<qreal>::max());
    EXPECT_EQ(scales.count(), 3);
    EXPECT_EQ(ts.size(), QSize(300, 100));
    EXPECT_EQ(ts.font(), originalFont);
    EXPECT_EQ(toggles.count(), 0);
}

TEST_F(ToggleSwitchTest, VisualScaleUpdatesLayoutHintsAndPreservesMinimumHitHeight)
{
    QWidget host;
    ToggleSwitch ts(&host);
    ts.setOffContent(QStringLiteral("Disconnected"));
    const QSize original = ts.sizeHint();
    QWidgetItem item(&ts);
    const QSize itemHint = item.sizeHint();
    ts.setVisualScale(2.0);
    EXPECT_EQ(ts.sizeHint().width(), original.width() + 50);
    EXPECT_EQ(ts.minimumSizeHint(), QSize(80, 40));
    EXPECT_GE(item.sizeHint().width(), itemHint.width() + 50);
    EXPECT_GE(item.sizeHint().height(), 40);
    ts.setVisualScale(0.5);
    EXPECT_EQ(ts.minimumSizeHint(), QSize(20, Spacing::ControlHeight::Small));
    EXPECT_GE(ts.sizeHint().height(), QFontMetrics(ts.font()).height());
    ts.setVisualScale(1.0);
    EXPECT_EQ(ts.sizeHint(), original);
}

TEST_F(ToggleSwitchTest, WidgetResizePreservesDefaultTrackPixels)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    ToggleSwitch ts;
    for (bool on : {false, true}) {
        ts.setIsOn(on);
        ts.setFixedSize(ts.sizeHint());
        const QImage normal = renderSwitch(ts);
        const QRect normalBounds = paintedBounds(normal);
        ts.setFixedSize(300, 100);
        const QImage enlarged = renderSwitch(ts);
        const QRect enlargedBounds = paintedBounds(enlarged);
        EXPECT_EQ(normalBounds.size(), QSize(40, 20));
        EXPECT_EQ(enlargedBounds, QRect(0, 40, 40, 20));
        EXPECT_EQ(normal.copy(normalBounds), enlarged.copy(enlargedBounds));
    }
}

TEST_F(ToggleSwitchTest, VisualScalePaintsProportionalTrackInBothDirections)
{
    ToggleSwitch ts;
    ts.resize(500, 220);
    for (qreal scale : {0.5, 1.25, 2.0, 5.0}) {
        ts.setVisualScale(scale);
        const int trackW = qRound(40 * scale);
        const int trackH = qRound(20 * scale);
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            ts.setLayoutDirection(direction);
            const QRect expected(direction == Qt::LeftToRight ? 0 : ts.width() - trackW, (ts.height() - trackH) / 2,
                trackW, trackH);
            EXPECT_EQ(paintedBounds(renderSwitch(ts)), expected);
        }
    }
}

TEST_F(ToggleSwitchTest, VisualScalePreservesAnimationProgressAndInput)
{
    QWidget host;
    ToggleSwitch ts(&host);
    ts.setFixedSize(300, 100);
    host.show();
    ts.show();
    QApplication::processEvents();
    ts.setIsOn(true);
    auto* animation = ts.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);
    animation->setCurrentTime(animation->duration() / 2);
    const qreal progress = ts.knobPosition();
    QSignalSpy toggles(&ts, &ToggleSwitch::toggled);
    ts.setVisualScale(3.0);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), progress);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(toggles.count(), 0);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 1.0);
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        ts.setLayoutDirection(direction);
        const bool before = ts.isOn();
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, QPoint(150, 90));
        EXPECT_NE(ts.isOn(), before);
        QTest::keyClick(&ts, Qt::Key_Space);
        EXPECT_EQ(ts.isOn(), before);
    }
    const int changesBeforeDisable = toggles.count();
    ts.setEnabled(false);
    QTest::mouseClick(&ts, Qt::LeftButton);
    QTest::keyClick(&ts, Qt::Key_Space);
    EXPECT_TRUE(ts.isOn());
    EXPECT_EQ(toggles.count(), changesBeforeDisable);
}

// ── Disabled 状态 ────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, DisabledState)
{
    ToggleSwitch ts;
    ts.setIsOn(true);
    ts.setEnabled(false);
    EXPECT_FALSE(ts.isEnabled());
    EXPECT_TRUE(ts.isOn());
}

TEST_F(ToggleSwitchTest, MouseActivationTakesFocusAndToggles)
{
    QWidget host;
    ToggleSwitch ts(&host);
    ts.setGeometry(20, 20, ts.sizeHint().width(), ts.sizeHint().height());
    Button focusSink(QStringLiteral("Focus sink"), &host);
    focusSink.setGeometry(20, 80, 120, 32);
    host.show();
    ts.show();
    focusSink.show();
    if (!tests::support::isHeadlessPlatform()) {
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
        if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
            host.activateWindow();
    }
    focusSink.setFocus(Qt::OtherFocusReason);
    ASSERT_TRUE(QTest::qWaitFor([&focusSink] { return focusSink.hasFocus(); }, 1000));
    ASSERT_FALSE(ts.hasFocus());

    QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.rect().center());

    ASSERT_TRUE(QTest::qWaitFor([&ts] { return ts.hasFocus(); }, 1000));
    EXPECT_TRUE(ts.isOn());
}

// ── 初始 Knob 位置 ──────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, InitialKnobPositionOff)
{
    ToggleSwitch ts;
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 0.0);
}

TEST_F(ToggleSwitchTest, GlobalMotionDisabledAppliesKnobFinalState)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    ToggleSwitch ts;

    ts.setIsOn(true);

    auto* animation = ts.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);
    EXPECT_TRUE(ts.isOn());
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 1.0);
    EXPECT_EQ(animation->duration(), 0);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
}

TEST_F(ToggleSwitchTest, GlobalMotionReducedCapsKnobTransition)
{
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    ToggleSwitch ts;

    ts.setIsOn(true);

    auto* animation = ts.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);
    EXPECT_GT(animation->duration(), 0);
    EXPECT_LE(animation->duration(), 50);
    ASSERT_TRUE(QTest::qWaitFor([&] { return qFuzzyCompare(ts.knobPosition(), 1.0); }, 300));
}

TEST_F(ToggleSwitchTest, ActiveKnobTransitionConvergesWhenMotionIsDisabled)
{
    ToggleSwitch ts;
    auto* animation = ts.findChild<QPropertyAnimation*>();
    ASSERT_NE(animation, nullptr);

    ts.setIsOn(true);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_LT(ts.knobPosition(), 1.0);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);

    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 1.0);
}
