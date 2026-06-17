#include <gtest/gtest.h>
#include <QApplication>
#include <QScrollBar>
#include <QTextEdit>
#include <QWheelEvent>
#include <QtTest/QSignalSpy>
#include "QtTestEnvironment.h"
#include "components/textfields/TextEdit.h"
#include "components/textfields/Label.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/Typography.h"

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

class TextEditTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(500, 400);
        window->setWindowTitle("Fluent TextEdit Test");
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

namespace {

QWheelEvent makeWheelEvent(QWidget* target, QPoint pixelDelta, QPoint angleDelta,
                           Qt::ScrollPhase phase = Qt::NoScrollPhase)
{
    const QPointF pos = target->rect().center();
    const QPointF globalPos = target->mapToGlobal(pos.toPoint());
    return QWheelEvent(pos, globalPos, pixelDelta, angleDelta,
                       Qt::NoButton, Qt::NoModifier, phase, false);
}

QTextEdit* innerTextEdit(TextEdit* edit)
{
    return edit ? edit->findChild<QTextEdit*>() : nullptr;
}

} // namespace

TEST_F(TextEditTest, TextAndPlaceholder) {
    TextEdit* edit = new TextEdit(window);
    edit->setPlaceholderText("Multi-line placeholder");
    EXPECT_EQ(edit->placeholderText(), "Multi-line placeholder");

    edit->setPlainText("line1\nline2");
    EXPECT_EQ(edit->toPlainText(), "line1\nline2");
}

TEST_F(TextEditTest, ContentMargins) {
    TextEdit* edit = new TextEdit(window);
    QMargins margins(12, 4, 12, 4);
    edit->setContentMargins(margins);
    EXPECT_EQ(edit->contentMargins(), margins);
}

TEST_F(TextEditTest, FluentPropertiesDefaultsAndSetters) {
    TextEdit* edit = new TextEdit(window);

    EXPECT_EQ(edit->contentMargins(),
              QMargins(Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical,
                       Spacing::Padding::TextFieldHorizontal, Spacing::Padding::TextFieldVertical));
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(edit->focusedBorderWidth(),   Spacing::Border::Focused);
    EXPECT_EQ(edit->unfocusedBorderWidth(), Spacing::Border::Normal);
    EXPECT_EQ(edit->lineHeight(), Spacing::ControlHeight::Standard);
    EXPECT_EQ(edit->minVisibleLines(), 1);
    EXPECT_EQ(edit->maxVisibleLines(), 4);

    QSignalSpy spyFocused(edit,   SIGNAL(focusedBorderWidthChanged()));
    QSignalSpy spyUnfocused(edit, SIGNAL(unfocusedBorderWidthChanged()));
    QSignalSpy spyLayout(edit,    SIGNAL(layoutMetricsChanged()));
    QSignalSpy spyFont(edit,      SIGNAL(fontRoleChanged()));

    edit->setFocusedBorderWidth(3);
    EXPECT_EQ(edit->focusedBorderWidth(), 3);
    EXPECT_EQ(spyFocused.count(), 1);

    edit->setUnfocusedBorderWidth(2);
    EXPECT_EQ(edit->unfocusedBorderWidth(), 2);
    EXPECT_EQ(spyUnfocused.count(), 1);

    edit->setMinVisibleLines(2);
    EXPECT_EQ(edit->minVisibleLines(), 2);
    EXPECT_EQ(spyLayout.count(), 1);

    edit->setMaxVisibleLines(6);
    EXPECT_EQ(edit->maxVisibleLines(), 6);
    EXPECT_EQ(spyLayout.count(), 2);

    edit->setFontRole(Typography::FontRole::Subtitle);
    EXPECT_EQ(edit->fontRole(), Typography::FontRole::Subtitle);
    EXPECT_EQ(spyFont.count(), 1);

    // 相同值不应再次触发信号
    edit->setFocusedBorderWidth(3);
    edit->setUnfocusedBorderWidth(2);
    edit->setMinVisibleLines(2);
    EXPECT_EQ(spyFocused.count(), 1);
    EXPECT_EQ(spyUnfocused.count(), 1);
    EXPECT_EQ(spyLayout.count(), 2);
}

TEST_F(TextEditTest, MinVisibleLinesClampsBelowContent) {
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(2);
    edit->setMaxVisibleLines(4);

    // height = clampedLines × lineHeight（无额外 top/bottom padding）
    EXPECT_EQ(edit->height(), 2 * 32);

    edit->setPlainText("A\nB\nC");
    EXPECT_EQ(edit->height(), 3 * 32);

    edit->clear();
    EXPECT_EQ(edit->height(), 2 * 32);
}

TEST_F(TextEditTest, MaxVisibleLinesClampsAboveContent) {
    TextEdit* edit = new TextEdit(window);
    edit->setLineHeight(32);
    edit->setMinVisibleLines(1);
    edit->setMaxVisibleLines(3);

    // 写入超过 3 行：高度固定在 maxVisibleLines × lineHeight，滚动条出现
    edit->setPlainText("A\nB\nC\nD\nE");
    EXPECT_EQ(edit->height(), 3 * 32);
}

TEST_F(TextEditTest, SingleLineDefaultHeight) {
    // 默认 minVisibleLines=1：空控件高度应与单行 TextBox 等高（lineHeight = 32）
    TextEdit* edit = new TextEdit(window);
    EXPECT_EQ(edit->height(), Spacing::ControlHeight::Standard);
}

TEST_F(TextEditTest, ReadOnly) {
    TextEdit* edit = new TextEdit(window);
    edit->setPlainText("read only content");
    edit->setReadOnly(true);
    EXPECT_TRUE(edit->isReadOnly());
    edit->setReadOnly(false);
    EXPECT_FALSE(edit->isReadOnly());
}

TEST_F(TextEditTest, ScrollChainingPropertyControlsBoundaryWheel) {
    TextEdit* edit = new TextEdit(window);
    edit->setMinVisibleLines(3);
    edit->setMaxVisibleLines(3);
    edit->setPlainText("A\nB\nC\nD\nE\nF\nG\nH");
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner->verticalScrollBar(), nullptr);
    ASSERT_GT(inner->verticalScrollBar()->maximum(), inner->verticalScrollBar()->minimum());

    EXPECT_FALSE(edit->isScrollChainingEnabled());
    QSignalSpy spy(edit, &TextEdit::scrollChainingEnabledChanged);

    inner->verticalScrollBar()->setValue(inner->verticalScrollBar()->maximum());
    QWheelEvent containedWheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    containedWheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &containedWheel);
    QApplication::processEvents();

    EXPECT_TRUE(containedWheel.isAccepted());
    EXPECT_EQ(inner->verticalScrollBar()->value(), inner->verticalScrollBar()->maximum());

    edit->setScrollChainingEnabled(true);
    EXPECT_TRUE(edit->isScrollChainingEnabled());
    EXPECT_EQ(spy.count(), 1);
    edit->setScrollChainingEnabled(true);
    EXPECT_EQ(spy.count(), 1);

    QWheelEvent chainedWheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    chainedWheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &chainedWheel);
    QApplication::processEvents();

    EXPECT_FALSE(chainedWheel.isAccepted());
    EXPECT_EQ(inner->verticalScrollBar()->value(), inner->verticalScrollBar()->maximum());
}

TEST_F(TextEditTest, WheelPassesThroughWhenContentFits) {
    TextEdit* edit = new TextEdit(window);
    edit->setMinVisibleLines(3);
    edit->setMaxVisibleLines(3);
    edit->setPlainText("A\nB");
    layout->addWidget(edit);
    window->show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(inner->verticalScrollBar(), nullptr);
    ASSERT_EQ(inner->verticalScrollBar()->maximum(), inner->verticalScrollBar()->minimum());

    QWheelEvent wheel = makeWheelEvent(inner->viewport(), QPoint(0, 0), QPoint(0, -120));
    wheel.setAccepted(false);
    QApplication::sendEvent(inner->viewport(), &wheel);
    QApplication::processEvents();

    EXPECT_FALSE(wheel.isAccepted());
}

TEST_F(TextEditTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    Label* header = new Label("TextEdit - 自适应行高 + 垂直居中:", window);
    header->anchors()->top  = {window, Edge::Top,  30};
    header->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header);

    // 默认 1 行（同 LineEdit 高度），自动居中
    TextEdit* edit1 = new TextEdit(window);
    edit1->setPlaceholderText("Type here... (auto grows up to 4 lines)");
    edit1->anchors()->top   = {header, Edge::Bottom, 8};
    edit1->anchors()->left  = {window, Edge::Left, 40};
    edit1->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit1);

    Label* header2 = new Label("预填 2 行（高度 = 64px）:", window);
    header2->anchors()->top  = {edit1, Edge::Bottom, 12};
    header2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(header2);

    TextEdit* edit2 = new TextEdit(window);
    edit2->setPlainText("First line\nSecond line");
    edit2->anchors()->top   = {header2, Edge::Bottom, 8};
    edit2->anchors()->left  = {window, Edge::Left, 40};
    edit2->anchors()->right = {window, Edge::Right, -40};
    layout->addWidget(edit2);

    Button* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFluentStyle(Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -30};
    themeBtn->anchors()->right  = {window, Edge::Right,  -30};
    layout->addWidget(themeBtn);

    QObject::connect(themeBtn, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
    });

    window->show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    qApp->exec();
}
