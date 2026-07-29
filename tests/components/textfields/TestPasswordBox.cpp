#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEvent>
#include <QImage>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "design/Typography.h"
#include "components/menus_toolbars/Menu.h"
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

namespace {

bool actionMatchesStandardKey(const QAction* action,
                              QKeySequence::StandardKey standardKey)
{
    if (!action)
        return false;

    QList<QKeySequence> shortcuts = action->shortcuts();
    if (shortcuts.isEmpty()) {
        const int tabIndex = action->text().indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            const QKeySequence embedded(
                action->text().mid(tabIndex + 1).trimmed(),
                QKeySequence::NativeText);
            if (!embedded.isEmpty())
                shortcuts.append(embedded);
        }
    }

    const QList<QKeySequence> bindings =
        QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& shortcut : shortcuts) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding) == QKeySequence::ExactMatch)
                return true;
        }
    }
    return false;
}

} // namespace

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

TEST_F(PasswordBoxTest, Contract_HiddenPasswordUsesInheritedFluentContextMenu)
{
    auto* box = new PasswordBox(window);
    box->setFixedWidth(240);
    box->setPassword(QStringLiteral("secret"));
    box->setPasswordRevealMode(
        PasswordBox::PasswordRevealMode::Hidden);
    box->setSelection(0, 1);
    layout->addWidget(box);
    showAndFocus(box);

    bool sawFluentMenu = false;
    bool sawCut = false;
    bool sawCopy = false;
    bool sawSelectAll = false;
    bool cutEnabled = true;
    bool copyEnabled = true;
    bool selectAllEnabled = false;
    QTimer::singleShot(0, [&]() {
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                QApplication::activePopupWidget());
        sawFluentMenu = menu != nullptr;
        if (!menu)
            return;

        EXPECT_EQ(
            menu->objectName(),
            QStringLiteral("FluentLineEdit.ContextMenu"));
        for (QAction* action : menu->actions()) {
            if (actionMatchesStandardKey(action, QKeySequence::Cut)) {
                sawCut = true;
                cutEnabled = action->isEnabled();
            } else if (actionMatchesStandardKey(
                           action, QKeySequence::Copy)) {
                sawCopy = true;
                copyEnabled = action->isEnabled();
            } else if (actionMatchesStandardKey(
                           action, QKeySequence::SelectAll)) {
                sawSelectAll = true;
                selectAllEnabled = action->isEnabled();
            }
        }
        menu->close();
    });

    const QPoint localPos = box->rect().center();
    const QPoint globalPos = box->mapToGlobal(localPos);
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, localPos, globalPos);
    QApplication::sendEvent(box, &event);

    EXPECT_TRUE(event.isAccepted());
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_TRUE(sawCut);
    EXPECT_TRUE(sawCopy);
    EXPECT_TRUE(sawSelectAll);
    EXPECT_FALSE(cutEnabled);
    EXPECT_FALSE(copyEnabled);
    EXPECT_TRUE(selectAllEnabled);
    EXPECT_EQ(box->password(), QStringLiteral("secret"));
}

TEST_F(
    PasswordBoxTest,
    Contract_PeekContextMenuEndsRevealAndNeverExportsText)
{
    auto* box = new PasswordBox(window);
    box->setFixedWidth(240);
    box->setPassword(QStringLiteral("secret"));
    layout->addWidget(box);
    showAndFocus(box);

    auto* revealButton =
        box->findChild<Button*>(QStringLiteral("PasswordBoxRevealButton"));
    ASSERT_NE(revealButton, nullptr);
    QTest::mousePress(revealButton, Qt::LeftButton);
    QApplication::processEvents();
    ASSERT_EQ(box->echoMode(), QLineEdit::Normal);
    box->setSelection(0, 1);
    QApplication::clipboard()->setText(
        QStringLiteral("clipboard sentinel"));

    bool sawCut = false;
    bool sawCopy = false;
    bool sawFluentMenu = false;
    bool cutEnabled = true;
    bool copyEnabled = true;
    QTimer::singleShot(0, [&]() {
        QWidget* popup = QApplication::activePopupWidget();
        auto* menu =
            qobject_cast<fluent::menus_toolbars::FluentMenu*>(
                popup);
        sawFluentMenu = menu != nullptr;
        if (!menu) {
            if (popup)
                popup->close();
            return;
        }
        for (QAction* action : menu->actions()) {
            if (actionMatchesStandardKey(action, QKeySequence::Cut)) {
                sawCut = true;
                cutEnabled = action->isEnabled();
            } else if (actionMatchesStandardKey(
                           action, QKeySequence::Copy)) {
                sawCopy = true;
                copyEnabled = action->isEnabled();
            }
        }
        menu->close();
    });

    const QPoint localPos = box->rect().center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse,
        localPos,
        box->mapToGlobal(localPos));
    QApplication::sendEvent(box, &event);
    QTest::mouseRelease(revealButton, Qt::LeftButton);
    QApplication::processEvents();

    EXPECT_TRUE(event.isAccepted());
    EXPECT_TRUE(sawFluentMenu);
    EXPECT_EQ(box->echoMode(), QLineEdit::Password);
    EXPECT_TRUE(sawCut);
    EXPECT_TRUE(sawCopy);
    EXPECT_FALSE(cutEnabled);
    EXPECT_FALSE(copyEnabled);
    EXPECT_EQ(box->password(), QStringLiteral("secret"));
    EXPECT_EQ(
        QApplication::clipboard()->text(),
        QStringLiteral("clipboard sentinel"));
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

    static QImage grabBox(QSize* logicalSize) {
        PasswordBox box;
        box.setPasswordRevealMode(PasswordBox::PasswordRevealMode::Peek);
        box.setPassword("hunter2");
        box.resize(220, 32);
        if (logicalSize)
            *logicalSize = box.size();
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

            QSize logicalSize;
            QImage image = grabBox(&logicalSize);
            ASSERT_FALSE(image.isNull()) << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(
                image.width(),
                qRound(logicalSize.width() * image.devicePixelRatio()))
                << "lang=" << lang << " theme=" << theme;
            EXPECT_EQ(
                image.height(),
                qRound(logicalSize.height() * image.devicePixelRatio()))
                << "lang=" << lang << " theme=" << theme;
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
