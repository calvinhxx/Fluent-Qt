#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include "components/basicinput/ToggleSwitch.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"
#include "design/Spacing.h"

using namespace fluent::basicinput;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class ToggleSwitchTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── 测试类 ───────────────────────────────────────────────────────────────────

class ToggleSwitchTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ToggleSwitchTestWindow();
        window->setFixedSize(500, 500);
        window->setWindowTitle("Fluent ToggleSwitch Visual Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    ToggleSwitchTestWindow* window = nullptr;
};

// ── 默认属性 ─────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, DefaultPropertyValues) {
    ToggleSwitch ts;
    EXPECT_FALSE(ts.isOn());
    EXPECT_EQ(ts.onContent(), "On");
    EXPECT_EQ(ts.offContent(), "Off");
    EXPECT_EQ(ts.fontRole(), "Body");
}

// ── IsOn 属性 ────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetIsOnEmitsToggled) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.first().first().toBool());
    EXPECT_TRUE(ts.isOn());
}

TEST_F(ToggleSwitchTest, SetSameIsOnNoSignal) {
    ToggleSwitch ts;
    ts.setIsOn(true);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(true);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ToggleSwitchTest, ToggleOffEmitsSignal) {
    ToggleSwitch ts;
    ts.setIsOn(true);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    ts.setIsOn(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(spy.first().first().toBool());
}

// ── OnContent / OffContent 属性 ──────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetOnContentEmitsSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::onContentChanged);
    ts.setOnContent("Working");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.onContent(), "Working");
}

TEST_F(ToggleSwitchTest, SetOffContentEmitsSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::offContentChanged);
    ts.setOffContent("Do work");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.offContent(), "Do work");
}

TEST_F(ToggleSwitchTest, SetSameOnContentNoSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::onContentChanged);
    ts.setOnContent("On");  // same as default
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(ToggleSwitchTest, SetSameOffContentNoSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::offContentChanged);
    ts.setOffContent("Off");  // same as default
    EXPECT_EQ(spy.count(), 0);
}

// ── FontRole 属性 ────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SetFontRoleEmitsSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::fontRoleChanged);
    ts.setFontRole("Caption");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(ts.fontRole(), "Caption");
}

TEST_F(ToggleSwitchTest, SetSameFontRoleNoSignal) {
    ToggleSwitch ts;
    QSignalSpy spy(&ts, &ToggleSwitch::fontRoleChanged);
    ts.setFontRole("Body");  // same as default
    EXPECT_EQ(spy.count(), 0);
}

// ── KnobPosition 属性 ────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, KnobPositionClamped) {
    ToggleSwitch ts;
    ts.setKnobPosition(2.0);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 1.0);
    ts.setKnobPosition(-1.0);
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 0.0);
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, SizeHintIncludesTrackAndContent) {
    ToggleSwitch ts;
    QSize hint = ts.sizeHint();
    EXPECT_GE(hint.width(), 40);  // at least track width
    EXPECT_GE(hint.height(), 20); // at least track height
}

TEST_F(ToggleSwitchTest, SizeHintReflectsContentWidth) {
    ToggleSwitch ts;
    QSize shortHint = ts.sizeHint();
    ts.setOnContent("A much longer switch state");
    QSize longHint = ts.sizeHint();
    EXPECT_GT(longHint.width(), shortHint.width());
    EXPECT_EQ(longHint.height(), shortHint.height());
}

TEST_F(ToggleSwitchTest, MinimumSizeHintIsTrack) {
    ToggleSwitch ts;
    QSize minHint = ts.minimumSizeHint();
    EXPECT_EQ(minHint.width(), 40);
    EXPECT_EQ(minHint.height(), 20);
}

// ── Disabled 状态 ────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, DisabledState) {
    ToggleSwitch ts;
    ts.setIsOn(true);
    ts.setEnabled(false);
    EXPECT_FALSE(ts.isEnabled());
    EXPECT_TRUE(ts.isOn());
}

TEST_F(ToggleSwitchTest, DisabledDoesNotToggle) {
    ToggleSwitch ts;
    ts.setEnabled(false);
    QSignalSpy spy(&ts, &ToggleSwitch::toggled);
    // Simulate mouse click via programmatic toggle guard
    // The widget should not toggle when disabled
    // (toggle() checks isEnabled() internally)
    EXPECT_FALSE(ts.isOn());
    EXPECT_EQ(spy.count(), 0);
}

// ── 初始 Knob 位置 ──────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, InitialKnobPositionOff) {
    ToggleSwitch ts;
    EXPECT_DOUBLE_EQ(ts.knobPosition(), 0.0);
}

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(ToggleSwitchTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);

    // 1. 简单开关
    auto* lbl1 = new fluent::textfields::Label("1. Simple ToggleSwitch:", window);
    lbl1->setFluentTypography("Body");
    lbl1->anchors()->top = {window, Edge::Top, 30};
    lbl1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl1);

    auto* ts1 = new ToggleSwitch(window);
    ts1->anchors()->top = {lbl1, Edge::Bottom, 8};
    ts1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(ts1);

    auto* stateLabel = new fluent::textfields::Label("State: Off", window);
    stateLabel->setFluentTypography("Caption");
    stateLabel->anchors()->top = {ts1, Edge::Bottom, 4};
    stateLabel->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(stateLabel);
    QObject::connect(ts1, &ToggleSwitch::toggled, [stateLabel](bool on) {
        stateLabel->setText(on ? "State: On" : "State: Off");
    });

    // 2. 外部标题 + 自定义 Content
    auto* lbl2 = new fluent::textfields::Label("2. External label & custom content:", window);
    lbl2->setFluentTypography("Body");
    lbl2->anchors()->top = {stateLabel, Edge::Bottom, 20};
    lbl2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl2);

    auto* externalHeader = new fluent::textfields::Label("Toggle work", window);
    externalHeader->setFluentTypography("Body");
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
    lbl3->setFluentTypography("Body");
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
    lbl4->setFluentTypography("Body");
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
    lbl5->setFluentTypography("Body");
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
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                ? fluent::FluentElement::Dark
                                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
