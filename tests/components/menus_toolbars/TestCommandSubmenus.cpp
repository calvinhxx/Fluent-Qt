#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/private/CommandMenuController_p.h"
#include "components/menus_toolbars/private/CommandPresenter_p.h"
#include "components/scrolling/ScrollView.h"

using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandBarFlyout;
using fluent::menus_toolbars::FluentMenu;

namespace {

enum class Surface { Inline, Overflow, Secondary, FlyoutPrimary, FlyoutSecondary };

TEST(CommandMenuSurfaceTest, Contract_HidingOrDestroyingPresenterClosesBorrowedMenu)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    QWidget window;
    window.resize(400, 300);
    FluentMenu menu(QStringLiteral("Borrowed"));
    menu.addAction(QStringLiteral("Choice"));
    QAction action(QStringLiteral("Open"));
    action.setMenu(&menu);
    CommandMenuController controller(&window);
    auto* presenter = new CommandPresenter(&action, CommandPresenter::Mode::Primary, {}, &window);
    presenter->setGeometry(20, 20, 120, 40);
    window.show();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    ASSERT_TRUE(controller.open(presenter));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    presenter->hide();
    EXPECT_FALSE(menu.isVisible());
    presenter->show();
    ASSERT_TRUE(controller.open(presenter));
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    delete presenter;
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible(); }));
    EXPECT_EQ(action.menu(), &menu);
    EXPECT_EQ(menu.parent(), nullptr);
}

TEST(CommandMenuSurfaceTest, Contract_ParentedMenuKeepsFirstNativeTransientParent)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    QWidget callerOwner;
    callerOwner.winId();
    QWidget invocationHost;
    invocationHost.resize(400, 300);
    FluentMenu menu(QStringLiteral("Borrowed"), &callerOwner);
    menu.addAction(QStringLiteral("Choice"));
    ASSERT_EQ(menu.windowHandle(), nullptr);
    QAction action(QStringLiteral("Open"));
    action.setMenu(&menu);
    CommandMenuController controller(&invocationHost);
    CommandPresenter presenter(&action, CommandPresenter::Mode::Primary, {}, &invocationHost);
    presenter.setGeometry(20, 20, 120, 40);
    invocationHost.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&invocationHost));
    ASSERT_TRUE(controller.open(&presenter));
    ASSERT_NE(menu.windowHandle(), nullptr);
    EXPECT_EQ(menu.windowHandle()->transientParent(), invocationHost.windowHandle());
    controller.close();
    EXPECT_EQ(menu.windowHandle()->transientParent(), callerOwner.windowHandle());
    EXPECT_EQ(menu.parentWidget(), &callerOwner);
}

TEST(CommandMenuSurfaceTest, Contract_BorrowedMenuRestoresNativeParentAcrossHosts)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    QWidget originalHost;
    QWidget firstHost;
    QWidget secondHost;
    originalHost.winId();
    firstHost.resize(400, 300);
    secondHost.resize(400, 300);
    firstHost.show();
    secondHost.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&firstHost));
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&secondHost));
    FluentMenu menu(QStringLiteral("Borrowed"));
    menu.addAction(QStringLiteral("Choice"));
    menu.winId();
    menu.windowHandle()->setTransientParent(originalHost.windowHandle());
    QAction action(QStringLiteral("Open"));
    action.setMenu(&menu);
    for (QWidget* host : {&firstHost, &secondHost}) {
        CommandMenuController controller(host);
        CommandPresenter presenter(&action, CommandPresenter::Mode::Primary, {}, host);
        presenter.setGeometry(20, 20, 120, 40);
        presenter.show();
        ASSERT_TRUE(controller.open(&presenter));
        ASSERT_TRUE(menu.isVisible());
        EXPECT_EQ(menu.parent(), nullptr);
        EXPECT_EQ(menu.windowHandle()->transientParent(), host->windowHandle());
        controller.close();
        EXPECT_FALSE(menu.isVisible());
        EXPECT_EQ(menu.windowHandle()->transientParent(), originalHost.windowHandle());
    }
    {
        CommandMenuController controller(&firstHost);
        CommandPresenter presenter(&action, CommandPresenter::Mode::Primary, {}, &firstHost);
        presenter.setGeometry(20, 20, 120, 40);
        presenter.show();
        ASSERT_TRUE(controller.open(&presenter));
        // A caller's deliberate reassignment must survive cleanup.
        menu.windowHandle()->setTransientParent(secondHost.windowHandle());
        controller.close();
        EXPECT_EQ(menu.windowHandle()->transientParent(), secondHost.windowHandle());
    }
    EXPECT_EQ(action.menu(), &menu);
    EXPECT_EQ(menu.parent(), nullptr);
}

TEST(CommandMenuSurfaceTest, Contract_TransientParentCallbacksCanReopenOnAnotherPresenter)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    for (const int phase : {0, 1, 2}) {
        SCOPED_TRACE(phase);
        QWidget originalHost;
        originalHost.winId();
        QWidget host;
        host.resize(400, 300);
        FluentMenu menu(QStringLiteral("Borrowed"));
        menu.addAction(QStringLiteral("Choice"));
        menu.winId();
        menu.windowHandle()->setTransientParent(originalHost.windowHandle());
        QAction action(QStringLiteral("Open"));
        action.setMenu(&menu);
        CommandMenuController controller(&host);
        CommandPresenter first(&action, CommandPresenter::Mode::Primary, {}, &host);
        CommandPresenter second(&action, CommandPresenter::Mode::Primary, {}, &host);
        CommandPresenter third(&action, CommandPresenter::Mode::Primary, {}, &host);
        first.setGeometry(20, 20, 120, 40);
        second.setGeometry(180, 20, 120, 40);
        third.setGeometry(20, 100, 120, 40);
        host.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
        bool reopened = false;
        bool armed = phase == 0;
        QObject::connect(menu.windowHandle(), &QWindow::transientParentChanged, &host, [&](QWindow* parent) {
            const auto expected = phase > 0 ? originalHost.windowHandle() : host.windowHandle();
            if (!armed || reopened || parent != expected)
                return;
            reopened = true;
            if (phase == 0)
                controller.close();
            EXPECT_TRUE(controller.open(&second));
        });
        ASSERT_TRUE(controller.open(&first));
        // QMenu::popup can emit for the existing transient parent as well.
        // Arm the close/switch scenarios only after the initial open completes.
        armed = true;
        if (phase == 1)
            controller.close();
        else if (phase == 2)
            controller.open(&third);
        ASSERT_TRUE(reopened);
        EXPECT_TRUE(menu.isVisible());
        EXPECT_FALSE(QAccessible::queryAccessibleInterface(&first)->state().expanded);
        EXPECT_TRUE(QAccessible::queryAccessibleInterface(&second)->state().expanded);
        EXPECT_FALSE(QAccessible::queryAccessibleInterface(&third)->state().expanded);
        EXPECT_EQ(menu.windowHandle()->transientParent(), host.windowHandle());
        controller.close();
        EXPECT_FALSE(menu.isVisible());
        EXPECT_EQ(menu.windowHandle()->transientParent(), originalHost.windowHandle());
        EXPECT_EQ(menu.parent(), nullptr);
    }
}

TEST(CommandMenuSurfaceTest, Contract_ClosingDuringPopupRestoresBorrowedNativeParent)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    QWidget originalHost;
    originalHost.winId();
    QWidget callerOwner;
    callerOwner.winId();
    QWidget host;
    host.resize(400, 300);
    FluentMenu menu(QStringLiteral("Borrowed"), &callerOwner);
    menu.addAction(QStringLiteral("Choice"));
    menu.winId();
    menu.windowHandle()->setTransientParent(originalHost.windowHandle());
    QAction action(QStringLiteral("Open"));
    action.setMenu(&menu);
    CommandMenuController controller(&host);
    CommandPresenter presenter(&action, CommandPresenter::Mode::Primary, {}, &host);
    presenter.setGeometry(20, 20, 120, 40);
    host.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
    bool closed = false;
    const auto connection = QObject::connect(menu.windowHandle(), &QWindow::transientParentChanged, &host,
        [&](QWindow* parent) {
            if (closed || parent != callerOwner.windowHandle())
                return;
            closed = true;
            controller.close();
        });
    const auto disconnect = qScopeGuard([&] { QObject::disconnect(connection); });
    ASSERT_TRUE(controller.open(&presenter));
    if (!closed) {
        // Older Qt backends keep an existing parent during Show. The shared
        // aboutToShow cancellation contract below still runs on those SDKs.
        EXPECT_TRUE(menu.isVisible());
        controller.close();
        EXPECT_EQ(menu.windowHandle()->transientParent(), originalHost.windowHandle());
        GTEST_SKIP() << "Qt kept the existing native parent during popup";
    }
    EXPECT_FALSE(menu.isVisible());
    EXPECT_FALSE(QAccessible::queryAccessibleInterface(&presenter)->state().expanded);
    EXPECT_EQ(menu.windowHandle()->transientParent(), originalHost.windowHandle());
    EXPECT_EQ(action.menu(), &menu);
    EXPECT_EQ(menu.parentWidget(), &callerOwner);
}

TEST(CommandMenuSurfaceTest, Contract_AboutToShowCancellationPreservesLaterInvocations)
{
    using fluent::menus_toolbars::detail::CommandMenuController;
    using fluent::menus_toolbars::detail::CommandPresenter;
    for (const int continuation : {0, 1, 2, 3, 4}) {
        SCOPED_TRACE(continuation);
        QWidget originalHost;
        originalHost.winId();
        QWidget callerOwner;
        callerOwner.winId();
        QWidget host;
        host.resize(400, 300);
        FluentMenu menu(QStringLiteral("Borrowed"), &callerOwner);
        FluentMenu other(QStringLiteral("Other"), &callerOwner);
        for (auto* item : {&menu, &other}) {
            item->addAction(QStringLiteral("Choice"));
            item->winId();
            item->windowHandle()->setTransientParent(originalHost.windowHandle());
        }
        QAction action(QStringLiteral("Open"));
        QAction otherAction(QStringLiteral("Other"));
        action.setMenu(&menu);
        otherAction.setMenu(&other);
        QPointer<CommandMenuController> controller = new CommandMenuController(&host);
        const auto cleanupController = qScopeGuard([&] { delete controller.data(); });
        CommandPresenter first(&action, CommandPresenter::Mode::Primary, {}, &host);
        CommandPresenter next(continuation == 2 ? &otherAction : &action, CommandPresenter::Mode::Primary, {}, &host);
        first.setGeometry(20, 20, 120, 40);
        next.setGeometry(180, 20, 120, 40);
        host.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&host));
        bool cancelled = false;
        bool reopened = false;
        const auto showConnection = QObject::connect(&menu, &QMenu::aboutToShow, &host, [&] {
            if (cancelled)
                return;
            cancelled = true;
            if (continuation == 4) {
                delete controller.data();
                return;
            }
            controller->close();
            if (continuation == 1 || continuation == 2) {
                reopened = true;
                EXPECT_TRUE(controller->open(&next));
            }
        });
        const auto hideConnection = QObject::connect(&menu, &QMenu::aboutToHide, &host, [&] {
            if (continuation == 3 && cancelled && !reopened) {
                reopened = true;
                EXPECT_TRUE(controller->open(&next));
            }
        });
        const auto disconnect = qScopeGuard([&] {
            QObject::disconnect(showConnection);
            QObject::disconnect(hideConnection);
        });
        ASSERT_TRUE(controller->open(&first));
        ASSERT_TRUE(cancelled);
        EXPECT_FALSE(QAccessible::queryAccessibleInterface(&first)->state().expanded);
        if (continuation > 0 && continuation < 4) {
            ASSERT_TRUE(reopened);
            auto* current = continuation == 2 ? &other : &menu;
            EXPECT_TRUE(current->isVisible());
            EXPECT_TRUE(QAccessible::queryAccessibleInterface(&next)->state().expanded);
            EXPECT_EQ(current->windowHandle()->transientParent(), host.windowHandle());
            controller->close();
            EXPECT_FALSE(current->isVisible());
            EXPECT_EQ(current->windowHandle()->transientParent(), originalHost.windowHandle());
        } else {
            EXPECT_FALSE(reopened);
        }
        EXPECT_FALSE(menu.isVisible());
        EXPECT_EQ(menu.windowHandle()->transientParent(), originalHost.windowHandle());
        EXPECT_EQ(menu.parentWidget(), &callerOwner);
        EXPECT_EQ(action.menu(), &menu);
    }
}

TEST(CommandMenuSurfaceTest, Contract_RestingRowsKeepTheMenuSurfaceAcrossThemeChangesAndReopen)
{
    const auto previousTheme = fluent::FluentElement::currentTheme();
    const auto previousMotion = fluent::MotionPolicy::instance().mode();
    const QPoint cursor = QCursor::pos();
    const auto restore = qScopeGuard([&] {
        fluent::FluentElement::setTheme(previousTheme);
        fluent::MotionPolicy::instance().setMode(previousMotion);
        QCursor::setPos(cursor);
    });
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    for (const bool useFlyout : {false, true}) {
        SCOPED_TRACE(useFlyout ? "flyout" : "overflow");
        QWidget window;
        window.resize(800, 500);
        CommandBar bar(&window);
        bar.setGeometry(30, 30, 400, 48);
        CommandBarFlyout flyout(&window);
        flyout.setAnimationEnabled(false);
        QAction first(QStringLiteral("Settings"));
        QAction last(QStringLiteral("Help"));
        if (useFlyout) {
            flyout.addSecondaryAction(&first);
            flyout.addSecondaryAction(&last);
        } else {
            bar.addSecondaryAction(&first);
            bar.addSecondaryAction(&last);
        }
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
        QCursor::setPos(window.mapToGlobal(QPoint(780, 480)));
        const auto open = [&] {
            if (useFlyout)
                flyout.showAt(&bar);
            else
                bar.setOverflowOpen(true);
            QApplication::processEvents();
            QCursor::setPos(window.mapToGlobal(QPoint(780, 480)));
        };
        const auto close = [&] {
            if (useFlyout)
                flyout.close();
            else
                bar.setOverflowOpen(false);
            QApplication::processEvents();
        };
        open();
        auto* scroll = window.findChild<fluent::scrolling::ScrollView*>(useFlyout
                ? QStringLiteral("FluentCommandBarFlyout.ScrollView")
                : QStringLiteral("FluentCommandBar.OverflowScrollView"));
        ASSERT_NE(scroll, nullptr);
        QWidget* surface = scroll->parentWidget();
        for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
            fluent::FluentElement::setTheme(theme);
            QApplication::processEvents();
            for (int pass = 0; pass < 2; ++pass) {
                ASSERT_TRUE(scroll->isVisible());
                ASSERT_NE(scroll->widget(), nullptr);
                EXPECT_FALSE(scroll->viewport()->autoFillBackground());
                EXPECT_FALSE(scroll->widget()->autoFillBackground());
                // The unhighlighted last row must reveal the same layer as the popup.
                const QPoint localPoint(scroll->widget()->width() - 8, scroll->widget()->height() - 8);
                auto* sampledRow = scroll->widget()->childAt(localPoint);
                ASSERT_NE(sampledRow, nullptr);
                ASSERT_TRUE(QTest::qWaitFor([&] {
                    return !sampledRow->underMouse()
                        && !QRect(sampledRow->mapToGlobal(QPoint()), sampledRow->size()).contains(QCursor::pos());
                }));
                const QPoint point = scroll->widget()->mapTo(surface, localPoint);
                const QPixmap image = surface->grab();
                EXPECT_EQ(image.toImage().pixelColor(point * image.devicePixelRatioF()), bar.themeColorsRef().bgLayer);
                close();
                open();
            }
        }
        close();
    }
}

class CommandSubmenuTest : public testing::TestWithParam<Surface> {
protected:
    void SetUp() override
    {
        window.resize(900, 650);
        anchor.setGeometry(30, 70, 120, 40);
        branch.setMenu(&menu);
        branch.setCheckable(true);
        menu.addAction(QStringLiteral("First"));
        menu.addMenu(&second);
        second.addAction(QStringLiteral("Second"));
        second.addMenu(&third);
        leaf = third.addAction(QStringLiteral("Choose"));
        leaf->setCheckable(true);
        window.show();
        window.activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
        if (GetParam() == Surface::FlyoutPrimary || GetParam() == Surface::FlyoutSecondary) {
            flyout = new CommandBarFlyout(&window);
            flyout->setAnimationEnabled(false);
            if (GetParam() == Surface::FlyoutPrimary)
                ASSERT_TRUE(flyout->addPrimaryAction(&branch));
            else
                ASSERT_TRUE(flyout->addSecondaryAction(&branch));
            flyout->showAt(&anchor);
            ASSERT_TRUE(QTest::qWaitFor([&] { return flyout->isOpen(); }));
        } else {
            bar = new CommandBar(&window);
            bar->setGeometry(30, 130, GetParam() == Surface::Overflow ? 48 : 650, 48);
            if (GetParam() == Surface::Secondary)
                ASSERT_TRUE(bar->addSecondaryAction(&branch));
            else
                ASSERT_TRUE(bar->addPrimaryAction(&branch));
            bar->show();
            QApplication::processEvents();
            if (GetParam() != Surface::Inline) {
                bar->setOverflowOpen(true);
                ASSERT_TRUE(QTest::qWaitFor([&] { return bar->isOverflowOpen(); }));
            }
        }
        QApplication::processEvents();
    }

    void TearDown() override
    {
        menu.hide();
        second.hide();
        third.hide();
        if (flyout)
            flyout->close();
        if (bar)
            bar->setOverflowOpen(false);
        QApplication::processEvents();
    }

    QAbstractButton* presenter() const
    {
        for (auto* button : window.findChildren<QAbstractButton*>()) {
            if (button->property("commandAction").value<QObject*>() == &branch && button->isVisible())
                return button;
        }
        return nullptr;
    }

    bool parentOpen() const
    {
        if (flyout)
            return flyout->isOpen();
        return bar && (GetParam() == Surface::Inline ? bar->isVisible() : bar->isOverflowOpen());
    }

    void openMenu()
    {
        auto* button = presenter();
        ASSERT_NE(button, nullptr);
        button->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(button, Qt::Key_Return);
        ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
        EXPECT_TRUE(parentOpen());
    }

    void openNestedMenus()
    {
        menu.setActiveAction(second.menuAction());
        QTest::keyClick(&menu, Qt::Key_Right);
        ASSERT_TRUE(QTest::qWaitFor([&] { return second.isVisible(); }));
        second.setActiveAction(third.menuAction());
        QTest::keyClick(&second, Qt::Key_Right);
        ASSERT_TRUE(QTest::qWaitFor([&] { return third.isVisible(); }));
    }

    QWidget window;
    fluent::basicinput::Button anchor{QStringLiteral("Commands"), &window};
    QAction branch{QStringLiteral("Options")};
    FluentMenu menu{QStringLiteral("Options")};
    FluentMenu second{QStringLiteral("Sort"), &menu};
    FluentMenu third{QStringLiteral("Order"), &second};
    QAction* leaf = nullptr;
    QPointer<CommandBar> bar;
    QPointer<CommandBarFlyout> flyout;
};

TEST_P(CommandSubmenuTest, Contract_ThreeLevelsTriggerOnlyTheLeafAndCloseTheSurface)
{
    QSignalSpy branchTriggered(&branch, &QAction::triggered);
    QSignalSpy leafTriggered(leaf, &QAction::triggered);
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    openNestedMenus();
    ASSERT_FALSE(HasFatalFailure());
    third.setActiveAction(leaf);
    QTest::keyClick(&third, Qt::Key_Return);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible(); }));
    QApplication::processEvents();
    EXPECT_EQ(leafTriggered.count(), 1);
    EXPECT_TRUE(leaf->isChecked());
    EXPECT_EQ(branchTriggered.count(), 0);
    EXPECT_FALSE(branch.isChecked());
    EXPECT_FALSE(second.isVisible());
    EXPECT_FALSE(third.isVisible());
    EXPECT_EQ(parentOpen(), GetParam() == Surface::Inline);
    EXPECT_EQ(menu.parent(), nullptr);
    EXPECT_EQ(branch.parent(), nullptr);
}

TEST_P(CommandSubmenuTest, Contract_EscapeReturnsOneLevelAtATime)
{
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    menu.setActiveAction(second.menuAction());
    QTest::keyClick(&menu, Qt::Key_Right);
    ASSERT_TRUE(QTest::qWaitFor([&] { return second.isVisible(); }));
    QTest::keyClick(&second, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !second.isVisible(); }));
    EXPECT_TRUE(menu.isVisible());
    EXPECT_TRUE(parentOpen());
    QTest::keyClick(&menu, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible(); }));
    ASSERT_TRUE(QTest::qWaitFor([&] { return presenter() && presenter()->hasFocus(); }));
    EXPECT_TRUE(parentOpen());
    if (GetParam() != Surface::Inline) {
        QTest::keyClick(presenter(), Qt::Key_Escape);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !parentOpen(); }));
    }
}

TEST_P(CommandSubmenuTest, Contract_MenuMousePressDoesNotLightDismissTheParent)
{
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    QAction* first = menu.actions().first();
    QSignalSpy triggered(first, &QAction::triggered);
    const QPoint point = menu.actionGeometry(first).center();
    QTest::mousePress(&menu, Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_TRUE(parentOpen());
    EXPECT_TRUE(menu.isVisible());
    QTest::mouseRelease(&menu, Qt::LeftButton, Qt::NoModifier, point);
    QApplication::processEvents();
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_EQ(parentOpen(), GetParam() == Surface::Inline);
}

TEST_P(CommandSubmenuTest, Contract_ReplacementAndRemovalKeepMenusBorrowed)
{
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    FluentMenu replacement(QStringLiteral("Replacement"));
    replacement.addAction(QStringLiteral("New choice"));
    branch.setMenu(&replacement);
    EXPECT_FALSE(menu.isVisible());
    ASSERT_NE(presenter(), nullptr);
    presenter()->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return replacement.isVisible(); }));
    EXPECT_EQ(replacement.parent(), nullptr);
    if (flyout)
        EXPECT_TRUE(flyout->removeCommandAction(&branch));
    else
        EXPECT_TRUE(bar->removeCommandAction(&branch));
    QApplication::processEvents();
    EXPECT_FALSE(replacement.isVisible());
    EXPECT_EQ(replacement.parent(), nullptr);
    EXPECT_EQ(branch.parent(), nullptr);
}

TEST_P(CommandSubmenuTest, Contract_LeafCanDestroyTheSurface)
{
    QWidget* surface = flyout ? static_cast<QWidget*>(flyout.data()) : static_cast<QWidget*>(bar.data());
    QPointer<QWidget> guard(surface);
    QObject::connect(leaf, &QAction::triggered, &window, [surface] { delete surface; });
    QSignalSpy triggered(leaf, &QAction::triggered);
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    openNestedMenus();
    ASSERT_FALSE(HasFatalFailure());
    third.setActiveAction(leaf);
    QTest::keyClick(&third, Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_FALSE(menu.isVisible());
}

TEST_P(CommandSubmenuTest, Contract_AccessibilityReportsSubmenuAvailabilityAndExpansion)
{
    auto* button = presenter();
    ASSERT_NE(button, nullptr);
    auto* accessible = QAccessible::queryAccessibleInterface(button);
    ASSERT_NE(accessible, nullptr);
    const bool primary = GetParam() == Surface::Inline || GetParam() == Surface::FlyoutPrimary;
    EXPECT_EQ(accessible->role(), primary ? QAccessible::ButtonMenu : QAccessible::MenuItem);
    EXPECT_TRUE(accessible->state().hasPopup);
    EXPECT_TRUE(accessible->state().collapsed);
    ASSERT_NE(accessible->actionInterface(), nullptr);
    EXPECT_TRUE(accessible->actionInterface()->actionNames().contains(QAccessibleActionInterface::showMenuAction()));
    accessible->actionInterface()->doAction(QAccessibleActionInterface::showMenuAction());
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    EXPECT_TRUE(accessible->state().expanded);
    QTest::keyClick(&menu, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible(); }));
    QApplication::processEvents();
    EXPECT_TRUE(accessible->state().collapsed);
    branch.setMenu(nullptr);
    QApplication::processEvents();
    EXPECT_FALSE(accessible->state().hasPopup);
    EXPECT_EQ(accessible->role(), primary ? QAccessible::Button : QAccessible::MenuItem);
}

TEST_P(CommandSubmenuTest, Contract_RtlForwardAndBackKeysReturnToPresenter)
{
    window.setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();
    auto* button = presenter();
    ASSERT_NE(button, nullptr);
    button->setFocus();
    const bool primary = GetParam() == Surface::Inline || GetParam() == Surface::FlyoutPrimary;
    QTest::keyClick(button, primary ? Qt::Key_Down : Qt::Key_Left);
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    EXPECT_EQ(menu.layoutDirection(), Qt::RightToLeft);
    menu.setActiveAction(second.menuAction());
    QTest::keyClick(&menu, Qt::Key_Left);
    ASSERT_TRUE(QTest::qWaitFor([&] { return second.isVisible(); }));
    QTest::keyClick(&second, Qt::Key_Right);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !second.isVisible(); }));
    EXPECT_TRUE(menu.isVisible());
    QTest::keyClick(&menu, Qt::Key_Right);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible() && button->hasFocus(); }));
    EXPECT_TRUE(parentOpen());
}

TEST_P(CommandSubmenuTest, Contract_LazyMenuKeepsExclusiveActionState)
{
    menu.clear();
    QActionGroup choices(&menu);
    QAction* ascending = nullptr;
    QAction* descending = nullptr;
    QObject::connect(&menu, &QMenu::aboutToShow, &menu, [&] {
        ascending = menu.addAction(QStringLiteral("Ascending"));
        descending = menu.addAction(QStringLiteral("Descending"));
        for (QAction* action : {ascending, descending}) {
            action->setCheckable(true);
            choices.addAction(action);
        }
        ascending->setChecked(true);
    });
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_NE(descending, nullptr);
    QSignalSpy triggered(descending, &QAction::triggered);
    menu.setActiveAction(descending);
    QTest::keyClick(&menu, Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_TRUE(descending->isChecked());
    EXPECT_FALSE(ascending->isChecked());
}

TEST_P(CommandSubmenuTest, Contract_DisableOrDestroyMenuClosesWithoutTakingOwnership)
{
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    branch.setEnabled(false);
    EXPECT_FALSE(menu.isVisible());
    branch.setEnabled(true);
    auto* replacement = new FluentMenu(QStringLiteral("Temporary"));
    replacement->addAction(QStringLiteral("Choice"));
    branch.setMenu(replacement);
    ASSERT_NE(presenter(), nullptr);
    presenter()->click();
    ASSERT_TRUE(QTest::qWaitFor([&] { return replacement->isVisible(); }));
    delete replacement;
    QApplication::processEvents();
    EXPECT_EQ(branch.menu(), nullptr);
    QSignalSpy triggered(&branch, &QAction::triggered);
    ASSERT_NE(presenter(), nullptr);
    presenter()->click();
    EXPECT_EQ(triggered.count(), 1);
}

TEST_P(CommandSubmenuTest, Contract_OutsidePressClosesCascadeAndParent)
{
    const QPoint previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([previousCursor] { QCursor::setPos(previousCursor); });
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    openNestedMenus();
    ASSERT_FALSE(HasFatalFailure());
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(880, 630));
    QApplication::processEvents();
    EXPECT_EQ(parentOpen(), GetParam() == Surface::Inline);
    EXPECT_FALSE(menu.isVisible());
    EXPECT_FALSE(second.isVisible());
    EXPECT_FALSE(third.isVisible());
}

TEST_P(CommandSubmenuTest, Contract_WindowPointerClicksToggleWithoutReopening)
{
    const QPoint previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([previousCursor] { QCursor::setPos(previousCursor); });
    auto* button = presenter();
    ASSERT_NE(button, nullptr);
    const QPoint position = window.mapFromGlobal(button->mapToGlobal(button->rect().center()));
    QSignalSpy shown(&menu, &QMenu::aboutToShow);
    QSignalSpy hidden(&menu, &QMenu::aboutToHide);
    QSignalSpy triggered(&branch, &QAction::triggered);

    // Route through the window so QMenu's popup grab and mouse replay run,
    // instead of delivering directly to the command's private presenter.
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, position);
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::qWait(300);
    EXPECT_FALSE(menu.isVisible());
    EXPECT_EQ(shown.count(), 1);
    EXPECT_EQ(hidden.count(), 1);
    EXPECT_TRUE(parentOpen());
    EXPECT_EQ(triggered.count(), 0);

    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, position);
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    EXPECT_EQ(shown.count(), 2);
}

TEST_P(CommandSubmenuTest, Contract_FocusOutlineFollowsKeyboardInsteadOfPointerInput)
{
    const QPoint previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([previousCursor] { QCursor::setPos(previousCursor); });
    auto* button = qobject_cast<fluent::basicinput::Button*>(presenter());
    ASSERT_NE(button, nullptr);
    anchor.setFocus(Qt::OtherFocusReason);
    QTest::mouseClick(button, Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return menu.isVisible(); }));
    EXPECT_FALSE(button->hasFocusVisual());
    QTest::keyClick(&menu, Qt::Key_Escape);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !menu.isVisible() && button->hasFocus(); }));
    EXPECT_TRUE(button->hasFocusVisual());

    // A pointer press must remove the outline even if focus never changes.
    QTest::mousePress(button, Qt::LeftButton);
    EXPECT_TRUE(button->hasFocus());
    EXPECT_FALSE(button->hasFocusVisual());
    const QImage pointerImage = button->grab().toImage();
    button->setFocusVisual(true);
    EXPECT_NE(button->grab().toImage(), pointerImage);
    button->setFocusVisual(false);
    QTest::mouseRelease(button, Qt::LeftButton);
    menu.hide();
    QApplication::processEvents();

    // Composite navigation can consume Home without changing the focused row.
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    button->setFocus(Qt::MouseFocusReason);
    QTest::keyClick(button, Qt::Key_Home);
    EXPECT_TRUE(button->hasFocus());
    EXPECT_TRUE(button->hasFocusVisual());
    anchor.setFocus(Qt::OtherFocusReason);
    button->setFocus(Qt::TabFocusReason);
    EXPECT_TRUE(button->hasFocusVisual());
}

TEST_P(CommandSubmenuTest, Contract_LeafMayDetachItsOriginMenu)
{
    QObject::connect(leaf, &QAction::triggered, &window, [&] { branch.setMenu(nullptr); });
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    openNestedMenus();
    ASSERT_FALSE(HasFatalFailure());
    third.setActiveAction(leaf);
    QTest::keyClick(&third, Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_EQ(branch.menu(), nullptr);
    EXPECT_FALSE(menu.isVisible());
    EXPECT_EQ(parentOpen(), GetParam() == Surface::Inline);
}

TEST_P(CommandSubmenuTest, Contract_LeafFocusRedirectIsPreserved)
{
    fluent::basicinput::Button destination(QStringLiteral("Destination"), &window);
    destination.setFocusPolicy(Qt::StrongFocus);
    destination.setGeometry(500, 540, 150, 40);
    destination.show();
    QObject::connect(leaf, &QAction::triggered, &destination, [&] { destination.setFocus(); });
    openMenu();
    ASSERT_FALSE(HasFatalFailure());
    openNestedMenus();
    ASSERT_FALSE(HasFatalFailure());
    third.setActiveAction(leaf);
    QTest::keyClick(&third, Qt::Key_Return);
    QApplication::processEvents();
    // The offscreen backend can temporarily have no active top-level after a
    // popup closes. The owning window still records the selected focus target.
    EXPECT_EQ(window.focusWidget(), &destination);
    if (window.isActiveWindow())
        EXPECT_TRUE(destination.hasFocus());
    EXPECT_EQ(parentOpen(), GetParam() == Surface::Inline);
}

INSTANTIATE_TEST_SUITE_P(CommandSurfaces,
    CommandSubmenuTest,
    testing::Values(Surface::Inline,
        Surface::Overflow,
        Surface::Secondary,
        Surface::FlyoutPrimary,
        Surface::FlyoutSecondary));

} // namespace
