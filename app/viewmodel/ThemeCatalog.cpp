#include "viewmodel/ThemeCatalog.h"

#include <QSettings>

#include "components/foundation/StyleThemeCatalog.h"
#include "platform/GalleryPlatform.h"

namespace fluent::gallery {
namespace {

StyleTheme toCoreTheme(GallerySettings::StyleTheme theme)
{
    switch (theme) {
    case GallerySettings::StyleTheme::Material:
        return StyleTheme::Material;
    case GallerySettings::StyleTheme::MacOS:
        return StyleTheme::MacOS;
    case GallerySettings::StyleTheme::Fluent:
        break;
    }
    return StyleTheme::Fluent;
}

QString accentStorageKey(GallerySettings::StyleTheme theme)
{
    return QStringLiteral("appearance/accent/%1")
        .arg(StyleThemeCatalog::themeKey(toCoreTheme(theme)));
}

} // namespace

namespace ThemeCatalog {

void apply(GallerySettings::StyleTheme theme)
{
    StyleThemeCatalog::apply(toCoreTheme(theme));
    if (!platform::capabilities().editsThemeFiles) {
        const QColor accent(
            platform::createSettings().value(accentStorageKey(theme)).toString());
        if (accent.isValid())
            StyleThemeCatalog::applyAccentOverride(accent);
    }
}

QString userThemeFilePath(GallerySettings::StyleTheme theme)
{
    if (!platform::capabilities().editsThemeFiles)
        return {};
    return StyleThemeCatalog::userThemeFilePath(toCoreTheme(theme));
}

QString themesDirectory()
{
    if (!platform::capabilities().editsThemeFiles)
        return {};
    return StyleThemeCatalog::themesDirectory();
}

bool exportUserThemeTemplate(GallerySettings::StyleTheme theme, bool overwrite)
{
    if (!platform::capabilities().editsThemeFiles)
        return false;
    return StyleThemeCatalog::exportUserThemeTemplate(toCoreTheme(theme), overwrite);
}

void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent)
{
    if (platform::capabilities().editsThemeFiles) {
        StyleThemeCatalog::setUserAccent(toCoreTheme(theme), accent);
        return;
    }
    if (accent.isValid()) {
        QSettings settings = platform::createSettings();
        settings.setValue(accentStorageKey(theme), accent.name(QColor::HexArgb));
        settings.sync();
    }
}

void clearUserAccent(GallerySettings::StyleTheme theme)
{
    if (platform::capabilities().editsThemeFiles) {
        StyleThemeCatalog::clearUserAccent(toCoreTheme(theme));
        return;
    }
    QSettings settings = platform::createSettings();
    settings.remove(accentStorageKey(theme));
    settings.sync();
}

QColor presetAccent(GallerySettings::StyleTheme theme, bool dark)
{
    return StyleThemeCatalog::presetAccent(toCoreTheme(theme), dark);
}

} // namespace ThemeCatalog

} // namespace fluent::gallery
