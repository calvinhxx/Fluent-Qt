#include <gtest/gtest.h>

#include <QApplication>
#include <QEvent>
#include <QImage>
#include <QLineEdit>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "design/Typography.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/Label.h"

using namespace fluent;
using namespace fluent::basicinput;
using namespace fluent::textfields;

class PasswordBoxTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& colors = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(colors.bgCanvas.name()));
    }
};

class PasswordBoxTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<fluent::textfields::PasswordBox::PasswordRevealMode>(
            "fluent::textfields::PasswordBox::PasswordRevealMode");
    }

    void SetUp() override {
        window = new PasswordBoxTestWindow();
        window->setFixedSize(560, 460);
        layout = new AnchorLayout(window);
        window->setLayout(layout);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    void showAndFocus(PasswordBox* box) {
        window->show();
        box->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
    }

    PasswordBoxTestWindow* window = nullptr;
    AnchorLayout* layout = nullptr;
};

TEST_F(PasswordBoxTest, DefaultsAndRevealButton) {
    PasswordBox box(window);

    EXPECT_TRUE(box.password().isEmpty());
    EXPECT_TRUE(box.header().isEmpty());
    EXPECT_EQ(box.passwordRevealMode(), PasswordBox::PasswordRevealMode::Peek);
    EXPECT_EQ(box.echoMode(), QLineEdit::Password);
    EXPECT_EQ(box.sizeHint().height(), 32);

    auto* revealButton = box.findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);
    EXPECT_TRUE(revealButton->isHidden());
}

TEST_F(PasswordBoxTest, PasswordPropertyUsesTextValue) {
    PasswordBox box(window);
    QSignalSpy passwordSpy(&box, &PasswordBox::passwordChanged);

    box.setPassword("secret");

    EXPECT_EQ(box.password(), "secret");
    EXPECT_EQ(box.text(), "secret");
    ASSERT_EQ(passwordSpy.count(), 1);
    EXPECT_EQ(passwordSpy.first().at(0).toString(), "secret");

    box.setPassword("secret");
    EXPECT_EQ(passwordSpy.count(), 1);
}

TEST_F(PasswordBoxTest, UserEditingEmitsPasswordChanged) {
    auto* box = new PasswordBox(window);
    box->setFixedWidth(240);
    layout->addWidget(box);

    QSignalSpy passwordSpy(box, &PasswordBox::passwordChanged);
    showAndFocus(box);
    QTest::keyClicks(box, "abc");
    QApplication::processEvents();

    EXPECT_EQ(box->password(), "abc");
    ASSERT_GE(passwordSpy.count(), 1);
    EXPECT_EQ(passwordSpy.last().at(0).toString(), "abc");
}

TEST_F(PasswordBoxTest, RevealModesControlEchoAndButton) {
    PasswordBox box(window);
    box.resize(240, box.sizeHint().height());
    box.setPassword("secret");

    auto* revealButton = box.findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);

    EXPECT_EQ(box.echoMode(), QLineEdit::Password);
    EXPECT_FALSE(revealButton->isHidden());

    box.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Hidden);
    EXPECT_EQ(box.echoMode(), QLineEdit::Password);
    EXPECT_TRUE(revealButton->isHidden());

    box.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Visible);
    EXPECT_EQ(box.echoMode(), QLineEdit::Normal);
    EXPECT_TRUE(revealButton->isHidden());

    box.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Peek);
    EXPECT_EQ(box.echoMode(), QLineEdit::Password);
    EXPECT_FALSE(revealButton->isHidden());
}

TEST_F(PasswordBoxTest, PeekButtonTemporarilyRevealsAndKeepsFocus) {
    auto* box = new PasswordBox(window);
    box->setFixedWidth(240);
    box->setPassword("secret");
    layout->addWidget(box);
    showAndFocus(box);

    auto* revealButton = box->findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);
    ASSERT_FALSE(revealButton->isHidden());

    QTest::mousePress(revealButton, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Normal);
    EXPECT_TRUE(box->hasFocus());

    QTest::mouseRelease(revealButton, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Password);
    EXPECT_TRUE(box->hasFocus());
}

TEST_F(PasswordBoxTest, PeekRestoresOnLeaveAndFocusLoss) {
    auto* box = new PasswordBox(window);
    box->setFixedWidth(240);
    box->setPassword("secret");
    layout->addWidget(box);
    showAndFocus(box);

    auto* revealButton = box->findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);

    QTest::mousePress(revealButton, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Normal);

    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(revealButton, &leaveEvent);
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Password);

    QTest::mousePress(revealButton, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Normal);

    box->clearFocus();
    QApplication::processEvents();
    EXPECT_EQ(box->echoMode(), QLineEdit::Password);
}

TEST_F(PasswordBoxTest, HeaderHeightAndButtonLayout) {
    PasswordBox box(window);
    EXPECT_EQ(box.sizeHint().height(), 32);

    box.setHeader("Password");
    box.setPassword("secret");
    box.resize(260, box.sizeHint().height());

    auto* revealButton = box.findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);

    EXPECT_EQ(box.sizeHint().height(), 60);
    EXPECT_EQ(box.minimumSizeHint().height(), 60);
    EXPECT_GE(revealButton->geometry().top(), 28);
    EXPECT_LT(revealButton->geometry().bottom(), box.height());
}

TEST_F(PasswordBoxTest, DisabledAndReadOnlyHideRevealButton) {
    PasswordBox box(window);
    box.resize(240, box.sizeHint().height());
    box.setPassword("secret");

    auto* revealButton = box.findChild<Button*>("PasswordBoxRevealButton");
    ASSERT_NE(revealButton, nullptr);
    EXPECT_FALSE(revealButton->isHidden());

    box.setReadOnly(true);
    QApplication::processEvents();
    EXPECT_TRUE(revealButton->isHidden());
    EXPECT_EQ(box.echoMode(), QLineEdit::Password);

    box.setReadOnly(false);
    box.setEnabled(false);
    QApplication::processEvents();
    EXPECT_TRUE(revealButton->isHidden());
    EXPECT_EQ(box.echoMode(), QLineEdit::Password);
}

// ── Design-language × theme coverage ─────────────────────────────────────────
//
// PasswordBox subclasses LineEdit but paints its own input-row frame (so the header
// row stays unboxed). That frame is now brand-aware: Fluent keeps the WinUI fill +
// bottom accent underline, Material draws an outlined rect (2dp accent on focus),
// Cupertino draws a hairline + accent focus ring. The reveal button is a fluent::Button
// that follows the language for free. This suite grabs the rendered control across
// {language × theme}, asserting every combination paints valid, non-empty content and
// never crashes. Design language + theme are GLOBAL, so TearDown restores defaults.
// zh_CN: PasswordBox 继承 LineEdit 但自绘输入行边框(标题行不被框住)。该边框现已品牌感知:
// Fluent 保留 WinUI 填充+底部强调下划线,Material 描边矩形(聚焦 2dp accent),Cupertino 发丝边框+
// 强调焦点环。reveal 按钮是 fluent::Button,自动跟随语言。本套件遍历 {语言 × 主题} 抓取渲染结果,
// 确保每种组合绘制有效非空内容且不崩溃。语言与主题是全局状态,TearDown 复位。

class PasswordBoxDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static bool hasPaintedContent(const QImage& image) {
        if (image.isNull()) return false;
        const QRgb bg = image.pixel(0, 0);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixel(x, y) != bg) return true;
        return false;
    }

    static QImage grabBox() {
        PasswordBox box;
        box.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Peek);
        box.setPassword("hunter2");
        box.resize(220, 32);
        return box.grab().toImage();
    }
};

TEST_F(PasswordBoxDesignLanguageTest, AllLanguagesThemesPaintValid) {
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

            QImage image = grabBox();
            ASSERT_FALSE(image.isNull()) << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(image.width(), 220) << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(image.height(), 32) << "lang=" << lang << " theme=" << theme;
            EXPECT_TRUE(hasPaintedContent(image))
                << "painted nothing for lang=" << lang << " theme=" << theme;
        }
    }
}

TEST_F(PasswordBoxTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;
    window->setFixedSize(560, 520);

    auto* title = new Label("PasswordBox", window);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    title->anchors()->top = {window, Edge::Top, 28};
    title->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(title);

    auto* rest = new PasswordBox(window);
    rest->setPlaceholderText("Password");
    rest->anchors()->top = {title, Edge::Bottom, 16};
    rest->anchors()->left = {window, Edge::Left, 40};
    rest->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(rest);

    auto* focused = new PasswordBox(window);
    focused->setPlaceholderText("Focused peek");
    focused->setPassword("Fluent123");
    focused->anchors()->top = {rest, Edge::Bottom, 16};
    focused->anchors()->left = {window, Edge::Left, 40};
    focused->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(focused);

    auto* withHeader = new PasswordBox(window);
    withHeader->setHeader("Account password");
    withHeader->setPlaceholderText("Enter password");
    withHeader->anchors()->top = {focused, Edge::Bottom, 20};
    withHeader->anchors()->left = {window, Edge::Left, 40};
    withHeader->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(withHeader);

    auto* visible = new PasswordBox(window);
    visible->setPassword("Visible mode");
    visible->setPasswordRevealMode(PasswordBox::PasswordRevealMode::Visible);
    visible->anchors()->top = {withHeader, Edge::Bottom, 20};
    visible->anchors()->left = {window, Edge::Left, 40};
    visible->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(visible);

    auto* hidden = new PasswordBox(window);
    hidden->setPassword("Hidden mode");
    hidden->setPasswordRevealMode(PasswordBox::PasswordRevealMode::Hidden);
    hidden->anchors()->top = {visible, Edge::Bottom, 16};
    hidden->anchors()->left = {window, Edge::Left, 40};
    hidden->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(hidden);

    auto* disabled = new PasswordBox(window);
    disabled->setPassword("Disabled state");
    disabled->setEnabled(false);
    disabled->anchors()->top = {hidden, Edge::Bottom, 16};
    disabled->anchors()->left = {window, Edge::Left, 40};
    disabled->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(disabled);

    auto* themeButton = new Button("Switch Theme", window);
    themeButton->setFluentStyle(Button::Accent);
    themeButton->setFixedSize(120, 32);
    themeButton->anchors()->bottom = {window, Edge::Bottom, -28};
    themeButton->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(themeButton);

    QObject::connect(themeButton, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    });

    window->show();
    QTimer::singleShot(0, focused, [focused]() { focused->setFocus(Qt::OtherFocusReason); });
    qApp->exec();
}
