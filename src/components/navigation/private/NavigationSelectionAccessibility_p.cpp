#include "NavigationSelectionAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>

#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/Pivot.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/TabView.h"

namespace fluent::navigation::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::LogicalItemAccessibleAdapter;
using accessibility::detail::LogicalItemAccessibleState;

QString navigationText(const char* source)
{
    return QCoreApplication::translate(
        "NavigationSelectionAccessibility", source);
}

QString positionDescription(int index, int count)
{
    return navigationText("Item %1 of %2").arg(index + 1).arg(count);
}

QString closeAction()
{
    return navigationText("Close tab");
}

QString movePreviousAction()
{
    return navigationText("Move tab backward");
}

QString moveNextAction()
{
    return navigationText("Move tab forward");
}

} // namespace

class BreadcrumbAccessible final : public LogicalItemAccessibleAdapter {
public:
    explicit BreadcrumbAccessible(Breadcrumb* breadcrumb)
        : LogicalItemAccessibleAdapter(breadcrumb, QAccessible::List)
    {
    }

    int logicalChildCount() const override
    {
        const Breadcrumb* breadcrumb = view();
        return breadcrumb ? breadcrumb->itemCount() : 0;
    }

    QAccessible::Role logicalChildRole(int) const override
    {
        return QAccessible::Link;
    }

    QString logicalChildText(int index,
                             QAccessible::Text type) const override
    {
        const Breadcrumb* breadcrumb = view();
        if (!breadcrumb || index < 0 || index >= breadcrumb->itemCount())
            return {};
        const BreadcrumbItem item = breadcrumb->itemAt(index);
        if (type == QAccessible::Name) {
            return item.accessibleName.isEmpty()
                ? item.text : item.accessibleName;
        }
        if (type == QAccessible::Description) {
            return index == breadcrumb->itemCount() - 1
                ? navigationText("Current location, %1")
                      .arg(positionDescription(index, breadcrumb->itemCount()))
                : positionDescription(index, breadcrumb->itemCount());
        }
        return {};
    }

    QRect logicalChildRect(int index) const override
    {
        const Breadcrumb* breadcrumb = view();
        return breadcrumb
            ? toGlobalRect(breadcrumb->itemGeometry(index))
            : QRect();
    }

    LogicalItemAccessibleState logicalChildState(
        int index) const override
    {
        LogicalItemAccessibleState result;
        Breadcrumb* breadcrumb = view();
        if (!breadcrumb || index < 0 || index >= breadcrumb->itemCount()) {
            result.valid = false;
            result.invisible = true;
            return result;
        }

        const BreadcrumbItem item = breadcrumb->itemAt(index);
        const bool current = index == breadcrumb->itemCount() - 1;
        result.enabled = breadcrumb->isEnabled() && item.enabled;
        result.focusable = result.enabled && !current;
        result.selectable = current;
        result.selected = current;
        result.focused = breadcrumb->hasFocus()
            && focusedItemIndex() == index;
        result.offscreen = breadcrumb->itemGeometry(index).isEmpty();
        return result;
    }

    int logicalFocusChild() const override
    {
        return view() && view()->hasFocus() ? focusedItemIndex() : -1;
    }

    QStringList logicalChildActions(int index) const override
    {
        Breadcrumb* breadcrumb = view();
        if (!breadcrumb || index >= breadcrumb->itemCount() - 1)
            return {};
        return LogicalItemAccessibleAdapter::logicalChildActions(index);
    }

    void performLogicalChildAction(
        int index, const QString& actionName) override
    {
        Breadcrumb* breadcrumb = view();
        if (breadcrumb
            && actionName == QAccessibleActionInterface::pressAction()) {
            breadcrumb->activateItem(index);
        }
    }

    bool logicalSelectionSupported() const override { return false; }

private:
    Breadcrumb* view() const
    {
        return static_cast<Breadcrumb*>(ownerWidget());
    }

    int focusedItemIndex() const
    {
        Breadcrumb* breadcrumb = view();
        if (!breadcrumb)
            return -1;
        breadcrumb->ensureLayout();
        if (breadcrumb->m_focusedRecord < 0
            || breadcrumb->m_focusedRecord >= breadcrumb->m_records.size()) {
            return -1;
        }
        const Breadcrumb::DisplayRecord& record =
            breadcrumb->m_records.at(breadcrumb->m_focusedRecord);
        return record.type == Breadcrumb::RecordType::Item
            ? record.itemIndex : -1;
    }
};

class PivotAccessible final : public LogicalItemAccessibleAdapter {
public:
    explicit PivotAccessible(Pivot* pivot)
        : LogicalItemAccessibleAdapter(pivot, QAccessible::PageTabList)
    {
    }

    int logicalChildCount() const override
    {
        return view() ? view()->itemCount() : 0;
    }
    QAccessible::Role logicalChildRole(int) const override
    {
        return QAccessible::PageTab;
    }
    QString logicalChildText(int index,
                             QAccessible::Text type) const override
    {
        Pivot* pivot = view();
        if (!pivot || index < 0 || index >= pivot->itemCount())
            return {};
        const PivotItem item = pivot->itemAt(index);
        if (type == QAccessible::Name) {
            return item.accessibleName.isEmpty()
                ? item.header : item.accessibleName;
        }
        return type == QAccessible::Description
            ? positionDescription(index, pivot->itemCount()) : QString();
    }
    QRect logicalChildRect(int index) const override
    {
        return view()
            ? toGlobalRect(view()->itemHeaderGeometry(index)) : QRect();
    }
    LogicalItemAccessibleState logicalChildState(int index) const override
    {
        LogicalItemAccessibleState result;
        Pivot* pivot = view();
        if (!pivot || index < 0 || index >= pivot->itemCount()) {
            result.valid = false;
            result.invisible = true;
            return result;
        }
        const PivotItem item = pivot->itemAt(index);
        result.enabled = pivot->isEnabled() && item.enabled;
        result.selected = pivot->selectedIndex() == index;
        result.focused = pivot->hasFocus()
            && pivot->m_focusedIndex == index;
        result.offscreen = pivot->itemHeaderGeometry(index).isEmpty();
        return result;
    }
    int logicalFocusChild() const override
    {
        return view() && view()->hasFocus() ? view()->m_focusedIndex : -1;
    }
    void performLogicalChildAction(
        int index, const QString& actionName) override
    {
        if (view()
            && actionName == QAccessibleActionInterface::pressAction()) {
            view()->activateHeader(index);
        }
    }
    bool clearLogicalSelection() override
    {
        Pivot* pivot = view();
        if (!pivot || pivot->selectedIndex() < 0)
            return false;
        pivot->clearSelection();
        return pivot->selectedIndex() < 0;
    }

private:
    Pivot* view() const { return static_cast<Pivot*>(ownerWidget()); }
};

class SelectorBarAccessible final : public LogicalItemAccessibleAdapter {
public:
    explicit SelectorBarAccessible(SelectorBar* selector)
        : LogicalItemAccessibleAdapter(selector, QAccessible::PageTabList)
    {
    }

    int logicalChildCount() const override
    {
        return view() ? view()->itemCount() : 0;
    }
    QAccessible::Role logicalChildRole(int) const override
    {
        return QAccessible::PageTab;
    }
    QString logicalChildText(int index,
                             QAccessible::Text type) const override
    {
        SelectorBar* selector = view();
        if (!selector || index < 0 || index >= selector->itemCount())
            return {};
        const SelectorBarItem item = selector->itemAt(index);
        if (type == QAccessible::Name) {
            return item.accessibleName.isEmpty()
                ? item.text : item.accessibleName;
        }
        return type == QAccessible::Description
            ? positionDescription(index, selector->itemCount()) : QString();
    }
    QRect logicalChildRect(int index) const override
    {
        return view() ? toGlobalRect(view()->itemGeometry(index)) : QRect();
    }
    LogicalItemAccessibleState logicalChildState(int index) const override
    {
        LogicalItemAccessibleState result;
        SelectorBar* selector = view();
        if (!selector || index < 0 || index >= selector->itemCount()) {
            result.valid = false;
            result.invisible = true;
            return result;
        }
        const SelectorBarItem item = selector->itemAt(index);
        result.enabled = selector->isEnabled() && item.enabled && item.visible;
        result.selected = selector->selectedIndex() == index;
        result.focused = selector->hasFocus()
            && selector->m_focusedIndex == index;
        result.invisible = !item.visible;
        result.offscreen = item.visible
            && selector->itemGeometry(index).isEmpty();
        return result;
    }
    int logicalFocusChild() const override
    {
        return view() && view()->hasFocus() ? view()->m_focusedIndex : -1;
    }
    void performLogicalChildAction(
        int index, const QString& actionName) override
    {
        if (view()
            && actionName == QAccessibleActionInterface::pressAction()) {
            view()->activateItem(index);
        }
    }
    bool clearLogicalSelection() override
    {
        SelectorBar* selector = view();
        if (!selector || selector->selectedIndex() < 0)
            return false;
        selector->clearSelection();
        return selector->selectedIndex() < 0;
    }

private:
    SelectorBar* view() const
    {
        return static_cast<SelectorBar*>(ownerWidget());
    }
};

class TabViewAccessible final : public LogicalItemAccessibleAdapter {
public:
    explicit TabViewAccessible(TabView* tabs)
        : LogicalItemAccessibleAdapter(tabs, QAccessible::PageTabList)
    {
    }

    int logicalChildCount() const override
    {
        return view() ? view()->tabCount() + 1 : 0;
    }
    QAccessible::Role logicalChildRole(int index) const override
    {
        return isAddChild(index) ? QAccessible::Button
                                 : QAccessible::PageTab;
    }
    QString logicalChildText(int index,
                             QAccessible::Text type) const override
    {
        TabView* tabs = view();
        if (!tabs)
            return {};
        if (isAddChild(index)) {
            return type == QAccessible::Name
                ? navigationText("Add tab") : QString();
        }
        if (index < 0 || index >= tabs->tabCount())
            return {};
        const TabViewItem item = tabs->tabAt(index);
        if (type == QAccessible::Name) {
            return item.accessibleName.isEmpty()
                ? item.text : item.accessibleName;
        }
        return type == QAccessible::Description
            ? positionDescription(index, tabs->tabCount()) : QString();
    }
    QRect logicalChildRect(int index) const override
    {
        TabView* tabs = view();
        if (!tabs)
            return {};
        return toGlobalRect(isAddChild(index)
            ? tabs->addButtonGeometry() : tabs->tabGeometry(index));
    }
    LogicalItemAccessibleState logicalChildState(int index) const override
    {
        LogicalItemAccessibleState result;
        TabView* tabs = view();
        if (!tabs) {
            result.valid = false;
            result.invisible = true;
            return result;
        }
        if (isAddChild(index)) {
            result.enabled = tabs->isEnabled();
            result.selectable = false;
            result.focused = tabStripFocusKind(TabStrip::HitKind::Add);
            result.invisible = !tabs->addTabButtonVisible();
            result.offscreen = !result.invisible
                && tabs->addButtonGeometry().isEmpty();
            return result;
        }
        if (index < 0 || index >= tabs->tabCount()) {
            result.valid = false;
            result.invisible = true;
            return result;
        }
        const TabViewItem item = tabs->tabAt(index);
        result.enabled = tabs->isEnabled() && item.enabled;
        result.selected = tabs->selectedIndex() == index;
        result.focused = tabs->m_tabStrip
            && tabs->m_tabStrip->hasFocus()
            && tabs->m_tabStrip->m_focusedHit.kind == TabStrip::HitKind::Tab
            && tabs->m_tabStrip->m_focusedHit.tabIndex == index;
        result.offscreen = tabs->tabGeometry(index).isEmpty();
        return result;
    }
    int logicalFocusChild() const override
    {
        TabView* tabs = view();
        if (!tabs || !tabs->m_tabStrip || !tabs->m_tabStrip->hasFocus())
            return -1;
        const TabStrip::HitRecord& hit = tabs->m_tabStrip->m_focusedHit;
        if (hit.kind == TabStrip::HitKind::Tab)
            return hit.tabIndex;
        return hit.kind == TabStrip::HitKind::Add
            ? tabs->tabCount() : -1;
    }
    QStringList logicalChildActions(int index) const override
    {
        TabView* tabs = view();
        if (!tabs)
            return {};
        if (isAddChild(index)) {
            return tabs->isEnabled() && tabs->addTabButtonVisible()
                ? QStringList{QAccessibleActionInterface::pressAction()}
                : QStringList();
        }

        QStringList result =
            LogicalItemAccessibleAdapter::logicalChildActions(index);
        if (tabs->isCloseableIndex(index))
            result.append(closeAction());
        if (tabs->isEnabled() && tabs->tabReorderEnabled()
            && index > 0) {
            result.append(movePreviousAction());
        }
        if (tabs->isEnabled() && tabs->tabReorderEnabled()
            && index >= 0 && index < tabs->tabCount() - 1) {
            result.append(moveNextAction());
        }
        return result;
    }
    QStringList logicalChildKeyBindings(
        int index, const QString& actionName) const override
    {
        TabView* tabs = view();
        if (!tabs || !tabs->keyboardAcceleratorsEnabled())
            return {};
        if (isAddChild(index)
            && actionName == QAccessibleActionInterface::pressAction()) {
            return {QStringLiteral("Ctrl+T")};
        }
        if (index == tabs->selectedIndex() && actionName == closeAction())
            return {QStringLiteral("Ctrl+W")};
        if (actionName == QAccessibleActionInterface::pressAction()
            && index >= 0 && index < 9) {
            return {QStringLiteral("Ctrl+%1").arg(index + 1)};
        }
        return {};
    }
    void performLogicalChildAction(
        int index, const QString& actionName) override
    {
        TabView* tabs = view();
        if (!tabs)
            return;
        if (isAddChild(index)) {
            if (actionName == QAccessibleActionInterface::pressAction()
                && tabs->isEnabled() && tabs->addTabButtonVisible()) {
                emit tabs->addTabRequested();
            }
            return;
        }
        if (actionName == QAccessibleActionInterface::pressAction()) {
            tabs->setSelectedIndex(index);
        } else if (actionName == closeAction()
                   && tabs->isCloseableIndex(index)) {
            emit tabs->tabCloseRequested(index);
        } else if (actionName == movePreviousAction()) {
            tabs->moveTab(index, index - 1);
        } else if (actionName == moveNextAction()) {
            tabs->moveTab(index, index + 1);
        }
    }
    bool clearLogicalSelection() override
    {
        TabView* tabs = view();
        if (!tabs || tabs->selectedIndex() < 0)
            return false;
        tabs->setSelectedIndex(-1);
        return tabs->selectedIndex() < 0;
    }

private:
    TabView* view() const { return static_cast<TabView*>(ownerWidget()); }
    bool isAddChild(int index) const
    {
        return view() && index == view()->tabCount();
    }
    bool tabStripFocusKind(TabStrip::HitKind kind) const
    {
        TabView* tabs = view();
        return tabs && tabs->m_tabStrip && tabs->m_tabStrip->hasFocus()
            && tabs->m_tabStrip->m_focusedHit.kind == kind;
    }
};

namespace {

QAccessibleInterface* navigationSelectionAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* breadcrumb = dynamic_cast<Breadcrumb*>(object))
        return new BreadcrumbAccessible(breadcrumb);
    if (auto* pivot = dynamic_cast<Pivot*>(object))
        return new PivotAccessible(pivot);
    if (auto* selector = dynamic_cast<SelectorBar*>(object))
        return new SelectorBarAccessible(selector);
    if (auto* tabs = dynamic_cast<TabView*>(object))
        return new TabViewAccessible(tabs);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureNavigationSelectionAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(
            navigationSelectionAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

} // namespace fluent::navigation::detail
