#pragma once

#include "compatibility/AccessibilityCompat.h"

namespace compatibility::detail {

#if QT_CONFIG(accessibility)

// Components implement the same selection contract on every supported Qt line.
// Older Qt versions cannot expose it through their native accessibility bridge.
// zh_CN: 组件在各 Qt 版本实现相同的选择契约；旧版 Qt 不向原生无障碍桥暴露此接口。
class AccessibleSelectionInterface
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    : public QAccessibleSelectionInterface
#endif
{
public:
    virtual ~AccessibleSelectionInterface() = default;
    virtual int selectedItemCount() const = 0;
    virtual QList<QAccessibleInterface*> selectedItems() const = 0;
    virtual bool isSelected(QAccessibleInterface* childItem) const = 0;
    virtual bool select(QAccessibleInterface* childItem) = 0;
    virtual bool unselect(QAccessibleInterface* childItem) = 0;
    virtual bool selectAll() = 0;
    virtual bool clear() = 0;

    void* castSelectionInterface(QAccessible::InterfaceType type)
    {
#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
        if (type == QAccessible::SelectionInterface)
            return static_cast<QAccessibleSelectionInterface*>(this);
#else
        Q_UNUSED(type);
#endif
        return nullptr;
    }
};

class AccessibleHyperlinkInterface
#if FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE
    : public QAccessibleHyperlinkInterface
#endif
{
public:
    virtual ~AccessibleHyperlinkInterface() = default;
    virtual QString anchor() const = 0;
    virtual QString anchorTarget() const = 0;
    virtual int startIndex() const = 0;
    virtual int endIndex() const = 0;
    virtual bool isValid() const = 0;

    void* castHyperlinkInterface(QAccessible::InterfaceType type)
    {
#if FLUENT_HAS_ACCESSIBLE_HYPERLINK_INTERFACE
        if (type == QAccessible::HyperlinkInterface)
            return static_cast<QAccessibleHyperlinkInterface*>(this);
#else
        Q_UNUSED(type);
#endif
        return nullptr;
    }
};

inline QAccessible::Relation accessibleDescriptionRelation()
{
#if FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION
    return QAccessible::DescriptionFor;
#else
    return {};
#endif
}

inline FluentAccessibleRelationList appendAccessibleRelation(FluentAccessibleRelationList inherited,
    QObject* target,
    QAccessible::Relation relation,
    QAccessible::Relation match)
{
    if (!target || !(relation & match))
        return inherited;
    QAccessibleInterface* targetInterface = QAccessible::queryAccessibleInterface(target);
    if (!targetInterface)
        return inherited;
    for (const auto& existing : inherited) {
        if (existing.first == targetInterface && (existing.second & relation) == relation)
            return inherited;
    }
    inherited.append({targetInterface, relation});
    return inherited;
}

#endif // QT_CONFIG(accessibility)

} // namespace compatibility::detail
