#include "CommandAccessibility_p.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QVariant>
#include <QWidget>

namespace fluent::menus_toolbars::detail {
namespace {

constexpr const char* kAccessibleRoleProperty =
    "_fluentqt_commandAccessibleRole";
constexpr const char* kExpandedProperty =
    "_fluentqt_commandExpanded";
constexpr const char* kExpandableProperty =
    "_fluentqt_commandExpandable";

#if QT_CONFIG(accessibility)

QAccessible::Role qtRole(CommandAccessibleRole role)
{
    switch (role) {
    case CommandAccessibleRole::ToolbarRoot:
    case CommandAccessibleRole::PrimaryRow:
        return QAccessible::ToolBar;
    case CommandAccessibleRole::PopupRoot:
    case CommandAccessibleRole::MenuList:
        return QAccessible::PopupMenu;
    case CommandAccessibleRole::MenuCommand:
        return QAccessible::MenuItem;
    case CommandAccessibleRole::PrimaryCommand:
    case CommandAccessibleRole::MoreButton:
        return QAccessible::Button;
    }
    return QAccessible::Client;
}

class CommandButtonAccessible final : public QAccessibleWidget {
public:
    CommandButtonAccessible(QWidget* widget,
                            CommandAccessibleRole commandRole)
        : QAccessibleWidget(widget, qtRole(commandRole)),
          m_commandRole(commandRole)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        QWidget* commandWidget = widget();
        if (!commandWidget)
            return {};
        if (type == QAccessible::Name) {
            const QString name =
                commandWidget->property("commandText").toString();
            if (!name.isEmpty())
                return name;
        }
        if (type == QAccessible::Accelerator) {
            const QString shortcut =
                commandWidget->property("commandShortcut").toString();
            if (!shortcut.isEmpty())
                return shortcut;
        }
        return QAccessibleWidget::text(type);
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        QWidget* commandWidget = widget();
        auto* button =
            qobject_cast<QAbstractButton*>(commandWidget);
        if (button) {
            result.focusable = button->isEnabled();
            result.focused = button->hasFocus();
            result.pressed = button->isDown();
            result.checkable = button->isCheckable();
            result.checked =
                button->isCheckable() && button->isChecked();
        }
        if (m_commandRole == CommandAccessibleRole::MoreButton
            && commandWidget) {
            const bool expandable =
                commandWidget->property(
                    kExpandableProperty).toBool();
            const bool expanded =
                commandWidget->property(
                    kExpandedProperty).toBool();
            result.expandable = expandable;
            result.expanded = expandable && expanded;
            result.collapsed = expandable && !expanded;
            result.hasPopup = expandable;
        }
        return result;
    }

    QStringList actionNames() const override
    {
        auto* button = qobject_cast<QAbstractButton*>(widget());
        if (!button || !button->isEnabled())
            return {};
        return {QAccessibleActionInterface::pressAction()};
    }

    void doAction(const QString& actionName) override
    {
        if (actionName
            != QAccessibleActionInterface::pressAction()) {
            return;
        }
        auto* button = qobject_cast<QAbstractButton*>(widget());
        if (button && button->isEnabled())
            button->click();
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName
            != QAccessibleActionInterface::pressAction()) {
            return {};
        }
        const QString shortcut =
            widget()
            ? widget()->property("commandShortcut").toString()
            : QString();
        return shortcut.isEmpty()
            ? QStringList()
            : QStringList{shortcut};
    }

private:
    CommandAccessibleRole m_commandRole;
};

QAccessibleInterface* commandAccessibilityFactory(
    const QString&,
    QObject* object)
{
    auto* widget = qobject_cast<QWidget*>(object);
    if (!widget)
        return nullptr;
    const QVariant value =
        widget->property(kAccessibleRoleProperty);
    if (!value.isValid())
        return nullptr;

    const auto role =
        static_cast<CommandAccessibleRole>(value.toInt());
    switch (role) {
    case CommandAccessibleRole::PrimaryCommand:
    case CommandAccessibleRole::MenuCommand:
    case CommandAccessibleRole::MoreButton:
        return new CommandButtonAccessible(widget, role);
    case CommandAccessibleRole::ToolbarRoot:
    case CommandAccessibleRole::PopupRoot:
    case CommandAccessibleRole::PrimaryRow:
    case CommandAccessibleRole::MenuList:
        return new QAccessibleWidget(widget, qtRole(role));
    }
    return nullptr;
}

void ensureFactoryInstalled()
{
    static const bool installed = []() {
        QAccessible::installFactory(
            commandAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

#else

void ensureFactoryInstalled()
{
}

#endif

} // namespace

void markCommandAccessibleWidget(
    QWidget* widget,
    CommandAccessibleRole role)
{
    if (!widget)
        return;
    ensureFactoryInstalled();
    widget->setProperty(
        kAccessibleRoleProperty,
        static_cast<int>(role));
}

void updateCommandExpandedAccessibility(
    QWidget* widget,
    bool expanded,
    bool expandable)
{
    if (!widget)
        return;
    ensureFactoryInstalled();
    const bool changed =
        widget->property(kExpandedProperty).toBool()
            != expanded
        || widget->property(kExpandableProperty).toBool()
            != expandable;
    widget->setProperty(kExpandedProperty, expanded);
    widget->setProperty(kExpandableProperty, expandable);
    if (!changed)
        return;

#if QT_CONFIG(accessibility)
    QAccessible::State stateChanges;
    stateChanges.expandable = true;
    stateChanges.expanded = true;
    stateChanges.collapsed = true;
    stateChanges.hasPopup = true;
    QAccessibleStateChangeEvent event(widget, stateChanges);
    QAccessible::updateAccessibility(&event);
#endif
}

} // namespace fluent::menus_toolbars::detail
