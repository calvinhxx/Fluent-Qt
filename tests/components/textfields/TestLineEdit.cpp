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
