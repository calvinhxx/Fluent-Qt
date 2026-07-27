#include <gtest/gtest.h>

#include <QImage>
#include <QPainter>
#include <QSignalSpy>

#include "components/foundation/FontIcon.h"

TEST(FontIconTest, Contract_DefaultsAndInheritance)
{
    static_assert(std::is_base_of<QWidget, fluent::FontIcon>::value,
                  "FontIcon must remain a QWidget");
    static_assert(std::is_base_of<fluent::FluentElement, fluent::FontIcon>::value,
                  "FontIcon must expose FluentElement");
    static_assert(std::is_base_of<fluent::QMLPlus, fluent::FontIcon>::value,
                  "FontIcon must expose QMLPlus");

    fluent::FontIcon icon;
    EXPECT_TRUE(icon.glyph().isEmpty());
    EXPECT_EQ(icon.iconSize(), Typography::IconSize::Standard);
    EXPECT_FALSE(icon.color().isValid());
    EXPECT_EQ(icon.sizeHint(), QSize(Typography::IconSize::Standard,
                                    Typography::IconSize::Standard));
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
