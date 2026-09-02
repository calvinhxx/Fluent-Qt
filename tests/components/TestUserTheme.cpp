#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/UserTheme.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

namespace {

class UserThemeTest : public ::testing::Test {
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

TEST_F(UserThemeTest, Contract_PathAndDefaultsAreFluentOnly)
{
    EXPECT_EQ(fluent::UserTheme::filePath(),
              QDir(fluent::UserTheme::directory()).filePath(QStringLiteral("fluent.json")));
    EXPECT_EQ(fluent::UserTheme::defaultAccent(false), QColor(QStringLiteral("#005FB8")));
    EXPECT_EQ(fluent::UserTheme::defaultAccent(true), QColor(QStringLiteral("#60CDFF")));
}

TEST_F(UserThemeTest, Contract_ApplyDoesNotCreateUserFile)
{
    const QString path = fluent::UserTheme::filePath();

    fluent::UserTheme::apply();

    EXPECT_FALSE(QFile::exists(path));
}

TEST_F(UserThemeTest, Contract_ExportWritesVersionedEnvelopeAndPreservesIt)
{
    const QString path = fluent::UserTheme::filePath();

    ASSERT_TRUE(fluent::UserTheme::exportTemplate());
    EXPECT_FALSE(fluent::UserTheme::exportTemplate());

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    ASSERT_TRUE(document.isObject());
    const QJsonObject root = document.object();
    EXPECT_EQ(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
    EXPECT_EQ(root.value(QStringLiteral("theme")).toString(), QStringLiteral("fluent"));
    const QJsonObject overrides = root.value(QStringLiteral("overrides")).toObject();
    EXPECT_TRUE(overrides.value(QStringLiteral("radius")).isObject());
    EXPECT_TRUE(overrides.value(QStringLiteral("light")).isObject());
    EXPECT_TRUE(overrides.value(QStringLiteral("dark")).isObject());
}

TEST_F(UserThemeTest, Contract_LegacyFlatThemeMigratesOnExplicitEdit)
{
    using fluent::ThemeRegistry;
    const QString path = fluent::UserTheme::filePath();
    ASSERT_TRUE(QDir().mkpath(fluent::UserTheme::directory()));

    QJsonObject radius;
    radius.insert(QStringLiteral("control"), 17);
    QJsonObject light;
    light.insert(QStringLiteral("bgCanvas"), QStringLiteral("#123456"));
    QJsonObject dark;
    dark.insert(QStringLiteral("bgCanvas"), QStringLiteral("#654321"));
    QJsonObject legacy;
    legacy.insert(QStringLiteral("radius"), radius);
    legacy.insert(QStringLiteral("light"), light);
    legacy.insert(QStringLiteral("dark"), dark);

    QFile legacyFile(path);
    ASSERT_TRUE(legacyFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray legacyPayload = QJsonDocument(legacy).toJson();
    ASSERT_EQ(legacyFile.write(legacyPayload), legacyPayload.size());
    legacyFile.close();

    fluent::UserTheme::apply();
    EXPECT_EQ(ThemeRegistry::instance().radius().control, 17);
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Light).bgCanvas.rgb(),
              QColor(QStringLiteral("#123456")).rgb());
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Dark).bgCanvas.rgb(),
              QColor(QStringLiteral("#654321")).rgb());

    const QColor picked(QStringLiteral("#4DA04D"));
    fluent::UserTheme::setAccent(picked);

    QFile migratedFile(path);
    ASSERT_TRUE(migratedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root = QJsonDocument::fromJson(migratedFile.readAll()).object();
    EXPECT_EQ(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
    EXPECT_EQ(root.value(QStringLiteral("theme")).toString(), QStringLiteral("fluent"));
    const QJsonObject overrides = root.value(QStringLiteral("overrides")).toObject();
    EXPECT_EQ(overrides.value(QStringLiteral("radius"))
                  .toObject()
                  .value(QStringLiteral("control"))
                  .toInt(),
              17);
    EXPECT_EQ(overrides.value(QStringLiteral("light"))
                  .toObject()
                  .value(QStringLiteral("bgCanvas"))
                  .toString(),
              QStringLiteral("#123456"));
    EXPECT_EQ(overrides.value(QStringLiteral("dark"))
                  .toObject()
                  .value(QStringLiteral("bgCanvas"))
                  .toString(),
              QStringLiteral("#654321"));
    EXPECT_EQ(overrides.value(QStringLiteral("light"))
                  .toObject()
                  .value(QStringLiteral("accentDefault"))
                  .toString(),
              QStringLiteral("#4DA04D"));
    EXPECT_EQ(overrides.value(QStringLiteral("dark"))
                  .toObject()
                  .value(QStringLiteral("accentDefault"))
                  .toString(),
              QStringLiteral("#4DA04D"));
}

TEST_F(UserThemeTest, Contract_UnsupportedSchemaIsIgnoredAndPreserved)
{
    using fluent::ThemeRegistry;
    ASSERT_TRUE(QDir().mkpath(fluent::UserTheme::directory()));

    QJsonObject light;
    light.insert(QStringLiteral("accentDefault"), QStringLiteral("#FF0000"));
    QJsonObject overrides;
    overrides.insert(QStringLiteral("light"), light);
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 999);
    root.insert(QStringLiteral("theme"), QStringLiteral("fluent"));
    root.insert(QStringLiteral("overrides"), overrides);

    QFile file(fluent::UserTheme::filePath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload = QJsonDocument(root).toJson();
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    fluent::UserTheme::apply();
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Light).accentDefault.rgb(),
              fluent::UserTheme::defaultAccent(false).rgb());

    fluent::UserTheme::setAccent(QColor(QStringLiteral("#4DA04D")));
    QFile preservedFile(fluent::UserTheme::filePath());
    ASSERT_TRUE(preservedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(preservedFile.readAll(), payload);
}

TEST_F(UserThemeTest, Contract_MalformedThemeIsNotOverwrittenByAccentEdit)
{
    ASSERT_TRUE(QDir().mkpath(fluent::UserTheme::directory()));
    const QByteArray malformedPayload("{ this is not valid JSON");

    QFile file(fluent::UserTheme::filePath());
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_EQ(file.write(malformedPayload), malformedPayload.size());
    file.close();

    fluent::UserTheme::setAccent(QColor(QStringLiteral("#4DA04D")));

    QFile preservedFile(fluent::UserTheme::filePath());
    ASSERT_TRUE(preservedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(preservedFile.readAll(), malformedPayload);
}

TEST_F(UserThemeTest, Contract_SetAccentPersistsSparseOverrideAndDerivesVariants)
{
    using fluent::ThemeRegistry;
    const QColor picked(0x4D, 0xA0, 0x4D);

    fluent::UserTheme::setAccent(picked);

    QFile file(fluent::UserTheme::filePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    const QJsonObject overrides = root.value(QStringLiteral("overrides")).toObject();
    for (const QString& modeName : {QStringLiteral("light"), QStringLiteral("dark")}) {
        const QJsonObject mode = overrides.value(modeName).toObject();
        EXPECT_EQ(mode.size(), 1);
        EXPECT_EQ(mode.value(QStringLiteral("accentDefault")).toString(),
                  QStringLiteral("#4DA04D"));
    }

    fluent::UserTheme::apply();
    for (bool dark : {false, true}) {
        const auto colors = ThemeRegistry::instance().colors(dark ? fluent::FluentElement::Dark
                                                                  : fluent::FluentElement::Light);
        EXPECT_EQ(colors.accentDefault.rgb(), picked.rgb()) << "dark=" << dark;
        EXPECT_EQ(colors.accentSecondary.rgb(), picked.rgb()) << "dark=" << dark;
        EXPECT_EQ(colors.accentTertiary.rgb(), picked.rgb()) << "dark=" << dark;
        EXPECT_EQ(colors.textAccentPrimary.rgb(), picked.rgb()) << "dark=" << dark;
    }

    fluent::UserTheme::clearAccent();
    fluent::UserTheme::apply();
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Light).accentDefault.rgb(),
              fluent::UserTheme::defaultAccent(false).rgb());
}

TEST_F(UserThemeTest, Contract_InMemoryAccentDoesNotWriteAndInvalidColorIsNoOp)
{
    auto& registry = fluent::ThemeRegistry::instance();
    const quint64 initialRevision = registry.revision();

    fluent::UserTheme::applyAccentOverride(QColor());
    EXPECT_EQ(registry.revision(), initialRevision);

    const QColor picked(QStringLiteral("#AA3377"));
    fluent::UserTheme::applyAccentOverride(picked);

    EXPECT_FALSE(QFile::exists(fluent::UserTheme::filePath()));
    EXPECT_EQ(registry.colors(fluent::FluentElement::Light).accentDefault.rgb(), picked.rgb());
    EXPECT_EQ(registry.colors(fluent::FluentElement::Dark).accentDefault.rgb(), picked.rgb());
    EXPECT_GT(registry.revision(), initialRevision);
}

TEST_F(UserThemeTest, Contract_CustomBgLayerOverlayAppliesThroughUserTheme)
{
    using fluent::ThemeRegistry;
    const QString path = fluent::UserTheme::filePath();
    ASSERT_TRUE(QDir().mkpath(fluent::UserTheme::directory()));

    QJsonObject light;
    light.insert(QStringLiteral("bgLayerOverlay"), QStringLiteral("#11223344"));
    QJsonObject dark;
    dark.insert(QStringLiteral("bgLayerOverlay"), QStringLiteral("#55667788"));
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("theme"), QStringLiteral("fluent"));
    QJsonObject overrides;
    overrides.insert(QStringLiteral("light"), light);
    overrides.insert(QStringLiteral("dark"), dark);
    root.insert(QStringLiteral("overrides"), overrides);

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload = QJsonDocument(root).toJson();
    ASSERT_EQ(file.write(payload), payload.size());
    file.close();

    fluent::UserTheme::apply();
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Light).bgLayerOverlay.rgba(),
              QColor(0x11, 0x22, 0x33, 0x44).rgba());
    EXPECT_EQ(ThemeRegistry::instance().colors(fluent::FluentElement::Dark).bgLayerOverlay.rgba(),
              QColor(0x55, 0x66, 0x77, 0x88).rgba());
}

} // namespace
