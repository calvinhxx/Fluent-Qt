#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QSignalSpy>
#include <QVBoxLayout>
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/textfields/Label.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"

using namespace fluent::dialogs_flyouts;
using namespace fluent::basicinput;
using namespace fluent::textfields;
using namespace fluent;

// ── FluentTestWindow ─────────────────────────────────────────────────────────

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        // 注意：不能用 setStyleSheet("background-color: ...")，因为 QSS 会沿父子链
        // 传播到所有子孙控件（包括弹出的 ContentDialog 内的 Label / CheckBox），
        // 导致 dialog 内容区被错误地染成 bgCanvas 颜色。
        // 改用 QPalette + autoFillBackground，仅作用于自身。
        const auto& c = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, c.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class ContentDialogTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("ContentDialog Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
};

// ══════════════════════════════════════════════════════════════════════════════
//  自动化测试（不弹窗，避免闪烁）
// ══════════════════════════════════════════════════════════════════════════════

// --- 默认属性 ---

TEST_F(ContentDialogTest, DefaultProperties) {
    ContentDialog dialog(window);

    EXPECT_TRUE(dialog.title().isEmpty());
    EXPECT_TRUE(dialog.primaryButtonText().isEmpty());
    EXPECT_TRUE(dialog.secondaryButtonText().isEmpty());
    EXPECT_TRUE(dialog.closeButtonText().isEmpty());
    EXPECT_EQ(dialog.defaultButton(), static_cast<int>(ContentDialog::None));
    EXPECT_EQ(dialog.content(), nullptr);
    // ContentDialog 默认启用蒙层、禁用拖拽
    EXPECT_TRUE(dialog.isSmokeEnabled());
    EXPECT_FALSE(dialog.isDragEnabled());
    EXPECT_EQ(dialog.shadowSize(), 16);
}

// --- Title ---

TEST_F(ContentDialogTest, SetTitle) {
    ContentDialog dialog(window);
    dialog.setTitle("Save your work?");
    EXPECT_EQ(dialog.title(), "Save your work?");

    dialog.setTitle("");
    EXPECT_TRUE(dialog.title().isEmpty());
}

// --- Button text ---

TEST_F(ContentDialogTest, SetPrimaryButtonText) {
    ContentDialog dialog(window);
    dialog.setPrimaryButtonText("Save");
    EXPECT_EQ(dialog.primaryButtonText(), "Save");
}

TEST_F(ContentDialogTest, SetSecondaryButtonText) {
    ContentDialog dialog(window);
    dialog.setSecondaryButtonText("Don't Save");
    EXPECT_EQ(dialog.secondaryButtonText(), "Don't Save");
}

TEST_F(ContentDialogTest, SetCloseButtonText) {
    ContentDialog dialog(window);
    dialog.setCloseButtonText("Cancel");
    EXPECT_EQ(dialog.closeButtonText(), "Cancel");
}

// --- DefaultButton ---

TEST_F(ContentDialogTest, DefaultButtonEnum) {
    ContentDialog dialog(window);

    dialog.setDefaultButton(ContentDialog::Primary);
    EXPECT_EQ(dialog.defaultButton(), ContentDialog::Primary);

    dialog.setDefaultButton(ContentDialog::Secondary);
    EXPECT_EQ(dialog.defaultButton(), ContentDialog::Secondary);

    dialog.setDefaultButton(ContentDialog::Close);
    EXPECT_EQ(dialog.defaultButton(), ContentDialog::Close);

    dialog.setDefaultButton(ContentDialog::None);
    EXPECT_EQ(dialog.defaultButton(), ContentDialog::None);
}

// --- Content ---

TEST_F(ContentDialogTest, SetContent) {
    ContentDialog dialog(window);

    auto* label = new Label("Hello content");
    dialog.setContent(label);
    EXPECT_EQ(dialog.content(), label);

    auto* label2 = new Label("New content");
    dialog.setContent(label2);
    EXPECT_EQ(dialog.content(), label2);

    dialog.setContent(nullptr);
    EXPECT_EQ(dialog.content(), nullptr);
}

// --- Result constants ---

TEST_F(ContentDialogTest, ResultConstants) {
    EXPECT_EQ(ContentDialog::ResultNone, QDialog::Rejected);
    EXPECT_EQ(ContentDialog::ResultPrimary, QDialog::Accepted);
    EXPECT_EQ(ContentDialog::ResultSecondary, 2);
}

// --- 按钮点击信号 + done() 结果（无需 exec()，不弹窗） ---

TEST_F(ContentDialogTest, PrimaryButtonSignalAndResult) {
    ContentDialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setPrimaryButtonText("OK");

    QSignalSpy clickSpy(&dialog, &ContentDialog::primaryButtonClicked);
    QSignalSpy finishSpy(&dialog, &QDialog::finished);

    auto buttons = dialog.findChildren<fluent::basicinput::Button*>();
    for (auto* btn : buttons) {
        if (btn->text() == "OK") { btn->click(); break; }
    }

    EXPECT_EQ(clickSpy.count(), 1);
    EXPECT_EQ(finishSpy.count(), 1);
    EXPECT_EQ(dialog.result(), ContentDialog::ResultPrimary);
}

TEST_F(ContentDialogTest, SecondaryButtonSignalAndResult) {
    ContentDialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setSecondaryButtonText("Don't Save");

    QSignalSpy clickSpy(&dialog, &ContentDialog::secondaryButtonClicked);
    QSignalSpy finishSpy(&dialog, &QDialog::finished);

    auto buttons = dialog.findChildren<fluent::basicinput::Button*>();
    for (auto* btn : buttons) {
        if (btn->text() == "Don't Save") { btn->click(); break; }
    }

    EXPECT_EQ(clickSpy.count(), 1);
    EXPECT_EQ(finishSpy.count(), 1);
    EXPECT_EQ(dialog.result(), ContentDialog::ResultSecondary);
}

TEST_F(ContentDialogTest, CloseButtonSignalAndResult) {
    ContentDialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setCloseButtonText("Cancel");

    QSignalSpy clickSpy(&dialog, &ContentDialog::closeButtonClicked);
    QSignalSpy finishSpy(&dialog, &QDialog::finished);

    auto buttons = dialog.findChildren<fluent::basicinput::Button*>();
    for (auto* btn : buttons) {
        if (btn->text() == "Cancel") { btn->click(); break; }
    }

    EXPECT_EQ(clickSpy.count(), 1);
    EXPECT_EQ(finishSpy.count(), 1);
    EXPECT_EQ(dialog.result(), ContentDialog::ResultNone);
}

// --- Min width ---

TEST_F(ContentDialogTest, MinimumWidth) {
    ContentDialog dialog(window);
    EXPECT_GE(dialog.minimumWidth(), 320 + 2 * dialog.shadowSize());
}

// --- Theme switch no crash ---

TEST_F(ContentDialogTest, ThemeSwitchNoCrash) {
    ContentDialog dialog(window);
    dialog.setTitle("Theme Test");
    dialog.setPrimaryButtonText("OK");

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    dialog.onThemeUpdated();

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    dialog.onThemeUpdated();

    SUCCEED();
}

// --- Full ContentDialog 属性装配 ---

TEST_F(ContentDialogTest, FullContentDialogSetup) {
    ContentDialog dialog(window);
    dialog.setTitle("Save your work?");
    dialog.setPrimaryButtonText("Save");
    dialog.setSecondaryButtonText("Don't Save");
    dialog.setCloseButtonText("Cancel");
    dialog.setDefaultButton(ContentDialog::Primary);

    auto* body = new Label("Your changes will be lost if you don't save.");
    dialog.setContent(body);

    EXPECT_EQ(dialog.title(), "Save your work?");
    EXPECT_EQ(dialog.primaryButtonText(), "Save");
    EXPECT_EQ(dialog.secondaryButtonText(), "Don't Save");
    EXPECT_EQ(dialog.closeButtonText(), "Cancel");
    EXPECT_EQ(dialog.defaultButton(), ContentDialog::Primary);
    EXPECT_EQ(dialog.content(), body);
}

// ══════════════════════════════════════════════════════════════════════════════
//  WinUI 3 GIF 对齐：按钮条紧凑布局
// ══════════════════════════════════════════════════════════════════════════════

namespace {
// 找到按钮条容器（QHBoxLayout 的 parent widget）
QWidget* findButtonBar(ContentDialog* dialog) {
    auto buttons = dialog->findChildren<fluent::basicinput::Button*>();
    if (buttons.isEmpty()) return nullptr;
    return buttons.first()->parentWidget();
}
} // namespace

TEST_F(ContentDialogTest, ButtonsHaveMinimumWidth96) {
    ContentDialog dialog(window);
    dialog.setPrimaryButtonText("OK");
    dialog.setSecondaryButtonText("No");
    dialog.setCloseButtonText("X");

    auto buttons = dialog.findChildren<fluent::basicinput::Button*>();
    ASSERT_EQ(buttons.size(), 3);
    for (auto* btn : buttons) {
        EXPECT_GE(btn->minimumWidth(), 96)
            << "Button '" << btn->text().toStdString()
            << "' minimumWidth should be >= 96, got " << btn->minimumWidth();
    }
}

TEST_F(ContentDialogTest, ButtonBarHeightIs68) {
    ContentDialog dialog(window);
    dialog.setPrimaryButtonText("Save");

    QWidget* bar = findButtonBar(&dialog);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->height(), 68)
        << "Button bar height should be 68px (was 80), got " << bar->height();
}

TEST_F(ContentDialogTest, ButtonBarIsLeftAligned) {
    // 按钮条应左对齐：可见按钮的最右侧位置 < 按钮条 contentsRect 右边界
    ContentDialog dialog(window);
    dialog.setFixedSize(572, 300);
    dialog.setPrimaryButtonText("Save");
    dialog.setSecondaryButtonText("Don't Save");
    dialog.setCloseButtonText("Cancel");

    // 触发 layout
    window->show();
    QApplication::processEvents();
    dialog.show();
    QApplication::processEvents();

    QWidget* bar = findButtonBar(&dialog);
    ASSERT_NE(bar, nullptr);

    auto buttons = dialog.findChildren<fluent::basicinput::Button*>();
    int rightmost = -1;
    for (auto* btn : buttons) {
        // 检查相对于 bar 的可见性（而非屏幕可见性）；geometry 在 layout 后即有效
        if (!btn->isVisibleTo(bar)) continue;
        rightmost = std::max(rightmost, btn->geometry().right());
    }

    ASSERT_GT(rightmost, -1);
    EXPECT_LT(rightmost, bar->contentsRect().right())
        << "Rightmost visible button should not reach button bar's right edge "
           "(should be left-aligned with whitespace). rightmost=" << rightmost
        << " barRight=" << bar->contentsRect().right();

    dialog.setAnimationEnabled(false);
    dialog.done(0);
}

// ══════════════════════════════════════════════════════════════════════════════
//  VisualCheck
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(ContentDialogTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);
    window->setFixedSize(700, 500);

    using Edge = AnchorLayout::Edge;

    // --- Standard 2-button dialog (Figma 540px variant) ---
    Button* btn1 = new Button("Standard 2-Button Dialog", window);
    btn1->setFluentStyle(Button::Accent);
    btn1->setFixedSize(240, 32);
    btn1->anchors()->top  = {window, Edge::Top,  40};
    btn1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn1);

    QObject::connect(btn1, &Button::clicked, [this]() {
        ContentDialog dialog(window);
        dialog.setFixedSize(454, 232);
        dialog.setTitle("Save your work?");

        // 内容区：短正文 + CheckBox（对齐 WinUI 3 Gallery 参考图）
        auto* contentHost = new QWidget();
        auto* vbox = new QVBoxLayout(contentHost);
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(8);

        auto* body = new Label("Lorem ipsum dolor sit amet, adipisicing elit.");
        body->setWordWrap(true);
        vbox->addWidget(body);

        auto* cb = new CheckBox("Upload your content to the cloud.");
        vbox->addWidget(cb);

        vbox->addStretch(1);

        dialog.setContent(contentHost);
        dialog.setPrimaryButtonText("Save");
        dialog.setSecondaryButtonText("Don't Save");
        dialog.setCloseButtonText("Cancel");
        dialog.setDefaultButton(ContentDialog::Primary);
        dialog.exec();
    });

    // --- 3-button dialog ---
    Button* btn2 = new Button("3-Button Dialog", window);
    btn2->setFixedSize(240, 32);
    btn2->anchors()->top  = {btn1, Edge::Bottom, 16};
    btn2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn2);

    QObject::connect(btn2, &Button::clicked, [this]() {
        ContentDialog dialog(window);
        dialog.setFixedSize(480, 260);
        dialog.setTitle("Delete this file permanently?");
        auto* body = new Label(
            "If you delete this file, you won't be able to recover it. "
            "If you delete this file, you won't be able to recover it. "
            "If you delete this file, you won't be able to recover it. "
            "Do you want to delete it?");
        body->setWordWrap(true);
        dialog.setContent(body);
        dialog.setPrimaryButtonText("Delete");
        dialog.setSecondaryButtonText("Move to Recycle Bin");
        dialog.setCloseButtonText("Cancel");
        dialog.setDefaultButton(ContentDialog::Close);
        dialog.exec();
    });

    // --- Min-width 2-button dialog (Figma 320px variant) ---
    Button* btn3 = new Button("Min-Width Dialog", window);
    btn3->setFixedSize(240, 32);
    btn3->anchors()->top  = {btn2, Edge::Bottom, 16};
    btn3->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn3);

    QObject::connect(btn3, &Button::clicked, [this]() {
        ContentDialog dialog(window);
        dialog.setFixedSize(352, 232);
        dialog.setTitle("Title");
        auto* body = new Label("Windows 11 is faster and more intuitive.");
        body->setWordWrap(true);
        dialog.setContent(body);
        dialog.setPrimaryButtonText("OK");
        dialog.setCloseButtonText("Cancel");
        dialog.setDefaultButton(ContentDialog::Primary);
        dialog.exec();
    });

    // --- Toggle theme ---
    Button* themeBtn = new Button("Toggle Dark/Light", window);
    themeBtn->setFixedSize(240, 32);
    themeBtn->anchors()->top  = {btn3, Edge::Bottom, 32};
    themeBtn->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, [this]() {
        auto theme = fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                         ? fluent::FluentElement::Dark : fluent::FluentElement::Light;
        fluent::FluentElement::setTheme(theme);
        window->onThemeUpdated();
    });

    window->show();
    qApp->exec();
}
