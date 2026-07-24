#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QPainter>
#include <QLabel>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QPropertyAnimation>
#include <QTest>
#include "components/collections/FlipView.h"
#include "components/basicinput/Button.h"
#include "components/status_info/Shimmer.h"
#include "components/textfields/Label.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/QMLPlus.h"
#include "compatibility/QtCompat.h"
#include "design/Typography.h"

using namespace fluent::collections;

namespace {
constexpr int kMinimumShimmerVisibleMs = 1200;
}

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
        m_shimmer = new fluent::status_info::Shimmer(this);
        m_shimmer->setShimmerTemplate(fluent::status_info::Shimmer::ShimmerTemplate::ImageCard);

        QTimer::singleShot(kMinimumShimmerVisibleMs, this, [this, url]() {
            auto* nam = new QNetworkAccessManager(this);
            auto* reply = nam->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                reply->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    m_pixmap.loadFromData(reply->readAll());
                }
                m_shimmer->setActive(false);
                m_shimmer->hide();
                update();
            });
        });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QRectF r = rect();

        if (m_pixmap.isNull()) {
            p.fillRect(r, m_bgColor);
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
        QFont labelFont(Typography::FontFamily::UIText, -1, QFont::DemiBold);
        labelFont.setPixelSize(13);
        p.setFont(labelFont);
        p.drawText(labelBar, Qt::AlignCenter, m_label);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const int margin = 8;
        const int labelHeight = 36;
        const QRect shimmerRect = rect().adjusted(margin, margin, -margin, -labelHeight - margin);
        m_shimmer->setGeometry(shimmerRect);
    }

private:
    QPixmap m_pixmap;
    QString m_label;
    QColor m_bgColor;
    fluent::status_info::Shimmer* m_shimmer = nullptr;
};

class LoadingImagePage : public QWidget {
public:
    explicit LoadingImagePage(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        m_shimmer = new fluent::status_info::Shimmer(this);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#f8f8f8"));
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const int margin = 28;
        m_shimmer->setGeometry(rect().adjusted(margin, margin, -margin, -margin));

        using Element = fluent::status_info::ShimmerPainter::Element;
        using Shape = fluent::status_info::ShimmerPainter::Shape;
        const QRectF area = QRectF(m_shimmer->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal textBlockHeight = 48.0;
        const QRectF mediaRect(area.left(),
                               area.top(),
                               area.width(),
                               qMax<qreal>(40.0, area.height() - textBlockHeight - 14.0));
        const qreal textTop = mediaRect.bottom() + 14.0;
        m_shimmer->setElements({
            Element(Shape::RoundedRect, mediaRect, 6.0),
            Element(Shape::Line, QRectF(area.left(), textTop, area.width() * 0.58, 12.0)),
            Element(Shape::Line, QRectF(area.left(), textTop + 20.0, area.width() * 0.34, 12.0)),
        });
    }

private:
    fluent::status_info::Shimmer* m_shimmer = nullptr;
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

    // Wait for the actual slide transition instead of assuming that a fixed
    // delay outlives a cold-start animation on every host.
    // zh_CN: 等待真实滑动动画结束，而不是假设固定延时在所有冷启动环境下
    // 都一定长于动画。
    QPropertyAnimation* slideAnimation =
        fv.findChild<QPropertyAnimation*>();
    ASSERT_NE(slideAnimation, nullptr);
    EXPECT_EQ(slideAnimation->propertyName(), QByteArrayLiteral("slideOffset"));
    QTRY_COMPARE_WITH_TIMEOUT(
        slideAnimation->state(), QAbstractAnimation::Stopped, 1500);

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

// ─── Design-language × theme sweep for the pips indicator + nav buttons ─────
//
// FlipView's overlay draws two themed elements: the bottom page-dot PIPS indicator and the prev/next
// circular NAV BUTTONS. Material 3 / macOS recolor them (accent selected pip, neutral state-layer nav
// hover/press) while Fluent is unchanged. Design language + theme are GLOBAL singletons, so the
// fixture restores both in TearDown. The critical guard here is the dark-mode regression history:
// FlipView once rendered WHITE nav buttons and WHITE corners in dark theme from default-QPalette /
// invalid-QColor bugs — so we assert NO opaque near-black (and, paired with hasPaintedContent, a real
// painted surface) at both a nav-button center and a pip center across every language × theme.
// zh_CN: FlipView 覆盖层绘制两个主题化元素:底部页点 PIPS 指示器与 prev/next 圆形 NAV 按钮。Material 3 /
// macOS 给它们重新着色(accent 选中 pip、中性 state-layer 导航 hover/press),Fluent 不变。设计语言与主题
// 为全局单例,夹具在 TearDown 中复位二者。关键守卫是暗色回归历史:FlipView 曾因默认 QPalette / 无效 QColor
// 在暗色下绘出白色导航按钮与白角——故对每个 语言 × 主题 在导航按钮中心与 pip 中心断言无不透明近黑(配合
// hasPaintedContent 表示确有绘制的表面)。
class FlipViewDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    // True if the sampled pixel is an opaque near-#000 surface — the exact dark-mode regression the
    // valid-token rule prevents. zh_CN: 采样像素为不透明近黑表面则为真——即有效 token 规则所防止的暗色回归。
    static bool isOpaqueBlack(const QImage& img, int x, int y) {
        const QColor c = img.pixelColor(qBound(0, x, img.width() - 1),
                                        qBound(0, y, img.height() - 1));
        const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
        return c.alpha() > 200 && lum < 16;
    }
};

TEST_F(FlipViewDesignLanguageTest, AllLanguagesAndThemesPaintPipsAndNavWithoutDarkRegression) {
    struct LangCase { fluent::FluentElement::DesignLanguage lang; const char* name; };
    struct ThemeCase { fluent::FluentElement::Theme theme; const char* name; };

    const LangCase langs[] = {
        { fluent::FluentElement::DesignFluent, "Fluent" },
        { fluent::FluentElement::DesignMaterial, "Material" },
        { fluent::FluentElement::DesignCupertino, "Cupertino" },
    };
    const ThemeCase themes[] = {
        { fluent::FluentElement::Light, "Light" },
        { fluent::FluentElement::Dark, "Dark" },
    };

    // Geometry mirrors FlipView's overlay metrics (horizontal mode). Nav buttons: 16×28, 2px from the
    // edge, vertically centered. Pips: 6px dots, bottom-centered, 12px up from the bottom.
    // zh_CN: 几何与 FlipView 覆盖层尺寸一致(水平模式)。导航按钮 16×28,距边 2px,垂直居中;pip 6px,
    // 底部居中,距底 12px。
    constexpr int kW = 400, kH = 270;

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            FlipView fv;
            fv.resize(kW, kH);
            // Three pages with currentIndex 1 so BOTH prev and next nav buttons are visible.
            // zh_CN: 三页且 currentIndex=1,使 prev 与 next 导航按钮都可见。
            fv.addPage(new QWidget);
            fv.addPage(new QWidget);
            fv.addPage(new QWidget);
            fv.setCurrentIndex(1);

            // Nav buttons only paint while hovered; deliver an enter event to set the hover flag.
            // zh_CN: 导航按钮仅在悬停时绘制;投递 enter 事件以置位悬停标志。
            const QPointF center(kW / 2.0, kH / 2.0);
            FLUENT_MAKE_ENTER_EVENT(enter, center.x(), center.y());
            QApplication::sendEvent(&fv, &enter);

            const QImage img = fv.grab().toImage();

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // 1. The overlay grabbed a valid, non-empty image with painted content. zh_CN: 覆盖层抓取出有效、非空且有内容的图像。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(img)) << "FlipView painted nothing: " << ctx;

            // 2. Nav-button centers (prev at left, next at right; both ~14px tall band centered). The
            //    dark-mode regression guard: neither button surface may be an opaque near-black fill.
            //    zh_CN: 导航按钮中心(prev 在左、next 在右;均居中)。暗色回归守卫:按钮表面都不得为不透明近黑填充。
            const int navY = kH / 2;
            const int prevCx = 2 + 16 / 2;          // left margin + half width. zh_CN: 左边距 + 半宽。
            const int nextCx = kW - 2 - 16 / 2;     // mirror on the right. zh_CN: 右侧镜像。
            EXPECT_FALSE(isOpaqueBlack(img, prevCx, navY))
                << "prev nav button is opaque-black (dark-mode regression): " << ctx;
            EXPECT_FALSE(isOpaqueBlack(img, nextCx, navY))
                << "next nav button is opaque-black (dark-mode regression): " << ctx;

            // 3. Pip center: the selected pip (index 1) sits in the bottom-centered row. Compute the
            //    indicator strip the same way pageIndicatorRect() does, then sample the index-1 dot.
            //    zh_CN: pip 中心:选中 pip(index 1)位于底部居中行。按 pageIndicatorRect() 的方式计算指示条,
            //    再采样 index-1 的点。
            constexpr int kDot = 6, kSpacing = 8, kMargin = 12;
            const int totalW = 3 * kDot + 2 * kSpacing;
            const int pipsLeft = kW / 2 - totalW / 2;
            const int pipY = kH - kMargin - kDot + kDot / 2;
            const int selPipCx = pipsLeft + 1 * (kDot + kSpacing) + kDot / 2; // selected = index 1. zh_CN: 选中=index 1。
            EXPECT_FALSE(isOpaqueBlack(img, selPipCx, pipY))
                << "selected pip is opaque-black (dark-mode regression): " << ctx;
        }
    }
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
    fv1->addPage(new LoadingImagePage(fv1));
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

    auto* indexLabel = new fluent::textfields::Label(QString("1 / %1").arg(fv1->pageCount()), window);
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
