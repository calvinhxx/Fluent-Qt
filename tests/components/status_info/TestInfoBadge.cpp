#include <gtest/gtest.h>

#include <QApplication>
#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

#include "design/Typography.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/basicinput/Button.h"
#include "components/status_info/InfoBadge.h"

using namespace fluent::status_info;
using fluent::basicinput::Button;

class InfoBadgeTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override {
        const auto& c = themeColors();
        QPalette pal = palette();
        pal.setColor(QPalette::Window, c.bgCanvas);
        setPalette(pal);
        setAutoFillBackground(true);
    }
};

class InfoBadgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
        window = new InfoBadgeTestWindow();
        window->setWindowTitle("InfoBadge Visual Test");
        window->resize(900, 720);
        window->onThemeUpdated();
    }

    void TearDown() override {
        delete window;
        window = nullptr;
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    QImage renderBadge(InfoBadge& badge) {
        badge.resize(badge.sizeHint());
        QImage image(badge.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        badge.render(&image);
        return image;
    }

    bool hasVisiblePixel(const QImage& image) const {
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0) return true;
            }
        }
        return false;
    }

    QRect visibleAlphaBounds(const QImage& image) const {
        int left = image.width();
        int top = image.height();
        int right = -1;
        int bottom = -1;

        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) == 0) continue;
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }

        if (right < left || bottom < top) return QRect();
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }

    InfoBadgeTestWindow* window = nullptr;
};

TEST_F(InfoBadgeTest, DefaultPropertyValues) {
    InfoBadge badge;

    EXPECT_EQ(badge.value(), -1);
    EXPECT_TRUE(badge.iconGlyph().isEmpty());
    EXPECT_EQ(badge.displayMode(), InfoBadge::InfoBadgeDisplayMode::Auto);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.status(), InfoBadge::InfoBadgeStatus::Attention);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 1.0);
    EXPECT_FALSE(badge.customBackgroundColor().isValid());
    EXPECT_FALSE(badge.customTextColor().isValid());
    EXPECT_EQ(badge.valueFontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(badge.beaconDiameter(), 4);
    EXPECT_EQ(badge.badgeHeight(), 16);
    EXPECT_EQ(badge.valueHorizontalPadding(), 8);
    EXPECT_EQ(badge.iconGlyphSize(), 10);
    EXPECT_EQ(badge.iconFontFamily(), Typography::FontFamily::SegoeFluentIcons);
    EXPECT_EQ(badge.badgeBackgroundInset(), 0);
    EXPECT_EQ(badge.contentOffset(), QPoint(0, 0));
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));
}

TEST_F(InfoBadgeTest, PropertySignalsAndSameValueNoSignal) {
    InfoBadge badge;

    QSignalSpy valueSpy(&badge, &InfoBadge::valueChanged);
    badge.setValue(5);
    EXPECT_EQ(valueSpy.count(), 1);
    badge.setValue(5);
    EXPECT_EQ(valueSpy.count(), 1);

    QSignalSpy iconSpy(&badge, &InfoBadge::iconGlyphChanged);
    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(iconSpy.count(), 1);
    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(iconSpy.count(), 1);

    QSignalSpy modeSpy(&badge, &InfoBadge::displayModeChanged);
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(modeSpy.count(), 1);
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(modeSpy.count(), 1);

    QSignalSpy statusSpy(&badge, &InfoBadge::statusChanged);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(statusSpy.count(), 1);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(statusSpy.count(), 1);

    QSignalSpy opacitySpy(&badge, &InfoBadge::badgeOpacityChanged);
    badge.setBadgeOpacity(0.5);
    EXPECT_EQ(opacitySpy.count(), 1);
    badge.setBadgeOpacity(0.5);
    EXPECT_EQ(opacitySpy.count(), 1);

    QSignalSpy backgroundSpy(&badge, &InfoBadge::customBackgroundColorChanged);
    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    EXPECT_EQ(backgroundSpy.count(), 1);
    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    EXPECT_EQ(backgroundSpy.count(), 1);

    QSignalSpy textSpy(&badge, &InfoBadge::customTextColorChanged);
    badge.setCustomTextColor(Qt::white);
    EXPECT_EQ(textSpy.count(), 1);
    badge.setCustomTextColor(Qt::white);
    EXPECT_EQ(textSpy.count(), 1);

    QSignalSpy glyphSizeSpy(&badge, &InfoBadge::iconGlyphSizeChanged);
    badge.setIconGlyphSize(12);
    EXPECT_EQ(glyphSizeSpy.count(), 1);
    badge.setIconGlyphSize(12);
    EXPECT_EQ(glyphSizeSpy.count(), 1);
}

TEST_F(InfoBadgeTest, ConfigurableMetricsAffectGeometryAndRendering) {
    InfoBadge badge;

    QSignalSpy beaconSpy(&badge, &InfoBadge::beaconDiameterChanged);
    badge.setBeaconDiameter(6);
    EXPECT_EQ(beaconSpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), QSize(6, 6));
    badge.setBeaconDiameter(0);
    EXPECT_EQ(badge.beaconDiameter(), 6);
    EXPECT_EQ(beaconSpy.count(), 1);

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    badge.setIconGlyph(Typography::Icons::Message);
    QSignalSpy heightSpy(&badge, &InfoBadge::badgeHeightChanged);
    badge.setBadgeHeight(18);
    EXPECT_EQ(heightSpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), QSize(18, 18));

    QSignalSpy fontFamilySpy(&badge, &InfoBadge::iconFontFamilyChanged);
    badge.setIconFontFamily(Typography::FontFamily::SegoeUI);
    EXPECT_EQ(fontFamilySpy.count(), 1);
    badge.setIconFontFamily(Typography::FontFamily::SegoeUI);
    EXPECT_EQ(fontFamilySpy.count(), 1);

    QSignalSpy insetSpy(&badge, &InfoBadge::badgeBackgroundInsetChanged);
    badge.setBadgeBackgroundInset(1);
    EXPECT_EQ(insetSpy.count(), 1);
    badge.setBadgeBackgroundInset(-2);
    EXPECT_EQ(badge.badgeBackgroundInset(), 0);
    EXPECT_EQ(insetSpy.count(), 2);

    QSignalSpy offsetSpy(&badge, &InfoBadge::contentOffsetChanged);
    badge.setContentOffset(QPoint(1, -1));
    EXPECT_EQ(offsetSpy.count(), 1);
    EXPECT_EQ(badge.contentOffset(), QPoint(1, -1));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);
    badge.setValue(100);
    badge.setBadgeHeight(16);
    const int compactWidth = badge.sizeHint().width();
    QSignalSpy paddingSpy(&badge, &InfoBadge::valueHorizontalPaddingChanged);
    badge.setValueHorizontalPadding(18);
    EXPECT_EQ(paddingSpy.count(), 1);
    EXPECT_GT(badge.sizeHint().width(), compactWidth);
    badge.setValueHorizontalPadding(18);
    EXPECT_EQ(paddingSpy.count(), 1);
}

TEST_F(InfoBadgeTest, DisplayModeSelection) {
    InfoBadge badge;
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);

    badge.setIconGlyph(Typography::Icons::Message);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(5);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Value);
    badge.setValue(-1);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));
}

TEST_F(InfoBadgeTest, ValueClampAndSentinelFallback) {
    InfoBadge badge;
    badge.setIconGlyph(Typography::Icons::Message);
    badge.setValue(10);
    EXPECT_EQ(badge.value(), 10);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Value);
    EXPECT_GT(badge.sizeHint().width(), 16);

    badge.setValue(-1);
    EXPECT_EQ(badge.value(), -1);
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Icon);

    badge.setIconGlyph(QString());
    EXPECT_EQ(badge.effectiveDisplayMode(), InfoBadge::InfoBadgeDisplayMode::Dot);

    badge.setValue(6);
    QSignalSpy valueSpy(&badge, &InfoBadge::valueChanged);
    badge.setValue(-9);
    EXPECT_EQ(badge.value(), -1);
    EXPECT_EQ(valueSpy.count(), 1);
}

TEST_F(InfoBadgeTest, SizeHintsReflectVariants) {
    InfoBadge badge;
    EXPECT_EQ(badge.sizeHint(), QSize(4, 4));
    EXPECT_EQ(badge.minimumSizeHint(), QSize(4, 4));

    badge.setIconGlyph(Typography::Icons::Mail);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(7);
    EXPECT_EQ(badge.sizeHint(), QSize(16, 16));

    badge.setValue(100);
    EXPECT_EQ(badge.sizeHint().height(), 16);
    EXPECT_GT(badge.sizeHint().width(), 16);
}

TEST_F(InfoBadgeTest, OpacityClampAndRendering) {
    InfoBadge badge;
    badge.setValue(100);
    const QSize wideSize = badge.sizeHint();

    QSignalSpy opacitySpy(&badge, &InfoBadge::badgeOpacityChanged);
    badge.setBadgeOpacity(2.0);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 1.0);
    EXPECT_EQ(opacitySpy.count(), 0);

    badge.setBadgeOpacity(-0.5);
    EXPECT_DOUBLE_EQ(badge.badgeOpacity(), 0.0);
    EXPECT_EQ(opacitySpy.count(), 1);
    EXPECT_EQ(badge.sizeHint(), wideSize);
    EXPECT_FALSE(hasVisiblePixel(renderBadge(badge)));

    badge.setBadgeOpacity(1.0);
    EXPECT_TRUE(hasVisiblePixel(renderBadge(badge)));
}

TEST_F(InfoBadgeTest, StatusCustomColorsAndThemeRendering) {
    InfoBadge badge;
    badge.setValue(5);

    badge.setStatus(InfoBadge::InfoBadgeStatus::Informational);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemInfo);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Caution);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemCaution);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Success);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemSuccess);
    badge.setStatus(InfoBadge::InfoBadgeStatus::Critical);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().systemCritical);

    badge.setCustomBackgroundColor(QColor("#C42B1C"));
    badge.setCustomTextColor(QColor("#FFFFFF"));
    EXPECT_EQ(badge.effectiveBackgroundColor(), QColor("#C42B1C"));
    EXPECT_EQ(badge.effectiveForegroundColor(), QColor("#FFFFFF"));

    const QImage light = renderBadge(badge);
    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    badge.setCustomBackgroundColor(QColor());
    badge.setCustomTextColor(QColor());
    badge.onThemeUpdated();
    badge.setStatus(InfoBadge::InfoBadgeStatus::Attention);
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().accentDefault);
    const QImage dark = renderBadge(badge);
    EXPECT_NE(light, dark);

    badge.setEnabled(false);
    QApplication::processEvents();
    EXPECT_EQ(badge.effectiveBackgroundColor(), badge.themeColors().accentDisabled);
    EXPECT_TRUE(hasVisiblePixel(renderBadge(badge)));
}

TEST_F(InfoBadgeTest, IconGlyphIsVisuallyCentered) {
    InfoBadge badge;
    badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    badge.setIconGlyph(Typography::Icons::Message);
    badge.setCustomBackgroundColor(QColor(0, 0, 0, 0));
    badge.setCustomTextColor(Qt::black);

    const QImage image = renderBadge(badge);
    const QRect bounds = visibleAlphaBounds(image);
    ASSERT_FALSE(bounds.isNull());

    const QPointF imageCenter((image.width() - 1) / 2.0, (image.height() - 1) / 2.0);
    const QPointF glyphCenter = QRectF(bounds).center();
    EXPECT_LE(std::abs(glyphCenter.x() - imageCenter.x()), 1.0);
    EXPECT_LE(std::abs(glyphCenter.y() - imageCenter.y()), 1.0);
}

// ─── Design-language × theme compatibility ──────────────────────────────────
//
// InfoBadge paints the SAME fully-rounded badge under all three design languages (the Fluent shape
// is already circle-for-dot / pill-for-value), but Material 3 and macOS force WHITE on-color text and
// macOS defaults the neutral Attention state to system RED. Fluent is byte-for-byte unchanged. Design
// language + theme are GLOBAL singletons, so the fixture restores both in TearDown.
// zh_CN: InfoBadge 在三种设计语言下绘制相同的全圆角徽标(Fluent 形状本就是提示点圆形/数值胶囊),但
// Material 3 与 macOS 强制白色 on-color 文字,且 macOS 在中性 Attention 状态下默认系统红。Fluent 逐字节
// 不变。设计语言与主题为全局单例,夹具在 TearDown 中恢复二者。
class InfoBadgeDesignLanguageTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Design language + theme are GLOBAL — reset so later suites see defaults.
        // zh_CN: 设计语言与主题为全局状态;复位以保证后续套件看到默认值。
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    // Numbered/value badge sized like a real notification count (~24x16). zh_CN: 数值徽标,尺寸接近真实通知计数(约 24x16)。
    static QImage grabValueBadge(InfoBadge::InfoBadgeStatus status) {
        InfoBadge badge;
        badge.setStatus(status);
        badge.setValue(5);
        badge.resize(24, 16);
        return badge.grab().toImage();
    }

    // Dot badge sized like a real beacon dot (~20x20 host so the small dot has room). zh_CN: 提示点徽标(约 20x20 容器,使小点有余量)。
    static QImage grabDotBadge(InfoBadge::InfoBadgeStatus status) {
        InfoBadge badge;
        badge.setStatus(status);
        badge.setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Dot);
        badge.setBeaconDiameter(12);
        badge.resize(20, 20);
        return badge.grab().toImage();
    }

    static bool hasPaintedContent(const QImage& img) {
        const QRgb bg = img.pixel(0, 0);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) != bg)
                    return true;
        return false;
    }

    // Average color of all pixels whose alpha is appreciably opaque — i.e. the painted fill region.
    // Returns whether any such pixel exists. zh_CN: 所有不透明像素(即填充区域)的平均色;返回是否存在此类像素。
    static bool averageFilledColor(const QImage& img, QColor& outAvg, int& outCount) {
        long long r = 0, g = 0, b = 0, n = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.alpha() <= 200) continue;
                r += c.red();
                g += c.green();
                b += c.blue();
                ++n;
            }
        }
        outCount = static_cast<int>(n);
        if (n == 0) return false;
        outAvg = QColor(static_cast<int>(r / n), static_cast<int>(g / n), static_cast<int>(b / n));
        return true;
    }
};

TEST_F(InfoBadgeDesignLanguageTest, AllLanguagesAndThemesPaintColoredFill) {
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

            const std::string ctx = std::string(lang.name) + "/" + th.name;

            // Use a NON-critical, colored state (Informational) so the fill is a real accent/blue
            // color — never legitimately near-black — letting us assert against the invalid-QColor
            // opaque-black trap. zh_CN: 用非危险的彩色状态(Informational),填充为真实强调/蓝色,绝不会合法
            // 接近黑色,从而可针对无效 QColor 不透明黑陷阱断言。
            const QImage value = grabValueBadge(InfoBadge::InfoBadgeStatus::Informational);
            const QImage dot = grabDotBadge(InfoBadge::InfoBadgeStatus::Informational);

            // 1. Valid, non-empty images with painted content. zh_CN: 有效、非空且有内容的图像。
            ASSERT_FALSE(value.isNull()) << ctx << "/value";
            ASSERT_FALSE(dot.isNull()) << ctx << "/dot";
            EXPECT_GT(value.width(), 0) << ctx;
            EXPECT_GT(value.height(), 0) << ctx;
            EXPECT_TRUE(hasPaintedContent(value)) << "value badge painted nothing: " << ctx;
            EXPECT_TRUE(hasPaintedContent(dot)) << "dot badge painted nothing: " << ctx;

            // 2. The fill region is non-empty AND is a real color (NOT opaque near-#000). The
            // invalid-QColor trap (setBrush on an unassigned color) would paint solid black here.
            // zh_CN: 填充区域非空且为真实颜色(非不透明近黑)。无效 QColor 陷阱会在此涂成纯黑。
            QColor valueFill;
            int valueFillCount = 0;
            ASSERT_TRUE(averageFilledColor(value, valueFill, valueFillCount))
                << "value badge has no filled pixels: " << ctx;
            EXPECT_GT(valueFillCount, 0) << ctx;
            const int valueLum = qRound(0.299 * valueFill.red() + 0.587 * valueFill.green()
                                        + 0.114 * valueFill.blue());
            EXPECT_GT(valueLum, 24)
                << "value badge fill is opaque near-black (invalid-QColor trap?): " << ctx
                << " rgb=(" << valueFill.red() << "," << valueFill.green() << "," << valueFill.blue() << ")";

            QColor dotFill;
            int dotFillCount = 0;
            ASSERT_TRUE(averageFilledColor(dot, dotFill, dotFillCount))
                << "dot badge has no filled pixels: " << ctx;
            EXPECT_GT(dotFillCount, 0) << ctx;
            const int dotLum = qRound(0.299 * dotFill.red() + 0.587 * dotFill.green()
                                      + 0.114 * dotFill.blue());
            EXPECT_GT(dotLum, 24)
                << "dot badge fill is opaque near-black (invalid-QColor trap?): " << ctx
                << " rgb=(" << dotFill.red() << "," << dotFill.green() << "," << dotFill.blue() << ")";
        }
    }
}

// macOS defaults the neutral Attention status to system RED (the canonical notification/dock badge),
// whereas Fluent reads accent blue. Verify the macOS Attention fill is red-dominant and differs from
// the Fluent Attention fill. zh_CN: macOS 在中性 Attention 状态默认系统红(经典通知/程序坞徽标),Fluent 读
// 强调蓝。验证 macOS Attention 填充偏红且与 Fluent Attention 填充不同。
TEST_F(InfoBadgeDesignLanguageTest, CupertinoAttentionReadsRed) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignFluent);
    QColor fluentFill;
    int fluentCount = 0;
    ASSERT_TRUE(averageFilledColor(grabDotBadge(InfoBadge::InfoBadgeStatus::Attention),
                                   fluentFill, fluentCount));

    fluent::ThemeRegistry::instance().setDesignLanguage(fluent::FluentElement::DesignCupertino);
    QColor macFill;
    int macCount = 0;
    ASSERT_TRUE(averageFilledColor(grabDotBadge(InfoBadge::InfoBadgeStatus::Attention),
                                   macFill, macCount));

    // macOS Attention fill is red-dominant. zh_CN: macOS Attention 填充偏红。
    EXPECT_GT(macFill.red(), macFill.blue())
        << "macOS Attention badge should read red: rgb=(" << macFill.red() << ","
        << macFill.green() << "," << macFill.blue() << ")";
    EXPECT_GT(macFill.red(), macFill.green())
        << "macOS Attention badge should read red: rgb=(" << macFill.red() << ","
        << macFill.green() << "," << macFill.blue() << ")";

    // And it must differ from the Fluent (accent blue) treatment. zh_CN: 且必须区别于 Fluent(强调蓝)。
    EXPECT_NE(fluentFill.rgb(), macFill.rgb())
        << "macOS Attention fill should differ from Fluent accent fill";
}

// Material 3 / macOS value badges paint WHITE on-color text. Sampling a colored Informational badge,
// the brightest filled pixel must read near-white (the glyph), proving white-on-color text.
// zh_CN: M3/macOS 数值徽标绘制白色 on-color 文字。对彩色 Informational 徽标采样,最亮的填充像素必近白
//(即字形),证明彩底白字。
TEST_F(InfoBadgeDesignLanguageTest, MaterialAndCupertinoValueTextIsWhite) {
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);

    const fluent::FluentElement::DesignLanguage langs[] = {
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };

    for (auto lang : langs) {
        fluent::ThemeRegistry::instance().setDesignLanguage(lang);

        InfoBadge badge;
        badge.setStatus(InfoBadge::InfoBadgeStatus::Informational);
        badge.setValue(8);
        // Generous size so the digit glyph has clearly-white interior pixels. zh_CN: 放大尺寸,使数字字形内部有明确白像素。
        badge.setBadgeHeight(28);
        badge.setValueHorizontalPadding(16);
        badge.resize(badge.sizeHint());
        const QImage img = badge.grab().toImage();
        ASSERT_FALSE(img.isNull());

        // Find the brightest opaque pixel — should be the white glyph on the colored fill.
        // zh_CN: 找出最亮的不透明像素——应为彩底上的白色字形。
        int maxLum = -1;
        QColor brightest;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.alpha() <= 200) continue;
                const int lum = qRound(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
                if (lum > maxLum) {
                    maxLum = lum;
                    brightest = c;
                }
            }
        }
        ASSERT_GE(maxLum, 0) << "no opaque pixels in value badge";
        EXPECT_GT(maxLum, 200)
            << "expected white-on-color text; brightest fill pixel rgb=(" << brightest.red()
            << "," << brightest.green() << "," << brightest.blue() << ")";
    }
}

TEST_F(InfoBadgeTest, VisualCheck) {
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        GTEST_SKIP() << "Skipping visual test in offscreen mode";
    }

    window->resize(940, 760);

    auto* root = new QVBoxLayout(window);
    root->setContentsMargins(24, 22, 24, 24);
    root->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("InfoBadge", window);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    title->setFont(titleFont);
    auto* themeButton = new Button("Toggle Light/Dark", window);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(themeButton);
    root->addLayout(header);

    QObject::connect(themeButton, &Button::clicked, [this]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Light
            ? fluent::FluentElement::Dark
            : fluent::FluentElement::Light);
        window->onThemeUpdated();
    });

    auto makeSectionLabel = [this](const QString& text) {
        auto* label = new QLabel(text, window);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        return label;
    };

    auto makePanel = [this](int minimumHeight) {
        auto* panel = new QFrame(window);
        panel->setFrameShape(QFrame::StyledPanel);
        panel->setMinimumHeight(minimumHeight);
        panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return panel;
    };

    root->addWidget(makeSectionLabel("Variants"));
    auto* variantsPanel = makePanel(320);
    auto* grid = new QGridLayout(variantsPanel);
    grid->setContentsMargins(28, 22, 28, 22);
    grid->setHorizontalSpacing(30);
    grid->setVerticalSpacing(18);
    root->addWidget(variantsPanel);

    const QVector<InfoBadge::InfoBadgeStatus> statuses = {
        InfoBadge::InfoBadgeStatus::Informational,
        InfoBadge::InfoBadgeStatus::Attention,
        InfoBadge::InfoBadgeStatus::Caution,
        InfoBadge::InfoBadgeStatus::Success,
        InfoBadge::InfoBadgeStatus::Critical,
    };
    const QStringList statusNames = {"Info", "Attention", "Caution", "Success", "Critical"};

    auto addBadgeExample = [grid, variantsPanel](int row, int column, const QString& labelText,
                                                 InfoBadge::InfoBadgeStatus status,
                                                 InfoBadge::InfoBadgeDisplayMode mode,
                                                 int value,
                                                 const QString& glyph) {
        auto* cell = new QWidget(variantsPanel);
        auto* layout = new QVBoxLayout(cell);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->setAlignment(Qt::AlignHCenter);

        auto* badge = new InfoBadge(cell);
        badge->setStatus(status);
        badge->setDisplayMode(mode);
        badge->setIconGlyph(glyph);
        badge->setValue(value);

        auto* label = new QLabel(labelText, cell);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(badge, 0, Qt::AlignHCenter);
        layout->addWidget(label, 0, Qt::AlignHCenter);
        grid->addWidget(cell, row, column);
    };

    for (int i = 0; i < statuses.size(); ++i) {
        addBadgeExample(0, i, statusNames.at(i) + " dot", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Dot, -1, QString());
        addBadgeExample(1, i, statusNames.at(i) + " icon", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Icon, -1, Typography::Icons::ImportantBadge12);
        addBadgeExample(2, i, statusNames.at(i) + " 5", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Value, 5, QString());
        addBadgeExample(3, i, statusNames.at(i) + " 100", statuses.at(i), InfoBadge::InfoBadgeDisplayMode::Value, 100, QString());
    }

    root->addWidget(makeSectionLabel("Embedded"));
    auto* embeddedPanel = makePanel(190);
    auto* embeddedLayout = new QHBoxLayout(embeddedPanel);
    embeddedLayout->setContentsMargins(28, 24, 28, 24);
    embeddedLayout->setSpacing(42);
    root->addWidget(embeddedPanel);

    auto* buttonHost = new QWidget(embeddedPanel);
    buttonHost->setFixedSize(170, 64);
    auto* inboxButton = new Button("Inbox", buttonHost);
    inboxButton->setGeometry(0, 16, 150, 32);
    auto* cornerBadge = new InfoBadge(buttonHost);
    cornerBadge->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::Icon);
    cornerBadge->setIconGlyph(Typography::Icons::Mail);
    cornerBadge->setCustomBackgroundColor(QColor("#C42B1C"));
    cornerBadge->setCustomTextColor(Qt::white);
    cornerBadge->resize(cornerBadge->sizeHint());
    cornerBadge->move(138, 8);
    embeddedLayout->addWidget(buttonHost, 0, Qt::AlignTop);

    auto makeNavRow = [embeddedPanel](const QString& text, qreal opacity) {
        auto* row = new QFrame(embeddedPanel);
        row->setFrameShape(QFrame::StyledPanel);
        row->setFixedSize(230, 44);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(14, 6, 14, 6);
        layout->setSpacing(12);
        auto* label = new QLabel(text, row);
        auto* badge = new InfoBadge(row);
        badge->setValue(5);
        badge->setBadgeOpacity(opacity);
        layout->addWidget(label);
        layout->addStretch();
        layout->addWidget(badge, 0, Qt::AlignVCenter);
        return row;
    };
    embeddedLayout->addWidget(makeNavRow("Navigation item", 1.0), 0, Qt::AlignTop);
    embeddedLayout->addWidget(makeNavRow("Opacity reserved", 0.0), 0, Qt::AlignTop);
    embeddedLayout->addStretch();

    root->addWidget(makeSectionLabel("Dynamic values"));
    auto* dynamicPanel = makePanel(110);
    auto* dynamicLayout = new QHBoxLayout(dynamicPanel);
    dynamicLayout->setContentsMargins(28, 18, 28, 18);
    dynamicLayout->setSpacing(28);
    root->addWidget(dynamicPanel);

    for (int value : {-1, 0, 5, 10, 100}) {
        auto* cell = new QWidget(dynamicPanel);
        auto* layout = new QVBoxLayout(cell);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* badge = new InfoBadge(cell);
        badge->setIconGlyph(Typography::Icons::Message);
        badge->setValue(value);
        auto* label = new QLabel(QString::number(value), cell);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(badge, 0, Qt::AlignHCenter);
        layout->addWidget(label, 0, Qt::AlignHCenter);
        dynamicLayout->addWidget(cell, 0, Qt::AlignTop);
    }
    dynamicLayout->addStretch();

    window->show();
    qApp->exec();
}