#include <gtest/gtest.h>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QImage>
#include <QPainter>
#include <QRegion>
#include <QSignalSpy>
#include "components/basicinput/RatingControl.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Typography.h"
#include "design/Spacing.h"

using namespace fluent::basicinput;

// ── 测试窗口 ─────────────────────────────────────────────────────────────────

class RatingControlTestWindow : public QWidget, public fluent::FluentElement {
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

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// A star is geometrically design-neutral, so RatingControl's per-language delta is purely color
// semantics: the FILLED star is the accent in every language, while the UNFILLED tone differs
// (Fluent strokeSecondary / Material 3 strokeStrong "outline" / macOS dim textSecondary), plus a
// theme-aware hover-preview tint for M3/macOS. This suite sweeps 3 languages × 2 themes with a fixed
// value (3 of 5) and asserts each render is valid, paints content, and never emits an opaque
// near-black surface at a FILLED-star center — the invalid-QColor trap (a default-constructed QColor
// is INVALID yet QColor::alpha()==255, so setBrush/setPen on it paints SOLID OPAQUE BLACK).
// Design language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: 星形与设计语言无关,故 RatingControl 的语言差异纯为颜色语义:填充星在所有语言下均为 accent,
// 未填充色调不同(Fluent strokeSecondary / Material 3 strokeStrong「outline」/ macOS 偏暗 textSecondary),
// 外加 M3/macOS 的主题感知 hover 预览着色。本套件以固定值(5 中 3)遍历 3 语言 × 2 主题,断言每次渲染
// 有效、绘制出内容,且填充星中心绝不呈现不透明近黑表面——即无效 QColor 陷阱(默认构造 QColor 无效却返回
// alpha==255,对其 setBrush/setPen 会涂成不透明纯黑)。设计语言与主题为全局单例,夹具在 TearDown 中复位二者。
class RatingControlDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Build a RatingControl with a fixed value and grab it as an image at its sizeHint.
    // zh_CN: 以固定值构建 RatingControl,按 sizeHint 抓取为图像。
    static QImage grabRating(double value) {
        RatingControl rc;
        rc.setMaxRating(5);
        rc.setStarSize(16);
        rc.setValue(value);
        rc.resize(rc.sizeHint());
        return rc.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    // Sample the vertical center row across the filled-star band; report whether any pixel there is an
    // opaque near-black surface. zh_CN: 沿垂直中心行采样填充星区域;报告其中是否存在不透明近黑表面。
    static bool hasOpaqueBlackInFilledBand(const QImage& img, int starCount) {
        const int y = img.height() / 2;
        // The filled stars occupy the leftmost `starCount` cells; scan generously across that band.
        // zh_CN: 填充星占据最左 starCount 个单元;在该区域宽松扫描。
        const int bandRight = qMin(img.width(), (img.width() * starCount) / 5 + 4);
        for (int x = 0; x < bandRight; ++x) {
            const QColor px = img.pixelColor(x, y);
            const int lum = qRound(0.299 * px.red() + 0.587 * px.green() + 0.114 * px.blue());
            if (px.alpha() > 200 && lum < 16)
                return true;
        }
        return false;
    }
};

TEST_F(RatingControlDesignLanguageTest, AllLanguagesAndThemesPaintWithoutOpaqueBlackStars) {
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

    for (const auto& lang : langs) {
        for (const auto& th : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(lang.lang);
            fluent::FluentElement::setTheme(th.theme);

            const QImage img = grabRating(3.0);  // 3 of 5 stars filled. zh_CN: 5 中 3 填充。
            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // 1. Valid, non-empty image that actually painted something. zh_CN: 有效、非空且确实绘制了内容的图像。
            ASSERT_FALSE(img.isNull()) << ctx;
            EXPECT_GT(img.width(), 0) << ctx;
            EXPECT_GT(img.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(img)) << "RatingControl painted nothing: " << ctx;

            // 2. The filled-star band must not contain an opaque near-black surface (invalid-QColor trap).
            // zh_CN: 填充星区域不得包含不透明近黑表面(无效 QColor 陷阱)。
            EXPECT_FALSE(hasOpaqueBlackInFilledBand(img, 3))
                << "RatingControl filled stars rendered an opaque near-black surface: " << ctx;
        }
    }
}

// ── VisualCheck ──────────────────────────────────────────────────────────────

TEST_F(RatingControlDesignLanguageTest, SelectedStarHasSolidCenterAndEmptyStarDoesNot) {
    RatingControl rating;
    rating.setMaxRating(2);
    rating.setStarSize(28);
    rating.setValue(1.0);
    rating.resize(rating.sizeHint());

    const QColor background(17, 23, 31);
    QImage image(rating.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    QPainter painter(&image);
    rating.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    const QColor selectedCenter = image.pixelColor(image.width() / 4,
                                                    image.height() / 2);
    const QColor emptyCenter = image.pixelColor(image.width() * 3 / 4,
                                                 image.height() / 2);
    EXPECT_NE(selectedCenter, background);
    EXPECT_EQ(emptyCenter, background);
}

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
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
                                ? fluent::FluentElement::Dark
                                : fluent::FluentElement::Light);
    });

    window->show();
    qApp->exec();
}
