#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QLineEdit>
#include <QMenu>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "components/basicinput/Button.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/EditingCommandRouter.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TextEdit.h"

using fluent::basicinput::Button;
using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandBarFlyout;
using fluent::menus_toolbars::FluentMenu;
using fluent::textfields::AutoSuggestBox;
using fluent::textfields::EditingCommandRouter;
using fluent::textfields::LineEdit;
using fluent::textfields::NumberBox;
using fluent::textfields::PasswordBox;
using fluent::textfields::TextEdit;

namespace {

using Command = EditingCommandRouter::Command;

QAbstractButton* commandButton(
    QWidget* root,
    const QString& objectName,
    const QString& accessibleName)
{
    const QList<QAbstractButton*> buttons =
        root->findChildren<QAbstractButton*>(objectName);
    for (QAbstractButton* button : buttons) {
        if (button->accessibleName() == accessibleName)
            return button;
    }
    return nullptr;
}

class EditingCommandRouterTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<Command>(
            "fluent::textfields::EditingCommandRouter::Command");
    }

    void SetUp() override
    {
        QApplication::clipboard()->clear();
        window = new QWidget();
        window->setFixedSize(640, 420);
        window->setWindowTitle(
            QStringLiteral("EditingCommandRouter Test"));
    }

    void TearDown() override
    {
        QApplication::clipboard()->clear();
        delete secondaryWindow;
        secondaryWindow = nullptr;
        delete window;
        window = nullptr;
        QApplication::processEvents();
    }

    template<typename T>
    T* addEditor(int y = 20)
    {
        auto* editor = new T(window);
        editor->setGeometry(20, y, 280, editor->sizeHint().height());
        return editor;
    }

    void showAndFocus(QWidget* editor)
    {
        window->show();
        editor->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
        ASSERT_EQ(QApplication::focusWidget(), editor);
    }

    QWidget* window = nullptr;
    QWidget* secondaryWindow = nullptr;
};

} // namespace

TEST_F(EditingCommandRouterTest, Contract_ActionsAreStableAndWindowScoped)
{
    EditingCommandRouter router(window);

    EXPECT_EQ(router.scopeWindow(), window);
    EXPECT_FALSE(router.hasActiveTarget());
    ASSERT_EQ(router.actions().size(), 7);

    const QList<Command> commands = {
        Command::Undo,
        Command::Redo,
        Command::Cut,
        Command::Copy,
        Command::Paste,
        Command::Delete,
        Command::SelectAll,
    };
    for (Command command : commands) {
        QAction* action = router.action(command);
        ASSERT_NE(action, nullptr);
        EXPECT_EQ(action, router.action(command));
        EXPECT_EQ(action->parent(), &router);
        EXPECT_FALSE(action->objectName().isEmpty());
        EXPECT_FALSE(action->text().isEmpty());
        EXPECT_FALSE(action->shortcuts().isEmpty());
        EXPECT_FALSE(action->isEnabled());
    }

    QAction* copy = router.action(Command::Copy);
    copy->setText(QStringLiteral("Caller Copy"));
    QEvent languageChange(QEvent::LanguageChange);
    QApplication::sendEvent(window, &languageChange);
    EXPECT_EQ(copy->text(), QStringLiteral("Caller Copy"));
}

TEST_F(EditingCommandRouterTest, Contract_DuplicateWindowRouterRemainsInactive)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha"));

    auto* primary = new EditingCommandRouter(window, window);
    auto* duplicate = new EditingCommandRouter(window, window);

    ASSERT_EQ(primary->actions().size(), 7);
    ASSERT_EQ(duplicate->actions().size(), 7);
    for (QAction* action : duplicate->actions()) {
        ASSERT_NE(action, nullptr);
        EXPECT_FALSE(action->isEnabled());
        EXPECT_TRUE(action->shortcuts().isEmpty());
        EXPECT_FALSE(window->actions().contains(action));
    }

    showAndFocus(edit);
    edit->selectAll();
    QApplication::processEvents();

    EXPECT_TRUE(primary->hasActiveTarget());
    EXPECT_TRUE(primary->canExecute(Command::Copy));
    EXPECT_FALSE(duplicate->hasActiveTarget());
    EXPECT_FALSE(duplicate->canExecute(Command::Copy));
    EXPECT_FALSE(duplicate->execute(Command::Copy));
    EXPECT_TRUE(QApplication::clipboard()->text().isEmpty());

    delete primary;
    auto* replacement = new EditingCommandRouter(window, window);
    for (QAction* action : replacement->actions()) {
        ASSERT_NE(action, nullptr);
        EXPECT_FALSE(action->shortcuts().isEmpty());
        EXPECT_TRUE(window->actions().contains(action));
    }
    EXPECT_TRUE(replacement->hasActiveTarget());
    EXPECT_TRUE(replacement->canExecute(Command::Copy));

    delete duplicate;
    EXPECT_TRUE(replacement->hasActiveTarget());
    EXPECT_TRUE(replacement->canExecute(Command::Copy));
}

TEST_F(EditingCommandRouterTest, Contract_LineEditCommandsTrackAndMutateState)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha"));
    EditingCommandRouter router(window);
    showAndFocus(edit);

    edit->setCursorPosition(edit->text().size());
    edit->insert(QStringLiteral(" Beta"));
    ASSERT_EQ(edit->text(), QStringLiteral("Alpha Beta"));
    EXPECT_TRUE(router.canExecute(Command::Undo));

    EXPECT_TRUE(router.execute(Command::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    EXPECT_TRUE(router.canExecute(Command::Redo));
    EXPECT_TRUE(router.execute(Command::Redo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha Beta"));

    QApplication::clipboard()->setText(QStringLiteral("Gamma"));
    edit->setSelection(0, 5);
    EXPECT_TRUE(router.canExecute(Command::Cut));
    EXPECT_TRUE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.canExecute(Command::Paste));
    EXPECT_TRUE(router.canExecute(Command::Delete));
    EXPECT_TRUE(router.canExecute(Command::SelectAll));

    EXPECT_TRUE(router.execute(Command::Copy));
    EXPECT_EQ(
        QApplication::clipboard()->text(),
        QStringLiteral("Alpha"));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha Beta"));

    EXPECT_TRUE(router.execute(Command::Delete));
    EXPECT_EQ(edit->text(), QStringLiteral(" Beta"));
    EXPECT_TRUE(router.execute(Command::Paste));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha Beta"));

    EXPECT_TRUE(router.execute(Command::SelectAll));
    EXPECT_TRUE(edit->hasSelectedText());
    EXPECT_FALSE(router.canExecute(Command::SelectAll));
}

TEST_F(EditingCommandRouterTest, Contract_NativeShortcutsUseTheActiveEditor)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha"));
    EditingCommandRouter router(window);
    showAndFocus(edit);

    edit->setCursorPosition(edit->text().size());
    edit->insert(QStringLiteral(" Beta"));
    ASSERT_TRUE(router.canExecute(Command::Undo));

    QTest::keySequence(edit, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha"));
    QTest::keySequence(edit, QKeySequence(QKeySequence::Redo));
    EXPECT_EQ(edit->text(), QStringLiteral("Alpha Beta"));
}

TEST_F(EditingCommandRouterTest, Contract_ReadOnlyDisabledAndClipboardStateRefresh)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha Beta"));
    edit->setSelection(0, 5);
    QApplication::clipboard()->setText(QStringLiteral("Gamma"));
    EditingCommandRouter router(window);
    showAndFocus(edit);

    EXPECT_TRUE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.canExecute(Command::Cut));
    EXPECT_TRUE(router.canExecute(Command::Paste));
    EXPECT_TRUE(router.canExecute(Command::Delete));

    edit->setReadOnly(true);
    QApplication::processEvents();
    EXPECT_TRUE(router.canExecute(Command::Copy));
    EXPECT_FALSE(router.canExecute(Command::Cut));
    EXPECT_FALSE(router.canExecute(Command::Paste));
    EXPECT_FALSE(router.canExecute(Command::Delete));

    edit->setEnabled(false);
    QApplication::processEvents();
    EXPECT_FALSE(router.canExecute(Command::Copy));
    EXPECT_FALSE(router.canExecute(Command::SelectAll));

    edit->setEnabled(true);
    edit->setReadOnly(false);
    edit->setFocus();
    QApplication::clipboard()->clear();
    QApplication::processEvents();
    EXPECT_FALSE(router.canExecute(Command::Paste));
}

TEST_F(EditingCommandRouterTest, Contract_UnsupportedAndOtherWindowFocusClearTarget)
{
    auto* fluentEdit = addEditor<LineEdit>();
    auto* rawEdit = new QLineEdit(window);
    rawEdit->setGeometry(20, 80, 280, 32);
    EditingCommandRouter router(window);
    QSignalSpy targetSpy(
        &router, &EditingCommandRouter::activeTargetChanged);

    showAndFocus(fluentEdit);
    EXPECT_TRUE(router.hasActiveTarget());

    rawEdit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    EXPECT_FALSE(router.hasActiveTarget());

    rawEdit->setText(QStringLiteral("Raw"));
    rawEdit->setCursorPosition(rawEdit->text().size());
    rawEdit->insert(QStringLiteral(" edit"));
    ASSERT_EQ(rawEdit->text(), QStringLiteral("Raw edit"));
    QTest::keySequence(rawEdit, QKeySequence(QKeySequence::Undo));
    EXPECT_EQ(rawEdit->text(), QStringLiteral("Raw"));

    secondaryWindow = new QWidget();
    secondaryWindow->setFixedSize(320, 180);
    auto* otherEdit = new LineEdit(secondaryWindow);
    otherEdit->setGeometry(20, 20, 240, 32);
    EditingCommandRouter otherRouter(secondaryWindow);
    secondaryWindow->show();
    otherEdit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    EXPECT_FALSE(router.hasActiveTarget());
    EXPECT_TRUE(otherRouter.hasActiveTarget());
    EXPECT_NE(
        router.action(Command::Copy),
        otherRouter.action(Command::Copy));
    EXPECT_GE(targetSpy.count(), 2);
}

TEST_F(EditingCommandRouterTest, Contract_SignalsEmitOnlyForRealStateChanges)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha"));
    EditingCommandRouter router(window);
    QSignalSpy targetSpy(
        &router, &EditingCommandRouter::activeTargetChanged);
    QSignalSpy capabilitySpy(
        &router, &EditingCommandRouter::commandCapabilityChanged);

    showAndFocus(edit);
    ASSERT_EQ(targetSpy.count(), 1);
    EXPECT_TRUE(targetSpy.at(0).at(0).toBool());

    capabilitySpy.clear();
    edit->setSelection(0, 1);
    ASSERT_TRUE(router.canExecute(Command::Copy));

    bool sawCopyEnabled = false;
    for (const QList<QVariant>& emission : capabilitySpy) {
        const auto command =
            emission.at(0).value<Command>();
        if (command == Command::Copy && emission.at(1).toBool())
            sawCopyEnabled = true;
    }
    EXPECT_TRUE(sawCopyEnabled);

    const int stableCapabilityCount = capabilitySpy.count();
    router.refresh();
    router.refresh();
    EXPECT_EQ(capabilitySpy.count(), stableCapabilityCount);

    edit->deselect();
    EXPECT_FALSE(router.canExecute(Command::Copy));
    bool sawCopyDisabled = false;
    for (const QList<QVariant>& emission : capabilitySpy) {
        const auto command =
            emission.at(0).value<Command>();
        if (command == Command::Copy && !emission.at(1).toBool())
            sawCopyDisabled = true;
    }
    EXPECT_TRUE(sawCopyDisabled);
}

TEST_F(EditingCommandRouterTest, Contract_SupportedTargetSwitchRoutesToNewEditor)
{
    auto* first = addEditor<LineEdit>();
    auto* second = addEditor<LineEdit>(80);
    first->setText(QStringLiteral("First"));
    second->setText(QStringLiteral("Second"));
    EditingCommandRouter router(window);
    QSignalSpy targetSpy(
        &router, &EditingCommandRouter::activeTargetChanged);

    showAndFocus(first);
    first->setSelection(0, first->text().size());
    ASSERT_TRUE(router.canExecute(Command::Copy));

    second->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_EQ(QApplication::focusWidget(), second);
    EXPECT_TRUE(router.hasActiveTarget());
    EXPECT_FALSE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.execute(Command::SelectAll));
    EXPECT_TRUE(second->hasSelectedText());
    EXPECT_EQ(first->text(), QStringLiteral("First"));

    ASSERT_EQ(targetSpy.count(), 2);
    EXPECT_TRUE(targetSpy.at(0).at(0).toBool());
    EXPECT_TRUE(targetSpy.at(1).at(0).toBool());
}

TEST_F(EditingCommandRouterTest, Contract_ScopeDestructionLeavesSafeDisabledRouter)
{
    QObject owner;
    auto* router = new EditingCommandRouter(window, &owner);
    auto* copy = router->action(Command::Copy);
    ASSERT_NE(copy, nullptr);

    delete window;
    window = nullptr;
    QApplication::processEvents();

    EXPECT_EQ(router->scopeWindow(), nullptr);
    EXPECT_FALSE(router->hasActiveTarget());
    for (QAction* action : router->actions()) {
        ASSERT_NE(action, nullptr);
        EXPECT_FALSE(action->isEnabled());
    }
    EXPECT_FALSE(router->execute(Command::Copy));
}

TEST_F(EditingCommandRouterTest, Contract_TextEditRoutesWithoutPublicInnerEditor)
{
    auto* edit = addEditor<TextEdit>();
    edit->resize(300, edit->sizeHint().height());
    edit->setPlainText(QStringLiteral("Alpha"));
    EditingCommandRouter router(window);

    window->show();
    edit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    auto* inner =
        qobject_cast<QTextEdit*>(QApplication::focusWidget());
    ASSERT_NE(inner, nullptr);
    ASSERT_TRUE(router.hasActiveTarget());

    inner->moveCursor(QTextCursor::End);
    inner->insertPlainText(QStringLiteral(" Beta"));
    ASSERT_EQ(edit->toPlainText(), QStringLiteral("Alpha Beta"));
    ASSERT_TRUE(router.canExecute(Command::Undo));

    EXPECT_TRUE(router.execute(Command::Undo));
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("Alpha"));
    EXPECT_TRUE(router.execute(Command::Redo));
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("Alpha Beta"));

    QTextCursor cursor = inner->textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    inner->setTextCursor(cursor);
    EXPECT_TRUE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.canExecute(Command::Delete));
    EXPECT_TRUE(router.execute(Command::Delete));
    EXPECT_EQ(edit->toPlainText(), QStringLiteral(" Beta"));
}

TEST_F(EditingCommandRouterTest, Contract_PasswordPeekNeverExportsText)
{
    auto* box = addEditor<PasswordBox>();
    box->setPassword(QStringLiteral("secret"));
    EditingCommandRouter router(window);
    showAndFocus(box);

    box->setSelection(0, 1);
    QApplication::clipboard()->setText(QStringLiteral("sentinel"));
    EXPECT_FALSE(router.canExecute(Command::Cut));
    EXPECT_FALSE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.canExecute(Command::Delete));

    QTest::keySequence(box, QKeySequence(QKeySequence::Copy));
    EXPECT_EQ(
        QApplication::clipboard()->text(),
        QStringLiteral("sentinel"));

    auto* reveal =
        box->findChild<Button*>(QStringLiteral("PasswordBoxRevealButton"));
    ASSERT_NE(reveal, nullptr);
    QTest::mousePress(reveal, Qt::LeftButton);
    QApplication::processEvents();
    ASSERT_EQ(box->echoMode(), QLineEdit::Normal);

    box->setSelection(0, 1);
    QTest::keySequence(box, QKeySequence(QKeySequence::Copy));
    EXPECT_EQ(
        QApplication::clipboard()->text(),
        QStringLiteral("sentinel"));
    QTest::keySequence(box, QKeySequence(QKeySequence::Cut));
    EXPECT_EQ(box->password(), QStringLiteral("secret"));
    QTest::mouseRelease(reveal, Qt::LeftButton);

    box->setPasswordRevealMode(
        PasswordBox::PasswordRevealMode::Visible);
    box->setSelection(0, 1);
    EXPECT_TRUE(router.canExecute(Command::Cut));
    EXPECT_TRUE(router.canExecute(Command::Copy));
    EXPECT_TRUE(router.execute(Command::Copy));
    EXPECT_EQ(QApplication::clipboard()->text(), QStringLiteral("s"));
}

TEST_F(EditingCommandRouterTest, Contract_InheritedEditorsRouteThroughLineEdit)
{
    auto* number = addEditor<NumberBox>();
    auto* suggest = addEditor<AutoSuggestBox>(90);
    EditingCommandRouter router(window);

    number->setValue(5);
    showAndFocus(number);
    number->selectAll();
    QApplication::clipboard()->setText(QStringLiteral("42"));
    ASSERT_TRUE(router.canExecute(Command::Paste));
    EXPECT_TRUE(router.execute(Command::Paste));
    QTest::keyClick(number, Qt::Key_Return);
    QApplication::processEvents();
    EXPECT_DOUBLE_EQ(number->value(), 42.0);

    suggest->setText(QStringLiteral("query"));
    suggest->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    EXPECT_TRUE(router.hasActiveTarget());
    suggest->setSelection(0, 1);
    EXPECT_TRUE(router.canExecute(Command::Copy));
}

TEST_F(EditingCommandRouterTest, Contract_MenuActivationRetainsEditorTarget)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha Beta"));
    edit->setSelection(0, 5);
    EditingCommandRouter router(window);
    showAndFocus(edit);

    FluentMenu menu(QStringLiteral("Edit"), window);
    menu.addAction(router.action(Command::Cut));
    bool triggered = false;
    QTimer::singleShot(0, [&]() {
        EXPECT_TRUE(router.hasActiveTarget());
        QAction* cut = router.action(Command::Cut);
        if (!cut->isEnabled()) {
            menu.close();
            return;
        }
        cut->trigger();
        triggered = true;
        menu.close();
    });
    menu.exec(edit->mapToGlobal(edit->rect().bottomLeft()));
    QApplication::processEvents();

    EXPECT_TRUE(triggered);
    EXPECT_EQ(edit->text(), QStringLiteral(" Beta"));
    EXPECT_TRUE(router.hasActiveTarget());
    edit->selectAll();
    EXPECT_TRUE(router.canExecute(Command::Copy));
}

TEST_F(EditingCommandRouterTest,
       Contract_CommandSurfacesRetainAndRestoreEditorTarget)
{
    auto* edit = addEditor<LineEdit>();
    edit->setText(QStringLiteral("Alpha Beta"));
    edit->setSelection(0, 5);
    EditingCommandRouter router(window);

    auto* bar = new CommandBar(window);
    bar->setGeometry(20, 80, 420, 48);
    ASSERT_TRUE(
        bar->addSecondaryAction(router.action(Command::Cut)));
    auto* anchor = new Button(QStringLiteral("Flyout"), window);
    anchor->setGeometry(20, 150, 120, 40);
    auto* flyout = new CommandBarFlyout(window);
    flyout->setAnimationEnabled(false);
    ASSERT_TRUE(
        flyout->addPrimaryAction(
            router.action(Command::SelectAll)));

    showAndFocus(edit);
    edit->setSelection(0, 5);
    ASSERT_EQ(edit->selectedText(), QStringLiteral("Alpha"));
    ASSERT_TRUE(router.canExecute(Command::Cut));
    QSignalSpy cutSpy(
        router.action(Command::Cut), &QAction::triggered);
    bar->setOverflowOpen(true);
    QApplication::processEvents();
    ASSERT_TRUE(bar->isOverflowOpen());
    QAbstractButton* cutRow = commandButton(
        window,
        QStringLiteral("FluentCommandBar.OverflowRow"),
        router.action(Command::Cut)->text().remove(QLatin1Char('&')));
    ASSERT_NE(cutRow, nullptr);
    EXPECT_TRUE(router.hasActiveTarget());
    edit->setSelection(0, 5);
    router.refresh();
    ASSERT_EQ(edit->selectedText(), QStringLiteral("Alpha"));
    ASSERT_TRUE(router.canExecute(Command::Cut));
    QTest::mouseClick(cutRow, Qt::LeftButton);
    QApplication::processEvents();
    EXPECT_EQ(cutSpy.count(), 1);
    EXPECT_EQ(edit->text(), QStringLiteral(" Beta"));
    EXPECT_TRUE(router.hasActiveTarget());
    EXPECT_EQ(QApplication::focusWidget(), edit);

    edit->setText(QStringLiteral("Gamma Delta"));
    edit->setSelection(0, 5);
    edit->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    flyout->showAt(
        anchor, CommandBarFlyout::ShowMode::Standard);
    QApplication::processEvents();
    ASSERT_TRUE(flyout->isOpen());
    EXPECT_TRUE(router.hasActiveTarget());
    QAbstractButton* selectAll = commandButton(
        flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        router.action(Command::SelectAll)
            ->text()
            .remove(QLatin1Char('&')));
    ASSERT_NE(selectAll, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), selectAll);
    selectAll->click();
    QApplication::processEvents();
    EXPECT_FALSE(flyout->isOpen());
    EXPECT_TRUE(edit->hasSelectedText());
    EXPECT_TRUE(router.hasActiveTarget());
    EXPECT_EQ(QApplication::focusWidget(), edit);
}

TEST_F(EditingCommandRouterTest,
       Contract_WindowScopedActionsRejectCrossWindowSurfaces)
{
    EditingCommandRouter router(window);
    secondaryWindow = new QWidget();
    secondaryWindow->resize(360, 220);
    CommandBar otherBar(secondaryWindow);
    CommandBarFlyout otherFlyout(secondaryWindow);

    QAction* copy = router.action(Command::Copy);
    ASSERT_NE(copy, nullptr);
    EXPECT_FALSE(otherBar.addPrimaryAction(copy));
    EXPECT_FALSE(otherFlyout.addSecondaryAction(copy));
    EXPECT_TRUE(otherBar.primaryActions().isEmpty());
    EXPECT_TRUE(otherFlyout.secondaryActions().isEmpty());

    CommandBar localBar(window);
    CommandBarFlyout localFlyout(window);
    EXPECT_TRUE(localBar.addPrimaryAction(copy));
    EXPECT_TRUE(localFlyout.addSecondaryAction(copy));
}
