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
        QCursor::setPos(window.mapToGlobal(QPoint(780, 480)));
        const auto open = [&] {
            if (useFlyout)
                flyout.showAt(&bar);
            else
                bar.setOverflowOpen(true);
            QApplication::processEvents();
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
                const QPoint point = scroll->widget()->mapTo(surface,
                    QPoint(scroll->widget()->width() - 8, scroll->widget()->height() - 8));
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
