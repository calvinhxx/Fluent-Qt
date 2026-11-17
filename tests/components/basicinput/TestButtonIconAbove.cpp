#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QStyle>
#include <QTest>
#include <type_traits>

#include "components/basicinput/Button.h"
#include "components/basicinput/CompoundButton.h"
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/basicinput/RepeatButton.h"
#include "components/basicinput/SplitButton.h"
#include "components/basicinput/ToggleButton.h"
#include "components/basicinput/ToggleSplitButton.h"

using namespace fluent;
using namespace fluent::basicinput;

namespace {

template <typename Base> void checkDefaultIconLayout()
{
    struct Probe : Base {
        using Base::Base;
        using Base::sizeHint;
    };
    for (int source = 0; source < 4; ++source) {
        Probe button(QStringLiteral("Undo"));
        SCOPED_TRACE(::testing::Message() << button.metaObject()->className() << " source=" << source);
        const QSize plainSize = button.sizeHint();
        EXPECT_EQ(button.fluentLayout(), Button::TextOnly);
        QSignalSpy layoutChanged(&button, &Button::fluentLayoutChanged);
        QPixmap pixels(16, 16);
        pixels.fill(Qt::magenta);
        const QIcon icon(pixels);
        if (source == 0)
            button.setIconGlyph(Typography::Icons::Save);
        else if (source == 1)
            button.setIconGlyph(Typography::Icons::Save.at(0));
        else if (source == 2)
            button.setIcon(icon);
        else
            ASSERT_TRUE(button.setProperty("icon", icon));
        EXPECT_EQ(button.fluentLayout(), Button::IconBefore);
        EXPECT_EQ(layoutChanged.count(), 1);
        EXPECT_GT(button.sizeHint().width(), plainSize.width());
        button.resize(button.sizeHint());
        const QImage automatic = button.grab().toImage();
        const QSize automaticSize = button.sizeHint();
        button.setFluentLayout(Button::IconBefore);
        EXPECT_EQ(button.sizeHint(), automaticSize);
        EXPECT_EQ(button.grab().toImage(), automatic);
        EXPECT_EQ(layoutChanged.count(), 1);

        button.setFluentLayout(Button::TextOnly);
        EXPECT_EQ(button.sizeHint(), plainSize);
        button.resize(plainSize);
        const QImage textOnly = button.grab().toImage();
        button.setIconGlyph(Typography::Icons::Undo);
        EXPECT_EQ(button.fluentLayout(), Button::TextOnly);
        EXPECT_EQ(button.grab().toImage(), textOnly);

        Probe explicitText(QStringLiteral("Undo"));
        explicitText.setFluentLayout(Button::TextOnly);
        explicitText.setIconGlyph(Typography::Icons::Save);
        explicitText.setIcon(icon);
        EXPECT_EQ(explicitText.fluentLayout(), Button::TextOnly);
        EXPECT_EQ(explicitText.sizeHint(), plainSize);
    }
}

template <typename Base> class StackedButtonProbe : public Base {
public:
    using Base::Base;
    using Base::sizeHint;
    QRectF contentBounds() const
    {
        if constexpr (std::is_base_of<SplitButton, Base>::value) {
            const QRect primary(0, 0, this->width() - this->secondaryWidth(), this->height());
            return this->primaryContentRect(QStyle::visualRect(this->layoutDirection(), this->rect(), primary));
        } else {
            return this->contentPaintRect(this->rect());
        }
    }
};

QImage renderButton(QWidget& widget, qreal dpr)
{
    QImage image(QSize(qRound(widget.width() * dpr), qRound(widget.height() * dpr)),
        QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

QRect coloredInk(const QImage& image, const QColor& color, const QRectF& logicalArea)
{
    const qreal dpr = image.devicePixelRatio();
    const QRect area = QRectF(logicalArea.topLeft() * dpr, logicalArea.size() * dpr).toAlignedRect() & image.rect();
    if (area.isEmpty())
        return {};
    // Text tokens can be translucent; compare against their painted color on the surface.
    // zh_CN: 文字令牌可能半透明，应与合成到表面后的可见颜色比较。
    const QColor background = image.pixelColor(area.topLeft());
    const qreal alpha = color.alphaF();
    const QColor painted(qRound(color.red() * alpha + background.red() * (1 - alpha)),
        qRound(color.green() * alpha + background.green() * (1 - alpha)),
        qRound(color.blue() * alpha + background.blue() * (1 - alpha)));
    QRect result;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 200 && qAbs(pixel.red() - painted.red()) < 30
                && qAbs(pixel.green() - painted.green()) < 30 && qAbs(pixel.blue() - painted.blue()) < 30)
                result |= QRect(x, y, 1, 1);
        }
    }
    return result;
}

template <typename Base> void checkInheritedIconConfiguration()
{
    Base button(QStringLiteral("Read"));
    if constexpr (std::is_same<Base, CompoundButton>::value)
        button.setSecondaryText(QStringLiteral("Description"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.resize(260, 120);
    for (auto theme : {FluentElement::Light, FluentElement::Dark}) {
        button.setProperty("fluentThemeOverride", static_cast<int>(theme));
        for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
            button.setLayoutDirection(direction);
            for (bool glyph : {true, false}) {
                button.setIconGlyph(glyph ? Typography::Icons::Save : QString(), 24);
                QPixmap source(24, 24);
                source.fill(Qt::transparent);
                QPainter sourcePainter(&source);
                sourcePainter.fillRect(QRect(2, 2, 7, 18), Qt::magenta);
                sourcePainter.fillRect(QRect(2, 14, 16, 6), Qt::magenta);
                sourcePainter.end();
                button.setIcon(glyph ? QIcon() : QIcon(source));
                button.setIconSize(QSize(24, 24));
                QImage beforeImage;
                for (auto layout : {Button::TextOnly, Button::IconBefore, Button::IconAfter, Button::IconOnly,
                         Button::IconAbove, Button::IconBelow}) {
                    SCOPED_TRACE(::testing::Message()
                        << button.metaObject()->className() << " theme=" << theme << " direction=" << direction
                        << " glyph=" << glyph << " layout=" << layout);
                    button.setFluentLayout(layout);
                    button.setIconRotation(0);
                    button.setIconScale(1);
                    button.setIconOffset(QPoint());
                    const QImage initial = renderButton(button, 1.5);
                    if (layout == Button::IconBefore)
                        beforeImage = initial;
                    if (layout == Button::IconAfter)
                        EXPECT_NE(initial, beforeImage);
                    button.setIconRotation(90);
                    const QImage rotated = renderButton(button, 1.5);
                    button.setIconRotation(0);
                    button.setIconScale(0.6);
                    const QImage scaled = renderButton(button, 1.5);
                    button.setIconScale(1);
                    button.setIconOffset(QPoint(5, -3));
                    const QImage offset = renderButton(button, 1.5);
                    if (layout == Button::TextOnly) {
                        EXPECT_EQ(initial, rotated);
                        EXPECT_EQ(initial, scaled);
                        EXPECT_EQ(initial, offset);
                    } else {
                        EXPECT_NE(initial, rotated);
                        EXPECT_NE(initial, scaled);
                        EXPECT_NE(initial, offset);
                    }
                }
            }
        }
    }
}

template <typename Base>
void checkStackedContent(const QString& text, Button::ButtonLayout stackedLayout = Button::IconAbove)
{
    StackedButtonProbe<Base> button(text);
    SCOPED_TRACE(button.metaObject()->className());
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFluentLayout(stackedLayout);
    if constexpr (std::is_same<Base, CompoundButton>::value)
        button.setSecondaryText(QStringLiteral("Restore change"));
    for (bool glyph : {false, true}) {
        if (glyph) {
            button.setIconGlyph(Typography::Icons::Undo, 24);
        } else {
            button.setIconGlyph(QString());
            QPixmap source(24, 24);
            source.fill(Qt::magenta);
            button.setIcon(QIcon(source));
            button.setIconSize(QSize(24, 24));
        }
        for (auto density : {Button::Small, Button::StandardSize, Button::Large}) {
            button.setFluentSize(density);
            for (auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
                button.setProperty("fluentThemeOverride", static_cast<int>(theme));
                for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
                    button.setLayoutDirection(direction);
                    button.resize(button.sizeHint());
                    button.show();
                    button.clearFocus();
                    const QRectF bounds = button.contentBounds();
                    const int textHeight = QFontMetrics(button.font()).size(0, text).height();
                    const int gap =
                        density == Button::Small ? button.themeSpacing().gap.tight : button.themeSpacing().gap.normal;
                    const bool below = stackedLayout == Button::IconBelow;
                    const int captionHeight = std::is_same<Base, CompoundButton>::value
                        ? QFontMetrics(button.themeFont(Typography::FontRole::Caption).toQFont()).height()
                        : 0;
                    const int captionSpace =
                        below && captionHeight ? captionHeight + button.themeSpacing().gap.tight : 0;
                    const qreal top = bounds.center().y() - (24 + gap + textHeight + captionSpace) / 2.0;
                    const QRectF iconSlot(bounds.center().x() - 12, below ? top + textHeight + captionSpace + gap : top,
                        24, 24);
                    const QRectF labelSlot(bounds.left() + 2, below ? top : top + 24 + gap, bounds.width() - 4,
                        textHeight);
                    ASSERT_GE(iconSlot.top(), 0);
                    ASSERT_LE(labelSlot.bottom(), button.height());
                    const QColor textColor = std::is_same<Base, HyperlinkButton>::value
                        ? button.themeColorsRef().accentDefault
                        : button.themeColorsRef().textPrimary;
                    for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
                        SCOPED_TRACE(::testing::Message()
                            << "glyph=" << glyph << " density=" << density << " theme=" << theme
                            << " direction=" << direction << " dpr=" << dpr);
                        const QImage image = renderButton(button, dpr);
                        const QRect icon = coloredInk(image, glyph ? textColor : QColor(Qt::magenta), iconSlot);
                        const QRect label = coloredInk(image, textColor, labelSlot);
                        ASSERT_FALSE(icon.isEmpty());
                        ASSERT_FALSE(label.isEmpty());
                        if (below)
                            EXPECT_LT(label.bottom(), icon.top());
                        else
                            EXPECT_LT(icon.bottom(), label.top());
                        if constexpr (std::is_same<Base, CompoundButton>::value) {
                            const QRectF captionSlot(bounds.left(),
                                labelSlot.bottom() + button.themeSpacing().gap.tight, bounds.width(), captionHeight);
                            const QColor foreground = button.themeColorsRef().textSecondary;
                            const QColor background = image.pixelColor(qRound(bounds.center().x() * dpr),
                                qRound((captionSlot.top() - 1) * dpr));
                            const qreal alpha = foreground.alphaF();
                            const QColor captionColor(qRound(foreground.red() * alpha + background.red() * (1 - alpha)),
                                qRound(foreground.green() * alpha + background.green() * (1 - alpha)),
                                qRound(foreground.blue() * alpha + background.blue() * (1 - alpha)));
                            const QRect caption = coloredInk(image, captionColor, captionSlot);
                            EXPECT_FALSE(caption.isEmpty());
                            if (below)
                                EXPECT_LT(caption.bottom(), icon.top());
                        }
                        EXPECT_NEAR(icon.center().x() / dpr, bounds.center().x(), 3.0);
                        EXPECT_NEAR(label.center().x() / dpr, bounds.center().x(), 3.0);
                    }
                }
            }
        }
    }
    for (auto layout : {Button::TextOnly, Button::IconBefore, Button::IconOnly, Button::IconAfter, Button::IconAbove}) {
        if (layout == stackedLayout)
            continue;
        button.setFluentLayout(layout);
        const QSize original = button.sizeHint();
        button.resize(original);
        const QImage expected = renderButton(button, 1.5);
        button.setFluentLayout(stackedLayout);
        button.resize(button.sizeHint());
        button.setFluentLayout(layout);
        EXPECT_EQ(button.sizeHint(), original);
        button.resize(original);
        EXPECT_EQ(renderButton(button, 1.5), expected);
    }
    button.setFluentLayout(stackedLayout);
    button.resize(button.sizeHint());
    QSignalSpy clicks(&button, &QPushButton::clicked);
    const QPoint point = button.contentBounds().center().toPoint();
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(clicks.count(), 1);
    QTest::keyClick(&button, Qt::Key_Space);
    EXPECT_EQ(clicks.count(), 2);
    const QSize enabledSize = button.sizeHint();
    button.setEnabled(false);
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, point);
    QTest::keyClick(&button, Qt::Key_Space);
    EXPECT_EQ(clicks.count(), 2);
    EXPECT_EQ(button.sizeHint(), enabledSize);
}

} // namespace

TEST(ButtonDefaultIconLayoutTest, LegacyIconSettersPromoteOnlyUnconfiguredLayoutsAcrossFamilies)
{
    checkDefaultIconLayout<Button>();
    checkDefaultIconLayout<ToggleButton>();
    checkDefaultIconLayout<RepeatButton>();
    checkDefaultIconLayout<DropDownButton>();
    checkDefaultIconLayout<SplitButton>();
    checkDefaultIconLayout<ToggleSplitButton>();
    checkDefaultIconLayout<CompoundButton>();
    checkDefaultIconLayout<HyperlinkButton>();
}

TEST(ButtonIconAboveTest, Contract_EnumValuesSignalsAndEmptyContent)
{
    EXPECT_EQ(int(Button::TextOnly), 0);
    EXPECT_EQ(int(Button::IconBefore), 1);
    EXPECT_EQ(int(Button::IconOnly), 2);
    EXPECT_EQ(int(Button::IconAfter), 3);
    EXPECT_EQ(int(Button::IconAbove), 4);
    EXPECT_EQ(int(Button::IconBelow), 5);
    Button button(QStringLiteral("Undo"));
    const QSize textOnly = button.sizeHint();
    QSignalSpy changes(&button, &Button::fluentLayoutChanged);
    button.setFluentLayout(Button::IconAbove);
    button.setFluentLayout(Button::IconAbove);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(button.sizeHint(), textOnly);
    button.setIconGlyph(Typography::Icons::Undo, 24);
    const QSize withLabel = button.sizeHint();
    button.setText(QString());
    const QSize onlyIcon = button.sizeHint();
    EXPECT_LT(onlyIcon.height(), withLabel.height());
    EXPECT_GE(onlyIcon.height(), 24 + 2 * button.themeSpacing().padding.controlV);
    button.setText(QStringLiteral("Undo\nchange"));
    EXPECT_GT(button.sizeHint().height(), withLabel.height());
    button.setIconGlyph(QString());
    const QSize withoutIcon = button.sizeHint();
    button.setFluentLayout(Button::TextOnly);
    EXPECT_EQ(button.sizeHint(), withoutIcon);
}

TEST(ButtonIconAboveTest, Contract_AllGenericButtonFamiliesStackGlyphsAndIcons)
{
    for (const auto& text : {QStringLiteral("Undo"), QStringLiteral("撤销"), QStringLiteral("Undo\nchange")}) {
        SCOPED_TRACE(text.toStdString());
        checkStackedContent<Button>(text);
        checkStackedContent<ToggleButton>(text);
        checkStackedContent<RepeatButton>(text);
        checkStackedContent<DropDownButton>(text);
        checkStackedContent<SplitButton>(text);
        checkStackedContent<ToggleSplitButton>(text);
        checkStackedContent<CompoundButton>(text);
        checkStackedContent<HyperlinkButton>(text);
    }
}

TEST(ButtonIconAboveTest, Contract_ExistingLayoutsSurviveRoundTrip)
{
    Button button(QStringLiteral("Save\ncopy"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setIconGlyph(Typography::Icons::Save, 20);
    for (auto layout : {Button::TextOnly, Button::IconBefore, Button::IconOnly, Button::IconAfter}) {
        button.setFluentLayout(layout);
        const QSize expectedSize = button.sizeHint();
        button.resize(expectedSize);
        const QImage expected = renderButton(button, 1.5);
        button.setFluentLayout(Button::IconAbove);
        button.resize(button.sizeHint());
        button.setFluentLayout(layout);
        EXPECT_EQ(button.sizeHint(), expectedSize);
        button.resize(expectedSize);
        EXPECT_EQ(renderButton(button, 1.5), expected);
    }
}

TEST(ButtonIconBelowTest, Contract_AllGenericButtonFamiliesStackGlyphsAndIcons)
{
    for (const auto& text : {QStringLiteral("Undo"), QStringLiteral("撤销"), QStringLiteral("Undo\nchange")}) {
        SCOPED_TRACE(text.toStdString());
        checkStackedContent<Button>(text, Button::IconBelow);
        checkStackedContent<ToggleButton>(text, Button::IconBelow);
        checkStackedContent<RepeatButton>(text, Button::IconBelow);
        checkStackedContent<DropDownButton>(text, Button::IconBelow);
        checkStackedContent<SplitButton>(text, Button::IconBelow);
        checkStackedContent<ToggleSplitButton>(text, Button::IconBelow);
        checkStackedContent<CompoundButton>(text, Button::IconBelow);
        checkStackedContent<HyperlinkButton>(text, Button::IconBelow);
    }
}

TEST(ButtonIconBelowTest, Contract_ExistingLayoutsSurviveRoundTrip)
{
    Button button(QStringLiteral("Save\ncopy"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setIconGlyph(Typography::Icons::Save, 20);
    for (auto layout : {Button::TextOnly, Button::IconBefore, Button::IconOnly, Button::IconAfter, Button::IconAbove}) {
        button.setFluentLayout(layout);
        const QSize expectedSize = button.sizeHint();
        button.resize(expectedSize);
        const QImage expected = renderButton(button, 1.5);
        button.setFluentLayout(Button::IconBelow);
        button.resize(button.sizeHint());
        button.setFluentLayout(layout);
        EXPECT_EQ(button.sizeHint(), expectedSize);
        button.resize(expectedSize);
        EXPECT_EQ(renderButton(button, 1.5), expected);
    }
}

TEST(ButtonIconBelowTest, Contract_EnumValuesSignalsAndEmptyContent)
{
    EXPECT_EQ(int(Button::TextOnly), 0);
    EXPECT_EQ(int(Button::IconBefore), 1);
    EXPECT_EQ(int(Button::IconOnly), 2);
    EXPECT_EQ(int(Button::IconAfter), 3);
    EXPECT_EQ(int(Button::IconAbove), 4);
    EXPECT_EQ(int(Button::IconBelow), 5);
    Button button(QStringLiteral("Undo"));
    const QSize textOnly = button.sizeHint();
    QSignalSpy changes(&button, &Button::fluentLayoutChanged);
    button.setFluentLayout(Button::IconBelow);
    button.setFluentLayout(Button::IconBelow);
    EXPECT_EQ(changes.count(), 1);
    EXPECT_EQ(button.sizeHint(), textOnly);
    button.setIconGlyph(Typography::Icons::Undo, 24);
    const QSize withLabel = button.sizeHint();
    button.setText(QString());
    const QSize onlyIcon = button.sizeHint();
    EXPECT_LT(onlyIcon.height(), withLabel.height());
    EXPECT_GE(onlyIcon.height(), 24 + 2 * button.themeSpacing().padding.controlV);
    button.setText(QStringLiteral("Undo\nchange"));
    EXPECT_GT(button.sizeHint().height(), withLabel.height());
    button.setIconGlyph(QString());
    const QSize withoutIcon = button.sizeHint();
    button.setFluentLayout(Button::TextOnly);
    EXPECT_EQ(button.sizeHint(), withoutIcon);
}

TEST(ButtonIconBelowTest, Contract_CompoundEmptyRowsDoNotReserveUnusedSpace)
{
    CompoundButton compound(QStringLiteral("Undo"), QStringLiteral("Restore change"));
    compound.setFluentSize(Button::StandardSize);
    compound.setFluentLayout(Button::IconBelow);
    compound.setIconGlyph(Typography::Icons::Undo, 24);
    const QSize complete = compound.sizeHint();
    compound.setText(QString());
    EXPECT_LT(compound.sizeHint().height(), complete.height());
    const QSize captionAndIcon = compound.sizeHint();
    compound.setIconGlyph(QString());
    EXPECT_LT(compound.sizeHint().height(), captionAndIcon.height());
    Button textOnly(QStringLiteral("Undo"));
    EXPECT_EQ(compound.sizeHint().height(), textOnly.sizeHint().height());
    compound.setText(QStringLiteral("Undo"));
    compound.setSecondaryText(QString());
    compound.setIconGlyph(Typography::Icons::Undo, 24);
    textOnly.setFluentLayout(Button::IconBelow);
    textOnly.setIconGlyph(Typography::Icons::Undo, 24);
    EXPECT_EQ(compound.sizeHint(), textOnly.sizeHint());
}

TEST(ButtonIconConfigurationTest, AllDerivedButtonsPaintEveryLayoutAndTransformForGlyphsAndQIcons)
{
    checkInheritedIconConfiguration<Button>();
    checkInheritedIconConfiguration<CompoundButton>();
    checkInheritedIconConfiguration<DropDownButton>();
    checkInheritedIconConfiguration<HyperlinkButton>();
    checkInheritedIconConfiguration<RepeatButton>();
    checkInheritedIconConfiguration<SplitButton>();
    checkInheritedIconConfiguration<ToggleButton>();
    checkInheritedIconConfiguration<ToggleSplitButton>();
}

TEST(ButtonIconConfigurationTest, CompoundIconOnlyHidesBothTextRowsAndTheirSpace)
{
    StackedButtonProbe<CompoundButton> button(QStringLiteral("Title"));
    button.setSecondaryText(QStringLiteral("Description"));
    button.setIconGlyph(Typography::Icons::Save);
    button.setFluentLayout(Button::IconOnly);
    const QSize size = button.sizeHint();
    button.resize(size);
    const QImage iconOnly = renderButton(button, 2);
    button.setText(QString());
    button.setSecondaryText(QString());
    EXPECT_EQ(button.sizeHint(), size);
    EXPECT_EQ(renderButton(button, 2), iconOnly);
}
