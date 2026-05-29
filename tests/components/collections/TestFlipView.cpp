#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QPainter>
#include <QLabel>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QWheelEvent>
#include <QPropertyAnimation>
#include <QTest>
#include "components/collections/FlipView.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

using namespace fluent::collections;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class FlipViewTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── 网络图片页面：异步加载URL图片，居中填充显示 ─────────────────────────────

class NetworkImagePage : public QWidget {
public:
    NetworkImagePage(const QUrl& url, const QString& label, QWidget* parent = nullptr)
        : QWidget(parent), m_label(label), m_bgColor(QColor("#e0e0e0"))
    {
        auto* nam = new QNetworkAccessManager(this);
        auto* reply = nam->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_pixmap.loadFromData(reply->readAll());
            }
            update();
        });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QRectF r = rect();

        if (m_pixmap.isNull()) {
            // 加载中 — 浅灰背景 + 提示文字
            p.fillRect(r, m_bgColor);
            p.setPen(QColor("#888888"));
            p.setFont(QFont("Segoe UI Variable", 12));
            p.drawText(r, Qt::AlignCenter, "Loading...");
        } else {
            // 等比填充 (cover)
            QSizeF imgSize = m_pixmap.size();
            QSizeF viewSize = r.size();
            qreal scale = qMax(viewSize.width() / imgSize.width(),
                               viewSize.height() / imgSize.height());
            QSizeF scaled = imgSize * scale;
            QRectF src((scaled.width() - viewSize.width()) / 2.0 / scale,
                       (scaled.height() - viewSize.height()) / 2.0 / scale,
                       viewSize.width() / scale, viewSize.height() / scale);
            p.drawPixmap(r.toRect(), m_pixmap, src.toRect());
        }

        // 底部标签条
        QRectF labelBar(0, r.height() - 36, r.width(), 36);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRect(labelBar);
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI Variable", 13, QFont::DemiBold));
        p.drawText(labelBar, Qt::AlignCenter, m_label);
    }

private:
    QPixmap m_pixmap;
    QString m_label;
    QColor m_bgColor;
};

// ── 简单彩色页面（用于非图片示例） ───────────────────────────────────────────

static QWidget* makeColorPage(const QColor& color, const QString& text, QWidget* parent) {
    auto* page = new QWidget(parent);
    page->setAutoFillBackground(true);
    QPalette pal = page->palette();
    pal.setColor(QPalette::Window, color);
    page->setPalette(pal);

    auto* label = new fluent::textfields::Label(text, page);
    label->setFluentTypography("Subtitle");
    label->setAlignment(Qt::AlignCenter);
    return page;
}

// ── 测试类 ───────────────────────────────────────────────────────────────────

class FlipViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new FlipViewTestWindow();
        window->setFixedSize(640, 700);
        window->setWindowTitle("Fluent FlipView Visual Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    FlipViewTestWindow* window = nullptr;
};

// ── 默认属性 ─────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, DefaultPropertyValues) {
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

TEST_F(FlipViewTest, AddPageUpdatesCountAndIndex) {
    FlipView fv;
    auto* p1 = new QWidget;
    fv.addPage(p1);
    EXPECT_EQ(fv.pageCount(), 1);
    EXPECT_EQ(fv.currentIndex(), 0);
    EXPECT_EQ(fv.pageAt(0), p1);
}

TEST_F(FlipViewTest, AddMultiplePages) {
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

TEST_F(FlipViewTest, InsertPageAtBeginning) {
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

TEST_F(FlipViewTest, RemovePageUpdatesCount) {
    FlipView fv;
    auto* p1 = new QWidget;
    auto* p2 = new QWidget;
    fv.addPage(p1);
    fv.addPage(p2);
    fv.removePage(0);
    EXPECT_EQ(fv.pageCount(), 1);
    EXPECT_EQ(fv.pageAt(0), p2);
}

TEST_F(FlipViewTest, RemoveLastPageResetsIndex) {
    FlipView fv;
    auto* p1 = new QWidget;
    fv.addPage(p1);
    fv.removePage(0);
    EXPECT_EQ(fv.pageCount(), 0);
    EXPECT_EQ(fv.currentIndex(), -1);
}

TEST_F(FlipViewTest, PageAtOutOfBoundsReturnsNull) {
    FlipView fv;
    EXPECT_EQ(fv.pageAt(-1), nullptr);
    EXPECT_EQ(fv.pageAt(0), nullptr);
    EXPECT_EQ(fv.pageAt(5), nullptr);
}

// ── currentIndex ─────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SetCurrentIndexEmitsSignal) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    QSignalSpy spy(&fv, &FlipView::currentIndexChanged);
    fv.setCurrentIndex(2);
    ASSERT_GE(spy.count(), 1);
    EXPECT_EQ(fv.currentIndex(), 2);
}

TEST_F(FlipViewTest, SetCurrentIndexClampsToRange) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(100);
    EXPECT_EQ(fv.currentIndex(), 1);
    fv.setCurrentIndex(-5);
    EXPECT_EQ(fv.currentIndex(), 0);
}

TEST_F(FlipViewTest, SetSameIndexNoSignal) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(1);
    QSignalSpy spy(&fv, &FlipView::currentIndexChanged);
    fv.setCurrentIndex(1);
    EXPECT_EQ(spy.count(), 0);
}

// ── 导航 ─────────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, GoNextIncrementsIndex) {
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

TEST_F(FlipViewTest, GoNextAtEndDoesNothing) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(1);
    fv.goNext();
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, GoPreviousDecrementsIndex) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(2);
    fv.goPrevious();
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, GoPreviousAtStartDoesNothing) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.goPrevious();
    EXPECT_EQ(fv.currentIndex(), 0);
}

// ── 方向 ─────────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SetOrientationEmitsSignal) {
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::orientationChanged);
    fv.setOrientation(Qt::Vertical);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(fv.orientation(), Qt::Vertical);
}

TEST_F(FlipViewTest, SetSameOrientationNoSignal) {
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::orientationChanged);
    fv.setOrientation(Qt::Horizontal);
    EXPECT_EQ(spy.count(), 0);
}

// ── ShowNavigationButtons ────────────────────────────────────────────────────

TEST_F(FlipViewTest, ToggleNavigationButtons) {
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::showNavigationButtonsChanged);
    fv.setShowNavigationButtons(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(fv.showNavigationButtons());
    EXPECT_FALSE(fv.areNavigationButtonsVisible());
}

// ── ShowPageIndicator ────────────────────────────────────────────────────────

TEST_F(FlipViewTest, TogglePageIndicator) {
    FlipView fv;
    QSignalSpy spy(&fv, &FlipView::showPageIndicatorChanged);
    fv.setShowPageIndicator(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(fv.showPageIndicator());
    EXPECT_FALSE(fv.isPageIndicatorVisible());
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, SizeHint) {
    FlipView fv;
    EXPECT_EQ(fv.sizeHint(), QSize(400, 270));
    EXPECT_EQ(fv.minimumSizeHint(), QSize(100, 60));
}

// ── 移除页面后索引调整 ──────────────────────────────────────────────────────

TEST_F(FlipViewTest, RemoveCurrentPageAdjustsIndex) {
    FlipView fv;
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.setCurrentIndex(2);
    fv.removePage(2);
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, RemoveBeforeCurrentAdjustsIndex) {
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

TEST_F(FlipViewTest, MouseWheelDiscreteFlipsImmediately) {
    // 鼠标滚轮单次 angleDelta=±120 应立即翻页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 向下滚动一格 → goNext
    QWheelEvent wheelDown(
        QPointF(200, 135), QPointF(200, 135),
        QPoint(0, 0), QPoint(0, -120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheelDown);
    EXPECT_EQ(fv.currentIndex(), 1);

    // 等动画结束
    QTest::qWait(400);

    // 向上滚动一格 → goPrevious
    QWheelEvent wheelUp(
        QPointF(200, 135), QPointF(200, 135),
        QPoint(0, 0), QPoint(0, 120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheelUp);
    EXPECT_EQ(fv.currentIndex(), 0);
}

TEST_F(FlipViewTest, WindowsTouchpadHighFreqFlipsOnce) {
    // 模拟 Windows 触控板：高频 NoScrollPhase 事件（间隔 10ms, angleDelta=30）
    // 整组手势应只翻一页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 发送 10 个高频事件，每个 angleDelta.y = -30, 总计 -300 (远超阈值 50)
    // 应只翻一页
    for (int i = 0; i < 10; ++i) {
        QWheelEvent ev(
            QPointF(200, 135), QPointF(200, 135),
            QPoint(0, 0), QPoint(0, -30),
            Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(&fv, &ev);
        QTest::qWait(10); // 模拟 10ms 间隔
    }
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, RdpTouchpadHighFreq120FlipsOnce) {
    // 模拟 Mac RDP → Windows：触控板事件映射为 WM_MOUSEWHEEL
    // angleDelta=±120 per event, 高频连续到达, NoScrollPhase
    // 一次手势应只翻一页，不能链式翻到底
    FlipView fv;
    fv.setFixedSize(400, 270);
    for (int i = 0; i < 5; ++i) fv.addPage(new QWidget);
    fv.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 发送 8 个高频 ±120 事件（模拟 Mac 触控板 RDP 传输）
    for (int i = 0; i < 8; ++i) {
        QWheelEvent ev(
            QPointF(200, 135), QPointF(200, 135),
            QPoint(0, 0), QPoint(0, -120),
            Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(&fv, &ev);
        QTest::qWait(10);
    }
    // 等动画完成（包括可能的 pending）
    QTest::qWait(500);
    // 应最多翻 1 页，不能翻到底（index 4）
    EXPECT_EQ(fv.currentIndex(), 1);
}

TEST_F(FlipViewTest, NoScrollPhaseNoPendingDuringAnimation) {
    // NoScrollPhase 事件在动画期间不设置 pending（防止 RDP 链式翻页）
    // 动画结束后用户可再次操作翻页
    FlipView fv;
    fv.setFixedSize(400, 270);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.addPage(new QWidget);
    fv.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fv));
    EXPECT_EQ(fv.currentIndex(), 0);

    // 第一次翻页 → 触发动画
    QWheelEvent wheel1(
        QPointF(200, 135), QPointF(200, 135),
        QPoint(0, 0), QPoint(0, -120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel1);
    EXPECT_EQ(fv.currentIndex(), 1);

    // 动画期间发送第二次事件（新 cluster）→ 应被消费，不设 pending
    QTest::qWait(130); // > 120ms = 新 cluster，但仍在动画中
    QWheelEvent wheel2(
        QPointF(200, 135), QPointF(200, 135),
        QPoint(0, 0), QPoint(0, -120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel2);

    // 等动画完成 — 不应链式翻页
    QTest::qWait(800);
    EXPECT_EQ(fv.currentIndex(), 1); // 停在 page 1，无 pending

    // 动画结束后再操作 → 正常翻页
    QWheelEvent wheel3(
        QPointF(200, 135), QPointF(200, 135),
        QPoint(0, 0), QPoint(0, -120),
        Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&fv, &wheel3);
    EXPECT_EQ(fv.currentIndex(), 2);
}

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(FlipViewTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using Edge = fluent::AnchorLayout::Edge;
    auto* layout = new fluent::AnchorLayout(window);
    window->setFixedSize(560, 680);

    // ── 1. A simple FlipView with image items ──
    auto* title1 = new fluent::textfields::Label("A simple FlipView with inline items.", window);
    title1->setFluentTypography("Body");
    title1->anchors()->top = {window, Edge::Top, 24};
    title1->anchors()->left = {window, Edge::Left, 30};
    layout->addWidget(title1);

    auto* fv1 = new FlipView(window);
    fv1->setFixedSize(480, 270);
    fv1->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/mountain/960/540"),
        "Landscape 1 — Mountain Lake", fv1));
    fv1->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/desert/960/540"),
        "Landscape 2 — Desert Sunset", fv1));
    fv1->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/snow/960/540"),
        "Landscape 3 — Snowy Peaks", fv1));
    fv1->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/forest/960/540"),
        "Landscape 4 — Twilight Forest", fv1));
    fv1->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/ocean/960/540"),
        "Landscape 5 — Ocean Shore", fv1));
    fv1->anchors()->top = {title1, Edge::Bottom, 10};
    fv1->anchors()->left = {window, Edge::Left, 30};
    layout->addWidget(fv1);

    auto* indexLabel = new fluent::textfields::Label("1 / 5", window);
    indexLabel->setFluentTypography("Caption");
    indexLabel->anchors()->top = {fv1, Edge::Bottom, 6};
    indexLabel->anchors()->left = {window, Edge::Left, 30};
    layout->addWidget(indexLabel);
    QObject::connect(fv1, &FlipView::currentIndexChanged, [indexLabel, fv1](int idx) {
        indexLabel->setText(QString("%1 / %2").arg(idx + 1).arg(fv1->pageCount()));
    });

    // ── 2. Vertical FlipView ──
    auto* title2 = new fluent::textfields::Label("A vertical FlipView.", window);
    title2->setFluentTypography("Body");
    title2->anchors()->top = {indexLabel, Edge::Bottom, 24};
    title2->anchors()->left = {window, Edge::Left, 30};
    layout->addWidget(title2);

    auto* fv2 = new FlipView(window);
    fv2->setOrientation(Qt::Vertical);
    fv2->setFixedSize(220, 180);
    fv2->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/tropical/440/360"),
        "Tropical", fv2));
    fv2->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/autumn/440/360"),
        "Autumn", fv2));
    fv2->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/arctic/440/360"),
        "Arctic", fv2));
    fv2->anchors()->top = {title2, Edge::Bottom, 10};
    fv2->anchors()->left = {window, Edge::Left, 30};
    layout->addWidget(fv2);

    // ── 3. No navigation buttons ──
    auto* title3 = new fluent::textfields::Label("No navigation buttons (swipe only).", window);
    title3->setFluentTypography("Body");
    title3->anchors()->top = {title2, Edge::Bottom, 10};
    title3->anchors()->left = {fv2, Edge::Right, 30};
    layout->addWidget(title3);

    auto* fv3 = new FlipView(window);
    fv3->setShowNavigationButtons(false);
    fv3->setFixedSize(220, 180);
    fv3->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/farmland/440/360"),
        "Farmland", fv3));
    fv3->addPage(new NetworkImagePage(
        QUrl("https://picsum.photos/seed/meadow/440/360"),
        "Meadow", fv3));
    fv3->anchors()->top = {title3, Edge::Bottom, 10};
    fv3->anchors()->left = {fv2, Edge::Right, 30};
    layout->addWidget(fv3);

    // 主题切换按钮
    auto* themeBtn = new fluent::basicinput::Button("Switch Theme", window);
    themeBtn->setFluentStyle(fluent::basicinput::Button::Accent);
    themeBtn->setFixedSize(120, 32);
    themeBtn->anchors()->bottom = {window, Edge::Bottom, -20};
    themeBtn->anchors()->right = {window, Edge::Right, -20};
    layout->addWidget(themeBtn);
    QObject::connect(themeBtn, &fluent::basicinput::Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                ? fluent::FluentElement::Dark
                                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
