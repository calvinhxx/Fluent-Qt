#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/UserTheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QWidget>

#include <cmath>

#include <gtest/gtest.h>

namespace {

class ThemeProbe : public QWidget, public fluent::FluentElement {
public:
    explicit ThemeProbe(QWidget* parent = nullptr) : QWidget(parent) {}

    void onThemeUpdated() override { ++updateCount; }

    int updateCount = 0;
};

void expectCompleteColorSet(const fluent::FluentElement::Colors& colors)
{
    const QColor* const fields[] = {
        &colors.accentDefault,
        &colors.accentSecondary,
        &colors.accentTertiary,
        &colors.accentDisabled,
        &colors.controlDefault,
        &colors.controlSecondary,
        &colors.controlTertiary,
        &colors.controlDisabled,
        &colors.controlAltSecondary,
        &colors.controlAltTertiary,
        &colors.subtleTransparent,
        &colors.subtleSecondary,
        &colors.subtleTertiary,
        &colors.strokeDefault,
        &colors.strokeSecondary,
        &colors.strokeStrong,
        &colors.strokeCard,
        &colors.strokeDivider,
        &colors.strokeSurface,
        &colors.strokeFocusOuter,
        &colors.strokeFocusInner,
        &colors.textPrimary,
        &colors.textSecondary,
        &colors.textTertiary,
        &colors.textDisabled,
        &colors.textOnAccent,
        &colors.textAccentPrimary,
        &colors.bgCanvas,
        &colors.bgLayer,
        &colors.bgLayerAlt,
        &colors.bgSolid,
        &colors.grey10,
        &colors.grey20,
        &colors.grey30,
        &colors.grey40,
        &colors.grey50,
        &colors.grey60,
        &colors.grey90,
        &colors.grey130,
        &colors.grey160,
        &colors.grey190,
        &colors.systemCritical,
        &colors.systemCriticalBg,
        &colors.systemCaution,
        &colors.systemCautionBg,
        &colors.systemInfo,
        &colors.systemInfoBg,
        &colors.systemSuccess,
        &colors.systemSuccessBg,
        &colors.bgLayerOverlay,
    };
    for (const QColor* field : fields)
        EXPECT_TRUE(field->isValid());

    ASSERT_FALSE(colors.charts.isEmpty());
    for (const QColor& chart : colors.charts)
        EXPECT_TRUE(chart.isValid());
}

qreal relativeLuminance(const QColor& color)
{
    const auto linearize = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearize(color.redF()) + 0.7152 * linearize(color.greenF()) +
           0.0722 * linearize(color.blueF());
}

qreal contrastRatio(const QColor& first, const QColor& second)
{
    const qreal firstLuminance = relativeLuminance(first);
    const qreal secondLuminance = relativeLuminance(second);
    const qreal lighter = qMax(firstLuminance, secondLuminance);
    const qreal darker = qMin(firstLuminance, secondLuminance);
    return (lighter + 0.05) / (darker + 0.05);
}

bool writeThemeEnvelope(const QJsonObject& overrides)
{
    if (!QDir().mkpath(fluent::UserTheme::directory()))
        return false;

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("theme"), QStringLiteral("fluent"));
    root.insert(QStringLiteral("overrides"), overrides);

    QFile file(fluent::UserTheme::filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray payload = QJsonDocument(root).toJson();
    return file.write(payload) == payload.size();
}

class HighContrastThemeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        QFile::remove(fluent::UserTheme::filePath());
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }

    void TearDown() override
    {
        QFile::remove(fluent::UserTheme::filePath());
        fluent::ThemeRegistry::instance().resetToDefaults();
        fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    }
};

TEST_F(HighContrastThemeTest, Contract_ThemeValuesAndSwitchNoOpRemainCompatible)
{
    EXPECT_EQ(static_cast<int>(fluent::FluentElement::Light), 0);
    EXPECT_EQ(static_cast<int>(fluent::FluentElement::Dark), 1);
    EXPECT_EQ(static_cast<int>(fluent::FluentElement::HighContrast), 2);

    ThemeProbe probe;
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::HighContrast);
    EXPECT_EQ(probe.updateCount, 1);

    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    EXPECT_EQ(probe.updateCount, 1);

    fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    EXPECT_EQ(probe.updateCount, 2);
}

TEST_F(HighContrastThemeTest, Contract_DefaultPalettesAreCompleteAndKeepLightDarkValues)
{
    const auto snapshot = fluent::ThemeRegistry::defaultExtendedSnapshot();
    expectCompleteColorSet(snapshot.base.lightColors);
    expectCompleteColorSet(snapshot.base.darkColors);
    expectCompleteColorSet(snapshot.contrastColors);

    EXPECT_EQ(snapshot.base.lightColors.bgCanvas, QColor("#F3F3F3"));
    EXPECT_EQ(snapshot.base.lightColors.bgLayer, QColor("#FFFFFF"));
    EXPECT_EQ(snapshot.base.lightColors.accentDefault, QColor("#005FB8"));
    EXPECT_EQ(snapshot.base.darkColors.bgCanvas, QColor("#202020"));
    EXPECT_EQ(snapshot.base.darkColors.bgLayer, QColor("#2C2C2C"));
    EXPECT_EQ(snapshot.base.darkColors.accentDefault, QColor("#60CDFF"));

    EXPECT_EQ(snapshot.contrastColors.bgCanvas, QColor("#000000"));
    EXPECT_EQ(snapshot.contrastColors.bgLayer, QColor("#000000"));
    EXPECT_EQ(snapshot.contrastColors.textPrimary, QColor("#FFFFFF"));
    EXPECT_EQ(snapshot.contrastColors.strokeDefault, QColor("#FFFFFF"));
    EXPECT_EQ(snapshot.contrastColors.strokeFocusOuter, QColor("#1AEBFF"));
    EXPECT_EQ(snapshot.contrastColors.textOnAccent, QColor("#000000"));
    EXPECT_EQ(snapshot.contrastColors.accentDisabled, QColor("#C0C0C0"));
    EXPECT_EQ(snapshot.contrastColors.textDisabled, QColor("#C0C0C0"));
    EXPECT_NE(snapshot.contrastColors.accentDisabled, snapshot.contrastColors.systemSuccess);
    EXPECT_NE(snapshot.contrastColors.textDisabled, snapshot.contrastColors.systemSuccess);

    const auto& contrast = snapshot.contrastColors;
    EXPECT_GE(contrastRatio(contrast.textPrimary, contrast.bgCanvas), 4.5);
    EXPECT_GE(contrastRatio(contrast.textSecondary, contrast.bgLayer), 4.5);
    EXPECT_GE(contrastRatio(contrast.textDisabled, contrast.controlDisabled), 4.5);
    EXPECT_GE(contrastRatio(contrast.textPrimary, contrast.controlSecondary), 4.5);
    EXPECT_GE(contrastRatio(contrast.textPrimary, contrast.controlTertiary), 4.5);
    EXPECT_GE(contrastRatio(contrast.textOnAccent, contrast.accentDefault), 4.5);
    EXPECT_GE(contrastRatio(contrast.textOnAccent, contrast.accentSecondary), 4.5);
    EXPECT_GE(contrastRatio(contrast.textOnAccent, contrast.accentTertiary), 4.5);
    EXPECT_GE(contrastRatio(contrast.textOnAccent, contrast.accentDisabled), 4.5);
    EXPECT_GE(contrastRatio(contrast.systemCritical, contrast.systemCriticalBg), 4.5);
    EXPECT_GE(contrastRatio(contrast.systemCaution, contrast.systemCautionBg), 4.5);
    EXPECT_GE(contrastRatio(contrast.systemInfo, contrast.systemInfoBg), 4.5);
    EXPECT_GE(contrastRatio(contrast.systemSuccess, contrast.systemSuccessBg), 4.5);
    for (const QColor& chart : contrast.charts)
        EXPECT_GE(contrastRatio(chart, contrast.bgCanvas), 3.0);
}

TEST_F(HighContrastThemeTest, Contract_DeprecatedBooleanPaletteAdaptersRemainCompatible)
{
    auto& registry = fluent::ThemeRegistry::instance();
    auto updatedDark = registry.colors(fluent::FluentElement::Dark);
    updatedDark.bgCanvas = QColor(QStringLiteral("#123456"));

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    registry.setColors(true, updatedDark);
    const auto* legacyLight = &registry.colors(false);
    const auto* legacyDark = &registry.colors(true);
    QT_WARNING_POP

    EXPECT_EQ(legacyLight, &registry.colors(fluent::FluentElement::Light));
    EXPECT_EQ(legacyDark, &registry.colors(fluent::FluentElement::Dark));
    EXPECT_EQ(legacyDark->bgCanvas, QColor(QStringLiteral("#123456")));
    EXPECT_NE(legacyDark, &registry.colors(fluent::FluentElement::HighContrast));
}

TEST_F(HighContrastThemeTest, Contract_ContrastSnapshotCommitsAndNoOpRefreshesOnce)
{
    auto& registry = fluent::ThemeRegistry::instance();
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    ThemeProbe probe;
    probe.show();
    QApplication::processEvents();

    const int initialRevision = registry.revision();
    const int initialGeneration = fluent::FluentElement::themeGeneration();
    const int initialUpdates = probe.updateCount;

    auto next = registry.extendedSnapshot();
    next.contrastColors.accentDefault = QColor(QStringLiteral("#00FFFF"));

    EXPECT_TRUE(registry.applyExtendedSnapshot(next));
    EXPECT_EQ(registry.revision(), initialRevision + 1);
    EXPECT_EQ(fluent::FluentElement::themeGeneration(), initialGeneration + 1);
    EXPECT_EQ(probe.updateCount, initialUpdates + 1);
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).accentDefault,
              QColor(QStringLiteral("#00FFFF")));

    EXPECT_FALSE(registry.applyExtendedSnapshot(next));
    EXPECT_EQ(registry.revision(), initialRevision + 1);
    EXPECT_EQ(fluent::FluentElement::themeGeneration(), initialGeneration + 1);
    EXPECT_EQ(probe.updateCount, initialUpdates + 1);
}

TEST_F(HighContrastThemeTest, Contract_LegacySnapshotKeepsFiveMemberSourceShape)
{
    const auto defaults = fluent::ThemeRegistry::defaultSnapshot();
    const auto defaultContrast = fluent::ThemeRegistry::defaultExtendedSnapshot().contrastColors;
    fluent::ThemeRegistry::Snapshot legacySnapshot{
        defaults.lightColors,
        defaults.darkColors,
        defaults.radius,
        QStringLiteral("Legacy Family"),
        1.25,
    };

    auto [lightColors, darkColors, radius, fontFamily, fontScale] = legacySnapshot;
    EXPECT_EQ(lightColors.bgCanvas, defaults.lightColors.bgCanvas);
    EXPECT_EQ(darkColors.bgCanvas, defaults.darkColors.bgCanvas);
    EXPECT_EQ(radius.control, defaults.radius.control);
    EXPECT_EQ(fontFamily, QStringLiteral("Legacy Family"));
    EXPECT_DOUBLE_EQ(fontScale, 1.25);

    EXPECT_TRUE(fluent::ThemeRegistry::instance().applySnapshot(legacySnapshot));
    const auto applied = fluent::ThemeRegistry::instance().snapshot();
    EXPECT_EQ(applied.lightColors.bgCanvas, defaults.lightColors.bgCanvas);
    EXPECT_EQ(applied.darkColors.bgCanvas, defaults.darkColors.bgCanvas);
    EXPECT_EQ(applied.radius.control, defaults.radius.control);
    EXPECT_EQ(applied.fontFamilyOverride, QStringLiteral("Legacy Family"));
    EXPECT_DOUBLE_EQ(applied.fontScale, 1.25);
    EXPECT_EQ(
        fluent::ThemeRegistry::instance().colors(fluent::FluentElement::HighContrast).bgCanvas,
        defaultContrast.bgCanvas);
}

TEST_F(HighContrastThemeTest, Contract_HighContrastIsAnExplicitDarkBackedAppearance)
{
    EXPECT_FALSE(fluent::FluentElement::themeUsesDarkAppearance(fluent::FluentElement::Light));
    EXPECT_TRUE(fluent::FluentElement::themeUsesDarkAppearance(fluent::FluentElement::Dark));
    EXPECT_TRUE(
        fluent::FluentElement::themeUsesDarkAppearance(fluent::FluentElement::HighContrast));

    ThemeProbe probe;
    fluent::FluentElement::setTheme(fluent::FluentElement::HighContrast);
    EXPECT_TRUE(probe.effectiveThemeUsesDarkAppearance());
    EXPECT_EQ(probe.themeMica().baseColor, Material::Mica::dark().baseColor);
}

TEST_F(HighContrastThemeTest, Contract_LocalOverrideAcceptsEnumAndString)
{
    QWidget host;
    ThemeProbe child(&host);
    ThemeProbe outside;

    host.setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::HighContrast));
    EXPECT_EQ(child.effectiveTheme(), fluent::FluentElement::HighContrast);
    EXPECT_TRUE(child.effectiveThemeUsesDarkAppearance());
    EXPECT_EQ(child.themeColors().bgCanvas, QColor("#000000"));
    EXPECT_EQ(outside.effectiveTheme(), fluent::FluentElement::Light);

    host.setProperty("fluentThemeOverride", QStringLiteral("HighContrast"));
    EXPECT_EQ(child.effectiveTheme(), fluent::FluentElement::HighContrast);
    EXPECT_EQ(child.themeColors().textPrimary, QColor("#FFFFFF"));
    EXPECT_EQ(fluent::FluentElement::currentTheme(), fluent::FluentElement::Light);
}

TEST_F(HighContrastThemeTest, Contract_ExportIncludesOptionalContrastPalette)
{
    ASSERT_TRUE(fluent::UserTheme::exportTemplate());

    QFile file(fluent::UserTheme::filePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject overrides = root.value(QStringLiteral("overrides")).toObject();
    EXPECT_TRUE(overrides.value(QStringLiteral("light")).isObject());
    EXPECT_TRUE(overrides.value(QStringLiteral("dark")).isObject());
    EXPECT_TRUE(overrides.value(QStringLiteral("contrast")).isObject());
}

TEST_F(HighContrastThemeTest, Contract_OldEnvelopeKeepsDefaultContrastPalette)
{
    QJsonObject light;
    light.insert(QStringLiteral("bgCanvas"), QStringLiteral("#123456"));
    QJsonObject dark;
    dark.insert(QStringLiteral("bgCanvas"), QStringLiteral("#654321"));
    QJsonObject overrides;
    overrides.insert(QStringLiteral("light"), light);
    overrides.insert(QStringLiteral("dark"), dark);
    ASSERT_TRUE(writeThemeEnvelope(overrides));

    const auto defaults = fluent::ThemeRegistry::defaultExtendedSnapshot();
    fluent::UserTheme::apply();

    auto& registry = fluent::ThemeRegistry::instance();
    EXPECT_EQ(registry.colors(fluent::FluentElement::Light).bgCanvas,
              QColor(QStringLiteral("#123456")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::Dark).bgCanvas,
              QColor(QStringLiteral("#654321")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).bgCanvas,
              defaults.contrastColors.bgCanvas);
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).textPrimary,
              defaults.contrastColors.textPrimary);
}

TEST_F(HighContrastThemeTest, Contract_LegacyFlatJsonKeepsDefaultContrastPalette)
{
    QJsonObject light;
    light.insert(QStringLiteral("bgCanvas"), QStringLiteral("#123456"));
    QJsonObject dark;
    dark.insert(QStringLiteral("bgCanvas"), QStringLiteral("#654321"));
    QJsonObject legacy;
    legacy.insert(QStringLiteral("light"), light);
    legacy.insert(QStringLiteral("dark"), dark);

    ASSERT_TRUE(QDir().mkpath(fluent::UserTheme::directory()));
    QFile file(fluent::UserTheme::filePath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload = QJsonDocument(legacy).toJson();
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    const auto defaults = fluent::ThemeRegistry::defaultExtendedSnapshot();
    fluent::UserTheme::apply();

    auto& registry = fluent::ThemeRegistry::instance();
    EXPECT_EQ(registry.colors(fluent::FluentElement::Light).bgCanvas,
              QColor(QStringLiteral("#123456")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::Dark).bgCanvas,
              QColor(QStringLiteral("#654321")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).bgCanvas,
              defaults.contrastColors.bgCanvas);
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).textPrimary,
              defaults.contrastColors.textPrimary);
}

TEST_F(HighContrastThemeTest, Contract_OptionalContrastOverridesOnlyContrastPalette)
{
    QJsonObject contrast;
    contrast.insert(QStringLiteral("bgCanvas"), QStringLiteral("#010203"));
    contrast.insert(QStringLiteral("textPrimary"), QStringLiteral("#FEFDFC"));
    contrast.insert(QStringLiteral("accentDefault"), QStringLiteral("#00FFAA"));
    QJsonObject overrides;
    overrides.insert(QStringLiteral("contrast"), contrast);
    ASSERT_TRUE(writeThemeEnvelope(overrides));

    const auto defaults = fluent::ThemeRegistry::defaultExtendedSnapshot();
    fluent::UserTheme::apply();

    auto& registry = fluent::ThemeRegistry::instance();
    const auto& highContrast = registry.colors(fluent::FluentElement::HighContrast);
    EXPECT_EQ(highContrast.bgCanvas, QColor(QStringLiteral("#010203")));
    EXPECT_EQ(highContrast.textPrimary, QColor(QStringLiteral("#FEFDFC")));
    EXPECT_EQ(highContrast.accentDefault, QColor(QStringLiteral("#00FFAA")));
    EXPECT_EQ(highContrast.accentSecondary.rgb(), QColor(QStringLiteral("#00FFAA")).rgb());
    EXPECT_EQ(highContrast.accentSecondary.alpha(), 255);
    EXPECT_EQ(highContrast.accentTertiary.alpha(), 255);
    EXPECT_EQ(registry.colors(fluent::FluentElement::Light).bgCanvas,
              defaults.base.lightColors.bgCanvas);
    EXPECT_EQ(registry.colors(fluent::FluentElement::Dark).bgCanvas,
              defaults.base.darkColors.bgCanvas);
}

TEST_F(HighContrastThemeTest, Contract_GenericAccentOverridePreservesHighContrastAccent)
{
    auto& registry = fluent::ThemeRegistry::instance();
    const QColor defaultContrastAccent =
        fluent::ThemeRegistry::defaultExtendedSnapshot().contrastColors.accentDefault;

    fluent::UserTheme::applyAccentOverride(QColor(QStringLiteral("#AA3377")));

    EXPECT_EQ(registry.colors(fluent::FluentElement::Light).accentDefault,
              QColor(QStringLiteral("#AA3377")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::Dark).accentDefault,
              QColor(QStringLiteral("#AA3377")));
    EXPECT_EQ(registry.colors(fluent::FluentElement::HighContrast).accentDefault,
              defaultContrastAccent);
    EXPECT_FALSE(QFile::exists(fluent::UserTheme::filePath()));
}

} // namespace
