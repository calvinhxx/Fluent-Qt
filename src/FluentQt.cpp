#include <FluentQt/FluentQt.h>

#include <QFont>
#include <QFontDatabase>
#include <QResource>
#include <QString>
#include <QStringList>

#include "compatibility/FontCompat.h"

static void initializeFluentQtResourceFile()
{
    Q_INIT_RESOURCE(resources);
}

namespace {

void configureLinuxEmojiFontFallback()
{
#ifdef Q_OS_LINUX
    const QStringList installedFamilies = fluentFontDatabaseFamilies();
    const QStringList preferredFamilies = {
        QStringLiteral("Noto Color Emoji"),
        QStringLiteral("Noto Emoji"),
        QStringLiteral("Twemoji Mozilla"),
        QStringLiteral("Twitter Color Emoji"),
        QStringLiteral("EmojiOne Color"),
        QStringLiteral("Segoe UI Emoji"),
        QStringLiteral("Apple Color Emoji"),
    };

    QString emojiFamily;
    for (const QString& candidate : preferredFamilies) {
        for (const QString& installed : installedFamilies) {
            if (installed.compare(candidate, Qt::CaseInsensitive) == 0) {
                emojiFamily = installed;
                break;
            }
        }
        if (!emojiFamily.isEmpty())
            break;
    }
    if (emojiFamily.isEmpty())
        return;

    const QStringList aliases = {
        QStringLiteral("Segoe UI Emoji"),
        QStringLiteral("Apple Color Emoji"),
        QStringLiteral("Noto Color Emoji"),
    };
    for (const QString& alias : aliases) {
        if (alias.compare(emojiFamily, Qt::CaseInsensitive) != 0)
            QFont::insertSubstitution(alias, emojiFamily);
    }
#endif
}

bool loadBundledFonts()
{
    const int iconFontId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/res/Segoe Fluent Icons.ttf"));
    const int uiFontId =
        QFontDatabase::addApplicationFont(QStringLiteral(":/res/SegoeUI-VF.ttf"));

#ifdef Q_OS_LINUX
    const QStringList uiFamilies = QFontDatabase::applicationFontFamilies(uiFontId);
    if (!uiFamilies.isEmpty()) {
        const QString uiFamily = uiFamilies.constFirst();
        const QStringList aliases = {
            QStringLiteral("Segoe UI"),
            QStringLiteral("Segoe UI Semibold"),
            QStringLiteral("Segoe UI Bold"),
            QStringLiteral("Segoe UI Variable"),
            QStringLiteral("Segoe UI Variable Small"),
            QStringLiteral("Segoe UI Variable Text"),
            QStringLiteral("Segoe UI Variable Display"),
        };
        for (const QString& alias : aliases) {
            if (alias != uiFamily)
                QFont::insertSubstitution(alias, uiFamily);
        }
    }
#endif

    configureLinuxEmojiFontFallback();

    return iconFontId >= 0 && uiFontId >= 0;
}

} // namespace

namespace fluent {

bool initializeResources()
{
    static const bool initialized = [] {
        initializeFluentQtResourceFile();
        return loadBundledFonts();
    }();
    return initialized;
}

} // namespace fluent
