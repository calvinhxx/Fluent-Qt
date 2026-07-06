#include "viewmodel/ThemeCatalog.h"

#include "components/foundation/StyleThemeCatalog.h"

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

} // namespace

namespace ThemeCatalog {

void apply(GallerySettings::StyleTheme theme)
{
    StyleThemeCatalog::apply(toCoreTheme(theme));
}

QString userThemeFilePath(GallerySettings::StyleTheme theme)
{
    return StyleThemeCatalog::userThemeFilePath(toCoreTheme(theme));
}

QString themesDirectory()
{
    return StyleThemeCatalog::themesDirectory();
}

void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent)
{
    StyleThemeCatalog::setUserAccent(toCoreTheme(theme), accent);
}

void clearUserAccent(GallerySettings::StyleTheme theme)
{
    StyleThemeCatalog::clearUserAccent(toCoreTheme(theme));
}

QColor presetAccent(GallerySettings::StyleTheme theme, bool dark)
{
    return StyleThemeCatalog::presetAccent(toCoreTheme(theme), dark);
}

} // namespace ThemeCatalog

} // namespace fluent::gallery
