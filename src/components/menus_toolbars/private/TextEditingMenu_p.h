#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H

#include <QPoint>
#include <QString>

class QMenu;
class QWidget;

namespace fluent::menus_toolbars::detail {

// Shows a Fluent proxy for a caller-owned standard Qt editing menu.
// The function consumes standardMenu and deletes it with the asynchronous popup.
bool showTextEditingContextMenu(QWidget* parent,
                                QMenu* standardMenu,
                                const QPoint& globalPosition,
                                const QString& objectName);

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_TEXTEDITINGMENU_P_H
