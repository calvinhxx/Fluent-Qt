#include <gtest/gtest.h>

#include <QAccessible>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTest>
#include <QWidgetAction>

#include "components/collections/ListView.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandPalette.h"
#include "components/textfields/EditingCommandRouter.h"
#include "components/textfields/LineEdit.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayScrim.h"

using fluent::collections::ListView;
using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandPalette;
using fluent::textfields::EditingCommandRouter;
using fluent::textfields::LineEdit;

TEST(CommandPaletteTest, Contract_AnimatedDismissalPreservesImmediateCommandDispatch)
{
    auto& motion = fluent::MotionPolicy::instance();
    const auto oldMode = motion.mode();
    const auto restore = qScopeGuard([&] { motion.setMode(oldMode); });
    motion.setMode(fluent::MotionPolicy::Mode::Full);
    QWidget host;
    host.resize(800, 600);
    LineEdit editor(&host);
    CommandPalette palette(&host);
    QAction action(QStringLiteral("Run"), &host);
    palette.addCommand(&action);
    host.show();
    host.activateWindow();
    editor.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return editor.hasFocus(); }));
    ASSERT_TRUE(palette.isExitAnimationEnabled());
    palette.open();
    ASSERT_TRUE(QTest::qWaitFor([&] { return palette.popupProgress() == 1.0; }));
    QSignalSpy closed(&palette, &CommandPalette::closed);
    QTest::keyClick(palette.findChild<LineEdit*>(), Qt::Key_Escape);
    EXPECT_FALSE(palette.isOpen());
    EXPECT_TRUE(palette.isVisible());
    EXPECT_EQ(closed.count(), 0);
    ASSERT_NE(host.findChild<fluent::overlay::OverlayScrim*>(), nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&] { return closed.count() == 1; }));
    EXPECT_TRUE(editor.hasFocus());
    bool dispatchedWithFocus = false;
    QObject::connect(&action, &QAction::triggered, &host, [&] {
        dispatchedWithFocus = editor.hasFocus() && !palette.isVisible();
    });
    palette.open();
    EXPECT_TRUE(palette.triggerCurrent());
    EXPECT_TRUE(dispatchedWithFocus);
    EXPECT_EQ(closed.count(), 2);
    EXPECT_TRUE(palette.isExitAnimationEnabled());
}

TEST(CommandPaletteTest, Contract_SearchGroupsKeywordsAndLiveActionState)
{
    QWidget host;
    CommandPalette palette(&host);
    QAction save(QStringLiteral("&Save && close"), &host);
    QAction copy(QStringLiteral("Copy"), &host);
    QAction exportFile(QStringLiteral("Export"), &host);
    save.setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    ASSERT_TRUE(palette.addCommand(&save, QStringLiteral("File"), {QStringLiteral("保存")}));
    ASSERT_TRUE(palette.addCommand(&copy, QStringLiteral("Edit")));
    ASSERT_TRUE(palette.addCommand(&exportFile, QStringLiteral("File")));
    EXPECT_EQ(palette.filteredCommands(), QList<QAction*>({&save, &exportFile, &copy}));
    palette.setQuery(QStringLiteral("  FILE 保存 "));
    EXPECT_EQ(palette.filteredCommands(), QList<QAction*>({&save}));
    palette.setQuery(QStringLiteral("SAVE &"));
    EXPECT_EQ(palette.currentAction(), &save);
    save.setEnabled(false);
    EXPECT_EQ(palette.resultCount(), 1);
    EXPECT_EQ(palette.currentAction(), nullptr);
    save.setEnabled(true);
    EXPECT_EQ(palette.currentAction(), &save);
    save.setVisible(false);
    EXPECT_EQ(palette.resultCount(), 0);
    save.setVisible(true);
    save.setText(QStringLiteral("Publish"));
    EXPECT_EQ(palette.resultCount(), 0);
    palette.setQuery(QStringLiteral("publish"));
    EXPECT_EQ(palette.currentAction(), &save);
    auto* view = palette.findChild<ListView*>();
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->model()->index(0, 0).data(Qt::UserRole).toString(),
        save.shortcut().toString(QKeySequence::NativeText));
}

TEST(CommandPaletteTest, Contract_BorrowingRemovalAndDestroyedActions)
{
    QWidget host;
    CommandPalette palette(&host);
    auto* action = new QAction(QStringLiteral("Open"), &host);
    ASSERT_TRUE(palette.addCommand(action));
    EXPECT_EQ(action->parent(), &host);
    EXPECT_TRUE(palette.addCommand(action, QStringLiteral("File")));
    EXPECT_EQ(palette.commands().size(), 1);
    palette.setQuery(QStringLiteral("file"));
    EXPECT_EQ(palette.resultCount(), 1);
    delete action;
    EXPECT_TRUE(palette.commands().isEmpty());
    EXPECT_EQ(palette.currentAction(), nullptr);
    ASSERT_TRUE(QTest::qWaitFor([&]() { return palette.resultCount() == 0; }));
    QAction retained(QStringLiteral("Retained"), &host);
    palette.addCommand(&retained);
    EXPECT_TRUE(palette.removeCommand(&retained));
    EXPECT_FALSE(palette.removeCommand(&retained));
    palette.addCommand(&retained);
    palette.clearCommands();
    EXPECT_EQ(retained.parent(), &host);
    EXPECT_TRUE(palette.actions().isEmpty());
}

TEST(CommandPaletteTest, Contract_RejectsNonLeafCommandsAndSuppressesMutatedBranches)
{
    CommandPalette palette;
    QAction separator;
    separator.setSeparator(true);
    QMenu menu;
    QWidgetAction custom(nullptr);
    EXPECT_FALSE(palette.addCommand(nullptr));
    EXPECT_FALSE(palette.addCommand(&separator));
    EXPECT_FALSE(palette.addCommand(menu.menuAction()));
    EXPECT_FALSE(palette.addCommand(&custom));
    QAction action(QStringLiteral("Open"), nullptr);
    palette.addCommand(&action);
    action.setMenu(&menu);
    EXPECT_EQ(palette.resultCount(), 0);
    action.setMenu(nullptr);
    EXPECT_EQ(palette.resultCount(), 1);
}

TEST(CommandPaletteTest, Contract_PropertiesDoNotEmitDuplicateChanges)
{
    CommandPalette palette;
    QSignalSpy query(&palette, &CommandPalette::queryChanged);
    QSignalSpy placeholder(&palette, &CommandPalette::placeholderTextChanged);
    QSignalSpy empty(&palette, &CommandPalette::emptyTextChanged);
    palette.setQuery(QStringLiteral("settings"));
    palette.setQuery(QStringLiteral("settings"));
    palette.setPlaceholderText(QStringLiteral("Search workspace commands"));
    palette.setPlaceholderText(QStringLiteral("Search workspace commands"));
    palette.setEmptyText(QStringLiteral("Nothing found"));
    palette.setEmptyText(QStringLiteral("Nothing found"));
    EXPECT_EQ(query.count(), 1);
    EXPECT_EQ(placeholder.count(), 1);
    EXPECT_EQ(empty.count(), 1);
    EXPECT_FALSE(palette.triggerCurrent());
}

TEST(CommandPaletteTest, Contract_KeyboardActivationSharesCheckStateAndRestoresFocus)
{
    QWidget host;
    host.resize(800, 600);
    LineEdit editor(&host);
    CommandPalette palette(&host);
    palette.setAnimationEnabled(false);
    QAction first(QStringLiteral("Unavailable"), &host);
    first.setEnabled(false);
    QAction second(QStringLiteral("Details"), &host);
    second.setCheckable(true);
    QAction third(QStringLiteral("Refresh"), &host);
    CommandBar bar(&host);
    bar.addPrimaryAction(&second);
    palette.addCommand(&first);
    palette.addCommand(&second);
    palette.addCommand(&third);
    QSignalSpy triggered(&second, &QAction::triggered);
    host.show();
    host.activateWindow();
    editor.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return editor.hasFocus(); }));
    palette.open();
    auto* search = palette.findChild<LineEdit*>();
    ASSERT_NE(search, nullptr);
    EXPECT_TRUE(search->hasFocus());
    EXPECT_EQ(palette.currentAction(), &second);
    QTest::keyClick(search, Qt::Key_Down);
    EXPECT_EQ(palette.currentAction(), &third);
    QTest::keyClick(search, Qt::Key_Down);
    EXPECT_EQ(palette.currentAction(), &second);
    QTest::keyClick(search, Qt::Key_Return);
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_TRUE(second.isChecked());
    EXPECT_FALSE(palette.isOpen());
    EXPECT_TRUE(editor.hasFocus());
    EXPECT_EQ(bar.primaryActions().first(), &second);
    palette.open();
    QTest::keyClick(search, Qt::Key_Escape);
    EXPECT_FALSE(palette.isOpen());
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_TRUE(editor.hasFocus());
    palette.open();
    QTest::keyClick(search, Qt::Key_Tab);
    EXPECT_TRUE(palette.findChild<ListView*>()->hasFocus());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    EXPECT_TRUE(search->hasFocus());
    QTest::keyClick(search, Qt::Key_Backtab, Qt::ShiftModifier);
    EXPECT_TRUE(palette.findChild<ListView*>()->hasFocus());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    EXPECT_FALSE(palette.isOpen());
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_TRUE(editor.hasFocus());
}

TEST(CommandPaletteTest, Contract_SearchDoesNotRetargetEditingCommands)
{
    QWidget host;
    host.resize(800, 600);
    LineEdit editor(&host);
    editor.setText(QStringLiteral("Original selection"));
    EditingCommandRouter router(&host);
    auto* copy = router.action(EditingCommandRouter::Command::Copy);
    copy->setText(QStringLiteral("Copy"));
    CommandPalette palette(&host);
    palette.setAnimationEnabled(false);
    palette.addCommand(copy, QStringLiteral("Edit"));
    host.show();
    host.activateWindow();
    editor.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return editor.hasFocus(); }));
    editor.setSelection(0, 8);
    router.refresh();
    ASSERT_TRUE(copy->isEnabled());
    palette.open();
    auto* search = palette.findChild<LineEdit*>();
    ASSERT_TRUE(search->hasFocus());
    QTest::keyClicks(search, "copy");
    EXPECT_TRUE(copy->isEnabled());
    EXPECT_EQ(editor.selectedText(), QStringLiteral("Original"));
    QSignalSpy triggered(copy, &QAction::triggered);
    QTest::keyClick(search, Qt::Key_Return);
    EXPECT_EQ(triggered.count(), 1);
    EXPECT_EQ(QApplication::clipboard()->text(), QStringLiteral("Original"));
    EXPECT_EQ(editor.selectedText(), QStringLiteral("Original"));
    EXPECT_TRUE(editor.hasFocus());
}

TEST(CommandPaletteTest, Contract_CloseHandlersCanInvalidateOrDestroyCommands)
{
    QWidget host;
    host.resize(800, 600);
    CommandPalette palette(&host);
    palette.setAnimationEnabled(false);
    QAction action(QStringLiteral("Save"), &host);
    palette.addCommand(&action);
    host.show();
    QSignalSpy triggered(&action, &QAction::triggered);
    auto connection = QObject::connect(&palette, &CommandPalette::closed, &host, [&]() { action.setEnabled(false); });
    palette.open();
    EXPECT_FALSE(palette.triggerCurrent());
    EXPECT_EQ(triggered.count(), 0);
    QObject::disconnect(connection);
    action.setEnabled(true);
    QPointer<CommandPalette> transient = new CommandPalette(&host);
    transient->setAnimationEnabled(false);
    transient->addCommand(&action);
    QObject::connect(transient, &CommandPalette::closed, &host, [&]() { delete transient.data(); });
    transient->open();
    EXPECT_FALSE(transient->triggerCurrent());
    EXPECT_TRUE(transient.isNull());
    EXPECT_EQ(triggered.count(), 0);
}

TEST(CommandPaletteTest, Contract_ObserversCanDestroyPaletteDuringRegistrationAndSearch)
{
    QWidget host;
    QAction action(QStringLiteral("Open"), &host);
    QPointer<CommandPalette> palette = new CommandPalette(&host);
    QObject::connect(palette, &CommandPalette::currentActionChanged, &host, [&]() { delete palette.data(); });
    EXPECT_FALSE(palette->addCommand(&action));
    EXPECT_TRUE(palette.isNull());
    palette = new CommandPalette(&host);
    palette->addCommand(&action);
    QObject::connect(palette, &CommandPalette::currentActionChanged, &host, [&]() { delete palette.data(); });
    palette->setQuery(QStringLiteral("No match"));
    EXPECT_TRUE(palette.isNull());
}

TEST(CommandPaletteTest, Contract_AccessibilityUsesNativeSearchAndModelRows)
{
    QWidget host;
    host.resize(800, 600);
    CommandPalette palette(&host);
    palette.setAccessibleName(QStringLiteral("Workspace commands"));
    palette.setAnimationEnabled(false);
    QAction action(QStringLiteral("Show details"), &host);
    action.setCheckable(true);
    action.setChecked(true);
    palette.addCommand(&action, QStringLiteral("View"));
    host.show();
    palette.open();
    auto* search = palette.findChild<LineEdit*>();
    auto* view = palette.findChild<ListView*>();
    ASSERT_NE(QAccessible::queryAccessibleInterface(search), nullptr);
    EXPECT_EQ(QAccessible::queryAccessibleInterface(search)->role(), QAccessible::EditableText);
    auto* list = QAccessible::queryAccessibleInterface(view);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->role(), QAccessible::List);
    ASSERT_NE(list->tableInterface(), nullptr);
    auto* row = list->tableInterface()->cellAt(0, 0);
    ASSERT_NE(row, nullptr);
    EXPECT_TRUE(row->text(QAccessible::Name).contains(QStringLiteral("Show details")));
    EXPECT_TRUE(row->state().checked);
    EXPECT_EQ(palette.accessibleName(), QStringLiteral("Workspace commands"));
}

TEST(CommandPaletteTest, Contract_LightDarkNarrowRtlAndHostResize)
{
    for (auto theme : {fluent::FluentElement::Theme::Light, fluent::FluentElement::Theme::Dark}) {
        QWidget host;
        host.resize(800, 600);
        CommandPalette palette(&host);
        palette.setTheme(theme);
        palette.setAnimationEnabled(false);
        QAction action(QStringLiteral("A long workspace command with a shortcut"), &host);
        action.setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
        palette.addCommand(&action, QStringLiteral("Workspace"));
        host.show();
        palette.open();
        EXPECT_EQ(palette.parentWidget(), &host);
        EXPECT_FALSE(palette.isWindow());
        EXPECT_TRUE(host.rect().contains(palette.geometry()));
        EXPECT_FALSE(palette.grab().isNull());
        host.setLayoutDirection(Qt::RightToLeft);
        host.resize(320, 280);
        QApplication::processEvents();
        EXPECT_TRUE(host.rect().contains(palette.geometry()));
        auto* search = palette.findChild<LineEdit*>();
        EXPECT_TRUE(palette.rect().contains(QRect(search->mapTo(&palette, QPoint()), search->size())));
        EXPECT_FALSE(palette.grab().isNull());
        palette.close();
    }
}

TEST(CommandPaletteTest, Contract_CompactFilteredHeightKeepsSearchPosition)
{
    QWidget host;
    host.resize(900, 700);
    CommandPalette palette(&host);
    palette.setAnimationEnabled(false);
    for (int i = 0; i < 12; ++i)
        palette.addCommand(new QAction(QStringLiteral("Command %1").arg(i), &host), QStringLiteral("Actions"));
    host.show();
    palette.open();
    EXPECT_LE(palette.width(), 464);
    const int fullHeight = palette.height();
    const QPoint topLeft = palette.pos();
    palette.setQuery(QStringLiteral("Command 11"));
    EXPECT_EQ(palette.resultCount(), 1);
    EXPECT_LT(palette.height(), fullHeight);
    EXPECT_EQ(palette.pos(), topLeft);
    palette.setQuery(QStringLiteral("unknown"));
    EXPECT_EQ(palette.resultCount(), 0);
    EXPECT_GT(palette.height(), 90);
    palette.setQuery(QString());
    EXPECT_EQ(palette.height(), fullHeight);
    palette.close();
    host.resize(480, 500);
    palette.setQuery(QStringLiteral("Command 11"));
    palette.open();
    palette.setQuery(QString());
    EXPECT_TRUE(host.rect().contains(palette.geometry()));
}
