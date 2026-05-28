#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalSpy>
#include "view/basicinput/RatingControl.h"
#include "view/basicinput/Button.h"
#include "view/foundation/FluentElement.h"
#include "design/Typography.h"
#include "design/Spacing.h"

using namespace view::basicinput;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class RatingControlTestWindow : public QWidget, public FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        setStyleSheet(QString("background-color: %1;").arg(c.bgCanvas.name()));
    }
};

// ── 测试类 ───────────────────────────────────────────────────────────────────

class RatingControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new RatingControlTestWindow();
        window->setFixedSize(600, 500);
        window->setWindowTitle("Fluent RatingControl Visual Test");
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
    }

    RatingControlTestWindow* window = nullptr;
};

// ── 默认属性 ─────────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, DefaultPropertyValues) {
    RatingControl rc;
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);
    EXPECT_DOUBLE_EQ(rc.placeholderValue(), 0.0);
    EXPECT_TRUE(rc.caption().isEmpty());
    EXPECT_TRUE(rc.isClearEnabled());
    EXPECT_FALSE(rc.isReadOnly());
    EXPECT_EQ(rc.maxRating(), 5);
    EXPECT_EQ(rc.starSize(), 16);
}

// ── Value 属性 ───────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetValueEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::valueChanged);
    rc.setValue(3.0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().first().toDouble(), 3.0);
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
}

TEST_F(RatingControlTest, SetSameValueNoSignal) {
    RatingControl rc;
    rc.setValue(3.0);
    QSignalSpy spy(&rc, &RatingControl::valueChanged);
    rc.setValue(3.0);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(RatingControlTest, ValueClampedToMaxRating) {
    RatingControl rc;
    rc.setMaxRating(5);
    rc.setValue(10.0);
    EXPECT_DOUBLE_EQ(rc.value(), 5.0);
}

TEST_F(RatingControlTest, ValueClampedToMinusOne) {
    RatingControl rc;
    rc.setValue(-5.0);
    EXPECT_DOUBLE_EQ(rc.value(), -1.0);
}

TEST_F(RatingControlTest, HalfStarValue) {
    RatingControl rc;
    rc.setValue(3.5);
    EXPECT_DOUBLE_EQ(rc.value(), 3.5);
}

// ── PlaceholderValue 属性 ────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetPlaceholderValueEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::placeholderValueChanged);
    rc.setPlaceholderValue(2.5);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_DOUBLE_EQ(spy.first().first().toDouble(), 2.5);
}

TEST_F(RatingControlTest, PlaceholderValueClampedToZero) {
    RatingControl rc;
    rc.setPlaceholderValue(-1.0);
    EXPECT_DOUBLE_EQ(rc.placeholderValue(), 0.0);
}

// ── Caption 属性 ─────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetCaptionEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::captionChanged);
    rc.setCaption("312 ratings");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.caption(), "312 ratings");
}

TEST_F(RatingControlTest, SetSameCaptionNoSignal) {
    RatingControl rc;
    rc.setCaption("test");
    QSignalSpy spy(&rc, &RatingControl::captionChanged);
    rc.setCaption("test");
    EXPECT_EQ(spy.count(), 0);
}

// ── IsClearEnabled 属性 ──────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetIsClearEnabledEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::isClearEnabledChanged);
    rc.setIsClearEnabled(false);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_FALSE(rc.isClearEnabled());
}

// ── IsReadOnly 属性 ──────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetIsReadOnlyEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::isReadOnlyChanged);
    rc.setIsReadOnly(true);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(rc.isReadOnly());
}

TEST_F(RatingControlTest, ReadOnlyCursorIsArrow) {
    RatingControl rc;
    rc.setIsReadOnly(true);
    EXPECT_EQ(rc.cursor().shape(), Qt::ArrowCursor);
}

// ── MaxRating 属性 ───────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetMaxRatingEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::maxRatingChanged);
    rc.setMaxRating(10);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.maxRating(), 10);
}

TEST_F(RatingControlTest, MaxRatingClampsExistingValue) {
    RatingControl rc;
    rc.setValue(5.0);
    rc.setMaxRating(3);
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
}

TEST_F(RatingControlTest, MaxRatingMinIsOne) {
    RatingControl rc;
    rc.setMaxRating(0);
    EXPECT_EQ(rc.maxRating(), 1);
}

// ── StarSize 属性 ────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SetStarSizeEmitsSignal) {
    RatingControl rc;
    QSignalSpy spy(&rc, &RatingControl::starSizeChanged);
    rc.setStarSize(32);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(rc.starSize(), 32);
}

// ── SizeHint ─────────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, SizeHintReflectsMaxRating) {
    RatingControl rc;
    QSize hint5 = rc.sizeHint();
    rc.setMaxRating(10);
    QSize hint10 = rc.sizeHint();
    EXPECT_GT(hint10.width(), hint5.width());
}

TEST_F(RatingControlTest, SizeHintIncludesCaption) {
    RatingControl rc;
    QSize hintNoCaption = rc.sizeHint();
    rc.setCaption("312 ratings");
    QSize hintWithCaption = rc.sizeHint();
    EXPECT_GT(hintWithCaption.width(), hintNoCaption.width());
}

// ── Disabled 状态 ────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, DisabledState) {
    RatingControl rc;
    rc.setValue(3.0);
    rc.setEnabled(false);
    EXPECT_FALSE(rc.isEnabled());
    EXPECT_DOUBLE_EQ(rc.value(), 3.0);
}

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(RatingControlTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);

    // 1. 简单评分
    layout->addWidget(new QLabel("1. Simple RatingControl:", window));
    auto* rc1 = new RatingControl(window);
    rc1->setCaption("312 ratings");
    auto* label1 = new QLabel("Output: Unset", window);
    QObject::connect(rc1, &RatingControl::valueChanged, [rc1, label1](double v) {
        if (v < 0)
            label1->setText("Output: Unset");
        else
            label1->setText(QString("Output: %1 stars").arg(v, 0, 'f', 1));
        rc1->setCaption("Your rating");
    });
    layout->addWidget(rc1);
    layout->addWidget(label1);

    // 2. 占位值
    layout->addWidget(new QLabel("2. PlaceholderValue = 3.5:", window));
    auto* rc2 = new RatingControl(window);
    rc2->setPlaceholderValue(3.5);
    layout->addWidget(rc2);

    // 3. 只读
    layout->addWidget(new QLabel("3. IsReadOnly (value = 4):", window));
    auto* rc3 = new RatingControl(window);
    rc3->setValue(4.0);
    rc3->setIsReadOnly(true);
    layout->addWidget(rc3);

    // 4. 禁用
    layout->addWidget(new QLabel("4. Disabled (value = 2.5):", window));
    auto* rc4 = new RatingControl(window);
    rc4->setValue(2.5);
    rc4->setEnabled(false);
    layout->addWidget(rc4);

    // 5. MaxRating = 10, 大星星
    layout->addWidget(new QLabel("5. MaxRating = 10, StarSize = 20:", window));
    auto* rc5 = new RatingControl(window);
    rc5->setMaxRating(10);
    rc5->setStarSize(20);
    rc5->setPlaceholderValue(7.0);
    layout->addWidget(rc5);

    // 6. IsClearEnabled = false
    layout->addWidget(new QLabel("6. IsClearEnabled = false:", window));
    auto* rc6 = new RatingControl(window);
    rc6->setIsClearEnabled(false);
    layout->addWidget(rc6);

    layout->addStretch();

    // 主题切换
    auto* themeBtn = new Button("Switch Theme", window);
    themeBtn->setFixedSize(120, 32);
    layout->addWidget(themeBtn);
    QObject::connect(themeBtn, &Button::clicked, []() {
        FluentElement::setTheme(FluentElement::currentTheme() == FluentElement::Light
                                ? FluentElement::Dark
                                : FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
