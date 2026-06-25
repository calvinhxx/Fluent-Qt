#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <cstdlib>

#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "design/Spacing.h"

using namespace fluent::dialogs_flyouts;
using fluent::AnchorLayout;
using fluent::basicinput::Button;
using fluent::textfields::Label;

// 与 Popup.cpp 内部 kShadowMargin 保持一致
static constexpr int kShadowMargin = ::Spacing::Standard;  // 16

// ── FluentTestWindow ─────────────────────────────────────────────────────────
class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── Fixture ──────────────────────────────────────────────────────────────────
class FlyoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FluentTestWindow();
        window->setFixedSize(800, 600);
        window->setWindowTitle("Flyout Test");
        window->onThemeUpdated();
        window->show();
        QTest::qWaitForWindowExposed(window);
    }
    void TearDown() override { delete window; window = nullptr; }

    /// 创建固定尺寸 anchor 按钮并放到指定位置
    Button* makeAnchor(const QPoint& pos, const QSize& sz = QSize(100, 32)) {
        auto* btn = new Button("Anchor", window);
        btn->setFixedSize(sz);
        btn->move(pos);
        btn->show();
        return btn;
    }

    /// 等到 flyout 完成 open（含动画）
    void waitOpen(Flyout* fl) {
        QTest::qWaitFor([&]() { return fl->isOpen(); }, 1500);
    }

    FluentTestWindow* window = nullptr;
};

// ══════════════════════════════════════════════════════════════════════════════
// 1. 默认属性
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, DefaultProperties) {
    Flyout fl(window);
    EXPECT_EQ(fl.placement(),     Flyout::Bottom);
    EXPECT_EQ(fl.anchorOffset(),  8);
    EXPECT_TRUE(fl.clampToWindow());
    EXPECT_FALSE(fl.isModal());
    EXPECT_FALSE(fl.isDim());
    EXPECT_TRUE(fl.closePolicy().testFlag(Flyout::CloseOnPressOutside));
    EXPECT_TRUE(fl.closePolicy().testFlag(Flyout::CloseOnEscape));
    EXPECT_EQ(fl.anchor(), nullptr);
}

// ══════════════════════════════════════════════════════════════════════════════
// 2. showAt → setAnchor + open
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, ShowAtSetsAnchorAndOpens) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.showAt(btn);

    EXPECT_EQ(fl.anchor(), btn);
    EXPECT_TRUE(fl.isOpen());
    EXPECT_EQ(fl.parentWidget(), window);
    EXPECT_FALSE(fl.isWindow());
    EXPECT_NE(fl.windowType(), Qt::Window);
    EXPECT_NE(fl.windowType(), Qt::Dialog);
    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 3. Bottom Placement
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, ShowAtInheritsThemeOverrideFromAnchor) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    window->onThemeUpdated();

    auto* host = new QWidget(window);
    host->setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    host->setGeometry(80, 120, 260, 180);
    host->show();

    auto* btn = new Button("Anchor", host);
    btn->setGeometry(24, 24, 100, 32);
    btn->show();

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.showAt(btn);

    EXPECT_TRUE(fl.isOpen());
    EXPECT_EQ(fl.effectiveTheme(), fluent::FluentElement::Dark);
    EXPECT_EQ(fl.themeColors().bgLayer, QColor("#2C2C2C"));
    fl.close();
}

TEST_F(FlyoutTest, BottomPlacement) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Bottom);
    fl.showAt(btn);

    // 卡片可见区域 = 整体几何 - shadow margin
    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);

    const QRect anchor(btn->mapTo(window, QPoint(0, 0)), btn->size());

    // 顶部 = anchor.bottom() + offset（与实现一致，边缘包含 QRect inclusive 偏移）
    EXPECT_EQ(card.top(), anchor.bottom() + fl.anchorOffset());
    // 水平居中对齐（1px 取整误差）
    EXPECT_NEAR(card.center().x(), anchor.center().x(), 1);

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 4. Top Placement
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, TopPlacement) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Top);
    fl.showAt(btn);

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    const QRect anchor(btn->mapTo(window, QPoint(0, 0)), btn->size());

    // QRect.bottom() = top + h - 1（闭区间）→ 卡片底边与 anchor 顶边之间留 anchorOffset 个像素
    EXPECT_EQ(card.bottom() + 1, anchor.top() - fl.anchorOffset());
    EXPECT_NEAR(card.center().x(), anchor.center().x(), 1);

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 5. Left / Right Placement
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, LeftPlacement) {
    auto* btn = makeAnchor(QPoint(400, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Left);
    fl.showAt(btn);

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    const QRect anchor(btn->mapTo(window, QPoint(0, 0)), btn->size());

    EXPECT_EQ(card.right() + 1, anchor.left() - fl.anchorOffset());
    EXPECT_NEAR(card.center().y(), anchor.center().y(), 1);

    fl.close();
}

TEST_F(FlyoutTest, RightPlacement) {
    auto* btn = makeAnchor(QPoint(300, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Right);
    fl.showAt(btn);

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    const QRect anchor(btn->mapTo(window, QPoint(0, 0)), btn->size());

    EXPECT_EQ(card.left(), anchor.right() + fl.anchorOffset());
    EXPECT_NEAR(card.center().y(), anchor.center().y(), 1);

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 6. Auto 反转
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, AutoFlipsToTopWhenBottomInsufficient) {
    // 把 anchor 放到窗口底部附近，下方空间不足以放 192px 高的默认 flyout
    auto* btn = makeAnchor(QPoint(350, 560));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Auto);
    fl.showAt(btn);

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    const QRect anchor(btn->mapTo(window, QPoint(0, 0)), btn->size());

    // Auto 应该反转到 Top → 卡片底部 < anchor 顶部
    EXPECT_LT(card.bottom(), anchor.top());

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 7. clampToWindow
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, ClampToWindowKeepsCardInside) {
    // anchor 紧贴右边缘
    auto* btn = makeAnchor(QPoint(window->width() - 80, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Bottom);
    fl.setClampToWindow(true);
    fl.showAt(btn);

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);

    // 右边 ≤ window.right - 4（4px 呼吸空间）
    EXPECT_LE(card.right(), window->width() - 4);
    EXPECT_GE(card.left(), 4);

    fl.close();
}

TEST_F(FlyoutTest, TopLevelResizeRepositionsAndRaisesAboveSiblings) {
    auto* btn = makeAnchor(QPoint(350, 280));
    auto* sibling = new QWidget(window);
    sibling->setGeometry(window->rect());
    sibling->show();

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Bottom);
    fl.showAt(btn);
    ASSERT_TRUE(fl.isOpen());

    sibling->raise();
    window->setFixedSize(840, 640);
    QApplication::processEvents();

    const QRect card = fl.geometry().adjusted(
        kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    EXPECT_EQ(window->childAt(card.center()), &fl);

    fl.close();
}

TEST_F(FlyoutTest, TracksMovingAnchorAncestorAndClosesWhenAnchorIsClipped) {
    auto* scrollingContent = new QWidget(window);
    scrollingContent->setGeometry(0, 0, window->width(), 1000);
    scrollingContent->show();

    auto* btn = new Button(QStringLiteral("Anchor"), scrollingContent);
    btn->setGeometry(350, 280, 100, 32);
    btn->show();

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Bottom);
    fl.showAt(btn);
    ASSERT_TRUE(fl.isOpen());
    const QPoint initialPosition = fl.pos();

    scrollingContent->move(0, -64);
    QTRY_COMPARE_WITH_TIMEOUT(fl.pos(), initialPosition - QPoint(0, 64), 1000);

    scrollingContent->move(0, -500);
    QTRY_VERIFY_WITH_TIMEOUT(!fl.isOpen(), 1000);
}

// ══════════════════════════════════════════════════════════════════════════════
// 8. 无 anchor 退化
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, NoAnchorFallsBackToCenter) {
    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.open();  // 没有 anchor

    EXPECT_TRUE(fl.isOpen());

    // 与基类 Popup 居中行为一致：top-level 中心 ≈ flyout 中心
    const int expectedX = (window->width()  - fl.width())  / 2;
    const int expectedY = (window->height() - fl.height()) / 2;
    EXPECT_EQ(fl.x(), expectedX);
    EXPECT_EQ(fl.y(), expectedY);

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 9. anchor 销毁安全
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, AnchorDestroyedSafely) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.showAt(btn);
    EXPECT_TRUE(fl.isOpen());

    // 销毁 anchor，QPointer 自动置空
    delete btn;
    EXPECT_EQ(fl.anchor(), nullptr);

    // 此刻强制重新计算位置（关闭 -> 重开）
    fl.close();
    fl.open();
    EXPECT_TRUE(fl.isOpen());  // 不 crash，回退到居中
    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 10. light-dismiss 行为（继承自 Popup）
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, EscapeClosesFlyout) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.showAt(btn);
    EXPECT_TRUE(fl.isOpen());

    QTest::keyClick(&fl, Qt::Key_Escape);
    QTest::qWait(50);
    EXPECT_FALSE(fl.isOpen());
}

TEST_F(FlyoutTest, ClickOutsideClosesFlyout) {
    auto* btn = makeAnchor(QPoint(350, 280));

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.showAt(btn);
    EXPECT_TRUE(fl.isOpen());

    // 在窗口角落（远离 flyout 卡片）模拟一次按下
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTest::qWait(50);
    EXPECT_FALSE(fl.isOpen());
}

// ══════════════════════════════════════════════════════════════════════════════
// 11. Full = 居中
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, FullPlacementCenters) {
    auto* btn = makeAnchor(QPoint(60, 60));  // 故意放角落

    Flyout fl(window);
    fl.setAnimationEnabled(false);
    fl.setPlacement(Flyout::Full);
    fl.showAt(btn);

    const int expectedX = (window->width()  - fl.width())  / 2;
    const int expectedY = (window->height() - fl.height()) / 2;
    EXPECT_EQ(fl.x(), expectedX);
    EXPECT_EQ(fl.y(), expectedY);

    fl.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 12. VisualCheck — 6 种 Placement + Auto 反转 演示
// ══════════════════════════════════════════════════════════════════════════════

TEST_F(FlyoutTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    window->hide();

    using Edge = AnchorLayout::Edge;

    auto* visual = new FluentTestWindow();
    visual->setFixedSize(800, 600);
    visual->setWindowTitle("Flyout VisualCheck — click anchors to open flyouts");
    visual->onThemeUpdated();

    auto* layout = new AnchorLayout(visual);
    visual->setLayout(layout);

    // Theme toggle
    auto* themeBtn = new Button("Toggle Theme", visual);
    themeBtn->setFixedSize(140, 32);
    themeBtn->anchors()->top   = {visual, Edge::Top,   12};
    themeBtn->anchors()->right = {visual, Edge::Right, -12};
    layout->addWidget(themeBtn);
    QObject::connect(themeBtn, &Button::clicked, [visual]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
        visual->onThemeUpdated();
    });

    // Helper to build an anchor button + a Flyout with given placement
    auto addDemo = [&](const QString& label, Flyout::Placement p,
                       int x, int y) {
        auto* btn = new Button(label, visual);
        btn->setFixedSize(140, 32);
        btn->move(x, y);
        btn->show();

        auto* fl = new Flyout(visual);
        fl->setPlacement(p);

        auto* pl = new AnchorLayout(fl);
        fl->setLayout(pl);
        auto* title = new Label(label, fl);
        title->setFluentTypography("BodyStrong");
        title->anchors()->top  = {fl, Edge::Top,  20};
        title->anchors()->left = {fl, Edge::Left, 20};
        pl->addWidget(title);
        auto* body = new Label(QStringLiteral("Placement = %1").arg(label), fl);
        body->anchors()->top  = {title, Edge::Bottom, 8};
        body->anchors()->left = {fl, Edge::Left, 20};
        pl->addWidget(body);

        QObject::connect(btn, &Button::clicked, fl, [fl, btn]() {
            fl->showAt(btn);
        });
    };

    addDemo("Top",    Flyout::Top,    330, 320);
    addDemo("Bottom", Flyout::Bottom, 330, 240);
    addDemo("Left",   Flyout::Left,   500, 280);
    addDemo("Right",  Flyout::Right,  160, 280);
    addDemo("Full",   Flyout::Full,    20, 540);
    addDemo("Auto (anchor near bottom → flips Top)",
            Flyout::Auto, 200, 540);

    visual->show();
    qApp->exec();
    delete visual;
}
