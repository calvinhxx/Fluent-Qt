#include <gtest/gtest.h>
#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QTimer>
#include "QtTestEnvironment.h"
#include "components/foundation/QMLPlus.h"
#include "utils/DebugOverlay.h"

using namespace fluent;

class AnchorLayoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new QWidget();
        window->setFixedSize(600, 600);
        window->setWindowTitle("AnchorLayout UT Visual Check");
        layout = new AnchorLayout(window);
        window->setLayout(layout);
    }

    void TearDown() override {
        delete window;
    }

    QWidget* window;
    AnchorLayout* layout;
};

TEST_F(AnchorLayoutTest, Contract_AnchorsResolveDeterministicallyWithoutVisualSession) {
    QWidget* child = new QWidget(window);
    child->setFixedSize(80, 40);

    AnchorLayout::Anchors anchors;
    anchors.left = {window, AnchorLayout::Edge::Left, 12};
    anchors.top = {window, AnchorLayout::Edge::Top, 16};
    layout->addAnchoredWidget(child, anchors);

    layout->setGeometry(window->rect());

    EXPECT_EQ(child->geometry(), QRect(12, 16, 80, 40));
}

TEST_F(AnchorLayoutTest, Contract_RightAndBottomEdgesUseExactVisualBoundaries) {
    QWidget* source = new QWidget(window);
    source->setFixedSize(80, 40);
    AnchorLayout::Anchors sourceAnchors;
    sourceAnchors.left = {window, AnchorLayout::Edge::Left, 12};
    sourceAnchors.top = {window, AnchorLayout::Edge::Top, 16};
    layout->addAnchoredWidget(source, sourceAnchors);

    QWidget* adjacent = new QWidget(window);
    adjacent->setFixedSize(30, 20);
    AnchorLayout::Anchors adjacentAnchors;
    adjacentAnchors.left = {source, AnchorLayout::Edge::Right, 7};
    adjacentAnchors.top = {source, AnchorLayout::Edge::Bottom, 5};
    layout->addAnchoredWidget(adjacent, adjacentAnchors);

    QWidget* trailing = new QWidget(window);
    trailing->setFixedSize(30, 20);
    AnchorLayout::Anchors trailingAnchors;
    trailingAnchors.right = {window, AnchorLayout::Edge::Right, -11};
    trailingAnchors.bottom = {window, AnchorLayout::Edge::Bottom, -13};
    layout->addAnchoredWidget(trailing, trailingAnchors);

    layout->setGeometry(window->rect());

    EXPECT_EQ(adjacent->geometry(), QRect(99, 61, 30, 20));
    EXPECT_EQ(trailing->geometry(), QRect(559, 567, 30, 20));
}

TEST_F(AnchorLayoutTest, Contract_FillOverridesWidgetSizePolicy) {
    QPushButton* fill = new QPushButton(window);
    ASSERT_EQ(fill->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);

    AnchorLayout::Anchors anchors;
    anchors.fill = true;
    anchors.fillMargins = QMargins(10, 20, 30, 40);
    layout->addAnchoredWidget(fill, anchors);

    layout->setGeometry(window->rect());

    EXPECT_EQ(fill->geometry(), QRect(10, 20, 560, 540));
}

TEST_F(AnchorLayoutTest, Contract_NonWidgetLayoutItemsDoNotCrashGeometryPass) {
    auto* spacer = new QSpacerItem(
        20, 10, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addItem(spacer);

    layout->setGeometry(window->rect());

    ASSERT_EQ(layout->count(), 1);
    EXPECT_NE(layout->itemAt(0), nullptr);
    EXPECT_EQ(spacer->geometry(), QRect(0, 0, 20, 10));
}

TEST_F(AnchorLayoutTest, FullScenarioVisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = AnchorLayout::Edge;

    // 1) 左上：锚定到父控件 (10, 10)
    QPushButton* btn1 = new QPushButton("左上", window);
    btn1->setFixedSize(100, 100);
    AnchorLayout::Anchors a1;
    a1.left = {window, Edge::Left, 10};
    a1.top  = {window, Edge::Top, 10};
    layout->addAnchoredWidget(btn1, a1);
    new DebugOverlay(btn1, Qt::red, window);

    // 2) btn2 放在 btn1 右侧 (+20)
    QPushButton* btn2 = new QPushButton("右侧 +20", window);
    btn2->setFixedSize(100, 100);
    AnchorLayout::Anchors a2;
    a2.left = {btn1, Edge::Right, 20}; 
    a2.top  = {window, Edge::Top, 10};
    layout->addAnchoredWidget(btn2, a2);
    new DebugOverlay(btn2, Qt::blue, window);

    // 3) 全居中 (250, 250)
    QPushButton* btn3 = new QPushButton("全居中", window);
    btn3->setFixedSize(100, 100);
    AnchorLayout::Anchors a3;
    a3.horizontalCenter = {window, Edge::HCenter, 0};
    a3.verticalCenter   = {window, Edge::VCenter, 0};
    layout->addAnchoredWidget(btn3, a3);
    new DebugOverlay(btn3, Qt::magenta, window);

    // 4) 仅水平居中 (位于底部 -20)
    QPushButton* btn6 = new QPushButton("仅水平居中", window);
    btn6->setFixedSize(120, 40);
    AnchorLayout::Anchors a6;
    a6.bottom = {window, Edge::Bottom, -20};
    a6.horizontalCenter = {window, Edge::HCenter, 0};
    layout->addAnchoredWidget(btn6, a6);
    new DebugOverlay(btn6, Qt::cyan, window);

    // 5) 填充区域 (Margins: 0, 200, 400, 200)
    QPushButton* btn4 = new QPushButton("填充区域", window);
    AnchorLayout::Anchors a4;
    a4.fill = true;
    a4.fillMargins = QMargins(0, 200, 400, 200); 
    layout->addAnchoredWidget(btn4, a4);
    new DebugOverlay(btn4, Qt::darkYellow, window);

    // 6) 右下锚定 (-16, -16)
    QPushButton* btn5 = new QPushButton("右下", window);
    btn5->setFixedSize(100, 100);
    AnchorLayout::Anchors a5;
    a5.right  = {window, Edge::Right, -16};
    a5.bottom = {window, Edge::Bottom, -16};
    layout->addAnchoredWidget(btn5, a5);
    new DebugOverlay(btn5, Qt::black, window);

    // 7) 动态 Resize 测试：QLabel 内容在 3 秒后改变
    QLabel* labelDynamic = new QLabel("等待 3 秒后文字会变长...", window);
    labelDynamic->setStyleSheet("background-color: #3498db; color: white; padding: 5px;");
    AnchorLayout::Anchors aDyn;
    aDyn.left = {window, Edge::Left, 20};
    aDyn.top  = {btn1, Edge::Bottom, 20};
    layout->addAnchoredWidget(labelDynamic, aDyn);
    new DebugOverlay(labelDynamic, Qt::red, window);

    QTimer::singleShot(3000, [labelDynamic]() {
        labelDynamic->setText("成功！文字变长了，布局和 DebugLine 应该自动跟随。");
    });

    // 8) 兄弟控件可见性变化测试
    QPushButton* btnA = new QPushButton("控件 A (点我隐藏)", window);
    btnA->setFixedSize(100, 40);
    AnchorLayout::Anchors aA;
    aA.left = {window, Edge::Left, 20};
    aA.top  = {labelDynamic, Edge::Bottom, 40};
    layout->addAnchoredWidget(btnA, aA);

    QPushButton* btnB = new QPushButton("控件 B (锚定 A 右侧)", window);
    btnB->setFixedSize(150, 40);
    AnchorLayout::Anchors aB;
    aB.left = {btnA, Edge::Right, 10}; // 锚定到 A 的右侧 10px
    aB.top  = {btnA, Edge::Top, 0};
    layout->addAnchoredWidget(btnB, aB);

    QLabel* statusLabel = new QLabel("A 可见, B 锚定 A 右侧", window);
    AnchorLayout::Anchors aStatus;
    aStatus.left = {btnB, Edge::Left, 0};
    aStatus.top = {btnB, Edge::Bottom, 5};
    layout->addAnchoredWidget(statusLabel, aStatus);

    new DebugOverlay(btnA, Qt::green, window);
    new DebugOverlay(btnB, Qt::yellow, window);

    // 点击 A 隐藏 A
    QObject::connect(btnA, &QPushButton::clicked, [btnA, statusLabel]() {
        btnA->hide();
        statusLabel->setText("A 已隐藏, B 位置应保持不变 (符合 Anchor 规范)");
    });

    // 增加一个重置按钮
    QPushButton* resetBtn = new QPushButton("显示 A", window);
    resetBtn->setFixedSize(80, 30);
    AnchorLayout::Anchors aReset;
    aReset.right = {window, Edge::Right, -10};
    aReset.top = {window, Edge::Top, 10};
    layout->addAnchoredWidget(resetBtn, aReset);
    QObject::connect(resetBtn, &QPushButton::clicked, [btnA, statusLabel]() {
        btnA->show();
        statusLabel->setText("A 已恢复, B 锚定 A 右侧");
    });

    window->show();

    // 验证初始位置
    EXPECT_EQ(btn1->pos(), QPoint(10, 10));
    EXPECT_EQ(btn2->x(), 130);
    
    int W = window->width();
    int H = window->height();
    EXPECT_EQ(btn3->pos(), QPoint((W - 100) / 2, (H - 100) / 2));
    EXPECT_EQ(btn6->pos(), QPoint((W - 120) / 2, H - 40 - 20));
    EXPECT_EQ(btn4->geometry(), QRect(0, 200, W - 400, H - 400));
    EXPECT_EQ(btn5->pos(), QPoint(W - 100 - 16, H - 100 - 16));

    if (tests::support::shouldCaptureVisualSnapshot()) {
        ASSERT_TRUE(tests::support::captureVisualSnapshot(window));
        return;
    }

    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") || qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen") {
        qApp->exec();
    }
}
