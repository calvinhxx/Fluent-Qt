#include "compatibility/QtCompat.h"
#include "components/collections/FlipView.h"
#include "components/foundation/FluentElement.h"
#include <QApplication>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>
#include <gtest/gtest.h>

using namespace fluent::collections;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class FlipViewTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── 测试类 ───────────────────────────────────────────────────────────────────

class FlipViewTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new FlipViewTestWindow();
        window->setFixedSize(640, 700);
        window->setWindowTitle("Fluent FlipView Visual Test");
        window->onThemeUpdated();
    }

    void TearDown() override { delete window; }

    FlipViewTestWindow* window = nullptr;
};

// ── 默认属性 ─────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, DefaultPropertyValues)
{
    FlipView fv;
    EXPECT_EQ(fv.currentIndex(), -1);
    EXPECT_EQ(fv.orientation(), Qt::Horizontal);
    EXPECT_TRUE(fv.showNavigationButtons());
    EXPECT_TRUE(fv.areNavigationButtonsVisible());
    EXPECT_TRUE(fv.showPageIndicator());
    EXPECT_TRUE(fv.isPageIndicatorVisible());
    EXPECT_EQ(fv.pageCount(), 0);
}

// ── 页面管理 ─────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, AddPageUpdatesCountAndIndex)
{
    FlipView fv;
    auto* p1 = new QWidget;
    fv.addPage(p1);
    EXPECT_EQ(fv.pageCount(), 1);
    EXPECT_EQ(fv.currentIndex(), 0);
    EXPECT_EQ(fv.pageAt(0), p1);
}

TEST_F(FlipViewTest, AddMultiplePages)
{
    FlipView fv;
    auto* p1 = new QWidget;
    auto* p2 = new QWidget;
    auto* p3 = new QWidget;
    fv.addPage(p1);
    fv.addPage(p2);
    fv.addPage(p3);
    EXPECT_EQ(fv.pageCount(), 3);
    EXPECT_EQ(fv.currentIndex(), 0);
    EXPECT_EQ(fv.pageAt(2), p3);
}

TEST_F(FlipViewTest, InsertPageAtBeginning)
{
    FlipView fv;
    auto* p1 = new QWidget;
    auto* p2 = new QWidget;
    fv.addPage(p1);
    fv.insertPage(0, p2);
    EXPECT_EQ(fv.pageCount(), 2);
    EXPECT_EQ(fv.pageAt(0), p2);
    EXPECT_EQ(fv.pageAt(1), p1);
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, RemovePageUpdatesCount)
{
    FlipView fv;
    auto* p1 = new QWidget;
    auto* p2 = new QWidget;
    fv.addPage(p1);
    fv.addPage(p2);
    fv.removePage(0);
    EXPECT_EQ(fv.pageCount(), 1);
    EXPECT_EQ(fv.pageAt(0), p2);
    EXPECT_EQ(p1->parentWidget(), nullptr);
    delete p1;
}

TEST_F(FlipViewTest, RemoveLastPageResetsIndex)
{
    FlipView fv;
    auto* p1 = new QWidget;
    fv.addPage(p1);
    fv.removePage(0);
    EXPECT_EQ(fv.pageCount(), 0);
    EXPECT_EQ(fv.currentIndex(), -1);
    EXPECT_EQ(p1->parentWidget(), nullptr);
    delete p1;
}

TEST_F(FlipViewTest, PageAtOutOfBoundsReturnsNull)
{
    FlipView fv;
    EXPECT_EQ(fv.pageAt(-1), nullptr);
    EXPECT_EQ(fv.pageAt(0), nullptr);
    EXPECT_EQ(fv.pageAt(5), nullptr);
}

TEST_F(FlipViewTest, ExplicitOwnershipReleaseAppliesConfiguredPolicy)
{
    FlipView fv;

    QPointer<QWidget> owned = new QWidget;
    ASSERT_TRUE(fv.addPage(owned.data(), fluent::WidgetOwnership::Owned));
    EXPECT_EQ(fv.pageOwnershipAt(0), fluent::WidgetOwnership::Owned);
    EXPECT_TRUE(fv.releasePage(0));
    EXPECT_TRUE(owned.isNull());

    auto* borrowed = new QWidget;
    ASSERT_TRUE(fv.addPage(borrowed, fluent::WidgetOwnership::Borrowed));
    EXPECT_TRUE(fv.releasePage(0));
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    delete borrowed;

    QWidget originalParent;
    auto* reparented = new QWidget(&originalParent);
    ASSERT_TRUE(fv.addPage(reparented, fluent::WidgetOwnership::Reparented));
    EXPECT_EQ(reparented->parentWidget(), &fv);
    EXPECT_TRUE(fv.releasePage(0));
    EXPECT_EQ(reparented->parentWidget(), &originalParent);

    EXPECT_FALSE(fv.releasePage(-1));
    EXPECT_FALSE(fv.releasePage(0));
}

TEST_F(FlipViewTest, HostDestructionHonorsExplicitOwnership)
{
    QWidget originalParent;
    QPointer<QWidget> owned = new QWidget;
    auto* borrowed = new QWidget;
    auto* reparented = new QWidget(&originalParent);
    auto* fv = new FlipView;

    ASSERT_TRUE(fv->addPage(owned.data(), fluent::WidgetOwnership::Owned));
    ASSERT_TRUE(fv->addPage(borrowed, fluent::WidgetOwnership::Borrowed));
    ASSERT_TRUE(fv->addPage(reparented, fluent::WidgetOwnership::Reparented));

    delete fv;

    EXPECT_TRUE(owned.isNull());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    EXPECT_EQ(reparented->parentWidget(), &originalParent);
    delete borrowed;
}

TEST_F(FlipViewTest, TakePageTransfersWithoutApplyingOwnership)
{
    QWidget originalParent;
    auto* page = new QWidget(&originalParent);
    FlipView fv;
    ASSERT_TRUE(fv.addPage(page, fluent::WidgetOwnership::Reparented));

    QWidget* taken = fv.takePage(0);

    EXPECT_EQ(taken, page);
    EXPECT_EQ(taken->parentWidget(), nullptr);
    EXPECT_EQ(fv.pageCount(), 0);
    EXPECT_EQ(fv.takePage(0), nullptr);
    delete taken;
}

TEST_F(FlipViewTest, RejectsInvalidPagesAndTracksExternalDestruction)
{
    FlipView fv;
    auto* page = new QWidget;
    EXPECT_FALSE(fv.addPage(nullptr, fluent::WidgetOwnership::Borrowed));
    ASSERT_TRUE(fv.addPage(page, fluent::WidgetOwnership::Borrowed));
    EXPECT_FALSE(fv.addPage(page, fluent::WidgetOwnership::Owned));

    QSignalSpy currentSpy(&fv, &FlipView::currentIndexChanged);
    delete page;

    EXPECT_EQ(fv.pageCount(), 0);
    EXPECT_EQ(fv.currentIndex(), -1);
    EXPECT_EQ(currentSpy.count(), 1);

    QWidget ancestor;
    auto* nested = new FlipView(&ancestor);
    EXPECT_FALSE(nested->addPage(&ancestor, fluent::WidgetOwnership::Borrowed));
}

// ── currentIndex ─────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SetCurrentIndexEmitsSignal)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    QSignalSpy spy(&fv, &FlipView::currentIndexChanged);
    fv.setCurrentIndex(2);
    ASSERT_GE(spy.count(), 1);
    EXPECT_EQ(fv.currentIndex(), 2);
}

TEST_F(FlipViewTest, SetCurrentIndexClampsToRange)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(100);
    EXPECT_EQ(fv.currentIndex(), 1);
    fv.setCurrentIndex(-5);
    EXPECT_EQ(fv.currentIndex(), 0);
}

TEST_F(FlipViewTest, SetSameIndexNoSignal)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(1);
    QSignalSpy spy(&fv, &FlipView::currentIndexChanged);
    fv.setCurrentIndex(1);
    EXPECT_EQ(spy.count(), 0);
}

// ── 导航 ─────────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, GoNextIncrementsIndex)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    EXPECT_EQ(fv.currentIndex(), 0);
    fv.goNext();
    EXPECT_EQ(fv.currentIndex(), 1);
    fv.goNext();
    EXPECT_EQ(fv.currentIndex(), 2);
}

TEST_F(FlipViewTest, GoNextAtEndDoesNothing)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(1);
    fv.goNext();
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, GoPreviousDecrementsIndex)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(2);
    fv.goPrevious();
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, GoPreviousAtStartDoesNothing)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.goPrevious();
    EXPECT_EQ(fv.currentIndex(), 0);
}

// ── 方向 ─────────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SetOrientationEmitsSignal)
{
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::orientationChanged);
    fv.setOrientation(Qt::Vertical);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(fv.orientation(), Qt::Vertical);
}

TEST_F(FlipViewTest, SetSameOrientationNoSignal)
{
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::orientationChanged);
    fv.setOrientation(Qt::Horizontal);
    EXPECT_EQ(spy.count(), 0);
}

// ── ShowNavigationButtons ────────────────────────────────────────────────────

TEST_F(FlipViewTest, ToggleNavigationButtons)
{
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::showNavigationButtonsChanged);
    fv.setShowNavigationButtons(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(fv.showNavigationButtons());
    EXPECT_FALSE(fv.areNavigationButtonsVisible());
}

// ── ShowPageIndicator ────────────────────────────────────────────────────────

TEST_F(FlipViewTest, TogglePageIndicator)
{
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::showPageIndicatorChanged);
    fv.setShowPageIndicator(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(fv.showPageIndicator());
    EXPECT_FALSE(fv.isPageIndicatorVisible());
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SizeHint)
{
    FlipView fv;
    EXPECT_EQ(fv.sizeHint(), QSize(400, 270));
    EXPECT_EQ(fv.minimumSizeHint(), QSize(100, 60));
}

// ── 移除页面后索引调整 ──────────────────────────────────────────────────────

TEST_F(FlipViewTest, RemoveCurrentPageAdjustsIndex)
{
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(2);
    fv.removePage(2);
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, RemoveBeforeCurrentAdjustsIndex)
{
    FlipView fv;
    auto* p1 = new QWidget;
    auto* p2 = new QWidget;
    auto* p3 = new QWidget;
    fv.addPage(p1);
    fv.addPage(p2);
    fv.addPage(p3);
    fv.setCurrentIndex(2);
    fv.removePage(0);
    EXPECT_EQ(fv.currentIndex(), 1);
    EXPECT_EQ(fv.pageAt(1), p3);
}

// ── 滚轮/触控板输入 ─────────────────────────────────────────────────────────

TEST_F(FlipViewTest, MouseWheelDiscreteFlipsImmediately)
{
    // 鼠标滚轮单次 angleDelta=±120 应立即翻页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 向下滚动一格 → goNext
    QWheelEvent wheelDown(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheelDown);
    EXPECT_EQ(fv.currentIndex(), 1);

    // Wait for the actual slide transition instead of assuming that a fixed
    // delay outlives a cold-start animation on every host.
    // zh_CN: 等待真实滑动动画结束，而不是假设固定延时在所有冷启动环境下
    // 都一定长于动画。
    QPropertyAnimation* slideAnimation = fv.findChild<QPropertyAnimation*>();
    ASSERT_NE(slideAnimation, nullptr);
    EXPECT_EQ(slideAnimation->propertyName(), QByteArrayLiteral("slideOffset"));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (slideAnimation->state()) == (QAbstractAnimation::Stopped); }, 1500));

    // 向上滚动一格 → goPrevious
    QWheelEvent wheelUp(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheelUp);
    EXPECT_EQ(fv.currentIndex(), 0);
}

TEST_F(FlipViewTest, WindowsTouchpadHighFreqFlipsOnce)
{
    // 模拟 Windows 触控板：高频 NoScrollPhase 事件（间隔 10ms, angleDelta=30）
    // 整组手势应只翻一页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 发送 10 个高频事件，每个 angleDelta.y = -30, 总计 -300 (远超阈值 50)
    // 应只翻一页
    for (int i = 0; i < 10; ++i) {
        QWheelEvent ev(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -30),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&fv, &ev);
        QTest::qWait(10); // 模拟 10ms 间隔
    }
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, RdpTouchpadHighFreq120FlipsOnce)
{
    // 模拟 Mac RDP → Windows：触控板事件映射为 WM_MOUSEWHEEL
    // angleDelta=±120 per event, 高频连续到达, NoScrollPhase
    // 一次手势应只翻一页，不能链式翻到底
    FlipView fv;
    fv.setFixedSize(400, 270);
    for (int i = 0; i < 5; ++i)
        fv.addPage(new QWidget);
    fv.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 发送 8 个高频 ±120 事件（模拟 Mac 触控板 RDP 传输）
    for (int i = 0; i < 8; ++i) {
        QWheelEvent ev(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&fv, &ev);
        QTest::qWait(10);
    }
    // 等动画完成（包括可能的 pending）
    QTest::qWait(500);
    // 应最多翻 1 页，不能翻到底（index 4）
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, NoScrollPhaseNoPendingDuringAnimation)
{
    // NoScrollPhase 事件在动画期间不设置 pending（防止 RDP 链式翻页）
    // 动画结束后用户可再次操作翻页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 第一次翻页 → 触发动画
    QWheelEvent wheel1(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel1);
    EXPECT_EQ(fv.currentIndex(), 1);

    // 动画期间发送第二次事件（新 cluster）→ 应被消费，不设 pending
    QTest::qWait(130); // > 120ms = 新 cluster，但仍在动画中
    QWheelEvent wheel2(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel2);

    // 等动画完成 — 不应链式翻页
    QTest::qWait(800);
    EXPECT_EQ(fv.currentIndex(), 1); // 停在 page 1，无 pending

    // 动画结束后再操作 → 正常翻页
    QWheelEvent wheel3(QPointF(200, 135), QPointF(200, 135), QPoint(0, 0), QPoint(0, -120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel3);
    EXPECT_EQ(fv.currentIndex(), 2);
}
