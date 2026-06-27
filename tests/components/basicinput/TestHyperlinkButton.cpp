#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QImage>
#include <QColor>
#include "components/basicinput/HyperlinkButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"

using namespace fluent;
using namespace fluent::basicinput;

class HyperlinkButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class HyperlinkButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new HyperlinkButtonTestWindow();
        window->setFixedSize(600, 800);
        window->setWindowTitle("Fluent HyperlinkButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic HyperlinkButton
        layout->addWidget(new QLabel("1. Basic HyperlinkButton:", window));
        auto* btn1 = new HyperlinkButton("Microsoft home page", window);
        btn1->setUrl(QUrl("https://www.microsoft.com"));
        layout->addWidget(btn1);

        // 2. HyperlinkButton with different sizes
        layout->addWidget(new QLabel("2. Different Sizes:", window));
        auto* h1 = new QHBoxLayout();
        auto* small = new HyperlinkButton("Small", window);
        small->setFluentSize(Button::Small);
        auto* normal = new HyperlinkButton("Standard", window);
        normal->setFluentSize(Button::StandardSize);
        auto* large = new HyperlinkButton("Large", window);
        large->setFluentSize(Button::Large);
        h1->addWidget(small);
        h1->addWidget(normal);
        h1->addWidget(large);
        h1->addStretch();
        layout->addLayout(h1);

        // 3. HyperlinkButton with click handler
        layout->addWidget(new QLabel("3. Click to navigate (Check Console):", window));
        auto* navBtn = new HyperlinkButton("Go to GitHub", QUrl("https://github.com"), window);
        layout->addWidget(navBtn);

        // 4. Disabled state
        layout->addWidget(new QLabel("4. Disabled state (No underline by default):", window));
        auto* disabledBtn = new HyperlinkButton("Disabled link", window);
        disabledBtn->setEnabled(false);
        layout->addWidget(disabledBtn);

        // 5. With underline (Explicitly enabled)
        layout->addWidget(new QLabel("5. With underline on hover (Explicitly enabled):", window));
        auto* withUnderline = new HyperlinkButton("Underline on hover", window);
        withUnderline->setShowUnderline(true); 
        layout->addWidget(withUnderline);

        // 6. Default behavior (No underline)
        layout->addWidget(new QLabel("6. Default behavior (No underline on hover):", window));
        auto* defaultBtn = new HyperlinkButton("Default no underline", window);
        layout->addWidget(defaultBtn);

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

    HyperlinkButtonTestWindow* window = nullptr;
};

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// HyperlinkButton paints a TEXT LINK per brand: Fluent (subtle-fill text link), Material 3
// (accent text + accent state-layer pill on hover/press), macOS (restrained accent link with a
// hover underline). A resting hyperlink has NO fill, so this suite grabs the control across the
// full {language × theme} matrix and asserts (1) it paints a valid sized image with ink on it,
// and (2) the resting CENTER pixel is never an opaque near-#000 fill — the signature of the
// invalid-QColor "solid black" trap (alpha()==255 on an unassigned QColor). Design language +
// theme are GLOBAL singletons, so TearDown restores defaults.
// zh_CN: HyperlinkButton 按品牌绘制文本链接:Fluent(subtle 填充文本链接)、Material 3(强调色文字 +
// hover/press 强调色 state-layer 胶囊)、macOS(克制强调链接 + hover 下划线)。静息超链接无填充,本套件
// 遍历 {设计语言 × 主题} 全矩阵,断言:(1) 渲染出有效尺寸且确有像素的图像;(2) 静息中心像素绝非不透明近黑
// 填充——即未赋值 QColor 的 alpha()==255「涂黑」陷阱特征。设计语言与主题为全局单例,故 TearDown 复位默认值。

class HyperlinkButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题为全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a hyperlink, size it ~120x32, and grab it at REST as an image.
    // zh_CN: 构建超链接,设定 ~120x32 尺寸,在静息态抓取为图像。
    static QImage grabLink() {
        HyperlinkButton link("Fluent QT home page");
        link.setUrl(QUrl("https://example.com"));
        link.resize(120, 32);
        return link.grab().toImage();
    }
};

TEST_F(HyperlinkButtonDesignLanguageTest, AllLanguagesThemesPaintAccentTextWithoutBlackFill) {
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

    for (const auto& l : langs) {
        for (const auto& t : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(l.lang);
            fluent::FluentElement::setTheme(t.theme);

            const QImage img = grabLink();
            const std::string ctx = std::string(l.name) + "/" + t.name;

            // 1. No crash + valid image of the requested size. zh_CN: 不崩溃 + 图像有效且尺寸正确。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;

            // 2. Painted content: a hyperlink always draws accent text, so some pixel must differ
            // from the top-left background pixel. zh_CN: 已绘制内容:超链接始终绘制强调色文字,必有
            // 像素不同于左上角背景。
            const QRgb bg = img.pixel(0, 0);
            bool painted = false;
            for (int y = 0; y < img.height() && !painted; ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    if (img.pixel(x, y) != bg) {
                        painted = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(painted) << "HyperlinkButton painted nothing: " << ctx;

            // 3. Resting hyperlink has NO fill: the center pixel must NOT be an opaque near-#000
            // fill (the invalid-QColor solid-black signature). zh_CN: 静息超链接无填充:中心像素
            // 绝非不透明近黑填充(无效 QColor 涂黑特征)。
            const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
            const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool opaqueBlack = c.alpha() > 200 && lum < 16;
            EXPECT_FALSE(opaqueBlack)
                << "HyperlinkButton painted an opaque black fill at rest: " << ctx
                << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << ","
                << c.alpha() << ")";
        }
    }
}

TEST_F(HyperlinkButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
