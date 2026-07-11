#include <FluentQt/FluentQt.h>

#include <QFont>
#include <QFontDatabase>
#include <QDebug>
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

    const auto loadStaticFace = [](const QString& path, const QString& expectedFamily) {
        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0)
            return false;
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        for (const QString& family : families) {
            if (family.compare(expectedFamily, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    };

    const bool smallRegularLoaded = loadStaticFace(
        QStringLiteral(":/res/fonts/FluentQtSegoeUISmall-Regular.ttf"),
        fluent::fontcompat::SegoeSmallFamily);
    const bool textRegularLoaded = loadStaticFace(
        QStringLiteral(":/res/fonts/FluentQtSegoeUIText-Regular.ttf"),
        fluent::fontcompat::SegoeTextFamily);
    const bool textSemiboldLoaded = loadStaticFace(
        QStringLiteral(":/res/fonts/FluentQtSegoeUIText-Semibold.ttf"),
        fluent::fontcompat::SegoeTextFamily);
    const bool displaySemiboldLoaded = loadStaticFace(
        QStringLiteral(":/res/fonts/FluentQtSegoeUIDisplay-Semibold.ttf"),
        fluent::fontcompat::SegoeDisplayFamily);

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

    // If an application-font load ever fails, resolve the FluentQt-specific
    // family through the original variable font or the platform system family.
    // A successfully registered family always wins before substitutions.
    QFont::insertSubstitution(fluent::fontcompat::SegoeSmallFamily,
                              QStringLiteral("Segoe UI Variable Small"));
    QFont::insertSubstitution(fluent::fontcompat::SegoeTextFamily,
                              QStringLiteral("Segoe UI Variable Text"));
    QFont::insertSubstitution(fluent::fontcompat::SegoeDisplayFamily,
                              QStringLiteral("Segoe UI Variable Display"));

    configureLinuxEmojiFontFallback();

    return iconFontId >= 0 && uiFontId >= 0
        && smallRegularLoaded && textRegularLoaded && textSemiboldLoaded
        && displaySemiboldLoaded;
}

} // namespace

namespace fluent {

bool initializeResources()
{
    static const bool initialized = [] {
        initializeFluentQtResourceFile();
        const bool loaded = loadBundledFonts();
        if (!loaded) {
            qWarning("FluentQt could not register every bundled typography face; "
                     "platform font fallback will be used.");
        }
        return loaded;
    }();
    return initialized;
}

} // namespace fluent
