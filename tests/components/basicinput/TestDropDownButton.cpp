#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QImage>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include "components/menus_toolbars/Menu.h"
#include "components/basicinput/DropDownButton.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"
#include "design/Typography.h"

using namespace fluent::basicinput;
using namespace fluent::textfields;
using namespace fluent::menus_toolbars;
using namespace fluent;

class InspectableDropDownButton : public DropDownButton {
public:
    using DropDownButton::DropDownButton;

    QRectF exposedContentPaintRect(const QRectF& surfaceRect) const {
        return contentPaintRect(surfaceRect);
    }
};

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class DropDownButtonTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(550, 450);
        window->setWindowTitle("DropDownButton Visual Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
    AnchorLayout* layout;
};

TEST_F(DropDownButtonTest, OpenSetterAliasTracksStateAndSignals) {
    DropDownButton button;
    QSignalSpy spy(&button, &DropDownButton::openChanged);

    EXPECT_FALSE(button.isOpen());
    button.setIsOpen(true);
    EXPECT_TRUE(button.isOpen());
    EXPECT_EQ(spy.count(), 1);

    button.setIsOpen(true);
    EXPECT_EQ(spy.count(), 1);

    button.setOpen(false);
    EXPECT_FALSE(button.isOpen());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(DropDownButtonTest, PressAnimationCompletesSmoothProgress) {
    DropDownButton button("Options");
    button.resize(140, 32);
    button.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&button));

    QTest::mousePress(&button, Qt::LeftButton, Qt::NoModifier, button.rect().center());

    QTRY_VERIFY_WITH_TIMEOUT(button.pressProgress() > 0.0, 300);
    QTRY_VERIFY_WITH_TIMEOUT(qFuzzyCompare(button.pressProgress(), 1.0), 1000);

    QTest::mouseRelease(&button, Qt::LeftButton, Qt::NoModifier, button.rect().center());
}

TEST_F(DropDownButtonTest, SizeHintReservesChevronAffordance) {
    Button plain("Email");
    DropDownButton dropdown("Email");

    EXPECT_GT(dropdown.sizeHint().width(), plain.sizeHint().width());
    EXPECT_GE(dropdown.sizeHint().width(),
              plain.sizeHint().width() + dropdown.chevronSize() + dropdown.chevronOffset().x());

    const int initialWidth = dropdown.sizeHint().width();
    dropdown.setChevronSize(dropdown.chevronSize() + 6);
    dropdown.setChevronOffset(dropdown.chevronOffset() + QPoint(4, 0));

    EXPECT_GT(dropdown.sizeHint().width(), initialWidth);
}

TEST_F(DropDownButtonTest, ContentPaintRectExcludesChevronReserve) {
    InspectableDropDownButton button("Email");
    const QSize hinted = button.sizeHint();
    const QRectF surfaceRect(0, 0, hinted.width(), hinted.height());
    const QRectF contentRect = button.exposedContentPaintRect(surfaceRect);

    const int expectedReserve = ::Spacing::Gap::Normal + button.chevronSize()
                                + button.chevronOffset().x();
    EXPECT_DOUBLE_EQ(contentRect.left(), surfaceRect.left());
    EXPECT_DOUBLE_EQ(contentRect.top(), surfaceRect.top());
    EXPECT_DOUBLE_EQ(contentRect.right(), surfaceRect.right() - expectedReserve);
    EXPECT_DOUBLE_EQ(contentRect.bottom(), surfaceRect.bottom());

    button.setChevronOffset(QPoint(4, 0));
    const QRectF tighterContentRect = button.exposedContentPaintRect(surfaceRect);
    EXPECT_GT(tighterContentRect.width(), contentRect.width());
}

TEST_F(DropDownButtonTest, MenuInheritsThemeOverrideFromButtonParent) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(24, 24, 240, 160);
    host->show();

    auto* button = new DropDownButton("Options", host);
    button->setGeometry(16, 16, 120, 32);
    button->show();

    FluentMenu menu(QStringLiteral("Options"), button);
    menu.addAction(new FluentMenuItem(QStringLiteral("Open"), &menu));
    menu.show();
    QApplication::processEvents();

    EXPECT_EQ(button->effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(menu.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(menu.themeColors().bgLayer, QColor("#2C2C2C"));
    menu.hide();
}

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// DropDownButton delegates its surface to Button::paintEvent (already brand-aware: Fluent /
// Material 3 / macOS) and overlays a trailing chevron whose tint is branched per design language.
// This suite grabs the rendered control across the full {language × theme × style} matrix to lock
// in that every combination paints, never crashes, and actually puts ink on the surface. Design
// language + theme are GLOBAL singletons, so TearDown restores the built-in defaults.
// zh_CN: DropDownButton 把表面委托给 Button::paintEvent(已按品牌 Fluent/Material 3/macOS 绘制),
// 再叠加一个按设计语言分支着色的尾随箭头。本套件遍历 {设计语言 × 主题 × 风格} 全矩阵抓取渲染结果,
// 确保每种组合都能绘制、不崩溃且确有像素落在表面上。设计语言与主题是全局单例,故 TearDown 恢复内置默认值。

class DropDownButtonDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题是全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a DropDownButton of the given style, size it ~140x36, and grab it as an image.
    // zh_CN: 构建指定风格的 DropDownButton,设定 ~140x36 尺寸并抓取为图像。
    static QImage grabDropDown(Button::ButtonStyle style) {
        DropDownButton btn("Options");
        btn.setFluentStyle(style);
        btn.resize(140, 36);
        return btn.grab().toImage();
    }
};

TEST_F(DropDownButtonDesignLanguageTest, EveryLanguageThemeStyleRendersWithInkOnSurface) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };
    struct StyleCase { Button::ButtonStyle style; const char* name; bool filled; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };
    const StyleCase styles[] = {
        { Button::Standard, "Standard", false },
        { Button::Accent, "Accent", true },   // Accent is always a filled surface. zh_CN: Accent 始终为填充表面。
        { Button::Subtle, "Subtle", false },
    };

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            for (const auto& st : styles) {
                fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
                fluent::FluentElement::setTheme(th.theme);

                const QImage img = grabDropDown(st.style);

                const std::string ctx =
                    std::string(lang.name) + "/" + th.name + "/" + st.name;
                ASSERT_FALSE(img.isNull()) << ctx;
                EXPECT_GT(img.width(), 0) << ctx;
                EXPECT_GT(img.height(), 0) << ctx;

                // Painted content: some pixel differs from the top-left background pixel.
                // A DropDownButton ALWAYS paints a chevron, so even Subtle/text styles qualify;
                // filled Accent additionally paints a fully colored surface.
                // zh_CN: 已绘制内容:存在与左上角背景不同的像素。DropDownButton 必绘制箭头,故 Subtle/文字样式也满足;
                // 填充 Accent 还会绘制整片着色表面。
                const QRgb corner = img.pixel(0, 0);
                bool painted = false;
                for (int y = 0; y < img.height() && !painted; ++y) {
                    for (int x = 0; x < img.width(); ++x) {
                        if (img.pixel(x, y) != corner) {
                            painted = true;
                            break;
                        }
                    }
                }
                EXPECT_TRUE(painted)
                    << "DropDownButton painted nothing (chevron expected): " << ctx;
            }
        }
    }
}

// Regression mirroring ComboBox's RestStateHasNoOpaqueBlackField for the macOS branch: an unset
// `QColor` is INVALID yet QColor::alpha() returns 255, so a bare `alpha() > 0` guard on a state-layer
// veil fills the bezel SOLID BLACK. The macOS bezel at rest must NOT be opaque near-#000 at center.
// zh_CN: 对应 ComboBox 的 RestStateHasNoOpaqueBlackField 回归(macOS 分支):未赋值的 QColor 虽无效,
// alpha() 却返回 255,裸 alpha()>0 守卫会把 bezel 涂成不透明黑。静息态 macOS bezel 中心不得为不透明近黑。
TEST_F(DropDownButtonDesignLanguageTest, MacOsRestStateHasNoOpaqueBlackField) {
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };

    for (auto theme : themes) {
        fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);
        fluent::FluentElement::setTheme(theme);

        // A fresh, never-interacted DropDownButton with no text is at REST — the condition that exposed
        // the invalid-veil bug. Empty text keeps the sampled interior on the bare bezel (no glyph ink),
        // so a near-black reading can only come from a bug-painted fill, not from centered text.
        // zh_CN: 全新、从未交互且无文字的 DropDownButton = 静息态,正是触发无效薄层 bug 的条件。
        // 空文字使采样的内部区域落在裸 bezel 上(无字形墨迹),近黑读数只可能来自 bug 的填充,而非居中文字。
        DropDownButton btn;
        btn.setFluentStyle(Button::Standard);
        btn.resize(140, 36);
        const QImage img = btn.grab().toImage();
        ASSERT_FALSE(img.isNull()) << "theme=" << theme;

        const QColor c = img.pixelColor(img.width() / 2, img.height() / 2);
        const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
        // Invalid-veil signature = an OPAQUE, near-#000 fill. The macOS bezel is a light/dark gray —
        // never opaque pure black at rest. zh_CN: 无效薄层特征=不透明近黑填充;macOS bezel 为浅/深灰,静息绝非不透明纯黑。
        const bool opaqueBlack = c.alpha() > 200 && lum < 16;
        EXPECT_FALSE(opaqueBlack)
            << "DropDownButton painted an opaque black bezel at rest for theme=" << theme
            << " rgba=(" << c.red() << "," << c.green() << "," << c.blue() << "," << c.alpha() << ")";
    }
}

TEST_F(DropDownButtonTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* title = new Label("DropDownButton Visual Test", window);
    title->setFont(title->themeFont("Title").toQFont());
    title->anchors()->top = {window, Edge::Top, 30};
    title->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(title);

    // 1. Standard DropDownButton
    Label* lbl1 = new Label("Standard Style (with Menu):", window);
    lbl1->anchors()->top = {title, Edge::Bottom, 40};
    lbl1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl1);

    DropDownButton* dropBtn = new DropDownButton("Options", window);
    dropBtn->setFixedSize(140, 32);
    dropBtn->anchors()->top = {lbl1, Edge::Bottom, 10};
    dropBtn->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(dropBtn);

    FluentMenu* menu1 = new FluentMenu("Options", dropBtn);
    menu1->addAction(new FluentMenuItem("Edit Profile", menu1));
    menu1->addAction(new FluentMenuItem("Account Settings", menu1));
    menu1->addSeparator();
    menu1->addAction(new FluentMenuItem("Logout", menu1));
    dropBtn->setMenu(menu1);

    // 2. Accent Style DropDownButton
    Label* lbl2 = new Label("Accent Style:", window);
    lbl2->anchors()->verticalCenter = {lbl1, Edge::VCenter, 0};
    lbl2->anchors()->left = {dropBtn, Edge::Right, 60};
    layout->addWidget(lbl2);

    DropDownButton* dropAccent = new DropDownButton("Primary Action", window);
    dropAccent->setFluentStyle(Button::Accent);
    dropAccent->setFixedSize(160, 32);
    dropAccent->anchors()->top = {lbl2, Edge::Bottom, 10};
    dropAccent->anchors()->left = {dropBtn, Edge::Right, 60};
    layout->addWidget(dropAccent);

    FluentMenu* menu2 = new FluentMenu("Primary Action", dropAccent);
    menu2->addAction(new FluentMenuItem("Confirm Selection", menu2));
    menu2->addAction(new FluentMenuItem("Review Changes", menu2));
    dropAccent->setMenu(menu2);

    // 3. 自定义图标属性测试（多种 Icon 对比）
    Label* lbl3 = new Label("Custom Glyph Variants:", window);
    lbl3->anchors()->top = {dropBtn, Edge::Bottom, 40};
    lbl3->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl3);

    // 3.1 向上 Chevron
    DropDownButton* customUp = new DropDownButton("Chevron Up", window);
    customUp->setChevronGlyph(Typography::Icons::ChevronUp);
    customUp->setChevronSize(16);
    customUp->setChevronOffset(QPoint(20, 0)); // x: 右侧间距, y: 垂直偏移
    customUp->setFixedSize(150, 32);
    customUp->anchors()->top = {lbl3, Edge::Bottom, 10};
    customUp->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(customUp);

    // 3.2 向下中号 Chevron
    DropDownButton* customDownMed = new DropDownButton("Chevron Down Med", window);
    customDownMed->setChevronGlyph(Typography::Icons::ChevronDownMed);
    customDownMed->setChevronSize(14);
    customDownMed->setChevronOffset(QPoint(5, 0));
    customDownMed->setFixedSize(170, 32);
    customDownMed->anchors()->top = {customUp, Edge::Bottom, 8};
    customDownMed->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(customDownMed);

    // 3.3 右箭头 Icon（演示非传统下拉箭头）
    DropDownButton* customRight = new DropDownButton("Next Step", window);
    customRight->setChevronGlyph(Typography::Icons::ChevronRight);
    customRight->setChevronSize(14);
    customRight->setChevronOffset(QPoint(16, 0));
    customRight->setFixedSize(150, 32);
    customRight->anchors()->top = {customDownMed, Edge::Bottom, 8};
    customRight->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(customRight);

    // 3.4 使用其它 Fluent Icon（例如“更多”菜单）
    DropDownButton* customMore = new DropDownButton("More Actions", window);
    customMore->setChevronGlyph(Typography::Icons::More);
    customMore->setChevronSize(14);
    customMore->setChevronOffset(QPoint(16, 3));
    customMore->setFixedSize(160, 32);
    customMore->anchors()->top = {customRight, Edge::Bottom, 8};
    customMore->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(customMore);

    // 3.5 Icon-only DropDownButton（按钮本体为 IconButton，左侧 IconFont，右侧下拉箭头）
    Label* lbl4 = new Label("Icon-only DropDownButton (IconFont):", window);
    lbl4->anchors()->top = {customMore, Edge::Bottom, 24};
    lbl4->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl4);

    DropDownButton* iconOnly = new DropDownButton("", window);
    iconOnly->setFluentLayout(Button::IconOnly);  // 使用 IconOnly 布局
    iconOnly->setIconGlyph(Typography::Icons::Send,  // 左侧 iconfont（Button 的 icon）
                           Typography::FontSize::Caption,
                           Typography::FontFamily::SegoeFluentIcons);
    iconOnly->setChevronSize(Typography::FontSize::Caption);  // 右侧 chevron
    iconOnly->setChevronOffset(QPoint(10, 0));
    iconOnly->setFixedSize(56, 32);
    iconOnly->anchors()->top = {lbl4, Edge::Bottom, 8};
    iconOnly->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(iconOnly);

    // 3.6 Icon + Text DropDownButton（左侧 IconFont，中间文本，右侧下拉箭头）
    Label* lbl5 = new Label("Icon + Text DropDownButton (IconFont):", window);
    lbl5->anchors()->top = {iconOnly, Edge::Bottom, 16};
    lbl5->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(lbl5);

    DropDownButton* iconWithText = new DropDownButton("More Actions", window);
    iconWithText->setFluentLayout(Button::IconBefore);
    iconWithText->setFixedSize(170, 32);
    iconWithText->setChevronSize(Typography::FontSize::Caption);
    iconWithText->setChevronOffset(QPoint(16, 0));
    iconWithText->setIconGlyph(Typography::Icons::More,
                               Typography::FontSize::Caption,
                               Typography::FontFamily::SegoeFluentIcons);
    iconWithText->anchors()->top = {lbl5, Edge::Bottom, 8};
    iconWithText->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(iconWithText);

    // 4. Theme Switcher
    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -20};
    themeBtn->anchors()->right = {window, Edge::Right, -20};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
