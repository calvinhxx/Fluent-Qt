#include <gtest/gtest.h>

#include <QImage>
#include <QMetaProperty>
#include <QPainter>
#include <QRawFont>
#include <QSignalSpy>

#include "components/foundation/FontIcon.h"

namespace {

QImage renderIcon(fluent::FontIcon& icon, qreal dpr = 1.0)
{
    icon.resize(64, 64);
    QImage image(QSize(64 * dpr, 64 * dpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    icon.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    return image;
}

QImage renderText(const QString& glyph, const QString& family, int pixelSize, const QColor& color, qreal dpr)
{
    QImage image(QSize(64 * dpr, 64 * dpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QFont font(family);
    font.setPixelSize(pixelSize);
    fluentConfigureTextRendering(font);
    QPainter painter(&image);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(QRectF(0, 0, 64, 64), Qt::AlignCenter, glyph);
    return image;
}

} // namespace

TEST(FontIconTest, Contract_DefaultsAndInheritance)
{
    static_assert(std::is_base_of<QWidget, fluent::FontIcon>::value, "FontIcon must remain a QWidget");
    static_assert(std::is_base_of<fluent::FluentElement, fluent::FontIcon>::value,
        "FontIcon must expose FluentElement");
    static_assert(std::is_base_of<fluent::QMLPlus, fluent::FontIcon>::value, "FontIcon must expose QMLPlus");

    fluent::FontIcon icon;
    EXPECT_TRUE(icon.glyph().isEmpty());
    EXPECT_EQ(icon.iconSize(), Typography::IconSize::Standard);
    EXPECT_FALSE(icon.color().isValid());
    EXPECT_EQ(icon.sizeHint(), QSize(Typography::IconSize::Standard, Typography::IconSize::Standard));
}

TEST(FontIconTest, Contract_SettersNormalizeAndSuppressDuplicateSignals)
{
    fluent::FontIcon icon;
    QSignalSpy glyphSpy(&icon, &fluent::FontIcon::glyphChanged);
    QSignalSpy sizeSpy(&icon, &fluent::FontIcon::iconSizeChanged);
    QSignalSpy colorSpy(&icon, &fluent::FontIcon::colorChanged);
    QSignalSpy rotationSpy(&icon, &fluent::FontIcon::rotationChanged);

    icon.setGlyph(Typography::Icons::Settings);
    icon.setGlyph(Typography::Icons::Settings);
    EXPECT_EQ(glyphSpy.count(), 1);

    icon.setIconSize(0);
    icon.setIconSize(1);
    EXPECT_EQ(icon.iconSize(), 1);
    EXPECT_EQ(sizeSpy.count(), 1);

    const QColor custom(Qt::red);
    icon.setColor(custom);
    icon.setColor(custom);
    EXPECT_EQ(colorSpy.count(), 1);

    icon.setRotation(90.0);
    icon.setRotation(90.0);
    EXPECT_DOUBLE_EQ(icon.rotation(), 90.0);
    EXPECT_EQ(rotationSpy.count(), 1);
}

TEST(FontIconTest, Contract_PaintsBundledGlyph)
{
    fluent::FontIcon icon(Typography::Icons::Settings);
    icon.setIconSize(Typography::IconSize::Large);
    icon.resize(32, 32);

    QImage image(icon.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    icon.render(&painter);
    painter.end();

    bool hasVisiblePixel = false;
    for (int y = 0; y < image.height() && !hasVisiblePixel; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                hasVisiblePixel = true;
                break;
            }
        }
    }
    EXPECT_TRUE(hasVisiblePixel);
}

TEST(FontIconTest, Contract_FontFamilyPropertyAndDuplicateSignals)
{
    fluent::FontIcon icon;
    EXPECT_TRUE(icon.fontFamily().isEmpty());
    const int index = icon.metaObject()->indexOfProperty("fontFamily");
    ASSERT_GE(index, 0);
    EXPECT_TRUE(icon.metaObject()->property(index).hasNotifySignal());
    QSignalSpy changes(&icon, &fluent::FontIcon::fontFamilyChanged);
    icon.setFontFamily(QString());
    EXPECT_EQ(changes.count(), 0);
    const QString family = Typography::FontFamily::UIText;
    ASSERT_TRUE(icon.setProperty("fontFamily", family));
    icon.setFontFamily(family);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(changes.at(0).at(0).toString(), family);
    EXPECT_EQ(icon.fontFamily(), family);
    EXPECT_EQ(icon.sizeHint(), QSize(icon.iconSize(), icon.iconSize()));
    icon.setFontFamily(QString());
    icon.setFontFamily(QString());
    EXPECT_EQ(changes.count(), 2);
    EXPECT_TRUE(changes.at(1).at(0).toString().isEmpty());
    EXPECT_TRUE(icon.fontFamily().isEmpty());
}

TEST(FontIconTest, Contract_CustomFontUsesExactPixelSizeAtMultipleDpr)
{
    const QString family = Typography::FontFamily::UIText;
    const QRawFont raw = QRawFont::fromFont(QFont(family));
    ASSERT_TRUE(raw.isValid());
    // Check the physical face: backend family names can include fallback aliases.
    const QRawFont expected(QStringLiteral(":/res/fonts/FluentQtUIText-Regular.ttf"), 19);
    ASSERT_TRUE(expected.isValid());
    for (const char* table : {"name", "cmap"}) {
        ASSERT_FALSE(expected.fontTable(table).isEmpty());
        ASSERT_EQ(raw.fontTable(table), expected.fontTable(table)) << table;
    }
    ASSERT_TRUE(raw.supportsCharacter('F'));
    fluent::FontIcon icon(QStringLiteral("F"));
    icon.setFontFamily(family);
    icon.setIconSize(19); // Must not snap to the bundled 20 px optical slot.
    icon.setColor(Qt::red);
    for (qreal dpr : {1.0, 1.25, 2.0, 3.0}) {
        SCOPED_TRACE(dpr);
        const QImage actual = renderIcon(icon, dpr);
        EXPECT_EQ(actual, renderText(QStringLiteral("F"), family, 19, Qt::red, dpr));
        EXPECT_NE(actual, renderText(QStringLiteral("F"), family, 20, Qt::red, dpr));
    }
}

TEST(FontIconTest, Contract_CustomFontBypassesOpticalSubstitutionAndCanReset)
{
    const QString glyph = Typography::Icons::glyph(QStringLiteral("ic_fluent_add_32_regular"));
    ASSERT_FALSE(glyph.isEmpty());
    ASSERT_NE(glyph, Typography::Icons::glyphForSize(glyph, 16));
    fluent::FontIcon icon(glyph);
    icon.setIconSize(16);
    icon.setColor(Qt::red);
    const QImage bundled = renderIcon(icon);
    icon.setFontFamily(Typography::FontFamily::FluentIcons);
    const QImage custom = renderIcon(icon);
    EXPECT_EQ(custom, renderText(glyph, Typography::FontFamily::FluentIcons, 16, Qt::red, 1.0));
    EXPECT_NE(custom, bundled);
    icon.setFontFamily(QString());
    EXPECT_EQ(renderIcon(icon), bundled);
}

TEST(FontIconTest, Contract_CustomFontPreservesSupplementaryGlyph)
{
    QString glyph;
    for (const auto& entry : Typography::Icons::catalog()) {
        if (entry.codepoint > 0xffff) {
            glyph = entry.glyph();
            break;
        }
    }
    ASSERT_EQ(glyph.size(), 2);
    fluent::FontIcon icon(glyph);
    icon.setFontFamily(Typography::FontFamily::FluentIcons);
    icon.setIconSize(24);
    icon.setColor(Qt::red);
    EXPECT_EQ(renderIcon(icon), renderText(glyph, Typography::FontFamily::FluentIcons, 24, Qt::red, 1.0));
    EXPECT_EQ(icon.glyph(), glyph);
}

TEST(FontIconTest, Contract_CustomFontKeepsThemeDisabledColorAndRotation)
{
    const auto originalTheme = fluent::FluentElement::currentTheme();
    fluent::FontIcon icon(QStringLiteral("F"));
    icon.setFontFamily(Typography::FontFamily::UIText);
    icon.setIconSize(24);
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        fluent::FluentElement::setTheme(theme);
        for (bool enabled : {true, false}) {
            icon.setEnabled(enabled);
            const QColor expected = enabled ? icon.themeColors().textPrimary : icon.themeColors().textDisabled;
            EXPECT_EQ(renderIcon(icon), renderText(icon.glyph(), icon.fontFamily(), 24, expected, 1.0));
        }
    }
    icon.setColor(Qt::red);
    EXPECT_EQ(renderIcon(icon), renderText(icon.glyph(), icon.fontFamily(), 24, Qt::red, 1.0));
    const QImage upright = renderIcon(icon);
    icon.setRotation(90.0);
    EXPECT_NE(renderIcon(icon), upright);
    icon.setRotation(0.0);
    EXPECT_EQ(renderIcon(icon), upright);
    fluent::FluentElement::setTheme(originalTheme);
}
