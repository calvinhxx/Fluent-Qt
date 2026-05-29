#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include "components/menus_toolbars/Menu.h"
#include "components/basicinput/DropDownButton.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using namespace fluent::basicinput;
using namespace fluent::textfields;
using namespace fluent::menus_toolbars;
using namespace fluent;

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
    iconOnly->setIconOffset(QPoint(-8, 0));
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
