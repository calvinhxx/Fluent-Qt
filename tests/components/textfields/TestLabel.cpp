#include <gtest/gtest.h>
#include <QApplication>
#include <QFontMetrics>
#include <QFrame>
#include <QTest>
#include <QVBoxLayout>
#include <QtTest/QSignalSpy>
#include "QtTestEnvironment.h"
#include "compatibility/QtCompat.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"
#include "components/status_info/ToolTip.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

using namespace fluent::textfields;
using namespace fluent::basicinput;
using namespace fluent;

namespace {
QString renderedLabelText(const Label* label) {
    return static_cast<const QLabel*>(label)->text();
}

fluent::status_info::ToolTip* elideToolTip(Label* label) {
    return label->findChild<fluent::status_info::ToolTip*>(
        QStringLiteral("LabelElideToolTip"), Qt::FindDirectChildrenOnly);
}

void sendEnter(QWidget* widget) {
    FLUENT_MAKE_ENTER_EVENT(event, 4, 4);
    QApplication::sendEvent(widget, &event);
    QApplication::processEvents();
}

void sendLeave(QWidget* widget) {
    QEvent event(QEvent::Leave);
    QApplication::sendEvent(widget, &event);
    QApplication::processEvents();
}
}

class LabelTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new QWidget();
        window->setFixedSize(800, 600);
        window->setWindowTitle("Fluent Typography Persistence Test");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
    }

    void TearDown() override {
        delete window;
    }

    QWidget* window;
    AnchorLayout* layout;
};

TEST_F(LabelTest, DefaultConstructor) {
    Label* label = new Label(window);
    EXPECT_EQ(label->text(), "");
    EXPECT_EQ(label->fluentTypography(), "Body");
    EXPECT_EQ(label->textElideMode(), Qt::ElideNone);
    EXPECT_NE(dynamic_cast<QLabel*>(label), nullptr);
}

TEST_F(LabelTest, TextConstructor) {
    Label* label = new Label("Static label text", window);
    EXPECT_EQ(label->text(), "Static label text");
    EXPECT_EQ(label->fluentTypography(), "Body");
}

TEST_F(LabelTest, FluentTypographyChange) {
    Label* label = new Label(window);
    QSignalSpy spy(label, SIGNAL(typographyChanged()));

    label->setFluentTypography("Title");
    EXPECT_EQ(label->fluentTypography(), "Title");
    EXPECT_EQ(spy.count(), 1);

    label->setFluentTypography("Title");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(LabelTest, ThemeUpdatePreservesTypography) {
    Label* label = new Label("Theme text", window);
    label->setFluentTypography("Caption");

    label->onThemeUpdated();

    EXPECT_EQ(label->fluentTypography(), "Caption");
    EXPECT_TRUE(label->palette().color(QPalette::WindowText).isValid());
}

TEST_F(LabelTest, TextColorRoleColorsViaOwnStyleSheet) {
    Label* label = new Label("Role text", window);

    // Default role keeps the legacy palette coloring and sets no own color style sheet, so
    // components that drive the label palette themselves (InfoBar, ToolTip, …) are untouched.
    EXPECT_EQ(label->textColorRole(), Label::TextColorRole::Default);
    EXPECT_FALSE(label->styleSheet().contains(QStringLiteral("color:")));
    EXPECT_TRUE(label->palette().color(QPalette::WindowText).isValid());

    // An explicit role colors through the label's OWN style sheet so an ancestor style sheet
    // (QStyleSheetStyle) can't drop it — the fix for value labels on a styled preview surface.
    label->setTextColorRole(Label::TextColorRole::Secondary);
    EXPECT_EQ(label->textColorRole(), Label::TextColorRole::Secondary);
    EXPECT_TRUE(label->styleSheet().contains(QStringLiteral("color:")));

    // The role survives a theme refresh (re-applied, not lost).
    label->onThemeUpdated();
    EXPECT_EQ(label->textColorRole(), Label::TextColorRole::Secondary);
    EXPECT_TRUE(label->styleSheet().contains(QStringLiteral("color:")));
}

TEST_F(LabelTest, ThemeUpdatePreservesExplicitFont) {
    Label* label = new Label("\uE790", window);
    QFont iconFont(Typography::FontFamily::FluentIcons);
    iconFont.setPixelSize(22);
    label->setFont(iconFont);

    FluentElement::setTheme(FluentElement::Dark);
    EXPECT_EQ(label->font().family(), Typography::FontFamily::FluentIcons);
    EXPECT_EQ(label->font().pixelSize(), 22);

    FluentElement::setTheme(FluentElement::Light);
}

TEST_F(LabelTest, DefaultElideModePreservesText) {
    const QString text = "Full label text";
    Label* label = new Label(text, window);
    QSignalSpy spy(label, &Label::textElideModeChanged);

    EXPECT_EQ(label->textElideMode(), Qt::ElideNone);
    EXPECT_EQ(label->text(), text);
    EXPECT_EQ(renderedLabelText(label), text);
    EXPECT_FALSE(label->isTextElided());

    label->setTextElideMode(Qt::ElideNone);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(LabelTest, ElideRightRendersConstrainedTextAndKeepsFullText) {
    const QString fullText = "This is a long Label value that should be truncated";
    Label* label = new Label(fullText, window);
    label->resize(96, 24);
    label->setTextElideMode(Qt::ElideRight);

    const QString expected = QFontMetrics(label->font()).elidedText(
        fullText, Qt::ElideRight, label->contentsRect().width());

    EXPECT_NE(expected, fullText);
    EXPECT_EQ(label->text(), fullText);
    EXPECT_EQ(renderedLabelText(label), expected);
    EXPECT_TRUE(label->isTextElided());
}

TEST_F(LabelTest, HoverShowsFullTextToolTipOnlyWhenActuallyElided) {
    const QString fullText = "A long Label value that needs a tooltip when clipped";
    Label* label = new Label(fullText, window);
    label->setGeometry(12, 12, 90, 24);
    label->setTextElideMode(Qt::ElideRight);
    ASSERT_TRUE(label->isTextElided());

    sendEnter(label);

    auto* tip = elideToolTip(label);
    ASSERT_NE(tip, nullptr);
    EXPECT_EQ(tip->text(), fullText);
    EXPECT_TRUE(tip->isVisible());

    sendLeave(label);
    QTRY_VERIFY_WITH_TIMEOUT(!tip->isVisible(), 1000);

    Label* wideLabel = new Label(fullText, window);
    wideLabel->setGeometry(12, 48, 800, 24);
    wideLabel->setTextElideMode(Qt::ElideRight);
    ASSERT_FALSE(wideLabel->isTextElided());

    sendEnter(wideLabel);
    EXPECT_EQ(elideToolTip(wideLabel), nullptr);
}

TEST_F(LabelTest, ElideStateRecomputesWhenInputsChange) {
    const QString longText = "A Label value long enough to be elided in a compact column";
    const QString updatedLongText = "Updated Label value that is still long enough for elision";
    Label* label = new Label(longText, window);
    label->setGeometry(12, 12, 92, 24);
    window->show();
    QApplication::processEvents();
    label->setTextElideMode(Qt::ElideRight);
    ASSERT_TRUE(label->isTextElided());

    const int wideEnoughWidth = QFontMetrics(label->font()).horizontalAdvance(longText) + 20;
    label->resize(wideEnoughWidth, 24);
    QApplication::processEvents();
    EXPECT_FALSE(label->isTextElided());
    EXPECT_EQ(renderedLabelText(label), longText);

    label->resize(92, 24);
    label->setText("Short");
    EXPECT_FALSE(label->isTextElided());
    EXPECT_EQ(label->text(), QStringLiteral("Short"));
    EXPECT_EQ(renderedLabelText(label), QStringLiteral("Short"));

    label->setText(longText);
    ASSERT_TRUE(label->isTextElided());
    sendEnter(label);
    auto* tip = elideToolTip(label);
    ASSERT_NE(tip, nullptr);

    label->setText(updatedLongText);
    EXPECT_EQ(label->text(), updatedLongText);
    EXPECT_TRUE(label->isTextElided());
    EXPECT_EQ(tip->text(), updatedLongText);

    label->setFluentTypography("Title");
    QString expected = QFontMetrics(label->font()).elidedText(
        updatedLongText, Qt::ElideRight, label->contentsRect().width());
    EXPECT_EQ(renderedLabelText(label), expected);
    EXPECT_EQ(label->isTextElided(), expected != updatedLongText);

    label->onThemeUpdated();
    expected = QFontMetrics(label->font()).elidedText(
        updatedLongText, Qt::ElideRight, label->contentsRect().width());
    EXPECT_EQ(renderedLabelText(label), expected);
    EXPECT_EQ(label->isTextElided(), expected != updatedLongText);
}

TEST_F(LabelTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    window->setFixedSize(1240, 680);
    window->setWindowTitle("Label Gallery");

    auto createTypographyLabel = [&](const QString& text, const QString& styleName, QWidget* anchor, int margin = 20) {
        Label* l = new Label(text + " (" + styleName + ")", window);
        // --- 核心修复：使用属性接口，内部会自动记忆并在切换主题时持久化 ---
        l->setFluentTypography(styleName);
        
        l->anchors()->top = {anchor, Edge::Bottom, margin};
        l->anchors()->left = {window, Edge::Left, 40};
        layout->addWidget(l);
        return l;
    };

    auto createElideRow = [&](const QString& captionText,
                              const QString& valueText,
                              Qt::TextElideMode elideMode,
                              int valueWidth,
                              QWidget* anchor,
                              int margin = 16) {
        Label* captionLabel = new Label(captionText, window);
        captionLabel->setFluentTypography("Caption");
        captionLabel->setFixedSize(130, 28);
        captionLabel->anchors()->top = {anchor, Edge::Bottom, margin};
        captionLabel->anchors()->left = {window, Edge::Left, 560};
        layout->addWidget(captionLabel);

        Label* valueLabel = new Label(valueText, window);
        valueLabel->setFluentTypography("Body");
        valueLabel->setTextElideMode(elideMode);
        valueLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        valueLabel->setContentsMargins(6, 0, 6, 0);
        valueLabel->setFrameShape(QFrame::Box);
        valueLabel->setLineWidth(1);
        valueLabel->setFixedSize(valueWidth, 30);
        valueLabel->anchors()->top = {captionLabel, Edge::Top, 0};
        valueLabel->anchors()->left = {window, Edge::Left, 700};
        layout->addWidget(valueLabel);
        return valueLabel;
    };

    // 1. Display (最顶层锚定)
    Label* display = new Label("Fluent UI (Display)", window);
    display->setFluentTypography("Display");
    display->anchors()->top = {window, Edge::Top, 30};
    display->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(display);
    
    // 2. 其余阶梯
    Label* titleLarge = createTypographyLabel("Large Title", "TitleLarge", display);
    Label* title = createTypographyLabel("Standard Title", "Title", titleLarge);
    Label* subtitle = createTypographyLabel("Subtitle Text", "Subtitle", title);
    Label* bodyStrong = createTypographyLabel("Strong Body Text", "BodyStrong", subtitle);
    
    Label* body = new Label("Standard Body Text (Default)", window);
    body->anchors()->top = {bodyStrong, Edge::Bottom, 20};
    body->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(body);
    
    Label* caption = createTypographyLabel("Small Caption Text", "Caption", body);

    Label* elideTitle = new Label("Elide + ToolTip", window);
    elideTitle->setFluentTypography("Title");
    elideTitle->anchors()->top = {window, Edge::Top, 40};
    elideTitle->anchors()->left = {window, Edge::Left, 560};
    layout->addWidget(elideTitle);

    Label* elideHint = new Label("Hover clipped values to see the full text.", window);
    elideHint->setFluentTypography("Caption");
    elideHint->anchors()->top = {elideTitle, Edge::Bottom, 8};
    elideHint->anchors()->left = {window, Edge::Left, 560};
    layout->addWidget(elideHint);

    const QString releaseText =
        "Quarterly release note summary with a long title that does not fit in the compact column";
    Label* rightElide = createElideRow("ElideRight",
                                       releaseText,
                                       Qt::ElideRight,
                                       190,
                                       elideHint,
                                       22);

    createElideRow("ElideMiddle",
                   "/Users/calvinhxx/Ws/Fluent-QT/src/components/textfields/Label.cpp",
                   Qt::ElideMiddle,
                   260,
                   rightElide);

    createElideRow("ElideLeft",
                   "Build pipeline artifact Fluent-Qt-macos-debug-test-label-bundle",
                   Qt::ElideLeft,
                   240,
                   rightElide,
                   64);

    Label* fitLabel = createElideRow("Fits",
                                     "Short value with no tooltip",
                                     Qt::ElideRight,
                                     260,
                                     rightElide,
                                     106);

    Label* resizeDemo = createElideRow("Resizable",
                                       "Click the button to switch this label between clipped and full width",
                                       Qt::ElideRight,
                                       170,
                                       fitLabel,
                                       18);

    Button* widthBtn = new Button("Toggle Width", window);
    widthBtn->setFixedSize(120, 32);
    widthBtn->anchors()->top = {resizeDemo, Edge::Bottom, 12};
    widthBtn->anchors()->left = {window, Edge::Left, 700};
    layout->addWidget(widthBtn);

    QObject::connect(widthBtn, &Button::clicked, [resizeDemo, widthBtn, this]() {
        const bool expand = resizeDemo->width() < 300;
        resizeDemo->setFixedWidth(expand ? 420 : 170);
        widthBtn->setText(expand ? "Compact" : "Expand");
        if (layout) {
            layout->invalidate();
            layout->activate();
        }
    });

    // --- 主题切换 ---
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
