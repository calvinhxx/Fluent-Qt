#include "GalleryCommands.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>

#include "compatibility/WindowChromeCompat.h"
#include "components/menus_toolbars/CommandPalette.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

GalleryCommands::GalleryCommands(QWidget* window) : QObject(window), m_window(window)
{
    setObjectName(QStringLiteral("galleryCommands"));
    m_palette = new fluent::menus_toolbars::CommandPalette(window);
    m_palette->setObjectName(QStringLiteral("galleryCommandPalette"));
    m_palette->setPlaceholderText(QStringLiteral("Search commands and shortcuts"));

    if (compatibility::WindowChromeCompat::currentPlatform() == compatibility::WindowChromeCompat::Platform::MacOS) {
        m_menuBar = std::make_unique<QMenuBar>(nullptr);
        m_menuBar->setObjectName(QStringLiteral("galleryMenuBar"));
        for (const Group group : {Group::View, Group::Go, Group::Help}) {
            QMenu* menu = m_menuBar->addMenu(groupTitle(group));
            menu->setObjectName(QStringLiteral("galleryMenu%1").arg(groupTitle(group)));
            m_menus.append({group, menu});
        }
    }
}

GalleryCommands::~GalleryCommands() = default;

void GalleryCommands::addCommand(Group group, QAction* action)
{
    if (!action)
        return;
    m_commands.append({group, action});
    m_window->addAction(action);
    m_palette->addCommand(action, groupTitle(group));
    if (QMenu* menu = menuFor(group))
        menu->addAction(action);
}

QAction* GalleryCommands::addCommand(Group group,
    const QString& objectName,
    const QString& text,
    const QList<QKeySequence>& shortcuts)
{
    auto* action = new QAction(text, m_window);
    action->setObjectName(objectName);
    action->setShortcuts(shortcuts);
    action->setShortcutContext(Qt::WindowShortcut);
    addCommand(group, action);
    return action;
}

QList<QAction*> GalleryCommands::commands(Group group) const
{
    QList<QAction*> actions;
    for (const auto& command : m_commands) {
        if (command.first == group)
            actions.append(command.second);
    }
    return actions;
}

void GalleryCommands::showPalette()
{
    if (m_palette->isOpen())
        return;
    m_palette->setQuery(QString());
    m_palette->open();
    LOG_DEBUG(QStringLiteral("GalleryCommands palette opened commands=%1").arg(m_commands.size()));
}

QString GalleryCommands::groupTitle(Group group)
{
    switch (group) {
    case Group::View:
        return QStringLiteral("View");
    case Group::Go:
        return QStringLiteral("Go");
    case Group::Help:
        return QStringLiteral("Help");
    }
    return QString();
}

QMenu* GalleryCommands::menuFor(Group group) const
{
    for (const auto& menu : m_menus) {
        if (menu.first == group)
            return menu.second;
    }
    return nullptr;
}

} // namespace fluent::gallery
