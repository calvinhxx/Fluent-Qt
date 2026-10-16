#ifndef FLUENTFONTCOMPAT_H
#define FLUENTFONTCOMPAT_H

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace fluent::fontcompat {

inline const QString UITextFamily = QStringLiteral("FluentQt UI Text");
inline const QString UIHeadingFamily = QStringLiteral("FluentQt UI Heading");
inline const QString UIDisplayFamily = QStringLiteral("FluentQt UI Display");
inline const QString UISimplifiedChineseFamily = QStringLiteral("FluentQt UI Simplified Chinese");
inline const QString IconFamily = QStringLiteral("FluentQt Icons");

} // namespace fluent::fontcompat

inline QStringList fluentFontDatabaseFamilies()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::families();
#else
    const QFontDatabase database;
    return database.families();
#endif
}

inline void fluentApplyFontStyleName(QFont& font, const QString& styleName)
{
    if (!styleName.isEmpty())
        font.setStyleName(styleName);
}

inline void fluentConfigureTextRendering(QFont& font)
{
    // DirectWrite's vertical grid fitting makes this face look noticeably
    // heavier at navigation/body sizes than FreeType and CoreText. Preserve
    // the outlines on Windows, while retaining light vertical hinting on the
    // other platforms where it improves small-text clarity.
#ifdef Q_OS_WIN
    font.setHintingPreference(QFont::PreferNoHinting);
#else
    font.setHintingPreference(QFont::PreferVerticalHinting);
#endif
    // Use the same high-quality grayscale strategy everywhere. This avoids
    // ClearType colour fringes and keeps screenshots/composited surfaces stable.
    font.setStyleStrategy(
        QFont::StyleStrategy(QFont::PreferQuality | QFont::PreferAntialias | QFont::NoSubpixelAntialias));
}

inline QString fluentCodepointGlyph(quint32 codepoint)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const char32_t value = static_cast<char32_t>(codepoint);
#else
    const uint value = codepoint;
#endif
    return QString::fromUcs4(&value, 1);
}

/**
 * @brief Registers an application font as a script-specific fallback when the
 * active Qt version supports that API.
 * zh_CN: 在当前 Qt 版本支持对应 API 时，将应用字体注册为指定 script 的回退字体。
 */
inline void fluentAddApplicationFallbackFontFamily(QChar::Script script, const QString& family)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QFontDatabase::addApplicationFallbackFontFamily(script, family);
#else
    Q_UNUSED(script);
    Q_UNUSED(family);
#endif
}

#endif // FLUENTFONTCOMPAT_H
