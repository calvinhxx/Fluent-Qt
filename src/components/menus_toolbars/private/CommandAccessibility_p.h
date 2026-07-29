#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACCESSIBILITY_P_H

class QWidget;

namespace fluent::menus_toolbars::detail {

enum class CommandAccessibleRole {
    ToolbarRoot,
    PopupRoot,
    PrimaryRow,
    MenuList,
    PrimaryCommand,
    MenuCommand,
    MoreButton,
};

// Installs the private property-driven accessibility factory once per process.
// None of the adapter types become installed or application-facing API.
void markCommandAccessibleWidget(QWidget* widget,
                                 CommandAccessibleRole role);

// Updates the accessible expanded/collapsed state and emits a state event.
void updateCommandExpandedAccessibility(QWidget* widget,
                                        bool expanded,
                                        bool expandable);

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACCESSIBILITY_P_H
