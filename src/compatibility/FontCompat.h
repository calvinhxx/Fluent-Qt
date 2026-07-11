#ifndef FLUENTFONTCOMPAT_H
#define FLUENTFONTCOMPAT_H

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace fluent::fontcompat {

inline const QString SegoeSmallFamily = QStringLiteral("FluentQt Segoe UI Small");
inline const QString SegoeTextFamily = QStringLiteral("FluentQt Segoe UI Text");
inline const QString SegoeDisplayFamily = QStringLiteral("FluentQt Segoe UI Display");

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

#endif // FLUENTFONTCOMPAT_H
