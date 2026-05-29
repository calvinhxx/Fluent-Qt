#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <QApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/basicinput/Button.h"
#include "components/scrolling/ScrollBar.h"
#include "components/scrolling/ScrollView.h"
#include "components/textfields/Label.h"

using fluent::basicinput::Button;
using fluent::scrolling::ScrollBar;
using fluent::scrolling::ScrollView;
using fluent::scrolling::ScrollViewZoomAware;
using fluent::textfields::Label;

namespace {

class ScrollViewTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override {
        const auto& colors = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, colors.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class DemoCanvas : public QWidget, public fluent::FluentElement, public ScrollViewZoomAware {
public:
    explicit DemoCanvas(const QSize& canvasSize, QWidget* parent = nullptr)
        : QWidget(parent),
          m_logicalSize(canvasSize) {
        setFixedSize(canvasSize);
    }

    QSizeF scrollViewUnscaledSize() const override {
        return QSizeF(m_logicalSize);
    }

    void setScrollViewZoomFactor(qreal factor) override {
        if (qFuzzyCompare(m_zoomFactor, factor))
            return;
        m_zoomFactor = factor;
        update();
    }

    void onThemeUpdated() override {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const auto colors = themeColors();
        painter.fillRect(rect(), colors.bgLayer);
        painter.scale(m_zoomFactor, m_zoomFactor);

        QPen gridPen(colors.strokeDivider);
        gridPen.setWidth(1);
        painter.setPen(gridPen);
        for (int x = 0; x < m_logicalSize.width(); x += 48)
            painter.drawLine(x, 0, x, m_logicalSize.height());
        for (int y = 0; y < m_logicalSize.height(); y += 48)
            painter.drawLine(0, y, m_logicalSize.width(), y);

        const QList<QColor> swatches = {
            colors.accentDefault,
            colors.systemSuccess,
            colors.systemCaution,
            colors.systemInfo
        };
        for (int index = 0; index < 18; ++index) {
            const int col = index % 6;
            const int row = index / 6;
            QRect tile(24 + col * 96, 24 + row * 96, 64, 64);
            QColor fill = swatches[index % swatches.size()];
            fill.setAlphaF(currentTheme() == fluent::FluentElement::Dark ? 0.72 : 0.88);
            painter.setBrush(fill);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(tile, themeRadius().control, themeRadius().control);
        }
    }

private:
    QSize m_logicalSize;
    qreal m_zoomFactor = 1.0;
};

QWidget* createContent(const QSize& size) {
    auto* content = new QWidget();
    content->setFixedSize(size);
    QPalette pal = content->palette();
    pal.setColor(QPalette::Window, QColor(232, 238, 244));
    content->setPalette(pal);
    content->setAutoFillBackground(true);
    return content;
}

void showAndProcess(QWidget& widget) {
    widget.show();
    QApplication::processEvents();
}

bool waitUntil(std::function<bool()> predicate, int timeoutMs = 1000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QApplication::processEvents();
        if (predicate())
            return true;
        QTest::qWait(10);
    }
    QApplication::processEvents();
    return predicate();
}

void sendWheel(QWidget* target, int angleDeltaY, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    FLUENT_MAKE_WHEEL_EVENT(event, 64, 48, angleDeltaY, modifiers);
    QApplication::sendEvent(target, &event);
}

void sendCtrlWheel(QWidget* target, int angleDeltaY) {
    sendWheel(target, angleDeltaY, Qt::ControlModifier);
}

void sendNativeZoom(QWidget* target, Qt::NativeGestureType gestureType, qreal value) {
    if (!fluentCanConstructNativeGestureEvent()) {
        Q_UNUSED(target);
        Q_UNUSED(gestureType);
        Q_UNUSED(value);
        return;
    }

    FLUENT_MAKE_NATIVE_GESTURE_EVENT(event, target, gestureType, 64, 48, value);
    QApplication::sendEvent(target, &event);
}

} // namespace

class ScrollViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(ScrollViewTest, DefaultScrollBarsAreCustomAndRangesReflectContent) {
    ScrollView view;
    view.resize(180, 140);
    view.setWidget(createContent(QSize(420, 360)));
    showAndProcess(view);

    EXPECT_NE(qobject_cast<ScrollBar*>(view.verticalScrollBar()), nullptr);
    EXPECT_NE(qobject_cast<ScrollBar*>(view.horizontalScrollBar()), nullptr);
    EXPECT_GT(view.scrollableWidth(), 0);
    EXPECT_GT(view.scrollableHeight(), 0);
}

TEST_F(ScrollViewTest, HiddenScrollBarKeepsProgrammaticScrollEnabled) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 100)));
    view.setHorizontalScrollMode(ScrollView::ScrollMode::Enabled);
    view.setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Hidden);
    view.setVerticalScrollMode(ScrollView::ScrollMode::Disabled);
    showAndProcess(view);

    ASSERT_GE(view.scrollableWidth(), 80);
    EXPECT_EQ(view.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_TRUE(view.horizontalScrollBar()->isEnabled());

    view.scrollTo(80, 0, false);
    EXPECT_EQ(view.horizontalOffset(), 80);
    EXPECT_EQ(view.verticalOffset(), 0);
}

TEST_F(ScrollViewTest, DisabledDirectionPreventsProgrammaticScroll) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));
    view.setVerticalScrollMode(ScrollView::ScrollMode::Disabled);
    showAndProcess(view);

    view.scrollTo(70, 90, false);
    EXPECT_EQ(view.horizontalOffset(), 70);
    EXPECT_EQ(view.verticalOffset(), 0);
    EXPECT_FALSE(view.verticalScrollBar()->isEnabled());
}

TEST_F(ScrollViewTest, ScrollToAndScrollByClampToValidRange) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));
    showAndProcess(view);

    view.scrollTo(10000, 10000, false);
    EXPECT_EQ(view.horizontalOffset(), view.scrollableWidth());
    EXPECT_EQ(view.verticalOffset(), view.scrollableHeight());

    view.scrollBy(-10000, -10000, false);
    EXPECT_EQ(view.horizontalOffset(), 0);
    EXPECT_EQ(view.verticalOffset(), 0);
}

TEST_F(ScrollViewTest, AnimatedScrollReachesTargetAndEmitsPositionChanges) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));
    showAndProcess(view);

    const int targetX = std::min(90, view.scrollableWidth());
    const int targetY = std::min(80, view.scrollableHeight());
    QSignalSpy spy(&view, &ScrollView::scrollPositionChanged);

    view.scrollTo(targetX, targetY, true);

    EXPECT_TRUE(waitUntil([&view, targetX, targetY]() {
        return view.horizontalOffset() == targetX && view.verticalOffset() == targetY;
    }));
    EXPECT_GT(spy.count(), 0);
}

TEST_F(ScrollViewTest, VisibilityPoliciesMapToQtPolicies) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));

    view.setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Visible);
    view.setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
    EXPECT_EQ(view.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOn);
    EXPECT_EQ(view.verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);

    view.setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Disabled);
    EXPECT_EQ(view.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_FALSE(view.horizontalScrollBar()->isEnabled());
}

TEST_F(ScrollViewTest, BidirectionalCornerIsTransparent) {
    ScrollView view;
    view.resize(180, 120);
    view.setWidget(createContent(QSize(420, 360)));
    view.setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Visible);
    view.setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Visible);
    showAndProcess(view);

    auto* corner = view.findChild<QWidget*>(QStringLiteral("FluentScrollViewTransparentCorner"));
    ASSERT_NE(corner, nullptr);
    EXPECT_FALSE(corner->autoFillBackground());
    EXPECT_TRUE(corner->testAttribute(Qt::WA_NoSystemBackground));
    EXPECT_TRUE(corner->testAttribute(Qt::WA_TranslucentBackground));
}

TEST_F(ScrollViewTest, ZoomToClampsAndUpdatesContentSizeAndRange) {
    ScrollView view;
    view.resize(180, 120);
    auto* content = createContent(QSize(420, 360));
    view.setWidget(content);
    view.setMinZoomFactor(0.5);
    view.setMaxZoomFactor(2.0);
    showAndProcess(view);

    const int initialRange = view.scrollableWidth();
    QSignalSpy spy(&view, &ScrollView::zoomFactorChanged);

    view.zoomTo(3.0, false);
    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), 2.0));
    EXPECT_EQ(content->size(), QSize(840, 720));
    EXPECT_GT(view.scrollableWidth(), initialRange);
    EXPECT_GT(spy.count(), 0);

    view.zoomTo(0.1, false);
    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), 0.5));
    EXPECT_EQ(content->size(), QSize(210, 180));
}

TEST_F(ScrollViewTest, CtrlWheelZoomRequiresEnabledZoomMode) {
    ScrollView view;
    view.resize(180, 120);
    auto* content = createContent(QSize(420, 360));
    view.setWidget(content);
    showAndProcess(view);

    sendCtrlWheel(view.viewport(), 120);
    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), 1.0));

    view.setZoomMode(ScrollView::ZoomMode::Enabled);
    sendCtrlWheel(view.viewport(), 120);
    EXPECT_GT(view.zoomFactor(), 1.0);
    EXPECT_GT(content->width(), 420);

    const qreal viewportZoom = view.zoomFactor();
    sendCtrlWheel(content, 120);
    EXPECT_GT(view.zoomFactor(), viewportZoom);
    EXPECT_GT(content->width(), 420);
}

TEST_F(ScrollViewTest, ZoomAwareContentReceivesZoomFactor) {
    class ProbeContent : public QWidget, public ScrollViewZoomAware {
    public:
        QSizeF scrollViewUnscaledSize() const override { return QSizeF(320, 240); }
        void setScrollViewZoomFactor(qreal factor) override { zoomFactor = factor; }
        qreal zoomFactor = 1.0;
    };

    ScrollView view;
    view.resize(180, 120);
    auto* content = new ProbeContent();
    view.setWidget(content);
    showAndProcess(view);

    view.zoomTo(1.5, false);
    EXPECT_TRUE(qFuzzyCompare(content->zoomFactor, 1.5));
    EXPECT_EQ(content->size(), QSize(480, 360));
}

TEST_F(ScrollViewTest, NativeTrackpadZoomPersistsAfterGestureEnd) {
    if (!fluentCanConstructNativeGestureEvent())
        GTEST_SKIP() << fluentNativeGestureEventSkipReason();

    ScrollView view;
    view.resize(180, 120);
    auto* content = new DemoCanvas(QSize(420, 360));
    view.setWidget(content);
    view.setZoomMode(ScrollView::ZoomMode::Enabled);
    showAndProcess(view);

    sendNativeZoom(content, Qt::BeginNativeGesture, 0.0);
    sendNativeZoom(content, Qt::ZoomNativeGesture, -0.12);
    const qreal zoomAfterPinch = view.zoomFactor();
    sendNativeZoom(content, Qt::EndNativeGesture, 0.0);

    EXPECT_LT(zoomAfterPinch, 1.0);
    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), zoomAfterPinch));
    EXPECT_EQ(content->size(), QSize(qRound(420 * zoomAfterPinch), qRound(360 * zoomAfterPinch)));
}

TEST_F(ScrollViewTest, NativeTrackpadZoomSuppressesWheelScrollDuringPinch) {
    if (!fluentCanConstructNativeGestureEvent())
        GTEST_SKIP() << fluentNativeGestureEventSkipReason();

    ScrollView view;
    view.resize(180, 120);
    auto* content = new DemoCanvas(QSize(420, 360));
    view.setWidget(content);
    view.setZoomMode(ScrollView::ZoomMode::Enabled);
    showAndProcess(view);

    sendNativeZoom(content, Qt::BeginNativeGesture, 0.0);
    sendNativeZoom(content, Qt::ZoomNativeGesture, 0.12);
    const qreal zoomAfterPinch = view.zoomFactor();
    const int horizontalOffset = view.horizontalOffset();
    const int verticalOffset = view.verticalOffset();

    sendWheel(view.viewport(), -120);

    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), zoomAfterPinch));
    EXPECT_EQ(view.horizontalOffset(), horizontalOffset);
    EXPECT_EQ(view.verticalOffset(), verticalOffset);

    sendNativeZoom(content, Qt::EndNativeGesture, 0.0);
}

TEST_F(ScrollViewTest, NativeTrackpadZoomIgnoresStraySmartZoom) {
    if (!fluentCanConstructNativeGestureEvent())
        GTEST_SKIP() << fluentNativeGestureEventSkipReason();

    ScrollView view;
    view.resize(180, 120);
    auto* content = new DemoCanvas(QSize(420, 360));
    view.setWidget(content);
    view.setZoomMode(ScrollView::ZoomMode::Enabled);
    showAndProcess(view);

    sendNativeZoom(content, Qt::BeginNativeGesture, 0.0);
    sendNativeZoom(content, Qt::ZoomNativeGesture, 0.18);
    const qreal zoomAfterPinch = view.zoomFactor();
    ASSERT_GT(zoomAfterPinch, 1.0);

    sendNativeZoom(content, Qt::SmartZoomNativeGesture, 0.0);
    QTest::qWait(60);
    QApplication::processEvents();

    EXPECT_TRUE(qFuzzyCompare(view.zoomFactor(), zoomAfterPinch));
    sendNativeZoom(content, Qt::EndNativeGesture, 0.0);
}

TEST_F(ScrollViewTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* window = new ScrollViewTestWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle("ScrollView Visual Test");
    window->resize(900, 760);
    window->onThemeUpdated();

    auto* root = new QVBoxLayout(window);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* header = new QHBoxLayout();
    auto* title = new Label(QStringLiteral("ScrollView"), window);
    title->setFluentTypography(QStringLiteral("Title"));
    header->addWidget(title);
    header->addStretch();

    auto* themeButton = new Button(QStringLiteral("Switch Theme"), window);
    themeButton->setFixedSize(132, 32);
    header->addWidget(themeButton);
    root->addLayout(header);

    auto* row = new QHBoxLayout();
    row->setSpacing(16);

    auto addExample = [&](const QString& label, const QSize& viewportSize, const QSize& contentSize) {
        auto* column = new QVBoxLayout();
        column->setSpacing(8);
        auto* text = new Label(label, window);
        column->addWidget(text);

        auto* scrollView = new ScrollView(window);
        scrollView->setFixedSize(viewportSize);
        scrollView->setWidget(new DemoCanvas(contentSize));
        scrollView->setZoomMode(ScrollView::ZoomMode::Enabled);
        scrollView->setHorizontalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
        scrollView->setVerticalScrollBarVisibility(ScrollView::ScrollBarVisibility::Auto);
        column->addWidget(scrollView);

        auto* controls = new QHBoxLayout();
        controls->setSpacing(8);

        auto* jumpButton = new Button(QStringLiteral("Scroll"), window);
        jumpButton->setFixedSize(96, 32);
        QObject::connect(jumpButton, &Button::clicked, scrollView, [scrollView]() {
            const bool atStart = scrollView->horizontalOffset() == 0 && scrollView->verticalOffset() == 0;
            scrollView->scrollTo(atStart ? scrollView->scrollableWidth() : 0,
                                 atStart ? scrollView->scrollableHeight() : 0,
                                 true);
        });
        controls->addWidget(jumpButton);

        auto* zoomButton = new Button(QStringLiteral("Zoom"), window);
        zoomButton->setFixedSize(96, 32);
        QObject::connect(zoomButton, &Button::clicked, scrollView, [scrollView]() {
            scrollView->zoomTo(scrollView->zoomFactor() > 1.0 ? 1.0 : 1.6, true);
        });
        controls->addWidget(zoomButton);
        column->addLayout(controls);
        row->addLayout(column);
    };

    addExample(QStringLiteral("Vertical"), QSize(240, 420), QSize(220, 760));
    addExample(QStringLiteral("Horizontal"), QSize(260, 220), QSize(760, 180));
    addExample(QStringLiteral("Bidirectional"), QSize(280, 420), QSize(760, 640));

    root->addLayout(row);
    root->addStretch();

    QObject::connect(themeButton, &Button::clicked, []() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                    ? fluent::FluentElement::Dark
                                    : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
