#include "viewmodel/GalleryUserTheme.h"

#include <QSettings>

#include "components/foundation/UserTheme.h"
#include "platform/GalleryPlatform.h"

namespace fluent::gallery {
namespace {

QString accentStorageKey()
{
    return QStringLiteral("appearance/accent/fluent");
}

} // namespace

namespace GalleryUserTheme {

void apply()
{
    UserTheme::apply();
    if (!platform::capabilities().editsThemeFiles) {
        const QColor accent(
            platform::createSettings().value(accentStorageKey()).toString());
        if (accent.isValid())
            UserTheme::applyAccentOverride(accent);
    }
}

QString filePath()
{
    if (!platform::capabilities().editsThemeFiles)
        return {};
    return UserTheme::filePath();
}

QString directory()
{
    if (!platform::capabilities().editsThemeFiles)
        return {};
    return UserTheme::directory();
}

bool exportTemplate(bool overwrite)
{
    if (!platform::capabilities().editsThemeFiles)
        return false;
    return UserTheme::exportTemplate(overwrite);
}

void setAccent(const QColor& accent)
{
    if (platform::capabilities().editsThemeFiles) {
        UserTheme::setAccent(accent);
        return;
    }
    if (accent.isValid()) {
        QSettings settings = platform::createSettings();
        settings.setValue(accentStorageKey(), accent.name(QColor::HexArgb));
        settings.sync();
    }
}

void clearAccent()
{
    if (platform::capabilities().editsThemeFiles) {
        UserTheme::clearAccent();
        return;
    }
    QSettings settings = platform::createSettings();
    settings.remove(accentStorageKey());
    settings.sync();
}

QColor defaultAccent(bool dark)
{
    return UserTheme::defaultAccent(dark);
}

} // namespace GalleryUserTheme

} // namespace fluent::gallery
