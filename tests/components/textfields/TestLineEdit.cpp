#include <gtest/gtest.h>
#include <QApplication>
#include <QIntValidator>
#include <QtTest/QSignalSpy>
#include "QtTestEnvironment.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"

using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

class LineEditTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent LineEdit Test");
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

TEST_F(LineEditTest, TextAndPlaceholder) {
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Enter value");
    EXPECT_EQ(edit->placeholderText(), "Enter value");

    edit->setText("hello");
    EXPECT_EQ(edit->text(), "hello");
}

TEST_F(LineEditTest, PlaceholderPaletteUsesResolvedOpaqueToken) {
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    LineEdit edit(window);
    edit.onThemeUpdated();
    const QColor placeholder = edit.palette().color(QPalette::Active,
                                                     QPalette::PlaceholderText);

    EXPECT_EQ(placeholder.alpha(), 255);
    EXPECT_GT(placeholder.red(), 130);
    EXPECT_LT(placeholder.red(), 150);
    EXPECT_EQ(placeholder.red(), placeholder.green());
    EXPECT_EQ(placeholder.green(), placeholder.blue());

    fluent::FluentElement::setTheme(previousTheme);
}

TEST_F(LineEditTest, StyledAncestorDoesNotReplaceDarkThemeTextPalette) {
    const auto previousTheme = fluent::FluentElement::currentTheme();
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    window->onThemeUpdated();

    LineEdit edit(window);
    edit.setText(QStringLiteral("42"));
    edit.onThemeUpdated();

    const auto colors = edit.themeColors();
    EXPECT_EQ(edit.palette().color(QPalette::Active, QPalette::Text),
              colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Inactive, QPalette::Text),
              colors.textPrimary);
    EXPECT_EQ(edit.palette().color(QPalette::Disabled, QPalette::Text),
              colors.textDisabled);

    fluent::FluentElement::setTheme(previousTheme);
    window->onThemeUpdated();
}

TEST_F(LineEditTest, ContentMargins) {
    LineEdit* edit = new LineEdit(window);
    QMargins margins(10, 2, 10, 2);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(LineEditTest, ReadOnly) {
    LineEdit* edit = new LineEdit(window);
    edit->setText("read only");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(LineEditTest, Validator) {
    LineEdit* edit = new LineEdit(window);
    auto* validator = new QIntValidator(0, 100, edit);
    edit->setValidator(validator);
    EXPECT_EQ(edit->validator(), validator);
}

TEST_F(LineEditTest, FluentPropertiesDefaultsAndSetters) {
    LineEdit* edit = new LineEdit(window);

    // 默认值验证（引用 Spacing/Typography 常量）
    EXPECT_TRUE(edit->isClearButtonEnabled());
    EXPECT_EQ(edit->clearButtonSize(), 22);
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(Spacing::XSmall, 0));
    EXPECT_EQ(edit->focusedBorderWidth(), Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), Spacing::Border::Normal);
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);

    QSignalSpy spyOffset(edit, SIGNAL(clearButtonOffsetChanged()));
    QSignalSpy spyFocused(edit, SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));

    edit->setClearButtonOffset(QPoint(10, 3));
    EXPECT_EQ(edit->clearButtonOffset(), QPoint(10, 3));
    EXPECT_EQ(spyOffset.count(), 1);

    edit->setFocusedBorderWidth(3);
    EXPECT_EQ(edit->focusedBorderWidth(), 3);
    EXPECT_EQ(spyFocused.count(), 1);

    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 2);
    EXPECT_EQ(spyUnfocused.count(), 1);

    // 相同值不应再次触发信号
    edit->setClearButtonOffset(QPoint(10, 3));
    edit->setFocusedBorderWidth(3);
    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(spyOffset.count(), 1);
    EXPECT_EQ(spyFocused.count(), 1);
    EXPECT_EQ(spyUnfocused.count(), 1);
}

TEST_F(LineEditTest, ClearButtonOffsetAffectsGeometry) {
    LineEdit* edit = new LineEdit(window);
    edit->setClearButtonEnabled(true);
    edit->setText("x");
    edit->setFixedSize(200, 40);
    // 在无屏环境下 resizeEvent 可能不会立刻触发，这里用 setter 主动刷新几何
    edit->setClearButtonSize(20);
    edit->setClearButtonOffset(QPoint(12, 5));

    // internal clear button is a fluent::basicinput::Button child
    const auto buttons = edit->findChildren<::fluent::basicinput::Button*>();
    ASSERT_EQ(buttons.size(), 1);
    auto* clearBtn = buttons.first();
    ASSERT_NE(clearBtn, nullptr);

    const int expectedX = edit->width() - 20 - 12;
    const int expectedY = (edit->height() - 20) / 2 + 5;
    EXPECT_EQ(clearBtn->pos(), QPoint(expectedX, expectedY));
}

// ── Design-language × theme compatibility ────────────────────────────────────
//
// LineEdit paints a per-brand border/focus treatment (Fluent bottom underline / Material
// outlined rect / macOS hairline + accent ring) under each App theme (Light / Dark). This
// suite grabs the rest-state control across the full {language × theme} matrix to lock in
// that every combination paints, never crashes, and produces a valid, non-empty image with
// visible content. Focus-ring visuals need a shown+focused widget (unreliable headless), so
// we only cover rest-state rendering here. Design language + theme are GLOBAL state, so
// TearDown restores the built-in defaults.
// zh_CN: LineEdit 按品牌绘制边框/焦点处理(Fluent 底部下划线 / Material 描边矩形 / macOS 发丝
// 边框 + 强调环),并在明暗主题下分支。本套件遍历 {设计语言 × 主题} 全矩阵抓取静息态渲染结果,
// 确保每种组合都能绘制、不崩溃,且生成有效、非空且有可见内容的图像。焦点环视觉需要已显示且聚焦的
// 控件(无屏环境不可靠),故此处仅覆盖静息态。设计语言与主题是全局状态,故 TearDown 恢复内置默认值。

class LineEditDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so other suites start clean.
        // zh_CN: 设计语言与主题是全局状态——重置以保证其它套件从干净状态开始。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a rest-state LineEdit with text and grab it as an image. zh_CN: 构建带文本的静息态 LineEdit 并抓取为图像。
    static QImage grabLineEdit() {
        LineEdit le;
        le.setText("Sample text");
        le.resize(200, 32);
        return le.grab().toImage();
    }
};

TEST_F(LineEditDesignLanguageTest, AllLanguagesThemesPaint) {
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

            QImage img = grabLineEdit();

            // No crash + valid, correctly-sized image. zh_CN: 不崩溃 + 图像有效且尺寸正确。
            ASSERT_FALSE(img.isNull()) << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(img.width(), 200) << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(img.height(), 32) << "lang=" << lang << " theme=" << theme;

            // Painted content: some pixel differs from the top-left background pixel.
            // zh_CN: 已绘制内容:存在与左上角背景像素不同的像素。
            const QRgb bg = img.pixel(0, 0);
            bool paintedContent = false;
            for (int y = 0; y < img.height() && !paintedContent; ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    if (img.pixel(x, y) != bg) {
                        paintedContent = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(paintedContent)
                << "painted nothing for lang=" << lang << " theme=" << theme;
        }
    }
}

TEST_F(LineEditTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("LineEdit (single-line):", window);
    header->anchors()->top = {window, Edge::Top, 30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认样式
    LineEdit* edit = new LineEdit(window);
    edit->setPlaceholderText("Default LineEdit...");
    edit->setText("Sample text");
    edit->setContentMargins(QMargins(8, 4, 8, 4));
    edit->anchors()->top = {header, Edge::Bottom, 8};
    edit->anchors()->left = {window, Edge::Left, 40};
    edit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit);

    // 带自定义 clearButtonOffset 的例子
    Label* offsetHeader = new Label("With custom clearButtonOffset:", window);
    offsetHeader->anchors()->top = {edit, Edge::Bottom, 20};
    offsetHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(offsetHeader);

    LineEdit* offsetEdit = new LineEdit(window);
    offsetEdit->setPlaceholderText("Clear button offset (x=12, y=4)...");
    offsetEdit->setText("Offset clear button");
    offsetEdit->setContentMargins(QMargins(8, 4, 8, 4));
    offsetEdit->setClearButtonOffset(QPoint(12, 4));
    offsetEdit->anchors()->top = {offsetHeader, Edge::Bottom, 8};
    offsetEdit->anchors()->left = {window, Edge::Left, 40};
    offsetEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(offsetEdit);

    // 带自定义边框粗细的例子
    Label* borderHeader = new Label("Custom focused/unfocused border widths:", window);
    borderHeader->anchors()->top = {offsetEdit, Edge::Bottom, 20};
    borderHeader->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(borderHeader);

    LineEdit* borderEdit = new LineEdit(window);
    borderEdit->setPlaceholderText("Focused=3px, Unfocused=2px...");
    borderEdit->setText("Border thickness demo");
    borderEdit->setContentMargins(QMargins(8, 4, 8, 4));
    borderEdit->setFocusedBorderWidth(3);
    borderEdit->setUnfocusedBorderWidth(2);
    borderEdit->anchors()->top = {borderHeader, Edge::Bottom, 8};
    borderEdit->anchors()->left = {window, Edge::Left, 40};
    borderEdit->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(borderEdit);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right = {window, Edge::Right, -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}
