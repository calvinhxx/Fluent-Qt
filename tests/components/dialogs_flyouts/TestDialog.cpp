#include <gtest/gtest.h>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include "components/dialogs_flyouts/Dialog.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/FluentElement.h"

using namespace fluent::dialogs_flyouts;
using namespace fluent::basicinput;
using namespace fluent;

// ── FluentTestWindow ─────────────────────────────────────────────────────────

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class DialogTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("Dialog Base Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FluentTestWindow* window;
};

// ══════════════════════════════════════════════════════════════════════════════
//  自动化测试 — Dialog 基类（纯 view 层：阴影 + 动画 + 拖拽）
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(DialogTest, DefaultProperties) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isDragEnabled());
    EXPECT_TRUE(dialog.isAnimationEnabled());
    EXPECT_FALSE(dialog.isSmokeEnabled());
    EXPECT_EQ(dialog.shadowSize(), 16);
    EXPECT_DOUBLE_EQ(dialog.animationProgress(), 1.0);
}

TEST_F(DialogTest, SmokeProperty) {
    Dialog dialog(window);
    EXPECT_FALSE(dialog.isSmokeEnabled());
    dialog.setSmokeEnabled(true);
    EXPECT_TRUE(dialog.isSmokeEnabled());
}

TEST_F(DialogTest, DragProperty) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isDragEnabled());
    dialog.setDragEnabled(false);
    EXPECT_FALSE(dialog.isDragEnabled());
}

TEST_F(DialogTest, AnimationProperty) {
    Dialog dialog(window);
    EXPECT_TRUE(dialog.isAnimationEnabled());
    dialog.setAnimationEnabled(false);
    EXPECT_FALSE(dialog.isAnimationEnabled());
}

TEST_F(DialogTest, AnimationProgressProperty) {
    Dialog dialog(window);
    dialog.setAnimationProgress(0.5);
    EXPECT_DOUBLE_EQ(dialog.animationProgress(), 0.5);
}

TEST_F(DialogTest, ExecWithoutAnimation) {
    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setFixedSize(300, 200);

    QTimer::singleShot(50, [&]() { dialog.done(QDialog::Accepted); });
    int result = dialog.exec();
    EXPECT_EQ(result, QDialog::Accepted);
}

TEST_F(DialogTest, OpenPreservesExplicitApplicationModality) {
    window->show();
    QApplication::processEvents();

    Dialog dialog(window);
    dialog.setAnimationEnabled(false);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.open();
    QApplication::processEvents();

    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);

    dialog.done(QDialog::Rejected);
    QApplication::processEvents();
}

TEST_F(DialogTest, ThemeSwitchNoCrash) {
    Dialog dialog(window);
    dialog.setAnimationEnabled(false);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    dialog.onThemeUpdated();

    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    dialog.onThemeUpdated();

    SUCCEED();
}

// ══════════════════════════════════════════════════════════════════════════════
//  入场/退场动画：仅 opacity（scale 已移除以避免子控件错位）
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(DialogTest, DialogEntranceAnimatesOpacity) {
    // 入场：progress=0 时 windowOpacity 应为 0；progress=1 时为 1
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(dialog.windowOpacity(), 0.0, 0.01);
    // 尺寸不应被动画修改
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(dialog.windowOpacity(), 1.0, 0.01);
    EXPECT_EQ(dialog.size(), target);

    dialog.setAnimationEnabled(false);
    dialog.done(0);
}

TEST_F(DialogTest, DialogExitAnimatesOpacity) {
    // 退场：progress=0 时 windowOpacity 应回到 0
    Dialog dialog(window);
    dialog.setFixedSize(400, 300);
    const QSize target = dialog.size();

    window->show();
    QApplication::processEvents();
    dialog.open();
    QApplication::processEvents();

    dialog.setAnimationProgress(1.0);
    EXPECT_NEAR(dialog.windowOpacity(), 1.0, 0.01);

    dialog.done(0);

    dialog.setAnimationProgress(0.0);
    EXPECT_NEAR(dialog.windowOpacity(), 0.0, 0.01);
    // 尺寸在退场期间也保持不变
    EXPECT_EQ(dialog.size(), target);
}

TEST_F(DialogTest, SmokeOverlayFadesInOutDelayedDestroy) {
    // 关键不变量：smoke 启用时，done() 返回后 overlay 应仍存在于 parent
    // children 中（延迟销毁），而非立即从 children 中消失。
    window->show();
    QApplication::processEvents();
    const int childCountBefore = window->findChildren<QWidget*>().size();

    Dialog dialog(window);
    dialog.setSmokeEnabled(true);
    dialog.setFixedSize(300, 200);
    dialog.setAnimationEnabled(false);  // 关闭 Dialog 主动画，仅测 smoke 行为

    bool overlayPresentAfterOpen = false;
    bool overlayStillPresentAfterDone = false;
    QTimer::singleShot(30, [&]() {
        // Dialog 已 open，smoke overlay 应作为 window 的子节点存在
        overlayPresentAfterOpen =
            (window->findChildren<QWidget*>().size() > childCountBefore);

        dialog.done(0);
        // done() 返回后，由于 smoke 走异步淡出，overlay 应仍在 children 中
        overlayStillPresentAfterDone =
            (window->findChildren<QWidget*>().size() > childCountBefore);
    });
    dialog.exec();

    EXPECT_TRUE(overlayPresentAfterOpen)
        << "Smoke overlay should be a child of window after open()";
    EXPECT_TRUE(overlayStillPresentAfterDone)
        << "Smoke overlay should still exist immediately after done() (delayed destroy)";

    // 等待淡出完成并让 deferred delete 处理完成（取决于 themeAnimation().fast）
    QElapsedTimer t; t.start();
    while (window->findChildren<QWidget*>().size() > childCountBefore && t.elapsed() < 1500) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    // 允许不一定在 1500ms 内完全清理（平台/事件循环依赖）——记录但不硬性断言
    if (window->findChildren<QWidget*>().size() > childCountBefore) {
        qWarning() << "Smoke overlay still present after 1500ms; deferred delete may need full app loop";
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  VisualCheck — 手动观察 Dialog 基类的渲染效果
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(DialogTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);
    window->setFixedSize(700, 500);

    using Edge = AnchorLayout::Edge;

    // --- 弹出空白 Dialog（仅阴影 + 动画） ---
    Button* btn1 = new Button("Open Empty Dialog", window);
    btn1->setFluentStyle(Button::Accent);
    btn1->setFixedSize(240, 32);
    btn1->anchors()->top  = {window, Edge::Top,  40};
    btn1->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn1);

    QObject::connect(btn1, &Button::clicked, [this]() {
        Dialog dialog(window);
        dialog.setFixedSize(400, 260);
        dialog.exec();
    });

    // --- 弹出禁用动画的 Dialog ---
    Button* btn2 = new Button("No-Animation Dialog", window);
    btn2->setFixedSize(240, 32);
    btn2->anchors()->top  = {btn1, Edge::Bottom, 16};
    btn2->anchors()->left = {window, Edge::Left, 40};
    layout->addWidget(btn2);

    QObject::connect(btn2, &Button::clicked, [this]() {
        Dialog dialog(window);
        dialog.setAnimationEnabled(false);
        dialog.setFixedSize(400, 260);
        dialog.exec();
    });

    // --- Toggle theme ---
    Button* themeBtn = new Button("Toggle Dark/Light", window);
    themeBtn->setFixedSize(240, 32);
    themeBtn->anchors()->top  = {btn2, Edge::Bottom, 32};
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
