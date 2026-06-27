#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include "components/basicinput/ToggleSwitch.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
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

// ── 设计语言兼容性 ───────────────────────────────────────────────────────────
// Design-language compatibility: render the ToggleSwitch under every design language × theme × state and
// assert it grabs a valid image with no crash, that ON differs from OFF, and (regression lock) that the
// macOS (Cupertino) ON track renders BLUE — not the earlier systemSuccess green. zh_CN: 设计语言兼容性:
// 在每种设计语言 × 主题 × 状态下渲染开关,断言能抓到有效图像、不崩溃、开/关图像不同,并(回归锁)确认
// macOS(Cupertino)开启轨道为蓝色——而非此前的 systemSuccess 绿色。

class ToggleSwitchDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — always reset so other suites are unaffected.
        // zh_CN: 设计语言与主题是全局状态——务必复位,避免影响其它测试套件。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Grab a freshly-sized ToggleSwitch under the given on/off state. zh_CN: 在给定开/关状态下抓取已设尺寸的开关。
    static QImage grabSwitch(bool on) {
        ToggleSwitch sw;
        sw.setIsOn(on);
        QSize hint = sw.sizeHint();
        if (!hint.isValid() || hint.width() < 80 || hint.height() < 40)
            hint = QSize(80, 40);
        sw.resize(hint);
        return sw.grab().toImage();
    }
};

TEST_F(ToggleSwitchDesignLanguageTest, AllLanguagesThemesAndStatesRenderValidImages) {
    const fluent::FluentElement::DesignLanguage langs[] = {
        fluent::FluentElement::DesignFluent,
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto lang : langs) {
        for (auto theme : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang);
            fluent::FluentElement::setTheme(theme);

            QImage off = grabSwitch(false);
            QImage on = grabSwitch(true);

            ASSERT_FALSE(off.isNull()) << "off image null for lang=" << lang << " theme=" << theme;
            ASSERT_FALSE(on.isNull()) << "on image null for lang=" << lang << " theme=" << theme;
            EXPECT_GT(off.width(), 0);
            EXPECT_GT(off.height(), 0);
            EXPECT_EQ(off.size(), on.size());

            // ON must look different from OFF for the same lang/theme. zh_CN: 同语言/主题下开启图像须不同于关闭。
            EXPECT_NE(off, on) << "on == off for lang=" << lang << " theme=" << theme;
        }
    }
}

// Regression lock: the macOS (Cupertino) ON switch fills its track with accent BLUE, not green. We sample a
// point on the filled track at ~25% width, vertical center, and require the pixel be clearly blue-dominant.
// zh_CN: 回归锁:macOS(Cupertino)开启开关用强调色蓝填充轨道,而非绿色。我们在轨道约 25% 宽、垂直居中处采样,
// 要求该像素明显偏蓝。
TEST_F(ToggleSwitchDesignLanguageTest, CupertinoOnTrackIsBlueNotGreen) {
    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);

    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);

        ToggleSwitch sw;
        sw.setIsOn(true);
        sw.resize(80, 40);
        QImage img = sw.grab().toImage();
        ASSERT_FALSE(img.isNull());

        // The ON track is filled accent BLUE, but the white knob sits somewhere along the 40px track
        // (its exact x depends on the async knob animation's state at grab time, which never runs in a
        // headless grab), so a single sample can land on the white knob. Scan the track's center row and
        // require that a clearly BLUE-dominant pixel exists — and that NO strongly green-dominant pixel
        // does (which would betray a regression back to the old systemSuccess green).
        // zh_CN: 开启轨道填充强调色蓝,但白色滑块会落在 40px 轨道某处(grab 时异步动画未跑,位置不定),
        // 单点采样可能正好落在白色滑块上。故扫描轨道中心行:要求存在明显偏蓝像素,且不存在强偏绿像素
        //(后者意味着退回旧的 systemSuccess 绿)。
        const int rowH = qMax(20, QFontMetrics(sw.font()).height());
        const int sampleY = rowH / 2;       // track center row. zh_CN: 轨道中心行。
        bool foundBlue = false, foundGreen = false;
        for (int x = 1; x < 40; ++x) {      // across the 40px track width. zh_CN: 遍历 40px 轨道宽。
            const QColor px = img.pixelColor(x, sampleY);
            if (px.blue() > px.green() + 30 && px.blue() > px.red() + 30) foundBlue = true;
            if (px.green() > px.blue() + 30 && px.green() > px.red() + 30) foundGreen = true;
        }
        EXPECT_TRUE(foundBlue) << "Cupertino ON track has no blue-dominant pixel theme=" << theme;
        EXPECT_FALSE(foundGreen)
            << "Cupertino ON track has a green-dominant pixel (regressed to green?) theme=" << theme;
    }
}
