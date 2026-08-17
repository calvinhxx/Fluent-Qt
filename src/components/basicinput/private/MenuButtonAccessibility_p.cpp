#include "MenuButtonAccessibility_p.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QKeySequence>
#include <QMenu>
#include <QPushButton>

#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/SplitButton.h"

namespace fluent::basicinput::detail {

namespace {

QString stripMnemonic(const QString& source)
{
    QString result;
    result.reserve(source.size());
    for (int index = 0; index < source.size(); ++index) {
        if (source.at(index) != QLatin1Char('&')) {
            result.append(source.at(index));
            continue;
        }
        if (index + 1 < source.size()
            && source.at(index + 1) == QLatin1Char('&')) {
            result.append(QLatin1Char('&'));
            ++index;
        }
    }
    return result;
}

template<typename ButtonType>
void showAttachedMenu(ButtonType* button)
{
    if (!button || !button->isEnabled())
        return;
    QMenu* menu = button->menu();
    if (!menu || menu->isVisible())
        return;

    if (button->focusPolicy() != Qt::NoFocus)
        button->setFocus(Qt::PopupFocusReason);
    QPoint popupPosition = button->mapToGlobal(button->rect().bottomLeft());
    if (button->layoutDirection() == Qt::RightToLeft)
        popupPosition.rx() -= menu->sizeHint().width() - button->width();
    menu->popup(popupPosition);
}

#if QT_CONFIG(accessibility)

class MenuButtonAccessibleBase : public QAccessibleWidget {
public:
    explicit MenuButtonAccessibleBase(QAbstractButton* button)
        : QAccessibleWidget(button, QAccessible::ButtonMenu)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        QAbstractButton* current = button();
        if (!current || !inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return stripMnemonic(current->text());
        if (type == QAccessible::Accelerator)
            return current->shortcut().toString(QKeySequence::NativeText);
        return inherited;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        QAbstractButton* current = button();
        if (!current)
            return result;

        const bool enabled = current->isEnabled();
        const bool hasMenu = menu() != nullptr;
        result.focusable = enabled
            && current->focusPolicy() != Qt::NoFocus;
        result.focused = current->hasFocus();
        result.pressed = current->isDown();
        if (auto* pushButton = qobject_cast<QPushButton*>(current))
            result.defaultButton = pushButton->isDefault();
        result.checkable = current->isCheckable();
        result.checked = current->isCheckable() && current->isChecked();
        result.hasPopup = hasMenu;
        result.expandable = hasMenu;
        result.expanded = hasMenu && isOpen();
        result.collapsed = hasMenu && !isOpen();
        return result;
    }

protected:
    virtual QAbstractButton* button() const = 0;
    virtual QMenu* menu() const = 0;
    virtual bool isOpen() const = 0;
};

class DropDownButtonAccessible final : public MenuButtonAccessibleBase {
public:
    explicit DropDownButtonAccessible(DropDownButton* button)
        : MenuButtonAccessibleBase(button)
    {
    }

    QStringList actionNames() const override
    {
        DropDownButton* current = view();
        if (!current || !current->isEnabled())
            return {};
        if (current->menu())
            return {QAccessibleActionInterface::showMenuAction()};
        return {current->isCheckable()
                    ? QAccessibleActionInterface::toggleAction()
                    : QAccessibleActionInterface::pressAction()};
    }

    void doAction(const QString& actionName) override
    {
        DropDownButton* current = view();
        if (!current || !current->isEnabled())
            return;
        if (actionName == QAccessibleActionInterface::showMenuAction()
            && current->menu()) {
            showMenuButtonMenu(current);
            return;
        }
        const QString primaryAction = current->isCheckable()
            ? QAccessibleActionInterface::toggleAction()
            : QAccessibleActionInterface::pressAction();
        if (!current->menu() && actionName == primaryAction)
            current->click();
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        DropDownButton* current = view();
        if (!current)
            return {};
        if (actionName == QAccessibleActionInterface::showMenuAction()
            && current->menu()) {
            return {QStringLiteral("Space"), QStringLiteral("Enter"),
                    QStringLiteral("Alt+Down"), QStringLiteral("F4")};
        }
        const QString primaryAction = current->isCheckable()
            ? QAccessibleActionInterface::toggleAction()
            : QAccessibleActionInterface::pressAction();
        if (!current->menu() && actionName == primaryAction)
            return {QStringLiteral("Space")};
        return {};
    }

protected:
    QAbstractButton* button() const override { return view(); }
    QMenu* menu() const override
    {
        return view() ? view()->menu() : nullptr;
    }
    bool isOpen() const override
    {
        return view() && view()->isOpen();
    }

private:
    DropDownButton* view() const
    {
        return static_cast<DropDownButton*>(widget());
    }
};

class SplitButtonAccessible final : public MenuButtonAccessibleBase {
public:
    explicit SplitButtonAccessible(SplitButton* button)
        : MenuButtonAccessibleBase(button)
    {
    }

    QStringList actionNames() const override
    {
        SplitButton* current = view();
        if (!current || !current->isEnabled())
            return {};

        QStringList result{
            current->isCheckable()
                ? QAccessibleActionInterface::toggleAction()
                : QAccessibleActionInterface::pressAction()};
        if (current->menu())
            result.append(QAccessibleActionInterface::showMenuAction());
        return result;
    }

    void doAction(const QString& actionName) override
    {
        SplitButton* current = view();
        if (!current || !current->isEnabled())
            return;

        const QString primaryAction = current->isCheckable()
            ? QAccessibleActionInterface::toggleAction()
            : QAccessibleActionInterface::pressAction();
        if (actionName == primaryAction) {
            current->click();
            return;
        }
        if (actionName == QAccessibleActionInterface::showMenuAction()
            && current->menu()) {
            showMenuButtonMenu(current);
        }
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        SplitButton* current = view();
        if (!current)
            return {};
        const QString primaryAction = current->isCheckable()
            ? QAccessibleActionInterface::toggleAction()
            : QAccessibleActionInterface::pressAction();
        if (actionName == primaryAction)
            return {QStringLiteral("Space")};
        if (actionName == QAccessibleActionInterface::showMenuAction()
            && current->menu()) {
            return {QStringLiteral("Alt+Down"), QStringLiteral("F4")};
        }
        return {};
    }

protected:
    QAbstractButton* button() const override { return view(); }
    QMenu* menu() const override
    {
        return view() ? view()->menu() : nullptr;
    }
    bool isOpen() const override
    {
        return view() && view()->isOpen();
    }

private:
    SplitButton* view() const
    {
        return static_cast<SplitButton*>(widget());
    }
};

QAccessibleInterface* menuButtonAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* dropDown = qobject_cast<DropDownButton*>(object))
        return new DropDownButtonAccessible(dropDown);
    if (auto* split = qobject_cast<SplitButton*>(object))
        return new SplitButtonAccessible(split);
    return nullptr;
}

template<typename ButtonType>
void notifyMenuAvailability(ButtonType* button, bool availabilityChanged)
{
    if (!button)
        return;
    if (availabilityChanged) {
        QAccessible::State changed;
        changed.hasPopup = true;
        changed.expandable = true;
        changed.expanded = true;
        changed.collapsed = true;
        QAccessibleStateChangeEvent stateEvent(button, changed);
        QAccessible::updateAccessibility(&stateEvent);
    }
    QAccessibleEvent actionEvent(button, QAccessible::ActionChanged);
    QAccessible::updateAccessibility(&actionEvent);
}

template<typename ButtonType>
void notifyOpenState(ButtonType* button)
{
    if (!button)
        return;
    QAccessible::State changed;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent event(button, changed);
    QAccessible::updateAccessibility(&event);
}

#endif // QT_CONFIG(accessibility)

} // namespace

namespace {

void ensureMenuButtonAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(menuButtonAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace

const QString& prepareMenuButtonAccessibility(const QString& text)
{
    // Run before Button's base constructor; Qt 5 can otherwise cache the
    // native QPushButton interface before this custom factory is installed.
    ensureMenuButtonAccessibilityFactory();
    return text;
}

QWidget* prepareMenuButtonAccessibility(QWidget* parent)
{
    // Keep the parent-only constructor on the same pre-base path.
    ensureMenuButtonAccessibilityFactory();
    return parent;
}

void showMenuButtonMenu(DropDownButton* button)
{
    showAttachedMenu(button);
}

void showMenuButtonMenu(SplitButton* button)
{
    showAttachedMenu(button);
}

void notifyMenuButtonMenuAccessibility(
    DropDownButton* button, bool availabilityChanged)
{
#if QT_CONFIG(accessibility)
    notifyMenuAvailability(button, availabilityChanged);
#else
    Q_UNUSED(button)
    Q_UNUSED(availabilityChanged)
#endif
}

void notifyMenuButtonMenuAccessibility(
    SplitButton* button, bool availabilityChanged)
{
#if QT_CONFIG(accessibility)
    notifyMenuAvailability(button, availabilityChanged);
#else
    Q_UNUSED(button)
    Q_UNUSED(availabilityChanged)
#endif
}

void notifyMenuButtonOpenAccessibility(DropDownButton* button)
{
#if QT_CONFIG(accessibility)
    notifyOpenState(button);
#else
    Q_UNUSED(button)
#endif
}

void notifyMenuButtonOpenAccessibility(SplitButton* button)
{
#if QT_CONFIG(accessibility)
    notifyOpenState(button);
#else
    Q_UNUSED(button)
#endif
}

} // namespace fluent::basicinput::detail
