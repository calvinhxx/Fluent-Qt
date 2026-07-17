#include <FluentQt/FluentQt.h>

#include <QFont>
#include <QFontDatabase>
#include <QResource>
#include <QString>
#include <QStringList>

#include "compatibility/FontCompat.h"
#include "utils/private/FluentQtLogging_p.h"

static void initializeFluentQtResourceFile()
{
    Q_INIT_RESOURCE(resources);
}

namespace {

void configureSystemTextFallback()
{
    const QString systemFamily =
        QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    if (systemFamily.isEmpty())
        return;

    const QStringList bundledFamilies = {
        fluent::fontcompat::UITextFamily,
        fluent::fontcompat::UIHeadingFamily,
        fluent::fontcompat::UIDisplayFamily,
    };
    for (const QString& family : bundledFamilies)
        QFont::insertSubstitution(family, systemFamily);
}

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

bool loadBundledFace(const QString& path, const QString& expectedFamily)
{
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0) {
        qCWarning(fluent::logging::typographyCategory)
            << "Could not register bundled font" << path;
        return false;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    for (const QString& family : families) {
        if (family.compare(expectedFamily, Qt::CaseInsensitive) == 0)
            return true;
    }

    qCWarning(fluent::logging::typographyCategory)
        << "Bundled font" << path << "registered unexpected families" << families
        << "instead of" << expectedFamily;
    return false;
}

bool loadBundledFonts()
{
    configureSystemTextFallback();

    const bool textRegularLoaded = loadBundledFace(
        QStringLiteral(":/res/fonts/FluentQtUIText-Regular.ttf"),
        fluent::fontcompat::UITextFamily);
    const bool textSemiboldLoaded = loadBundledFace(
        QStringLiteral(":/res/fonts/FluentQtUIText-Semibold.ttf"),
        fluent::fontcompat::UITextFamily);
    const bool headingSemiboldLoaded = loadBundledFace(
        QStringLiteral(":/res/fonts/FluentQtUIHeading-Semibold.ttf"),
        fluent::fontcompat::UIHeadingFamily);
    const bool displaySemiboldLoaded = loadBundledFace(
        QStringLiteral(":/res/fonts/FluentQtUIDisplay-Semibold.ttf"),
        fluent::fontcompat::UIDisplayFamily);
    const bool iconsLoaded = loadBundledFace(
        QStringLiteral(":/res/icons/FluentQtIcons.ttf"),
        fluent::fontcompat::IconFamily);

    configureLinuxEmojiFontFallback();

    return textRegularLoaded && textSemiboldLoaded && headingSemiboldLoaded
        && displaySemiboldLoaded && iconsLoaded;
}

} // namespace

namespace fluent {

bool initializeResources()
{
    static const bool initialized = [] {
        initializeFluentQtResourceFile();
        const bool loaded = loadBundledFonts();
        if (!loaded) {
            qCWarning(logging::typographyCategory)
                << "FluentQt could not register every bundled typography face;"
                   " platform text fallback will be used where possible.";
        }
        return loaded;
    }();
    return initialized;
}

} // namespace fluent
