#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include "components/basicinput/RepeatButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"

using namespace fluent;
using namespace fluent::basicinput;

class RepeatButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class RepeatButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new RepeatButtonTestWindow();
        window->setFixedSize(600, 400);
        window->setWindowTitle("Fluent RepeatButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic RepeatButton with counter
        layout->addWidget(new QLabel("1. Click and hold to increment counter:", window));
        
        auto* hLayout = new QHBoxLayout();
        auto* repeatBtn = new RepeatButton("Click and hold", window);
        auto* countLabel = new QLabel("Number of clicks: 0", window);
        
        static int count = 0;
        QObject::connect(repeatBtn, &RepeatButton::clicked, [countLabel]() {
            count++;
            countLabel->setText(QString("Number of clicks: %1").arg(count));
        });

        hLayout->addWidget(repeatBtn);
        hLayout->addWidget(countLabel);
        hLayout->addStretch();
        layout->addLayout(hLayout);

        // 2. Different Intervals
        layout->addWidget(new QLabel("2. Fast repeat (Interval: 20ms):", window));
        auto* fastBtn = new RepeatButton("Fast Repeat", window);
        fastBtn->setInterval(20);
        layout->addWidget(fastBtn);

        // 3. Different Styles (inherits from Button)
        layout->addWidget(new QLabel("3. Accent Style RepeatButton:", window));
        auto* accentBtn = new RepeatButton("Accent Repeat", window);
        accentBtn->setFluentStyle(Button::Accent);
        layout->addWidget(accentBtn);

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

    RepeatButtonTestWindow* window = nullptr;
};

TEST_F(RepeatButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}

// ─── Design-language × theme inheritance lock-in ────────────────────────────
//
// RepeatButton is `public Button`, so it inherits Button's per-design-language paintEvent (Fluent
// pill/Material state-layer/macOS bezel) for free. This suite locks that in: every language × theme
// must paint a valid, non-empty surface and never the invalid-QColor opaque-black trap. Design
// language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: RepeatButton 继承自 Button,自动获得 Button 的逐设计语言 paintEvent(Fluent 胶囊 / Material
// state layer / macOS bezel)。本套件锁定这一继承:每种语言×主题都必须绘制有效非空表面,且绝不出现
// 无效-QColor 的不透明纯黑陷阱。设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class RepeatButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    static bool isOpaqueBlack(const QColor& c) {
        const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
        return c.alpha() > 200 && lum < 16;
    }
};

TEST_F(RepeatButtonDesignLanguageTest, InheritedPaintRendersAcrossLanguagesAndThemes) {
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

            for (auto style : {Button::Standard, Button::Accent}) {
                RepeatButton btn(QStringLiteral("Hold"));
                btn.setFluentStyle(style);
                btn.resize(140, 36);
                const QImage img = btn.grab().toImage();

                const std::string ctx = std::string(lang.name) + "/" + th.name
                                      + (style == Button::Accent ? "/Accent" : "/Standard");
                ASSERT_FALSE(img.isNull()) << ctx;
                EXPECT_GT(img.width(), 0) << ctx;
                EXPECT_TRUE(hasPaintedContent(img)) << "painted nothing: " << ctx;

                // Empty-text variant so the sampled center lands on the bare surface, not a glyph.
                // zh_CN: 空文本变体,使采样中心落在纯表面而非字形上。
                RepeatButton bare{QString()};
                bare.setFluentStyle(style);
                bare.resize(140, 36);
                const QImage bareImg = bare.grab().toImage();
                const QColor center = bareImg.pixelColor(bareImg.width() / 2, bareImg.height() / 2);
                EXPECT_FALSE(isOpaqueBlack(center))
                    << "opaque-black surface at rest: " << ctx << " rgba=(" << center.red() << ","
                    << center.green() << "," << center.blue() << "," << center.alpha() << ")";
            }
        }
    }
}
