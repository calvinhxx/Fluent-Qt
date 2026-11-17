#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H

#include <QPoint>
#include <QString>

class QMenu;
class QWidget;

namespace fluent::menus_toolbars::detail {

// Shows a Fluent menu proxying a standard Qt editing menu, taking ownership of standardMenu.
bool showTextEditingContextMenu(QWidget* parent,
    QMenu* standardMenu,
    const QPoint& globalPosition,
    const QString& objectName);

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H
