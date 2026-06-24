#ifndef GALLERYCLOSEBEHAVIORUI_H
#define GALLERYCLOSEBEHAVIORUI_H

#include <QString>
#include <QStringList>

namespace fluent::gallery::closebehaviorui {

inline QString statusAreaName()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("menu bar");
#else
    return QStringLiteral("system tray");
#endif
}

inline QString keepRunningChoice()
{
    return QStringLiteral("Keep in %1").arg(statusAreaName());
}

inline QString minimizeDescription()
{
#ifdef Q_OS_MACOS
    return QStringLiteral("Keep it available from the Dock.");
#else
    return QStringLiteral("Keep it available from the taskbar.");
#endif
}

inline QStringList choices()
{
    return {
        QStringLiteral("Minimize window"),
        keepRunningChoice(),
        QStringLiteral("Quit app")
    };
}

inline QString keepRunningDescription()
{
    return QStringLiteral("Reopen it from the %1 icon.")
        .arg(statusAreaName());
}

} // namespace fluent::gallery::closebehaviorui

#endif // GALLERYCLOSEBEHAVIORUI_H
