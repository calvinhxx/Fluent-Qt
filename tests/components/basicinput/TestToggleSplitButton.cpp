#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include "components/menus_toolbars/Menu.h"
#include "components/basicinput/ToggleSplitButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Typography.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::menus_toolbars;

class ToggleSplitButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class ToggleSplitButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new ToggleSplitButtonTestWindow();
        window->setFixedSize(600, 400);
        window->setWindowTitle("Fluent ToggleSplitButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic ToggleSplitButton
        layout->addWidget(new QLabel("1. ToggleSplitButton (Icon + Text):", window));
        auto* toggleSplit = new ToggleSplitButton("List Options", window);
        
        // 设置图标和布局
        toggleSplit->setIconGlyph(Typography::Icons::List, Typography::FontSize::Body);
        toggleSplit->setFluentLayout(Button::IconBefore);
        toggleSplit->setFixedSize(160, 32);

        // 设置菜单
        FluentMenu* menu = new FluentMenu("Styles", toggleSplit);
        menu->addAction(new FluentMenuItem("None", menu));
        menu->addAction(new FluentMenuItem("Bulleted", menu));
        menu->addAction(new FluentMenuItem("Numbered", menu));
        toggleSplit->setMenu(menu);
        
        QLabel* status = new QLabel("State: Unchecked", window);
        QObject::connect(toggleSplit, &ToggleSplitButton::toggled, [status](bool checked) {
            status->setText(QString("State: %1").arg(checked ? "Checked" : "Unchecked"));
        });
        
        layout->addWidget(toggleSplit);
        layout->addWidget(status);

        // 2. Icon-only ToggleSplitButton
        layout->addWidget(new QLabel("2. Icon-only ToggleSplitButton:", window));
        auto* iconOnly = new ToggleSplitButton("", window);
        iconOnly->setIconGlyph(Typography::Icons::Settings, Typography::FontSize::Body);
        iconOnly->setFluentLayout(Button::IconOnly);
        iconOnly->setFixedSize(64, 32);
        layout->addWidget(iconOnly);

        layout->addStretch();

        // Theme switch button
        auto* themeBtn = new Button("Switch Theme", window);
        themeBtn->setFixedSize(120, 32);
        layout->addWidget(themeBtn);
        QObject::connect(themeBtn, &Button::clicked, []() {
            fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light 
                                    ? fluent::FluentElement::Dark 
                                    : fluent::FluentElement::Light);
        });

        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    ToggleSplitButtonTestWindow* window = nullptr;
};

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// ToggleSplitButton SUBCLASSES SplitButton and has NO own paintEvent — it INHERITS SplitButton's
// per-brand split painting verbatim. This suite verifies the inherited paint renders sensibly across
// {Fluent / Material 3 / macOS} × {Light / Dark}, in both the unchecked and the toggled-on (checked)
// states (checked routes through the accent-like / filled branch). Design language + theme are GLOBAL,
// so TearDown restores the built-in defaults. zh_CN: ToggleSplitButton 继承自 SplitButton 且无自有
// paintEvent——逐字继承其按品牌的拆分绘制。本套件验证继承的绘制在 {Fluent / M3 / macOS} × {明/暗} 下
// 于未选中与选中(选中走 accent-like/填充分支)两态都能合理渲染。设计语言与主题是全局,故 TearDown 恢复默认。

class ToggleSplitButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static QImage grabToggle(bool checked) {
        ToggleSplitButton tsb("Options");
        tsb.setChecked(checked);
        tsb.resize(160, 36);
        return tsb.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }
};

TEST_F(ToggleSplitButtonDesignLanguageTest, InheritedPaintRendersAcrossLanguagesThemesAndCheckedStates) {
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
            for (bool checked : {false, true}) {
                fluent::ThemeRegistry::instance().setDesignLanguage(lang);
                fluent::FluentElement::setTheme(theme);

                QImage img = grabToggle(checked);

                ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme << " checked=" << checked;
                EXPECT_GT(img.width(), 0) << "lang=" << lang << " theme=" << theme << " checked=" << checked;
                EXPECT_GT(img.height(), 0) << "lang=" << lang << " theme=" << theme << " checked=" << checked;
                EXPECT_TRUE(hasPaintedContent(img))
                    << "ToggleSplitButton painted nothing for lang=" << lang << " theme=" << theme
                    << " checked=" << checked;
            }
        }
    }
}

// macOS bezel rest-state must not paint an opaque near-#000 surface (invalid-QColor trap).
// zh_CN: macOS bezel 静息态不得绘制不透明近黑表面(无效 QColor 陷阱)。
TEST_F(ToggleSplitButtonDesignLanguageTest, MacOsRestStateHasNoOpaqueBlackSurface) {
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto theme : themes) {
        fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);
        fluent::FluentElement::setTheme(theme);

        QImage img = grabToggle(false);
        ASSERT_FALSE(img.isNull()) << "theme=" << theme;

        const QColor c = img.pixelColor(img.width() / 4, img.height() / 2);
        const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
        const bool opaqueBlack = c.alpha() > 200 && lum < 16;
        EXPECT_FALSE(opaqueBlack)
            << "ToggleSplitButton painted an opaque black surface at rest for theme=" << theme
            << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
    }
}

TEST_F(ToggleSplitButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
