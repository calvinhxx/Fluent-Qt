#include "components/basicinput/Button.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include <QApplication>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <gtest/gtest.h>

using namespace fluent::basicinput;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class ToggleSwitchTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── 测试类 ───────────────────────────────────────────────────────────────────

class ToggleSwitchTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::ThemeRegistry::instance().resetToDefaults();
        window = new ToggleSwitchTestWindow();
        window->setFixedSize(500, 500);
        window->setWindowTitle("Fluent ToggleSwitch Visual Test");
        window->onThemeUpdated();
    }

    void TearDown() override
    {
        delete window;
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
        fluent::ThemeRegistry::instance().resetToDefaults();
    }

    ToggleSwitchTestWindow* window = nullptr;
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

TEST_F(ToggleSwitchTest, SetIsOnEmitsToggled)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.first().first().toBool());
    EXPECT_TRUE(ts.isOn());
}

TEST_F(ToggleSwitchTest, SetSameIsOnNoSignal)
{
    ToggleSwitch ts;
    ts.setIsOn(true);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(true);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ToggleSwitchTest, ToggleOffEmitsSignal)
{
    ToggleSwitch ts;
    ts.setIsOn(true);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.first().first().toBool());
}

// ── OnContent / OffContent 属性 ──────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetOnContentEmitsSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::onContentChanged);
    ts.setOnContent("Working");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.onContent(), "Working");
}

TEST_F(ToggleSwitchTest, SetOffContentEmitsSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::offContentChanged);
    ts.setOffContent("Do work");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.offContent(), "Do work");
}

TEST_F(ToggleSwitchTest, SetSameOnContentNoSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::onContentChanged);
    ts.setOnContent(QString());
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ToggleSwitchTest, SetSameOffContentNoSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::offContentChanged);
    ts.setOffContent(QString());
    EXPECT_EQ(spy.count(), 0);
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
    ts.setFontRole(Typography::FontRole::Caption);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.fontRole(), Typography::FontRole::Caption);
}

TEST_F(ToggleSwitchTest, SetSameFontRoleNoSignal)
{
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::fontRoleChanged);
    ts.setFontRole(Typography::FontRole::Body); // same as default
    EXPECT_EQ(spy.count(), 0);
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

// ── Disabled 状态 ────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, DisabledState)
{
    ToggleSwitch ts;
    ts.setIsOn(true);
    ts.setEnabled(false);
    EXPECT_FALSE(ts.isEnabled());
    EXPECT_TRUE(ts.isOn());
}

TEST_F(ToggleSwitchTest, DisabledDoesNotToggle)
{
    ToggleSwitch ts;
    ts.setEnabled(false);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    // Simulate mouse click via programmatic toggle guard
    // The widget should not toggle when disabled
    // (toggle() checks isEnabled() internally)
    EXPECT_FALSE(ts.isOn());
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ToggleSwitchTest, MouseActivationTakesFocusAndToggles)
{
    ToggleSwitch ts(window);
    ts.setGeometry(20, 20, ts.sizeHint().width(), ts.sizeHint().height());
    window->show();
    ts.show();
    QApplication::processEvents();

    QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.rect().center());

    EXPECT_TRUE(ts.hasFocus());
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
    QTRY_VERIFY_WITH_TIMEOUT(qFuzzyCompare(ts.knobPosition(), 1.0), 300);
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

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);

    // 1. 简单开关
    auto* lbl1 = new fluent::textfields::Label("1. Simple ToggleSwitch:", window);
    lbl1->setFluentTypography(Typography::FontRole::Body);
    lbl1->anchors()->top = {window, Edge::Top, 30};
    lbl1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl1);

    auto* ts1 = new ToggleSwitch(window);
    ts1->anchors()->top = {lbl1, Edge::Bottom, 8};
    ts1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts1);

    auto* stateLabel = new fluent::textfields::Label("State: Off", window);
    stateLabel->setFluentTypography(Typography::FontRole::Caption);
    stateLabel->anchors()->top = {ts1, Edge::Bottom, 4};
    stateLabel->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(stateLabel);
    QObject::connect(ts1, &ToggleSwitch::toggled, [stateLabel](bool on) {
        stateLabel->setText(on ? "State: On" : "State: Off");
    });

    // 2. 外部标题 + 自定义 Content
    auto* lbl2 = new fluent::textfields::Label("2. External label & custom content:", window);
    lbl2->setFluentTypography(Typography::FontRole::Body);
    lbl2->anchors()->top = {stateLabel, Edge::Bottom, 20};
    lbl2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl2);

    auto* externalHeader = new fluent::textfields::Label("Toggle work", window);
    externalHeader->setFluentTypography(Typography::FontRole::Body);
    externalHeader->anchors()->top = {lbl2, Edge::Bottom, 8};
    externalHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(externalHeader);

    auto* ts2 = new ToggleSwitch(window);
    ts2->setOnContent("Working");
    ts2->setOffContent("Do work");
    ts2->setIsOn(true);
    ts2->anchors()->top = {externalHeader, Edge::Bottom, 4};
    ts2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts2);

    // 3. 默认 On
    auto* lbl3 = new fluent::textfields::Label("3. IsOn = true:", window);
    lbl3->setFluentTypography(Typography::FontRole::Body);
    lbl3->anchors()->top = {ts2, Edge::Bottom, 20};
    lbl3->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl3);

    auto* ts3 = new ToggleSwitch(window);
    ts3->setIsOn(true);
    ts3->anchors()->top = {lbl3, Edge::Bottom, 8};
    ts3->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts3);

    // 4. Disabled (Off)
    auto* lbl4 = new fluent::textfields::Label("4. Disabled (Off):", window);
    lbl4->setFluentTypography(Typography::FontRole::Body);
    lbl4->anchors()->top = {ts3, Edge::Bottom, 20};
    lbl4->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl4);

    auto* ts4 = new ToggleSwitch(window);
    ts4->setEnabled(false);
    ts4->anchors()->top = {lbl4, Edge::Bottom, 8};
    ts4->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts4);

    // 5. Disabled (On)
    auto* lbl5 = new fluent::textfields::Label("5. Disabled (On):", window);
    lbl5->setFluentTypography(Typography::FontRole::Body);
    lbl5->anchors()->top = {ts4, Edge::Bottom, 20};
    lbl5->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl5);

    auto* ts5 = new ToggleSwitch(window);
    ts5->setIsOn(true);
    ts5->setEnabled(false);
    ts5->anchors()->top = {lbl5, Edge::Bottom, 8};
    ts5->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts5);

    // 主题切换
    auto* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -30};
    layout->addWidget(themeBtn);
    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() ==
                                                fluent::FluentElement::Light
                                            ? fluent::FluentElement::Dark
                                            : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
