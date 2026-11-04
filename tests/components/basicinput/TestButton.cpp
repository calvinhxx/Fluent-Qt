#include "components/basicinput/Button.h"
#include "components/foundation/ThemeRegistry.h"
#include "design/Typography.h"
#include <QApplication>
#include <QImage>
#include <QMargins>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleOption>
#include <QTest>
#include <QVBoxLayout>
#include <gtest/gtest.h>

using namespace fluent::basicinput;
using namespace fluent;

namespace {

qreal renderedDarkPixelCenterY(int iconOffsetY,
    const QSize& buttonSize = QSize(48, 48),
    int iconPixelSize = Typography::IconSize::Standard,
    const QString& iconGlyph = Typography::Icons::GlobalNav)
{
    Button button;
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFluentStyle(Button::Subtle);
    button.setFluentLayout(Button::IconOnly);
    button.setIconGlyph(iconGlyph, iconPixelSize, Typography::FontFamily::FluentIcons);
    button.setIconOffset(QPoint(0, iconOffsetY));
    button.setFixedSize(buttonSize);
    button.ensurePolished();
    button.show();
    QApplication::processEvents();

    QImage image(button.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    button.render(&painter);
    painter.end();

    qreal weightedY = 0.0;
    qreal totalWeight = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            const int alpha = pixel.alpha();
            const int luminance = (pixel.red() * 299 + pixel.green() * 587 + pixel.blue() * 114) / 1000;
            if (alpha <= 16 || luminance >= 128)
                continue;

            const qreal weight = alpha * (255 - luminance);
            weightedY += y * weight;
            totalWeight += weight;
        }
    }

    return totalWeight > 0.0 ? weightedY / totalWeight : -1.0;
}

class InspectableButton final : public Button {
public:
    using Button::Button;

    int paintedIconPixelSize() const { return iconPixelSize(); }
};

class InsetPushButtonStyle final : public QProxyStyle {
public:
    QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget = nullptr) const override
    {
        if (element == SE_PushButtonLayoutItem && option)
            return option->rect.adjusted(0, 4, 0, -8);
        return QProxyStyle::subElementRect(element, option, widget);
    }
};

TEST(ButtonLayout, Contract_DynamicallyAddedButtonsKeepPaintedBoundsAndSpacing)
{
    InsetPushButtonStyle nativeStyle;
    QWidget host;
    host.setAttribute(Qt::WA_DontShowOnScreen);
    auto* column = new QVBoxLayout(&host);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(12);
    column->setAlignment(Qt::AlignTop);
    host.resize(320, 300);
    host.show();

    QList<Button*> buttons;
    for (int count = 1; count <= 4; ++count) {
        SCOPED_TRACE(count);
        auto* button = count % 2 ? new Button(&host) : new Button(QStringLiteral("Hello from an extension"), &host);
        if (count % 2)
            button->setText(QStringLiteral("Hello from an extension"));
        button->setStyle(&nativeStyle);
        column->addWidget(button);
        buttons.append(button);
        button->show();
        QApplication::processEvents();
        column->activate();

        for (int index = 0; index < buttons.size(); ++index) {
            EXPECT_EQ(column->itemAt(index)->geometry(), buttons[index]->geometry());
            if (index > 0) {
                const auto* previous = buttons[index - 1];
                EXPECT_EQ(buttons[index]->y() - previous->y() - previous->height(), 12);
            }
        }
    }
}

TEST(ButtonProperties, SemanticIconUsesWinUIStandardControlSlotByDefault)
{
    InspectableButton button;
    button.setIconGlyph(Typography::Icons::Add);

    EXPECT_EQ(button.paintedIconPixelSize(), Typography::IconSize::Standard);
}

TEST(ButtonProperties, IconRotationIsAnimatableWithoutChangingGeometry)
{
    Button button;
    button.setFixedSize(32, 32);
    button.setFluentLayout(Button::IconOnly);
    button.setIconGlyph(Typography::Icons::Settings, 16);
    const QSize before = button.sizeHint();

    button.setIconRotation(135.0);

    EXPECT_DOUBLE_EQ(button.iconRotation(), 135.0);
    EXPECT_EQ(button.sizeHint(), before);
}

QImage renderButtonToImage(Button& button)
{
    button.ensurePolished();
    button.show();
    QApplication::processEvents();

    QImage image(button.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    button.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();
    return image;
}

void expectFontMatchesRole(const Button& button, Typography::FontRole role)
{
    const QFont expected = button.themeFont(role).toQFont();
    EXPECT_EQ(button.font().family(), expected.family());
    if (!expected.styleName().isEmpty())
        EXPECT_EQ(button.font().styleName(), expected.styleName());
    EXPECT_EQ(button.font().pixelSize(), expected.pixelSize());
    EXPECT_EQ(button.font().weight(), expected.weight());
}

} // namespace

class ButtonTest : public ::testing::Test {
protected:
    void SetUp() override { ThemeRegistry::instance().resetToDefaults(); }
    void TearDown() override { ThemeRegistry::instance().resetToDefaults(); }
};

TEST_F(ButtonTest, DefaultFontRoleUsesBodyTypography)
{
    Button emptyButton;
    Button textButton(QStringLiteral("Default"));

    EXPECT_EQ(emptyButton.fontRole(), Typography::FontRole::Body);
    EXPECT_EQ(textButton.fontRole(), Typography::FontRole::Body);
    expectFontMatchesRole(emptyButton, Typography::FontRole::Body);
    expectFontMatchesRole(textButton, Typography::FontRole::Body);
}

TEST_F(ButtonTest, SetFontRoleEmitsOnlyWhenRoleChanges)
{
    Button button;
    QSignalSpy spy(&button, &Button::fontRoleChanged);

    button.setFontRole(Typography::FontRole::Caption);

    EXPECT_EQ(button.fontRole(), Typography::FontRole::Caption);
    EXPECT_EQ(spy.count(), 1);
    expectFontMatchesRole(button, Typography::FontRole::Caption);

    button.setFontRole(Typography::FontRole::Caption);

    EXPECT_EQ(spy.count(), 1);
    expectFontMatchesRole(button, Typography::FontRole::Caption);
}

TEST_F(ButtonTest, RoleFontTracksThemeRegistryFamilyAndScale)
{
    Button button(QStringLiteral("Theme typography"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFontRole(Typography::FontRole::BodyStrong);
    button.show();
    ASSERT_TRUE(button.isVisible());

    auto& registry = ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontFamilyOverride = QStringLiteral("Issue 50 Theme Font");
    themed.fontScale = 1.5;

    ASSERT_TRUE(registry.applySnapshot(themed));

    EXPECT_EQ(button.fontRole(), Typography::FontRole::BodyStrong);
    expectFontMatchesRole(button, Typography::FontRole::BodyStrong);
    EXPECT_EQ(button.font().family(), themed.fontFamilyOverride);
    EXPECT_EQ(button.font().pixelSize(), qRound(Typography::FontSize::Body * themed.fontScale));
}

TEST_F(ButtonTest, ExplicitFontSurvivesThemeRegistryRefresh)
{
    Button button(QStringLiteral("Explicit typography"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.show();
    ASSERT_TRUE(button.isVisible());

    QFont explicitFont(QStringLiteral("Issue 50 Explicit Font"));
    explicitFont.setPixelSize(23);
    explicitFont.setWeight(QFont::DemiBold);
    button.setFont(explicitFont);
    const QFont appliedExplicitFont = button.font();

    auto& registry = ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontFamilyOverride = QStringLiteral("Issue 50 Theme Font");
    themed.fontScale = 1.5;

    ASSERT_TRUE(registry.applySnapshot(themed));

    EXPECT_EQ(button.font(), appliedExplicitFont);
    EXPECT_EQ(button.fontRole(), Typography::FontRole::Body);
}

TEST_F(ButtonTest, BaseClassAndPropertyFontOverridesSurviveThemeRefresh)
{
    Button baseOverride(QStringLiteral("Base override"));
    Button propertyOverride(QStringLiteral("Property override"));
    baseOverride.setAttribute(Qt::WA_DontShowOnScreen);
    propertyOverride.setAttribute(Qt::WA_DontShowOnScreen);
    baseOverride.show();
    propertyOverride.show();

    QFont baseFont(QStringLiteral("Issue 50 Base Font"));
    baseFont.setPixelSize(22);
    static_cast<QWidget*>(&baseOverride)->setFont(baseFont);
    const QFont appliedBaseFont = baseOverride.font();

    QFont propertyFont(QStringLiteral("Issue 50 Property Font"));
    propertyFont.setPixelSize(24);
    ASSERT_TRUE(propertyOverride.setProperty("font", propertyFont));
    const QFont appliedPropertyFont = propertyOverride.font();

    auto& registry = ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontScale = 1.5;
    ASSERT_TRUE(registry.applySnapshot(themed));

    EXPECT_EQ(baseOverride.font(), appliedBaseFont);
    EXPECT_EQ(propertyOverride.font(), appliedPropertyFont);
}

TEST_F(ButtonTest, SetSameFontRoleRestoresRoleModeAfterExplicitFont)
{
    Button button(QStringLiteral("Restore role typography"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFontRole(Typography::FontRole::Caption);

    QFont explicitFont(QStringLiteral("Issue 50 Explicit Font"));
    explicitFont.setPixelSize(23);
    button.setFont(explicitFont);
    ASSERT_EQ(button.font().pixelSize(), 23);

    QSignalSpy spy(&button, &Button::fontRoleChanged);
    button.setFontRole(Typography::FontRole::Caption);

    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(button.fontRole(), Typography::FontRole::Caption);
    expectFontMatchesRole(button, Typography::FontRole::Caption);

    button.show();
    ASSERT_TRUE(button.isVisible());
    auto& registry = ThemeRegistry::instance();
    auto themed = registry.snapshot();
    themed.fontScale = 1.5;
    ASSERT_TRUE(registry.applySnapshot(themed));

    expectFontMatchesRole(button, Typography::FontRole::Caption);
}

TEST_F(ButtonTest, MouseClickHandlerCanSynchronouslyDeleteButtonOwner)
{
    auto* owner = new QWidget;
    auto* button = new Button(owner);
    button->resize(120, 36);
    owner->resize(160, 80);
    owner->show();
    QPointer<Button> buttonGuard(button);

    QObject receiver;
    QObject::connect(button, &Button::clicked, &receiver, [owner] { delete owner; });

    QTest::mouseClick(button, Qt::LeftButton);

    EXPECT_TRUE(buttonGuard.isNull());
}

TEST_F(ButtonTest, IconOffsetYMovesIconFontRendering)
{
    const qreal baselineCenterY = renderedDarkPixelCenterY(0);
    const qreal shiftedCenterY = renderedDarkPixelCenterY(6);

    ASSERT_GE(baselineCenterY, 0.0);
    ASSERT_GE(shiftedCenterY, 0.0);
    EXPECT_GT(shiftedCenterY - baselineCenterY, 4.0);
}

TEST_F(ButtonTest, IconFontRenderingIsVisuallyCenteredByDefault)
{
    const QSize titleBarButtonSize(24, 24);
    const qreal expectedCenterY = (titleBarButtonSize.height() - 1) / 2.0;

    const qreal backCenterY = renderedDarkPixelCenterY(0, titleBarButtonSize, Typography::IconSize::Standard,
        Typography::Icons::TitleBarBack);
    const qreal menuCenterY = renderedDarkPixelCenterY(0, titleBarButtonSize, Typography::IconSize::Standard,
        Typography::Icons::GlobalNav);

    ASSERT_GE(backCenterY, 0.0);
    ASSERT_GE(menuCenterY, 0.0);
    EXPECT_NEAR(backCenterY, expectedCenterY, 0.75);
    EXPECT_NEAR(menuCenterY, expectedCenterY, 0.75);
}

TEST_F(ButtonTest, MultilineCaptionKeepsRegularIconAndBothLinesInsidePreferredSize)
{
    QPixmap iconPixmap(16, 16);
    iconPixmap.fill(QColor(235, 34, 197));
    Button button(QStringLiteral("Review connection"));
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFluentStyle(Button::Subtle);
    button.setFluentLayout(Button::IconBefore);
    button.setIcon(QIcon(iconPixmap));
    button.setIconSize(iconPixmap.size());
    button.ensurePolished();
    const QSize firstLineSize = button.sizeHint();
    button.setText(QStringLiteral("settings and resume"));
    const QSize secondLineSize = button.sizeHint();
    button.setText(QStringLiteral("Review connection\nsettings and resume"));
    const QSize preferredSize = button.sizeHint();
    EXPECT_LE(preferredSize.width(), qMax(firstLineSize.width(), secondLineSize.width()) + 1);
    EXPECT_GT(preferredSize.height(), qMax(firstLineSize.height(), secondLineSize.height()));
    button.resize(preferredSize);
    const QImage compact = renderButtonToImage(button);

    QRect iconBounds;
    int iconPixels = 0;
    for (int y = 0; y < compact.height(); ++y) {
        for (int x = 0; x < compact.width(); ++x) {
            const QColor pixel = QColor::fromRgba(compact.pixel(x, y));
            if (pixel.alpha() > 128 && pixel.red() > 180 && pixel.green() < 80 && pixel.blue() > 140) {
                iconBounds = iconBounds.united(QRect(x, y, 1, 1));
                ++iconPixels;
            }
        }
    }
    ASSERT_GE(iconPixels, 15 * 15);
    EXPECT_GT(iconBounds.left(), 0);
    EXPECT_LT(iconBounds.right(), compact.width() - 1);
    EXPECT_GT(iconBounds.top(), 0);
    EXPECT_LT(iconBounds.bottom(), compact.height() - 1);

    const auto visiblePixels = [](const QImage& image) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 16)
                    ++count;
            }
        }
        return count;
    };
    button.resize(preferredSize + QSize(48, 48));
    const QImage spacious = renderButtonToImage(button);
    EXPECT_EQ(visiblePixels(compact), visiblePixels(spacious))
        << "The preferred size must paint the same complete content as a larger surface";
}

TEST_F(ButtonTest, DisabledSubtleButtonKeepsTransparentSurface)
{
    Button button;
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFluentStyle(Button::Subtle);
    button.setFluentLayout(Button::IconOnly);
    button.setFixedSize(24, 24);
    button.setEnabled(false);

    const QImage image = renderButtonToImage(button);
    int visiblePixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (QColor::fromRgba(image.pixel(x, y)).alpha() > 8)
                ++visiblePixels;
        }
    }

    EXPECT_EQ(visiblePixels, 0);
}

TEST_F(ButtonTest, CustomPaintedButtonSuppressesNativeMacFocusRing)
{
#ifdef Q_OS_MAC
    Button button;
    EXPECT_FALSE(button.testAttribute(Qt::WA_MacShowFocusRect));
#else
    GTEST_SKIP() << "The native focus ring is specific to macOS";
#endif
}

TEST_F(ButtonTest, CriticalHoverAndCornerRadiiProperties)
{
    Button button("Danger");
    QSignalSpy criticalSpy(&button, &Button::criticalOnHoverChanged);
    QSignalSpy cornerSpy(&button, &Button::cornerRadiiChanged);

    EXPECT_FALSE(button.criticalOnHover());

    button.setCriticalOnHover(true);
    EXPECT_TRUE(button.criticalOnHover());
    EXPECT_EQ(criticalSpy.count(), 1);

    button.setCriticalOnHover(true);
    EXPECT_EQ(criticalSpy.count(), 1);

    const int defaultRadius = button.themeRadius().control;
    EXPECT_EQ(button.cornerRadii(), QMargins(defaultRadius, defaultRadius, defaultRadius, defaultRadius));

    button.setCornerRadii(QMargins(0, 4, 0, 0));
    EXPECT_EQ(button.cornerRadii(), QMargins(0, 4, 0, 0));
    EXPECT_EQ(cornerSpy.count(), 1);

    button.setCornerRadii(QMargins(0, 4, 0, 0));
    EXPECT_EQ(cornerSpy.count(), 1);

    button.setCornerRadii(QMargins(-1, 2, -3, 4));
    EXPECT_EQ(button.cornerRadii(), QMargins(0, 2, 0, 4));
    EXPECT_EQ(cornerSpy.count(), 2);

    button.resetCornerRadii();
    EXPECT_EQ(button.cornerRadii(), QMargins(defaultRadius, defaultRadius, defaultRadius, defaultRadius));
    EXPECT_EQ(cornerSpy.count(), 3);
}

TEST_F(ButtonTest, Contract_PressedStandardKeepsBottomBorderInsideSurface)
{
    Button button;
    button.setAttribute(Qt::WA_DontShowOnScreen);
    button.setFixedSize(96, 32);
    button.setCornerRadii(QMargins());
    button.setInteractionState(Button::Pressed);

    const QImage image = renderButtonToImage(button);
    ASSERT_FALSE(image.isNull());

    const int centerX = image.width() / 2;
    const QColor interior = image.pixelColor(centerX, image.height() / 2);
    const QColor bottomBorder = image.pixelColor(centerX, image.height() - 1);

    EXPECT_GT(bottomBorder.alpha(), interior.alpha());
}
