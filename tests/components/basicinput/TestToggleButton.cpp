#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include "components/basicinput/ToggleButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"

using namespace fluent;
using namespace fluent::basicinput;

class ToggleButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ToggleButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ToggleButtonTestWindow();
        window->setFixedSize(600, 600);
        window->setWindowTitle("Fluent ToggleButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic ToggleButton
        layout->addWidget(new QLabel("1. Basic ToggleButton:", window));
        auto* hLayout1 = new QHBoxLayout();
        auto* toggle1 = new ToggleButton("ToggleButton", window);
        auto* label1 = new QLabel("Output: Off", window);
        
        QObject::connect(toggle1, &ToggleButton::toggled, [label1](bool checked) {
            label1->setText(QString("Output: %1").arg(checked ? "On" : "Off"));
        });

        hLayout1->addWidget(toggle1);
        hLayout1->addWidget(label1);
        hLayout1->addStretch();
        layout->addLayout(hLayout1);

        // 2. Disabled ToggleButton
        layout->addWidget(new QLabel("2. Disabled ToggleButton:", window));
        auto* hLayout2 = new QHBoxLayout();
        auto* toggle2 = new ToggleButton("Disabled Off", window);
        toggle2->setEnabled(false);
        auto* toggle3 = new ToggleButton("Disabled On", window);
        toggle3->setChecked(true);
        toggle3->setEnabled(false);
        hLayout2->addWidget(toggle2);
        hLayout2->addWidget(toggle3);
        hLayout2->addStretch();
        layout->addLayout(hLayout2);

        // 3. Different Sizes
        layout->addWidget(new QLabel("3. Different Sizes:", window));
        auto* hLayout3 = new QHBoxLayout();
        auto* small = new ToggleButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new ToggleButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new ToggleButton("Large", window);
        large->setFluentSize(Button::Large);
        hLayout3->addWidget(small);
        hLayout3->addWidget(normal);
        hLayout3->addWidget(large);
        hLayout3->addStretch();
        layout->addLayout(hLayout3);

        // 4. ThreeState ToggleButton
        layout->addWidget(new QLabel("4. ThreeState ToggleButton (Unchecked -> Checked -> Indeterminate):", window));
        auto* hLayout4 = new QHBoxLayout();
        auto* toggle4 = new ToggleButton("ThreeState", window);
        toggle4->setThreeState(true);
        auto* label4 = new QLabel("State: Unchecked", window);
        
        QObject::connect(toggle4, &ToggleButton::checkStateChanged, [label4](Qt::CheckState state) {
            QString stateStr = "Unchecked";
            if (state == Qt::Checked) stateStr = "Checked";
            else if (state == Qt::PartiallyChecked) stateStr = "Indeterminate";
            label4->setText(QString("State: %1").arg(stateStr));
        });

        hLayout4->addWidget(toggle4);
        hLayout4->addWidget(label4);
        hLayout4->addStretch();
        layout->addLayout(hLayout4);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new QPushButton("Switch Theme", window);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &QPushButton::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light 
                                    ? fluent::FluentElement::Dark 
                                    : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    ToggleButtonTestWindow* window = nullptr;
};

// ─── Design-language × theme × checked compatibility ────────────────────────
//
// ToggleButton is a SELECTABLE button whose checked ("on") state must read as visibly distinct
// from unchecked under each design language (Fluent / Material 3 / macOS) crossed with each App
// theme (Light / Dark). Fluent is unchanged from Button (Standard→filled when checked); Material 3
// switches Outlined↔Filled-tonal; macOS switches neutral bezel↔accent fill. Design language + theme
// are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: ToggleButton 是可选按钮,其选中(「on」)态在三种设计语言(Fluent/Material 3/macOS)× 两种主题
//(Light/Dark)下都必须明显区别于未选中态。Fluent 与 Button 一致(选中时 Standard→填充);Material 3
// 在 描边↔填充色调 间切换;macOS 在 中性 bezel↔accent 填充 间切换。设计语言与主题为全局单例,
// 夹具在 TearDown 中恢复二者。
class ToggleButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a ToggleButton in the requested check state, size it like a real toolbar toggle,
    // and grab it as an image. zh_CN: 以指定选中态构建 ToggleButton,设定与真实工具栏一致的尺寸并抓取为图像。
    static QImage grabToggle(bool checked) {
        ToggleButton btn("Toggle");
        btn.setChecked(checked);
        btn.resize(140, 36);
        return btn.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    // Fraction-weighted difference: how many pixels differ between two same-size images.
    // zh_CN: 两张同尺寸图像之间存在差异的像素数量。
    static int differingPixels(const QImage& a, const QImage& b) {
        if (a.size() != b.size())
            return -1;
        int diff = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x)
                if (a.pixel(x, y) != b.pixel(x, y))
                    ++diff;
        return diff;
    }
};

TEST_F(ToggleButtonDesignLanguageTest, AllLanguagesThemesAndStatesPaintAndCheckedDiffersFromUnchecked) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const QImage unchecked = grabToggle(false);
            const QImage checked = grabToggle(true);

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // 1. Both states paint a valid, non-empty image with content. zh_CN: 两态都绘制出有效、非空且有内容的图像。
            ASSERT_FALSE(unchecked.isNull()) << ctx << "/unchecked";
            ASSERT_FALSE(checked.isNull()) << ctx << "/checked";
            EXPECT_GT(unchecked.width(), 0) << ctx;
            EXPECT_GT(unchecked.height(), 0) << ctx;
            EXPECT_GT(checked.width(), 0) << ctx;
            EXPECT_GT(checked.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(unchecked)) << "unchecked painted nothing: " << ctx;
            EXPECT_TRUE(hasPaintedContent(checked)) << "checked painted nothing: " << ctx;

            // 2. The toggle MUST be visibly distinct between checked and unchecked. zh_CN: 选中与未选中必须明显不同。
            const int diff = differingPixels(unchecked, checked);
            ASSERT_GE(diff, 0) << ctx << " (size mismatch)";
            EXPECT_GT(diff, 0)
                << "ToggleButton checked state is indistinguishable from unchecked: " << ctx;
        }
    }
}

// Regression for the invalid-QColor trap (a default-constructed QColor is INVALID yet
// QColor::alpha() returns 255, so a bare alpha()>0 guard + setBrush(invalidColor) paints SOLID
// OPAQUE BLACK). The macOS branch fills a real bezel/accent surface, so neither state may render an
// opaque near-#000 center at rest. zh_CN: 无效 QColor 陷阱回归(默认构造的 QColor 无效却返回 alpha==255,
// 裸 alpha()>0 + setBrush(无效色) 会涂成不透明纯黑)。macOS 分支绘制真实 bezel/accent 表面,
// 故任一态在静息下都不得呈现不透明近黑中心。
TEST_F(ToggleButtonDesignLanguageTest, CupertinoRestStateHasNoOpaqueBlackSurface) {
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);

    for (auto theme : themes) {
        fluent::FluentElement::setTheme(theme);

        for (bool checked : {false, true}) {
            // Build a fresh, never-interacted (REST) toggle with EMPTY text so the sampled center lands
            // on the bare surface fill, not a glyph. (A checked button's on-accent label is legitimately
            // dark when the accent is light — e.g. the default Fluent-seeded textOnAccent is near-black on
            // the light-blue accent — so sampling text would be a false positive for "opaque black fill".)
            // zh_CN: 用空文本构建静息切换,使采样中心落在纯表面填充而非字形上(选中态 on-accent 文字在浅 accent 上
            // 合法地为深色——如默认 Fluent textOnAccent 在浅蓝 accent 上近黑——采样到文字会误报"不透明黑填充")。
            ToggleButton btn("");
            btn.setChecked(checked);
            btn.resize(140, 36);
            const QImage img = btn.grab().toImage();
            ASSERT_FALSE(img.isNull()) << "theme=" << theme << " checked=" << checked;

            const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "macOS ToggleButton painted an opaque black surface at rest: theme=" << theme
                << " checked=" << checked << " rgba=(" << c.red() << "," << c.green() << ","
                << c.blue() << "," << c.alpha() << ")";
        }
    }
}

TEST_F(ToggleButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
