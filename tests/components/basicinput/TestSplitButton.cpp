#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include <QVariantAnimation>
#include <QTest>
#include "components/menus_toolbars/Menu.h"
#include "components/basicinput/SplitButton.h"
#include "components/basicinput/ToggleSplitButton.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Typography.h"
#include "design/Spacing.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::menus_toolbars;

class SplitButtonTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class SplitButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new SplitButtonTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("Fluent SplitButton Visual Test");

        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(20);

        // 1. Basic SplitButton (Standard)
        layout->addWidget(new QLabel("1. Basic SplitButton (Standard):", window));
        auto* split1 = new SplitButton("Choose Color", window);
        
        // 使用自定义 FluentMenu
        FluentMenu* menu1 = new FluentMenu("Colors", split1);
        menu1->addAction(new FluentMenuItem("Red", menu1));
        menu1->addAction(new FluentMenuItem("Green", menu1));
        menu1->addAction(new FluentMenuItem("Blue", menu1));
        split1->setMenu(menu1);
        
        QLabel* status1 = new QLabel("Status: Ready", window);
        QObject::connect(split1, &SplitButton::clicked, [status1]() {
            status1->setText("Status: Primary Clicked!");
        });
        
        layout->addWidget(split1);
        layout->addWidget(status1);

        // 2. Accent SplitButton
        layout->addWidget(new QLabel("2. Accent SplitButton:", window));
        auto* split2 = new SplitButton("Submit", window);
        split2->setFluentStyle(Button::Accent);
        
        FluentMenu* menu2 = new FluentMenu("Actions", split2);
        menu2->addAction(new FluentMenuItem("Submit and close", menu2));
        menu2->addAction(new FluentMenuItem("Submit and notify", menu2));
        split2->setMenu(menu2);
        layout->addWidget(split2);

        // 3. Different Sizes
        layout->addWidget(new QLabel("3. Different Sizes:", window));
        auto* h1 = new QHBoxLayout();
        auto* sSmall = new SplitButton("Small", window);
        sSmall->setFluentSize(Button::Small);
        auto* sNormal = new SplitButton("Standard", window);
        sNormal->setFluentSize(Button::StandardSize);
        auto* sLarge = new SplitButton("Large", window);
        sLarge->setFluentSize(Button::Large);
        h1->addWidget(sSmall);
        h1->addWidget(sNormal);
        h1->addWidget(sLarge);
        h1->addStretch();
        layout->addLayout(h1);

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

    SplitButtonTestWindow* window = nullptr;
};

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// SplitButton paints a per-brand split surface (action segment + divider + chevron segment) under
// each App theme. The Fluent path is unchanged; Material 3 follows Button's M3 treatment for the
// active Style and macOS paints a bezel push-button. The split GEOMETRY/hit-testing is identical
// across languages — only the painted surface differs. This suite grabs the rendered control across
// the full {language × theme} matrix to lock in that every combination paints, never crashes, and
// actually puts ink on the surface. Design language + theme are GLOBAL state, so TearDown restores
// the built-in defaults. zh_CN: SplitButton 按品牌绘制拆分表面(操作段 + 分割线 + 箭头段),Fluent 不变,
// M3 沿用 Button 的 M3 处理,macOS 为 bezel 按钮;拆分几何/命中各语言一致,仅表面不同。本套件遍历
// {设计语言 × 主题} 全矩阵抓取渲染结果。设计语言与主题是全局状态,故 TearDown 恢复内置默认值。

class SplitButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题是全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a SplitButton with the given style, size it, and grab it as an image.
    // zh_CN: 构建指定风格的 SplitButton,设定尺寸并抓取为图像。
    static QImage grabSplit(Button::ButtonStyle style) {
        SplitButton sb("Choose");
        sb.setFluentStyle(style);
        sb.resize(160, 36);
        return sb.grab().toImage();
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

TEST_F(SplitButtonDesignLanguageTest, AllLanguagesThemesPaintAndPutInkOnSurface) {
    const fluent::FluentElement::DesignLanguage langs[] = {
        fluent::FluentElement::DesignFluent,
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };
    const Button::ButtonStyle styles[] = { Button::Standard, Button::Accent };

    for (auto lang : langs) {
        for (auto theme : themes) {
            for (auto style : styles) {
                fluent::ThemeRegistry::instance().setDesignLanguage(lang);
                fluent::FluentElement::setTheme(theme);

                QImage img = grabSplit(style);

                // No crash + valid image of the requested size. zh_CN: 不崩溃 + 图像有效且尺寸正确。
                ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme << " style=" << style;
                EXPECT_GT(img.width(), 0) << "lang=" << lang << " theme=" << theme << " style=" << style;
                EXPECT_GT(img.height(), 0) << "lang=" << lang << " theme=" << theme << " style=" << style;

                // Painted content: divider, text, or chevron all qualify. zh_CN: 已绘制内容:分割线、文字或箭头均可。
                EXPECT_TRUE(hasPaintedContent(img))
                    << "SplitButton painted nothing for lang=" << lang << " theme=" << theme
                    << " style=" << style;
            }
        }
    }
}

// Regression for the invalid-QColor trap: a default-constructed QColor is INVALID yet
// QColor::alpha() returns 255, so a bare `alpha() > 0` guard would fire at rest and
// setBrush(invalidColor) paints SOLID OPAQUE BLACK. The macOS bezel must paint its real surface
// (a light/dark-gray bgLayerAlt), never an opaque near-#000 field, when never hovered/pressed.
// zh_CN: 无效 QColor 陷阱回归:默认构造 QColor 无效但 alpha() 返回 255,裸 alpha()>0 在静息命中,
// setBrush(无效色) 涂成不透明纯黑。macOS bezel 静息须绘制真实表面(浅/深灰 bgLayerAlt),绝非不透明近黑。
TEST_F(SplitButtonDesignLanguageTest, MacOsRestStateHasNoOpaqueBlackSurface) {
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto theme : themes) {
        fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);
        fluent::FluentElement::setTheme(theme);

        // Fresh SplitButton, never hovered/pressed → REST state, exactly the condition that exposed
        // the bug. zh_CN: 全新 SplitButton,从未交互=静息态,正是触发 bug 的条件。
        QImage img = grabSplit(Button::Standard);
        ASSERT_FALSE(img.isNull()) << "theme=" << theme;

        // Sample the center of the action segment (left of the divider). zh_CN: 取操作段中心(分割线左侧)。
        const QColor c = img.pixelColor(img.width() / 4, img.height() / 2);
        const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
        const bool opaqueBlack = c.alpha() > 200 && lum < 16;
        EXPECT_FALSE(opaqueBlack)
            << "SplitButton painted an opaque black surface at rest for theme=" << theme
            << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
    }
}

static int rightmostInkColumn(const QImage& img, QRgb bg, int xMin, int xMax)
{
    for (int x = xMax; x >= xMin; --x) {
        for (int y = 0; y < img.height(); ++y) {
            if (img.pixel(x, y) != bg)
                return x;
        }
    }
    return xMin;
}

TEST(SplitButtonLayoutTest, ListOptionsTextDoesNotCrowdDivider) {
    ToggleSplitButton button(QStringLiteral("List options"));
    button.setFluentLayout(Button::IconBefore);
    button.setIconGlyph(Typography::Icons::List, Typography::IconSize::Standard);
    button.setMinimumWidth(160);
    button.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&button));
    button.adjustSize();

    const QImage img = button.grab().toImage();
    ASSERT_FALSE(img.isNull());

    const QRgb bg = img.pixel(0, 0);
    const int dividerX = button.width() - button.secondaryWidth();
    const int layoutRight = dividerX - ::Spacing::Gap::Normal - 1;
    const int textRight = rightmostInkColumn(img, bg, 0, layoutRight);
    const int gapPx = dividerX - textRight - 1;

    EXPECT_GE(gapPx, ::Spacing::Gap::Normal)
        << "textRight=" << textRight << " dividerX=" << dividerX
        << " width=" << button.width()
        << " gapPx=" << gapPx;
    EXPECT_GT(button.width(), 160);
}

TEST(SplitButtonLayoutTest, IconOnlyCentersGlyphInPrimaryZone) {
    ToggleSplitButton button;
    button.setFluentLayout(Button::IconOnly);
    button.setIconGlyph(Typography::Icons::Settings, Typography::IconSize::Standard);
    button.setFixedSize(64, 34);
    button.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&button));

    const QImage img = button.grab().toImage();
    ASSERT_FALSE(img.isNull());

    const QRgb bg = img.pixel(0, 0);
    const int primaryRight = button.width() - button.secondaryWidth() - 1;
    int left = primaryRight;
    int right = 0;
    for (int x = 0; x <= primaryRight; ++x) {
        for (int y = 0; y < img.height(); ++y) {
            if (img.pixel(x, y) == bg)
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
        }
    }
    ASSERT_LT(left, right);
    const int inkCenter = (left + right) / 2;
    const int primaryCenter = primaryRight / 2;
    EXPECT_NEAR(inkCenter, primaryCenter, 2)
        << "left=" << left << " right=" << right
        << " inkCenter=" << inkCenter << " primaryCenter=" << primaryCenter;
}

TEST_F(SplitButtonTest, BothSegmentsStartPressReboundAnimation) {
    SplitButton split(QStringLiteral("Choose"));
    split.resize(160, 36);
    split.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&split));

    auto* animation = split.findChild<QVariantAnimation*>();
    ASSERT_NE(animation, nullptr);

    const QPoint primaryPoint(split.width() / 4, split.height() / 2);
    QTest::mousePress(&split, Qt::LeftButton, Qt::NoModifier, primaryPoint);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    QTRY_VERIFY_WITH_TIMEOUT(animation->currentValue().toReal() > 0.0, 300);
    QTest::mouseRelease(&split, Qt::LeftButton, Qt::NoModifier, primaryPoint);

    const QPoint secondaryPoint(split.width() - 8, split.height() / 2);
    QTest::mousePress(&split, Qt::LeftButton, Qt::NoModifier, secondaryPoint);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    QTRY_VERIFY_WITH_TIMEOUT(animation->currentValue().toReal() > 0.0, 300);
    QTest::mouseRelease(&split, Qt::LeftButton, Qt::NoModifier, secondaryPoint);
}

TEST_F(SplitButtonTest, ToggleSplitButtonInheritsPressReboundAnimation) {
    ToggleSplitButton split(QStringLiteral("Pin"));
    split.resize(140, 36);
    split.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&split));

    auto* animation = split.findChild<QVariantAnimation*>();
    ASSERT_NE(animation, nullptr);
    const QPoint primaryPoint(split.width() / 4, split.height() / 2);
    QTest::mousePress(&split, Qt::LeftButton, Qt::NoModifier, primaryPoint);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    QTRY_VERIFY_WITH_TIMEOUT(animation->currentValue().toReal() > 0.0, 300);
    QTest::mouseRelease(&split, Qt::LeftButton, Qt::NoModifier, primaryPoint);
}

TEST_F(SplitButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->show();
    qApp->exec();
}
