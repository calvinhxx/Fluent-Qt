#ifndef FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_LOGICALITEMACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_LOGICALITEMACCESSIBILITY_P_H

#include <QAccessible>
#include <QAccessibleWidget>
#include <QHash>
#include <QStringList>

#include "compatibility/QtCompat.h"

class QWidget;

namespace fluent::accessibility::detail {

struct LogicalItemAccessibleState {
    bool valid = true;
    bool enabled = true;
    bool focusable = true;
    bool selectable = true;
    bool selected = false;
    bool focused = false;
    bool invisible = false;
    bool offscreen = false;
    bool readOnly = false;
    bool hasPopup = false;
    bool linked = false;
};

#if QT_CONFIG(accessibility)

/**
 * Private reusable root for custom-painted controls with logical items.
 * Component adapters provide only item text, geometry, state, and actions;
 * child caching, selection lookup, and Qt interface plumbing stay centralized.
 */
class LogicalItemAccessibleAdapter : public QAccessibleWidget
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
                                   , public QAccessibleSelectionInterface
#endif
{
public:
    LogicalItemAccessibleAdapter(QWidget* widget, QAccessible::Role role);
    ~LogicalItemAccessibleAdapter() override;

    QAccessibleInterface* childAt(int x, int y) const override;
    QAccessibleInterface* focusChild() const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface* child) const override;
    QAccessibleInterface* child(int logicalIndex) const override;
    void* interface_cast(QAccessible::InterfaceType type) override;

#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    int selectedItemCount() const override;
    QList<QAccessibleInterface*> selectedItems() const override;
    bool isSelected(QAccessibleInterface* childItem) const override;
    bool select(QAccessibleInterface* childItem) override;
    bool unselect(QAccessibleInterface* childItem) override;
    bool selectAll() override { return false; }
    bool clear() override;
#endif

    virtual int logicalChildCount() const = 0;
    virtual QAccessible::Role logicalChildRole(int logicalIndex) const = 0;
    virtual QString logicalChildText(int logicalIndex,
                                     QAccessible::Text type) const = 0;
    virtual QRect logicalChildRect(int logicalIndex) const = 0;
    virtual LogicalItemAccessibleState logicalChildState(
        int logicalIndex) const = 0;
    virtual int logicalFocusChild() const { return -1; }
    virtual QStringList logicalChildActions(int logicalIndex) const;
    virtual QStringList logicalChildKeyBindings(
        int logicalIndex, const QString& actionName) const;
    virtual void performLogicalChildAction(
        int logicalIndex, const QString& actionName) = 0;
    virtual bool setLogicalChildSelected(int logicalIndex, bool selected);
    virtual bool clearLogicalSelection() { return false; }
    virtual bool logicalSelectionSupported() const { return true; }

    QWidget* ownerWidget() const { return widget(); }
    QRect toGlobalRect(const QRect& localRect) const;
    void resetChildCache() const;

private:
    mutable QHash<int, QAccessible::Id> m_childToId;
};

#endif // QT_CONFIG(accessibility)

void notifyLogicalItemAccessibilityStructure(QWidget* widget);
void notifyLogicalItemAccessibilitySelection(QWidget* widget,
                                             int logicalIndex);
void notifyLogicalItemAccessibilityFocus(QWidget* widget,
                                         int logicalIndex);
void notifyLogicalItemAccessibilityName(QWidget* widget,
                                        int logicalIndex);
void notifyLogicalItemAccessibilityState(QWidget* widget,
                                         int logicalIndex);

} // namespace fluent::accessibility::detail

#endif // FLUENTQT_COMPONENTS_FOUNDATION_PRIVATE_LOGICALITEMACCESSIBILITY_P_H
