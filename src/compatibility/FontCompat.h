#ifndef FLUENTFONTCOMPAT_H
#define FLUENTFONTCOMPAT_H

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <QtGlobal>

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
#if defined(Q_OS_LINUX)
    Q_UNUSED(font);
    Q_UNUSED(styleName);
#else
    if (!styleName.isEmpty())
        font.setStyleName(styleName);
#endif
}

#endif // FLUENTFONTCOMPAT_H
