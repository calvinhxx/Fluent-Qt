#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "compatibility/QtCompat.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/Pivot.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/TabView.h"
#include "components/scrolling/PipsPager.h"
#include "QtTestEnvironment.h"

using fluent::navigation::Breadcrumb;
using fluent::navigation::BreadcrumbItem;
using fluent::navigation::Pivot;
using fluent::navigation::PivotItem;
using fluent::navigation::SelectorBar;
using fluent::navigation::SelectorBarItem;
using fluent::navigation::TabView;
using fluent::navigation::TabViewItem;
using fluent::scrolling::PipsPager;

namespace {

void showAndProcess(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    widget.show();
    QApplication::processEvents();
}

#if QT_CONFIG(accessibility)

QVector<QAccessible::Event> g_accessibilityEvents;

void captureAccessibilityEvent(QAccessibleEvent* event)
{
    if (event)
        g_accessibilityEvents.append(event->type());
}

class ScopedAccessibilityEventCapture {
public:
    ScopedAccessibilityEventCapture()
        : m_previous(
              QAccessible::installUpdateHandler(captureAccessibilityEvent))
    {
        g_accessibilityEvents.clear();
    }

    ~ScopedAccessibilityEventCapture()
    {
        QAccessible::installUpdateHandler(m_previous);
        g_accessibilityEvents.clear();
    }

    int count(QAccessible::Event type) const
    {
        return g_accessibilityEvents.count(type);
    }

    void clear() { g_accessibilityEvents.clear(); }

private:
    QAccessible::UpdateHandler m_previous = nullptr;
};

QAccessibleInterface* accessible(QWidget* widget)
{
    return widget ? QAccessible::queryAccessibleInterface(widget) : nullptr;
}

#endif

} // namespace

TEST(NavigationAccessibilityTest, Contract_AccessibilityBreadcrumbExposesOrderedPathItems)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Breadcrumb breadcrumb;
    breadcrumb.setItems(QVector<BreadcrumbItem>{
        BreadcrumbItem(QStringLiteral("Home")),
        BreadcrumbItem(QStringLiteral("Locked"), QVariant(), false,
                       QStringLiteral("Restricted folder")),
        BreadcrumbItem(QStringLiteral("Current"))});
    breadcrumb.setOverflowMode(Breadcrumb::OverflowMode::Beginning);
    showAndProcess(breadcrumb, QSize(130, 40));

    QAccessibleInterface* root = accessible(&breadcrumb);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::List);
    ASSERT_EQ(root->childCount(), 3);

    QAccessibleInterface* home = root->child(0);
    QAccessibleInterface* locked = root->child(1);
    QAccessibleInterface* current = root->child(2);
    ASSERT_NE(home, nullptr);
    ASSERT_NE(locked, nullptr);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(home->role(), QAccessible::Link);
    EXPECT_EQ(home->text(QAccessible::Name), QStringLiteral("Home"));
    EXPECT_EQ(locked->text(QAccessible::Name),
              QStringLiteral("Restricted folder"));
    EXPECT_TRUE(locked->state().disabled);
    EXPECT_TRUE(locked->actionInterface()->actionNames().isEmpty());
    EXPECT_TRUE(current->state().selected);
    EXPECT_TRUE(current->actionInterface()->actionNames().isEmpty());

    QSignalSpy activated(&breadcrumb, &Breadcrumb::itemActivated);
    home->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(activated.count(), 1);
    EXPECT_EQ(activated.first().at(0).toInt(), 0);

    breadcrumb.setAccessibleName(QStringLiteral("Project location"));
    breadcrumb.appendItem(QStringLiteral("Draft"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Project location"));
#endif
}

TEST(NavigationAccessibilityTest, Contract_AccessibilitySelectorsExposePageTabsAndSelection)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Pivot pivot;
    pivot.addItem(QStringLiteral("All"));
    pivot.addItem(PivotItem(QStringLiteral("Unread"), QString(), true,
                            QVariant(), QStringLiteral("Unread messages")));
    pivot.addItem(PivotItem(QStringLiteral("Disabled"), QString(), false));
    showAndProcess(pivot, QSize(420, 44));

    QAccessibleInterface* pivotRoot = accessible(&pivot);
    ASSERT_NE(pivotRoot, nullptr);
    EXPECT_EQ(pivotRoot->role(), QAccessible::PageTabList);
    ASSERT_EQ(pivotRoot->childCount(), 3);
    EXPECT_TRUE(pivotRoot->child(0)->state().selected);
    EXPECT_EQ(pivotRoot->child(1)->text(QAccessible::Name),
              QStringLiteral("Unread messages"));
    EXPECT_TRUE(pivotRoot->child(2)->state().disabled);
    EXPECT_TRUE(pivotRoot->child(2)->actionInterface()->actionNames().isEmpty());

    QSignalSpy pivotActivated(&pivot, &Pivot::itemActivated);
    pivotRoot->child(1)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(pivot.selectedIndex(), 1);
    EXPECT_EQ(pivotActivated.count(), 1);
    EXPECT_TRUE(pivotRoot->child(1)->state().selected);

#if FLUENT_HAS_ACCESSIBLE_SELECTION_INTERFACE
    auto* pivotSelection = static_cast<QAccessibleSelectionInterface*>(
        pivotRoot->interface_cast(QAccessible::SelectionInterface));
    ASSERT_NE(pivotSelection, nullptr);
    EXPECT_EQ(pivotSelection->selectedItemCount(), 1);
    EXPECT_TRUE(pivotSelection->isSelected(pivotRoot->child(1)));
#endif

    SelectorBar selector;
    selector.addItem(QStringLiteral("Overview"));
    selector.addItem(SelectorBarItem(
        QStringLiteral("Activity"), QString(), true, true,
        QVariant(), QStringLiteral("Activity timeline")));
    selector.addItem(SelectorBarItem(
        QStringLiteral("Hidden"), QString(), true, false));
    showAndProcess(selector, QSize(420, 44));

    QAccessibleInterface* selectorRoot = accessible(&selector);
    ASSERT_NE(selectorRoot, nullptr);
    EXPECT_EQ(selectorRoot->role(), QAccessible::PageTabList);
    ASSERT_EQ(selectorRoot->childCount(), 3);
    EXPECT_EQ(selectorRoot->child(1)->text(QAccessible::Name),
              QStringLiteral("Activity timeline"));
    EXPECT_TRUE(selectorRoot->child(2)->state().invisible);
    EXPECT_TRUE(selectorRoot->child(2)->actionInterface()->actionNames().isEmpty());

    selectorRoot->child(1)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(selector.selectedIndex(), 1);
    EXPECT_TRUE(selectorRoot->child(1)->state().selected);
#endif
}

TEST(NavigationAccessibilityTest, Contract_AccessibilityTabViewExposesAddCloseAndReorderActions)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    TabView tabs;
    tabs.addTab(TabViewItem(QStringLiteral("One"), QString(), true, true));
    tabs.addTab(TabViewItem(QStringLiteral("Two"), QString(), true, true,
                            QVariant(), QStringLiteral("Second document")));
    tabs.setTabReorderEnabled(true);
    showAndProcess(tabs, QSize(520, 40));

    QAccessibleInterface* root = accessible(&tabs);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::PageTabList);
    ASSERT_EQ(root->childCount(), 3);

    QAccessibleInterface* first = root->child(0);
    QAccessibleInterface* second = root->child(1);
    QAccessibleInterface* add = root->child(2);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(first->role(), QAccessible::PageTab);
    EXPECT_EQ(second->text(QAccessible::Name),
              QStringLiteral("Second document"));
    EXPECT_EQ(add->role(), QAccessible::Button);
    EXPECT_EQ(add->text(QAccessible::Name), QStringLiteral("Add tab"));

    const QStringList firstActions =
        first->actionInterface()->actionNames();
    EXPECT_TRUE(firstActions.contains(
        QAccessibleActionInterface::pressAction()));
    EXPECT_TRUE(firstActions.contains(QStringLiteral("Close tab")));
    EXPECT_TRUE(firstActions.contains(QStringLiteral("Move tab forward")));

    QSignalSpy addSpy(&tabs, &TabView::addTabRequested);
    QSignalSpy closeSpy(&tabs, &TabView::tabCloseRequested);
    add->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(addSpy.count(), 1);
    first->actionInterface()->doAction(QStringLiteral("Close tab"));
    EXPECT_EQ(closeSpy.count(), 1);
    EXPECT_EQ(closeSpy.first().at(0).toInt(), 0);

    first->actionInterface()->doAction(QStringLiteral("Move tab forward"));
    EXPECT_EQ(tabs.tabAt(0).text, QStringLiteral("Two"));
    EXPECT_EQ(tabs.tabAt(1).text, QStringLiteral("One"));

    root = accessible(&tabs);
    root->child(0)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(tabs.selectedIndex(), 0);
#endif
}

TEST(NavigationAccessibilityTest, Contract_AccessibilityPipsPagerExposesAllPagesAndNavigation)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    PipsPager pager;
    pager.setNumberOfPages(10);
    pager.setMaxVisiblePips(5);
    pager.setSelectedPageIndex(4);
    pager.setPreviousButtonVisibility(
        PipsPager::PipsPagerButtonVisibility::Visible);
    pager.setNextButtonVisibility(
        PipsPager::PipsPagerButtonVisibility::Visible);
    showAndProcess(pager, pager.sizeHint());

    QAccessibleInterface* root = accessible(&pager);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::PageTabList);
    ASSERT_EQ(root->childCount(), 12);
    EXPECT_EQ(root->child(0)->text(QAccessible::Name),
              QStringLiteral("Page 1 of 10"));
    EXPECT_TRUE(root->child(4)->state().selected);
    EXPECT_TRUE(root->child(9)->state().offscreen);
    EXPECT_EQ(root->child(10)->text(QAccessible::Name),
              QStringLiteral("Previous page"));
    EXPECT_EQ(root->child(11)->text(QAccessible::Name),
              QStringLiteral("Next page"));

    root->child(11)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(pager.selectedPageIndex(), 5);
    EXPECT_TRUE(root->child(5)->state().selected);

    root->child(0)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction());
    EXPECT_EQ(pager.selectedPageIndex(), 0);
    EXPECT_TRUE(root->child(10)->state().disabled);
#endif
}

TEST(NavigationAccessibilityTest, Contract_AccessibilityEventsFollowEffectiveChangesAndNoOps)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Pivot pivot;
    pivot.addItem(QStringLiteral("All"));
    pivot.addItem(QStringLiteral("Unread"));
    showAndProcess(pivot, QSize(320, 44));
    ASSERT_NE(accessible(&pivot), nullptr);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    pivot.setSelectedIndex(1);
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);
    pivot.setSelectedIndex(1);
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);

    capture.clear();
    EXPECT_TRUE(pivot.setItemHeader(1, QStringLiteral("Unread mail")));
    EXPECT_GE(capture.count(QAccessible::NameChanged), 1);
    const int nameEvents = capture.count(QAccessible::NameChanged);
    EXPECT_FALSE(pivot.setItemHeader(1, QStringLiteral("Unread mail")));
    EXPECT_EQ(capture.count(QAccessible::NameChanged), nameEvents);

    capture.clear();
    pivot.addItem(QStringLiteral("Flagged"));
    EXPECT_EQ(capture.count(QAccessible::ObjectReorder), 1);

    PipsPager pager;
    pager.setNumberOfPages(4);
    showAndProcess(pager, pager.sizeHint());
    ASSERT_NE(accessible(&pager), nullptr);
    capture.clear();
    pager.setSelectedPageIndex(2);
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);
    pager.setSelectedPageIndex(2);
    EXPECT_EQ(capture.count(QAccessible::Selection), 1);
#endif
}
