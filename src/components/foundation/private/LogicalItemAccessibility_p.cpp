#include "LogicalItemAccessibility_p.h"

#include <QPointer>
#include <QWidget>

namespace fluent::accessibility::detail {

#if QT_CONFIG(accessibility)

namespace {

class LogicalItemAccessibleChild final : public QAccessibleInterface,
                                         public QAccessibleActionInterface {
public:
    LogicalItemAccessibleChild(QWidget* owner, int logicalIndex)
        : m_owner(owner)
        , m_logicalIndex(logicalIndex)
    {
    }

    bool isValid() const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root && m_logicalIndex >= 0
            && m_logicalIndex < root->logicalChildCount();
    }
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override
    {
        return m_owner
            ? QAccessible::queryAccessibleInterface(m_owner)
            : nullptr;
    }
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text type) const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root ? root->logicalChildText(m_logicalIndex, type)
                    : QString();
    }
    void setText(QAccessible::Text, const QString&) override {}
    QRect rect() const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root ? root->logicalChildRect(m_logicalIndex) : QRect();
    }
    QAccessible::Role role() const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root ? root->logicalChildRole(m_logicalIndex)
                    : QAccessible::NoRole;
    }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        if (!root || !isValid()) {
            result.invalid = true;
            result.invisible = true;
            return result;
        }

        const LogicalItemAccessibleState item =
            root->logicalChildState(m_logicalIndex);
        result.invalid = !item.valid;
        result.disabled = !item.enabled;
        result.focusable = item.focusable && item.enabled;
        result.selectable = item.selectable && item.enabled;
        result.selected = item.selected;
        result.focused = item.focused;
        result.readOnly = item.readOnly;
        result.hasPopup = item.hasPopup;
        result.linked = item.linked;
        result.invisible = item.invisible
            || !root->ownerWidget()
            || !root->ownerWidget()->isVisible();
        result.offscreen = item.offscreen
            || (!result.invisible && rect().isEmpty());
        return result;
    }
    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ActionInterface)
            return static_cast<QAccessibleActionInterface*>(this);
        return nullptr;
    }

    QStringList actionNames() const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root ? root->logicalChildActions(m_logicalIndex)
                    : QStringList();
    }
    void doAction(const QString& actionName) override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        if (root)
            root->performLogicalChildAction(m_logicalIndex, actionName);
    }
    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        LogicalItemAccessibleAdapter* root = accessibleRoot();
        return root
            ? root->logicalChildKeyBindings(m_logicalIndex, actionName)
            : QStringList();
    }

    QWidget* owner() const { return m_owner; }
    int logicalIndex() const { return m_logicalIndex; }

private:
    LogicalItemAccessibleAdapter* accessibleRoot() const
    {
        return m_owner
            ? dynamic_cast<LogicalItemAccessibleAdapter*>(
                  QAccessible::queryAccessibleInterface(m_owner))
            : nullptr;
    }

    QPointer<QWidget> m_owner;
    int m_logicalIndex = -1;
};

LogicalItemAccessibleAdapter* adapterFor(QWidget* widget)
{
    return widget
        ? dynamic_cast<LogicalItemAccessibleAdapter*>(
              QAccessible::queryAccessibleInterface(widget))
        : nullptr;
}

QAccessibleInterface* eventTarget(QWidget* widget, int logicalIndex)
{
    LogicalItemAccessibleAdapter* adapter = adapterFor(widget);
    if (!adapter)
        return QAccessible::queryAccessibleInterface(widget);
    QAccessibleInterface* item = adapter->child(logicalIndex);
    return item ? item : adapter;
}

} // namespace

LogicalItemAccessibleAdapter::LogicalItemAccessibleAdapter(
    QWidget* widget, QAccessible::Role role)
    : QAccessibleWidget(widget, role)
{
}

LogicalItemAccessibleAdapter::~LogicalItemAccessibleAdapter()
{
    resetChildCache();
}

QAccessibleInterface* LogicalItemAccessibleAdapter::childAt(
    int x, int y) const
{
    for (int index = childCount() - 1; index >= 0; --index) {
        QAccessibleInterface* item = child(index);
        if (item && !item->state().invisible
            && item->rect().contains(x, y)) {
            return item;
        }
    }
    return nullptr;
}

QAccessibleInterface* LogicalItemAccessibleAdapter::focusChild() const
{
    return child(logicalFocusChild());
}

int LogicalItemAccessibleAdapter::childCount() const
{
    return qMax(0, logicalChildCount());
}

int LogicalItemAccessibleAdapter::indexOfChild(
    const QAccessibleInterface* childInterface) const
{
    const auto* item =
        dynamic_cast<const LogicalItemAccessibleChild*>(childInterface);
    return item && item->owner() == widget()
        ? item->logicalIndex()
        : -1;
}

QAccessibleInterface* LogicalItemAccessibleAdapter::child(
    int logicalIndex) const
{
    if (!widget() || logicalIndex < 0 || logicalIndex >= childCount())
        return nullptr;

    auto cached = m_childToId.constFind(logicalIndex);
    if (cached != m_childToId.constEnd()) {
        if (QAccessibleInterface* item =
                QAccessible::accessibleInterface(cached.value())) {
            return item;
        }
        m_childToId.remove(logicalIndex);
    }

    auto* item = new LogicalItemAccessibleChild(widget(), logicalIndex);
    const QAccessible::Id id =
        QAccessible::registerAccessibleInterface(item);
    m_childToId.insert(logicalIndex, id);
    return item;
}

void* LogicalItemAccessibleAdapter::interface_cast(
    QAccessible::InterfaceType type)
{
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    if (type == QAccessible::SelectionInterface
        && logicalSelectionSupported()) {
        return static_cast<QAccessibleSelectionInterface*>(this);
    }
#endif
    return QAccessibleWidget::interface_cast(type);
}

#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
int LogicalItemAccessibleAdapter::selectedItemCount() const
{
    int count = 0;
    for (int index = 0; index < childCount(); ++index) {
        if (logicalChildState(index).selected)
            ++count;
    }
    return count;
}

QList<QAccessibleInterface*>
LogicalItemAccessibleAdapter::selectedItems() const
{
    QList<QAccessibleInterface*> result;
    for (int index = 0; index < childCount(); ++index) {
        if (logicalChildState(index).selected) {
            if (QAccessibleInterface* item = child(index))
                result.append(item);
        }
    }
    return result;
}

bool LogicalItemAccessibleAdapter::isSelected(
    QAccessibleInterface* childItem) const
{
    const int index = indexOfChild(childItem);
    return index >= 0 && logicalChildState(index).selected;
}

bool LogicalItemAccessibleAdapter::select(
    QAccessibleInterface* childItem)
{
    const int index = indexOfChild(childItem);
    return index >= 0 && setLogicalChildSelected(index, true);
}

bool LogicalItemAccessibleAdapter::unselect(
    QAccessibleInterface* childItem)
{
    const int index = indexOfChild(childItem);
    return index >= 0 && setLogicalChildSelected(index, false);
}

bool LogicalItemAccessibleAdapter::clear()
{
    return clearLogicalSelection();
}
#endif

QStringList LogicalItemAccessibleAdapter::logicalChildActions(
    int logicalIndex) const
{
    const LogicalItemAccessibleState item =
        logicalChildState(logicalIndex);
    return item.valid && item.enabled && item.focusable
        ? QStringList{QAccessibleActionInterface::pressAction()}
        : QStringList();
}

QStringList LogicalItemAccessibleAdapter::logicalChildKeyBindings(
    int, const QString&) const
{
    return {};
}

bool LogicalItemAccessibleAdapter::setLogicalChildSelected(
    int logicalIndex, bool selected)
{
    if (!selected)
        return false;
    const LogicalItemAccessibleState before =
        logicalChildState(logicalIndex);
    if (!before.valid || !before.enabled || !before.selectable)
        return false;
    performLogicalChildAction(
        logicalIndex, QAccessibleActionInterface::pressAction());
    return logicalChildState(logicalIndex).selected;
}

QRect LogicalItemAccessibleAdapter::toGlobalRect(
    const QRect& localRect) const
{
    return widget() && !localRect.isEmpty()
        ? QRect(widget()->mapToGlobal(localRect.topLeft()), localRect.size())
        : QRect();
}

void LogicalItemAccessibleAdapter::resetChildCache() const
{
    const QList<QAccessible::Id> ids = m_childToId.values();
    m_childToId.clear();
    for (QAccessible::Id id : ids)
        QAccessible::deleteAccessibleInterface(id);
}

void notifyLogicalItemAccessibilityStructure(QWidget* widget)
{
    if (!widget)
        return;
    // Do not delete cached children synchronously: a logical child's action
    // may itself insert, remove, or reorder items. ObjectReorder tells clients
    // to re-query while index-backed children continue to resolve live data.
    QAccessibleEvent event(widget, QAccessible::ObjectReorder);
    QAccessible::updateAccessibility(&event);
}

void notifyLogicalItemAccessibilitySelection(QWidget* widget,
                                             int logicalIndex)
{
    if (QAccessibleInterface* target = eventTarget(widget, logicalIndex)) {
        QAccessibleEvent event(target, QAccessible::Selection);
        QAccessible::updateAccessibility(&event);
    }
}

void notifyLogicalItemAccessibilityFocus(QWidget* widget,
                                         int logicalIndex)
{
    if (QAccessibleInterface* target = eventTarget(widget, logicalIndex)) {
        QAccessibleEvent event(target, QAccessible::Focus);
        QAccessible::updateAccessibility(&event);
    }
}

void notifyLogicalItemAccessibilityName(QWidget* widget,
                                        int logicalIndex)
{
    if (QAccessibleInterface* target = eventTarget(widget, logicalIndex)) {
        QAccessibleEvent event(target, QAccessible::NameChanged);
        QAccessible::updateAccessibility(&event);
    }
}

void notifyLogicalItemAccessibilityState(QWidget* widget,
                                         int logicalIndex)
{
    if (QAccessibleInterface* target = eventTarget(widget, logicalIndex)) {
        QAccessible::State changed;
        changed.disabled = true;
        changed.focusable = true;
        changed.focused = true;
        changed.selectable = true;
        changed.selected = true;
        changed.invisible = true;
        changed.offscreen = true;
        changed.readOnly = true;
        changed.hasPopup = true;
        QAccessibleStateChangeEvent event(target, changed);
        QAccessible::updateAccessibility(&event);
    }
}

#else

void notifyLogicalItemAccessibilityStructure(QWidget*) {}
void notifyLogicalItemAccessibilitySelection(QWidget*, int) {}
void notifyLogicalItemAccessibilityFocus(QWidget*, int) {}
void notifyLogicalItemAccessibilityName(QWidget*, int) {}
void notifyLogicalItemAccessibilityState(QWidget*, int) {}

#endif // QT_CONFIG(accessibility)

} // namespace fluent::accessibility::detail
