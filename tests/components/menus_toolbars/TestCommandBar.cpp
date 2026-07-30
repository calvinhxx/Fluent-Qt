#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QIcon>
#include <QImage>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPointer>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTest>
#include <QWidgetAction>

#include <algorithm>

#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"

using fluent::menus_toolbars::CommandBar;
using fluent::menus_toolbars::CommandBarFlyout;

static_assert(std::is_base_of<QWidget, CommandBar>::value,
              "CommandBar must remain a QWidget");
static_assert(std::is_base_of<fluent::FluentElement, CommandBar>::value,
              "CommandBar must participate in Fluent themes");
static_assert(std::is_base_of<fluent::QMLPlus, CommandBar>::value,
              "CommandBar must expose QMLPlus composition");

namespace {

void processDeferredUiWork()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(
        nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
}

QAbstractButton* commandButton(
    QWidget* root,
    const QString& objectName,
    const QString& accessibleName = QString())
{
    if (!root)
        return nullptr;
    const QList<QAbstractButton*> buttons =
        root->findChildren<QAbstractButton*>(objectName);
    for (QAbstractButton* button : buttons) {
        if (accessibleName.isEmpty()
            || button->accessibleName() == accessibleName) {
            return button;
        }
    }
    return nullptr;
}

QList<QAbstractButton*> overflowRowsInVisualOrder(QWidget* root)
{
    QList<QAbstractButton*> rows =
        root->findChildren<QAbstractButton*>(
            QStringLiteral("FluentCommandBar.OverflowRow"));
    std::sort(
        rows.begin(),
        rows.end(),
        [](QAbstractButton* first, QAbstractButton* second) {
            return first->mapToGlobal(QPoint()).y()
                < second->mapToGlobal(QPoint()).y();
        });
    return rows;
}

QIcon testIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(0x20, 0x78, 0xD4));
    return QIcon(pixmap);
}

} // namespace

TEST(CommandBarTest, DefaultsAndPropertiesNotifyOnlyOnChange)
{
    CommandBar bar;

    EXPECT_EQ(bar.labelPosition(), CommandBar::LabelPosition::Right);
    EXPECT_TRUE(bar.isDynamicOverflowEnabled());
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_TRUE(bar.backgroundVisible());
    EXPECT_TRUE(bar.isBackgroundVisible());
    EXPECT_TRUE(bar.primaryActions().isEmpty());
    EXPECT_TRUE(bar.secondaryActions().isEmpty());
    EXPECT_TRUE(bar.overflowedPrimaryActions().isEmpty());

    QSignalSpy labelSpy(&bar, &CommandBar::labelPositionChanged);
    QSignalSpy dynamicSpy(
        &bar, &CommandBar::dynamicOverflowEnabledChanged);
    QSignalSpy backgroundSpy(
        &bar, &CommandBar::backgroundVisibleChanged);
    QSignalSpy overflowSpy(&bar, &CommandBar::overflowOpenChanged);

    bar.setLabelPosition(CommandBar::LabelPosition::Collapsed);
    bar.setLabelPosition(CommandBar::LabelPosition::Collapsed);
    EXPECT_EQ(labelSpy.count(), 1);

    bar.setDynamicOverflowEnabled(false);
    bar.setDynamicOverflowEnabled(false);
    EXPECT_EQ(dynamicSpy.count(), 1);

    bar.setBackgroundVisible(false);
    bar.setBackgroundVisible(false);
    EXPECT_EQ(backgroundSpy.count(), 1);

    bar.setOverflowOpen(true);
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_EQ(overflowSpy.count(), 0);
}

TEST(CommandBarTest, ExplicitSectionsPreserveOrderAndSupportMoves)
{
    CommandBar bar;
    QAction first(QStringLiteral("&First"));
    QAction second(QStringLiteral("Second"));
    QAction third(QStringLiteral("Third"));

    EXPECT_TRUE(bar.addPrimaryAction(&first));
    EXPECT_TRUE(bar.addPrimaryAction(&third));
    EXPECT_TRUE(bar.insertPrimaryAction(&third, &second));
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&first, &second, &third}));
    EXPECT_TRUE(bar.actions().contains(&first));
    EXPECT_EQ(first.parent(), nullptr);

    EXPECT_TRUE(bar.addPrimaryAction(&second));
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&first, &second, &third}));

    EXPECT_TRUE(bar.addSecondaryAction(&second));
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&first, &third}));
    EXPECT_EQ(bar.secondaryActions(), (QList<QAction*>{&second}));
    EXPECT_TRUE(bar.actions().contains(&second));

    const QList<QAction*> beforeInvalid = bar.primaryActions();
    EXPECT_FALSE(bar.insertPrimaryAction(&second, &third));
    EXPECT_EQ(bar.primaryActions(), beforeInvalid);

    EXPECT_TRUE(bar.insertPrimaryAction(&first, &third));
    EXPECT_EQ(bar.primaryActions(), (QList<QAction*>{&third, &first}));
    EXPECT_TRUE(bar.insertPrimaryAction(nullptr, &third));
    EXPECT_EQ(bar.primaryActions(), (QList<QAction*>{&first, &third}));
    EXPECT_FALSE(bar.removeCommandAction(nullptr));
    EXPECT_TRUE(bar.removeCommandAction(&second));
    EXPECT_FALSE(bar.actions().contains(&second));
}

TEST(CommandBarTest, QWidgetActionApisArePrimaryShorthands)
{
    CommandBar bar;
    QAction first(QStringLiteral("First"));
    QAction inserted(QStringLiteral("Inserted"));
    QAction appended(QStringLiteral("Appended"));

    bar.addAction(&first);
    bar.insertAction(&first, &inserted);
    bar.addAction(&appended);
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&inserted, &first, &appended}));

    ASSERT_TRUE(bar.addSecondaryAction(&appended));
    bar.addAction(&appended);
    bar.insertAction(&inserted, &appended);
    EXPECT_EQ(bar.secondaryActions(), (QList<QAction*>{&appended}));
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&inserted, &first}));

    QWidget* qtView = &bar;
    qtView->insertAction(&inserted, &appended);
    EXPECT_EQ(bar.secondaryActions(), (QList<QAction*>{&appended}));
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&inserted, &first}));

    QAction baseInserted(QStringLiteral("Base inserted"));
    qtView->insertAction(&first, &baseInserted);
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&inserted, &baseInserted, &first}));

    QAction invalidThroughBase;
    qtView->addAction(&invalidThroughBase);
    EXPECT_FALSE(bar.actions().contains(&invalidThroughBase));
    EXPECT_FALSE(bar.primaryActions().contains(&invalidThroughBase));

    bar.removeAction(&first);
    EXPECT_EQ(
        bar.primaryActions(),
        (QList<QAction*>{&inserted, &baseInserted}));
    qtView->removeAction(&baseInserted);
    EXPECT_EQ(bar.primaryActions(), (QList<QAction*>{&inserted}));
}

TEST(CommandBarTest, SupportsQtActionSemanticsAndRejectsUnsupportedKinds)
{
    CommandBar bar;
    QAction toggle(QStringLiteral("Toggle"));
    toggle.setCheckable(true);
    QAction other(QStringLiteral("Other"));
    other.setCheckable(true);
    QActionGroup group(nullptr);
    group.setExclusive(true);
    group.addAction(&toggle);
    group.addAction(&other);
    QAction separator;
    separator.setSeparator(true);

    EXPECT_TRUE(bar.addPrimaryAction(&toggle));
    EXPECT_TRUE(bar.addPrimaryAction(&other));
    EXPECT_TRUE(bar.addPrimaryAction(&separator));

    QAction empty;
    QWidgetAction widgetAction(nullptr);
    widgetAction.setText(QStringLiteral("Custom widget"));
    QAction nested(QStringLiteral("Nested"));
    QMenu menu;
    nested.setMenu(&menu);

    EXPECT_FALSE(bar.addPrimaryAction(nullptr));
    EXPECT_FALSE(bar.addPrimaryAction(&empty));
    EXPECT_FALSE(bar.addPrimaryAction(&widgetAction));
    EXPECT_FALSE(bar.addPrimaryAction(&nested));
    EXPECT_FALSE(bar.actions().contains(&empty));
    EXPECT_FALSE(bar.actions().contains(&widgetAction));
    EXPECT_FALSE(bar.actions().contains(&nested));

    QAction mutableAction(QStringLiteral("Initially valid"));
    ASSERT_TRUE(bar.addSecondaryAction(&mutableAction));
    mutableAction.setText(QString());
    mutableAction.setIconText(QString());
    EXPECT_TRUE(bar.secondaryActions().contains(&mutableAction));
    mutableAction.setText(QStringLiteral("Valid again"));
    EXPECT_TRUE(bar.secondaryActions().contains(&mutableAction));
}

TEST(CommandBarTest, BorrowedLifetimeAndActionDestructionAreSafe)
{
    QPointer<QAction> borrowed = new QAction(QStringLiteral("Borrowed"));
    {
        CommandBar bar;
        ASSERT_TRUE(bar.addPrimaryAction(borrowed));
        EXPECT_EQ(borrowed->parent(), nullptr);
        bar.clearPrimaryActions();
        EXPECT_FALSE(borrowed.isNull());
        EXPECT_FALSE(bar.actions().contains(borrowed));
    }
    EXPECT_FALSE(borrowed.isNull());
    delete borrowed;

    CommandBar bar;
    QPointer<QAction> transient =
        new QAction(QStringLiteral("Transient"));
    ASSERT_TRUE(bar.addPrimaryAction(transient));
    delete transient;
    EXPECT_TRUE(bar.primaryActions().isEmpty());

    auto* ownedBar = new CommandBar();
    QPointer<QAction> surfaceOwned =
        new QAction(QStringLiteral("Surface owned"), ownedBar);
    ASSERT_TRUE(ownedBar->addPrimaryAction(surfaceOwned));
    delete ownedBar;
    EXPECT_TRUE(surfaceOwned.isNull());
}

TEST(CommandBarTest,
     Contract_WindowTeardownDoesNotRebuildAfterBorrowedActionDestruction)
{
    auto* window = new QWidget;
    auto* action =
        new QAction(QStringLiteral("Window-owned action"), window);
    auto* bar = new CommandBar(window);
    ASSERT_TRUE(bar->addPrimaryAction(action));

    delete window;
    SUCCEED();
}

TEST(CommandBarTest, OneBorrowedActionCanServeMultipleCommandSurfaces)
{
    QWidget window;
    CommandBar bar(&window);
    CommandBarFlyout flyout(&window);
    QAction shared(QStringLiteral("Shared"));

    EXPECT_TRUE(bar.addPrimaryAction(&shared));
    EXPECT_TRUE(flyout.addSecondaryAction(&shared));
    EXPECT_EQ(shared.parent(), nullptr);
    EXPECT_EQ(bar.primaryActions(), (QList<QAction*>{&shared}));
    EXPECT_EQ(flyout.secondaryActions(), (QList<QAction*>{&shared}));

    bar.clearPrimaryActions();
    EXPECT_EQ(flyout.secondaryActions(), (QList<QAction*>{&shared}));
}

TEST(CommandBarTest,
     Contract_ResponsiveOverflowUsesPriorityAndLogicalTailOrder)
{
    QWidget window;
    window.resize(900, 200);
    CommandBar bar(&window);
    bar.move(20, 20);

    QAction first(QStringLiteral("Command"));
    QAction second(QStringLiteral("Command"));
    QAction third(QStringLiteral("Command"));
    QAction fourth(QStringLiteral("Command"));
    first.setPriority(QAction::LowPriority);
    second.setPriority(QAction::NormalPriority);
    third.setPriority(QAction::LowPriority);
    fourth.setPriority(QAction::HighPriority);
    ASSERT_TRUE(bar.addPrimaryAction(&first));
    ASSERT_TRUE(bar.addPrimaryAction(&second));
    ASSERT_TRUE(bar.addPrimaryAction(&third));
    ASSERT_TRUE(bar.addPrimaryAction(&fourth));

    const QSize fullSize = bar.sizeHint();
    bar.resize(fullSize);
    window.show();
    processDeferredUiWork();
    EXPECT_TRUE(bar.overflowedPrimaryActions().isEmpty());

    QSignalSpy overflowSpy(
        &bar, &CommandBar::overflowedPrimaryActionsChanged);
    bar.resize(fullSize.width() - 1, fullSize.height());
    processDeferredUiWork();
    ASSERT_EQ(bar.overflowedPrimaryActions().size(), 1);
    EXPECT_EQ(bar.overflowedPrimaryActions().first(), &third);
    EXPECT_TRUE(bar.isPrimaryActionOverflowed(&third));
    EXPECT_FALSE(bar.isPrimaryActionOverflowed(&first));
    EXPECT_EQ(overflowSpy.count(), 1);

    bar.resize(fullSize.width() - 1, fullSize.height());
    processDeferredUiWork();
    EXPECT_EQ(overflowSpy.count(), 1);

    bar.resize(fullSize);
    processDeferredUiWork();
    EXPECT_TRUE(bar.overflowedPrimaryActions().isEmpty());
    EXPECT_EQ(overflowSpy.count(), 2);
}

TEST(CommandBarTest,
     Contract_OverflowProjectionNormalizesSeparatorsAndSections)
{
    QWidget window;
    window.resize(720, 360);
    CommandBar bar(&window);
    bar.move(20, 20);

    QAction primary(QStringLiteral("Primary"));
    QAction primarySeparator;
    primarySeparator.setSeparator(true);
    QAction overflow(QStringLiteral("Overflow"));
    overflow.setPriority(QAction::LowPriority);
    QAction secondaryOne(QStringLiteral("Secondary one"));
    QAction secondarySeparator;
    secondarySeparator.setSeparator(true);
    QAction secondaryTwo(QStringLiteral("Secondary two"));

    ASSERT_TRUE(bar.addPrimaryAction(&primary));
    ASSERT_TRUE(bar.addPrimaryAction(&primarySeparator));
    ASSERT_TRUE(bar.addPrimaryAction(&overflow));
    ASSERT_TRUE(bar.addSecondaryAction(&secondaryOne));
    ASSERT_TRUE(bar.addSecondaryAction(&secondarySeparator));
    ASSERT_TRUE(bar.addSecondaryAction(&secondaryTwo));

    const QSize fullSize = bar.sizeHint();
    bar.resize(fullSize.width() - 1, fullSize.height());
    window.show();
    processDeferredUiWork();

    ASSERT_EQ(
        bar.overflowedPrimaryActions(),
        (QList<QAction*>{&overflow}));

    QAbstractButton* more = commandButton(
        &window,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(more, nullptr);
    EXPECT_TRUE(more->isVisible());
    EXPECT_GE(more->width(), 40);
    EXPECT_GE(more->height(), 40);

    QSignalSpy openSpy(&bar, &CommandBar::overflowOpenChanged);
    bar.setOverflowOpen(true);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    ASSERT_EQ(openSpy.count(), 1);
    EXPECT_TRUE(openSpy.at(0).at(0).toBool());

    QWidget* popup = window.findChild<QWidget*>(
        QStringLiteral("FluentCommandBar.OverflowPopup"));
    ASSERT_NE(popup, nullptr);
    EXPECT_EQ(popup->parentWidget(), &window);
    EXPECT_FALSE(popup->isWindow());

    const QList<QAbstractButton*> rows =
        overflowRowsInVisualOrder(popup);
    ASSERT_EQ(rows.size(), 3);
    auto* scrollView = popup->findChild<QScrollArea*>(
        QStringLiteral("FluentCommandBar.OverflowScrollView"));
    ASSERT_NE(scrollView, nullptr);
    for (QAbstractButton* row : rows) {
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(
            row->width(),
            scrollView->viewport()->width());
        EXPECT_GE(row->width(), 180);
    }
    EXPECT_EQ(rows.at(0)->accessibleName(), QStringLiteral("Overflow"));
    EXPECT_EQ(
        rows.at(1)->accessibleName(),
        QStringLiteral("Secondary one"));
    EXPECT_EQ(
        rows.at(2)->accessibleName(),
        QStringLiteral("Secondary two"));
    EXPECT_NE(
        popup->findChild<QWidget*>(
            QStringLiteral(
                "FluentCommandBar.OverflowGroupSeparator")),
        nullptr);

    bar.setOverflowOpen(true);
    processDeferredUiWork();
    EXPECT_EQ(openSpy.count(), 1);
    bar.setOverflowOpen(false);
    processDeferredUiWork();
    EXPECT_FALSE(bar.isOverflowOpen());
    ASSERT_EQ(openSpy.count(), 2);
    EXPECT_FALSE(openSpy.at(1).at(0).toBool());
}

TEST(CommandBarTest,
     Contract_DisablingDynamicOverflowKeepsPrimaryCommandsInline)
{
    QWidget window;
    window.resize(720, 200);
    CommandBar bar(&window);
    QAction first(QStringLiteral("First command"));
    QAction second(QStringLiteral("Second command"));
    QAction third(QStringLiteral("Third command"));
    QAction secondary(QStringLiteral("Secondary"));
    ASSERT_TRUE(bar.addPrimaryAction(&first));
    ASSERT_TRUE(bar.addPrimaryAction(&second));
    ASSERT_TRUE(bar.addPrimaryAction(&third));
    ASSERT_TRUE(bar.addSecondaryAction(&secondary));

    bar.setDynamicOverflowEnabled(false);
    const QSize fullSize = bar.sizeHint();
    EXPECT_EQ(bar.minimumSizeHint(), fullSize);
    bar.resize(
        qMax(1, fullSize.width() / 2), fullSize.height());
    window.show();
    processDeferredUiWork();

    EXPECT_TRUE(bar.overflowedPrimaryActions().isEmpty());
    for (const QString& caption :
         {QStringLiteral("First command"),
          QStringLiteral("Second command"),
          QStringLiteral("Third command")}) {
        QAbstractButton* presenter = commandButton(
            &bar,
            QStringLiteral("FluentCommandBar.PrimaryPresenter"),
            caption);
        ASSERT_NE(presenter, nullptr);
        EXPECT_TRUE(presenter->isVisible());
    }

    secondary.setVisible(false);
    processDeferredUiWork();
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(more, nullptr);
    EXPECT_FALSE(more->isVisible());
}

TEST(CommandBarTest,
     Contract_PresentersTrackActionStateAndCollapsedLabels)
{
    QWidget window;
    window.resize(720, 200);
    CommandBar bar(&window);
    QAction iconCommand(testIcon(), QStringLiteral("&Open"));
    iconCommand.setCheckable(true);
    QAction textOnly(QStringLiteral("Text only"));
    ASSERT_TRUE(bar.addPrimaryAction(&iconCommand));
    ASSERT_TRUE(bar.addPrimaryAction(&textOnly));
    bar.resize(bar.sizeHint());
    window.show();
    processDeferredUiWork();

    QAbstractButton* iconPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Open"));
    QAbstractButton* textPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Text only"));
    ASSERT_NE(iconPresenter, nullptr);
    ASSERT_NE(textPresenter, nullptr);
    EXPECT_FALSE(iconPresenter->text().isEmpty());

    bar.setLabelPosition(CommandBar::LabelPosition::Collapsed);
    processDeferredUiWork();
    EXPECT_TRUE(iconPresenter->text().isEmpty());
    EXPECT_EQ(
        iconPresenter->accessibleName(), QStringLiteral("Open"));
    EXPECT_EQ(textPresenter->text(), QStringLiteral("Text only"));

    iconCommand.setEnabled(false);
    iconCommand.setChecked(true);
    processDeferredUiWork();
    EXPECT_FALSE(iconPresenter->isEnabled());
    EXPECT_TRUE(iconPresenter->isChecked());

    iconCommand.setVisible(false);
    processDeferredUiWork();
    EXPECT_FALSE(iconPresenter->isVisible());
}

TEST(CommandBarTest,
     Contract_RtlMirrorsVisualOrderWithoutChangingOverflowChoice)
{
    QWidget window;
    window.resize(900, 200);
    CommandBar bar(&window);
    QAction first(QStringLiteral("First"));
    QAction second(QStringLiteral("Second"));
    QAction third(QStringLiteral("Third"));
    third.setPriority(QAction::LowPriority);
    ASSERT_TRUE(bar.addPrimaryAction(&first));
    ASSERT_TRUE(bar.addPrimaryAction(&second));
    ASSERT_TRUE(bar.addPrimaryAction(&third));

    const QSize fullSize = bar.sizeHint();
    bar.resize(fullSize.width() - 1, fullSize.height());
    window.show();
    processDeferredUiWork();
    ASSERT_EQ(
        bar.overflowedPrimaryActions(),
        (QList<QAction*>{&third}));

    QAbstractButton* firstPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("First"));
    QAbstractButton* secondPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Second"));
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(firstPresenter, nullptr);
    ASSERT_NE(secondPresenter, nullptr);
    ASSERT_NE(more, nullptr);
    EXPECT_LT(firstPresenter->x(), secondPresenter->x());
    EXPECT_LT(secondPresenter->x(), more->x());

    bar.setLayoutDirection(Qt::RightToLeft);
    processDeferredUiWork();
    EXPECT_EQ(
        bar.overflowedPrimaryActions(),
        (QList<QAction*>{&third}));
    EXPECT_LT(more->x(), secondPresenter->x());
    EXPECT_LT(secondPresenter->x(), firstPresenter->x());
}

TEST(CommandBarTest,
     Contract_CompositeKeyboardFocusAndOverflowNavigation)
{
    QWidget window;
    window.resize(760, 240);
    QLineEdit before(&window);
    before.setGeometry(20, 20, 160, 32);
    CommandBar bar(&window);
    bar.setGeometry(20, 72, 620, 48);
    QLineEdit after(&window);
    after.setGeometry(20, 140, 160, 32);

    QAction disabled(QStringLiteral("Disabled"));
    disabled.setEnabled(false);
    QAction second(QStringLiteral("Second"));
    QAction third(QStringLiteral("Third"));
    QAction overflowDisabled(QStringLiteral("Overflow disabled"));
    overflowDisabled.setEnabled(false);
    QAction overflowEnabled(QStringLiteral("Overflow enabled"));
    ASSERT_TRUE(bar.addPrimaryAction(&disabled));
    ASSERT_TRUE(bar.addPrimaryAction(&second));
    ASSERT_TRUE(bar.addPrimaryAction(&third));
    ASSERT_TRUE(bar.addSecondaryAction(&overflowDisabled));
    ASSERT_TRUE(bar.addSecondaryAction(&overflowEnabled));

    QWidget::setTabOrder(&before, &bar);
    QWidget::setTabOrder(&bar, &after);
    window.show();
    before.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();
    ASSERT_EQ(QApplication::focusWidget(), &before);

    QTest::keyClick(&before, Qt::Key_Tab);
    processDeferredUiWork();
    QAbstractButton* secondPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Second"));
    QAbstractButton* thirdPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Third"));
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(secondPresenter, nullptr);
    ASSERT_NE(thirdPresenter, nullptr);
    ASSERT_NE(more, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), secondPresenter);

    QTest::keyClick(secondPresenter, Qt::Key_Right);
    EXPECT_EQ(QApplication::focusWidget(), thirdPresenter);
    QTest::keyClick(thirdPresenter, Qt::Key_End);
    EXPECT_EQ(QApplication::focusWidget(), more);

    QTest::keyClick(more, Qt::Key_Down);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    QWidget* popup = window.findChild<QWidget*>(
        QStringLiteral("FluentCommandBar.OverflowPopup"));
    ASSERT_NE(popup, nullptr);
    QAbstractButton* enabledRow = commandButton(
        popup,
        QStringLiteral("FluentCommandBar.OverflowRow"),
        QStringLiteral("Overflow enabled"));
    ASSERT_NE(enabledRow, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), enabledRow);

    QTest::keyClick(enabledRow, Qt::Key_Escape);
    processDeferredUiWork();
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), more);

    QSignalSpy triggerSpy(&second, &QAction::triggered);
    QTest::keyClick(more, Qt::Key_Home);
    EXPECT_EQ(QApplication::focusWidget(), secondPresenter);
    QTest::keyClick(secondPresenter, Qt::Key_Return);
    EXPECT_EQ(triggerSpy.count(), 1);

    QTest::keyClick(secondPresenter, Qt::Key_Tab);
    processDeferredUiWork();
    EXPECT_EQ(QApplication::focusWidget(), &after);
}

TEST(CommandBarTest,
     Contract_PointerOverflowOpenKeepsFocusOnMoreUntilKeyboardNavigation)
{
    QWidget window;
    window.resize(520, 180);
    CommandBar bar(&window);
    bar.setGeometry(20, 20, 360, 48);
    QAction secondary(QStringLiteral("Secondary command"));
    ASSERT_TRUE(bar.addSecondaryAction(&secondary));
    window.show();
    processDeferredUiWork();

    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(more, nullptr);
    ASSERT_TRUE(more->isVisible());

    QTest::mouseClick(more, Qt::LeftButton);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), more);

    QWidget* popup = window.findChild<QWidget*>(
        QStringLiteral("FluentCommandBar.OverflowPopup"));
    ASSERT_NE(popup, nullptr);
    QAbstractButton* row = commandButton(
        popup,
        QStringLiteral("FluentCommandBar.OverflowRow"),
        QStringLiteral("Secondary command"));
    ASSERT_NE(row, nullptr);
    EXPECT_FALSE(row->hasFocus());

    bar.setOverflowOpen(false);
    processDeferredUiWork();
    QTest::keyClick(more, Qt::Key_Down);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), row);
}

TEST(CommandBarTest,
     Contract_FocusRepairsToNearestCommandBeforeMore)
{
    QWidget window;
    window.resize(720, 180);
    CommandBar bar(&window);
    QAction first(QStringLiteral("First"));
    QAction second(QStringLiteral("Second"));
    QAction third(QStringLiteral("Third"));
    QAction secondary(QStringLiteral("Secondary"));
    third.setPriority(QAction::LowPriority);
    secondary.setEnabled(false);
    ASSERT_TRUE(bar.addPrimaryAction(&first));
    ASSERT_TRUE(bar.addPrimaryAction(&second));
    ASSERT_TRUE(bar.addPrimaryAction(&third));
    ASSERT_TRUE(bar.addSecondaryAction(&secondary));

    const QSize fullSize = bar.sizeHint();
    bar.setGeometry(20, 20, fullSize.width(), fullSize.height());
    window.show();
    bar.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();

    QAbstractButton* firstPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("First"));
    QAbstractButton* secondPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Second"));
    QAbstractButton* thirdPresenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Third"));
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(firstPresenter, nullptr);
    ASSERT_NE(secondPresenter, nullptr);
    ASSERT_NE(thirdPresenter, nullptr);
    ASSERT_NE(more, nullptr);

    thirdPresenter->setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();
    ASSERT_EQ(QApplication::focusWidget(), thirdPresenter);
    bar.resize(fullSize.width() - 1, fullSize.height());
    processDeferredUiWork();
    ASSERT_EQ(
        bar.overflowedPrimaryActions(),
        (QList<QAction*>{&third}));
    EXPECT_EQ(QApplication::focusWidget(), secondPresenter);

    second.setEnabled(false);
    processDeferredUiWork();
    EXPECT_EQ(QApplication::focusWidget(), firstPresenter);

    third.setEnabled(false);
    processDeferredUiWork();
    first.setVisible(false);
    processDeferredUiWork();
    ASSERT_NE(QApplication::focusWidget(), nullptr);
    EXPECT_EQ(QApplication::focusWidget(), more)
        << "actual focus object="
        << QApplication::focusWidget()
               ->objectName()
               .toStdString()
        << " moreVisible=" << more->isVisible()
        << " moreEnabled=" << more->isEnabled()
        << " moreFocusPolicy=" << int(more->focusPolicy());
}

TEST(CommandBarTest,
     Contract_OverflowDismissAndActivationRespectFocusDestination)
{
    QWidget window;
    window.resize(640, 240);
    QLineEdit editor(&window);
    editor.setGeometry(20, 120, 180, 32);
    CommandBar bar(&window);
    bar.setGeometry(20, 20, 360, 48);
    QAction command(QStringLiteral("Secondary command"));
    ASSERT_TRUE(bar.addSecondaryAction(&command));
    window.show();
    bar.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();

    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(more, nullptr);
    ASSERT_EQ(QApplication::focusWidget(), more);

    bar.setOverflowOpen(true);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    QTest::mouseClick(more, Qt::LeftButton);
    processDeferredUiWork();
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), more);

    bar.setOverflowOpen(true);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    QTest::mousePress(more, Qt::LeftButton);
    processDeferredUiWork();
    EXPECT_FALSE(bar.isOverflowOpen());
    QTest::mouseRelease(
        more,
        Qt::LeftButton,
        Qt::NoModifier,
        QPoint(-4, -4));
    processDeferredUiWork();
    QTest::mouseClick(more, Qt::LeftButton);
    processDeferredUiWork();
    EXPECT_TRUE(bar.isOverflowOpen());
    bar.setOverflowOpen(false);
    processDeferredUiWork();

    QSignalSpy triggerSpy(&command, &QAction::triggered);
    bar.setOverflowOpen(true);
    processDeferredUiWork();
    QWidget* popup = window.findChild<QWidget*>(
        QStringLiteral("FluentCommandBar.OverflowPopup"));
    ASSERT_NE(popup, nullptr);
    QAbstractButton* row = commandButton(
        popup,
        QStringLiteral("FluentCommandBar.OverflowRow"),
        QStringLiteral("Secondary command"));
    ASSERT_NE(row, nullptr);
    ASSERT_EQ(QApplication::focusWidget(), row);
    row->click();
    processDeferredUiWork();
    EXPECT_EQ(triggerSpy.count(), 1);
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), more);

    QObject::connect(
        &command,
        &QAction::triggered,
        &editor,
        [&editor]() { editor.setFocus(Qt::OtherFocusReason); });
    bar.setOverflowOpen(true);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    row = commandButton(
        popup,
        QStringLiteral("FluentCommandBar.OverflowRow"),
        QStringLiteral("Secondary command"));
    ASSERT_NE(row, nullptr);
    row->click();
    processDeferredUiWork();
    EXPECT_EQ(triggerSpy.count(), 2);
    EXPECT_FALSE(bar.isOverflowOpen());
    EXPECT_EQ(QApplication::focusWidget(), &editor);
}

TEST(CommandBarTest,
     Contract_PresenterActivationIsExactAndDeletionSafe)
{
    QWidget window;
    window.resize(480, 180);
    auto* bar = new CommandBar(&window);
    bar->setGeometry(20, 20, 360, 48);
    auto* action = new QAction(QStringLiteral("Delete surface"));
    QPointer<CommandBar> barGuard = bar;
    QSignalSpy triggerSpy(action, &QAction::triggered);
    ASSERT_TRUE(bar->addPrimaryAction(action));
    window.show();
    processDeferredUiWork();

    QAbstractButton* presenter = commandButton(
        bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Delete surface"));
    ASSERT_NE(presenter, nullptr);
    QObject::connect(
        action,
        &QAction::triggered,
        &window,
        [bar]() { delete bar; });
    presenter->click();
    EXPECT_EQ(triggerSpy.count(), 1);
    EXPECT_TRUE(barGuard.isNull());
    delete action;
}

TEST(CommandBarTest,
     Contract_AccessibleToolbarCommandsAndMoreExpansion)
{
#if QT_CONFIG(accessibility)
    QWidget window;
    window.resize(560, 260);
    CommandBar bar(&window);
    bar.setGeometry(20, 20, 420, 48);
    bar.setAccessibleName(QStringLiteral("Document commands"));
    QAction primary(QStringLiteral("&Save"));
    primary.setShortcut(QKeySequence::Save);
    QAction secondary(QStringLiteral("Properties"));
    ASSERT_TRUE(bar.addPrimaryAction(&primary));
    ASSERT_TRUE(bar.addSecondaryAction(&secondary));
    window.show();
    processDeferredUiWork();

    QAbstractButton* presenter = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.PrimaryPresenter"),
        QStringLiteral("Save"));
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(presenter, nullptr);
    ASSERT_NE(more, nullptr);

    QAccessibleInterface* rootInterface =
        QAccessible::queryAccessibleInterface(&bar);
    QAccessibleInterface* commandInterface =
        QAccessible::queryAccessibleInterface(presenter);
    QAccessibleInterface* moreInterface =
        QAccessible::queryAccessibleInterface(more);
    ASSERT_NE(rootInterface, nullptr);
    ASSERT_NE(commandInterface, nullptr);
    ASSERT_NE(moreInterface, nullptr);
    EXPECT_EQ(rootInterface->role(), QAccessible::ToolBar);
    EXPECT_EQ(
        rootInterface->text(QAccessible::Name),
        QStringLiteral("Document commands"));
    EXPECT_EQ(commandInterface->role(), QAccessible::Button);
    EXPECT_EQ(
        commandInterface->text(QAccessible::Name),
        QStringLiteral("Save"));
    EXPECT_EQ(
        commandInterface->text(QAccessible::Accelerator),
        primary.shortcut().toString(QKeySequence::NativeText));
    EXPECT_TRUE(moreInterface->state().collapsed);

    bar.setOverflowOpen(true);
    processDeferredUiWork();
    ASSERT_TRUE(bar.isOverflowOpen());
    EXPECT_TRUE(moreInterface->state().expanded);
    bar.setOverflowOpen(false);
    processDeferredUiWork();
    EXPECT_TRUE(moreInterface->state().collapsed);
#else
    GTEST_SKIP() << "Qt accessibility support is disabled";
#endif
}

TEST(CommandBarTest,
     Contract_AllDesignLanguagesAndThemesPaintInlineSurface)
{
    QWidget window;
    window.resize(520, 180);
    CommandBar bar(&window);
    bar.setGeometry(20, 20, 420, 48);
    QAction primary(QStringLiteral("Primary"));
    QAction secondary(QStringLiteral("Secondary"));
    ASSERT_TRUE(bar.addPrimaryAction(&primary));
    ASSERT_TRUE(bar.addSecondaryAction(&secondary));
    window.show();
    processDeferredUiWork();
    QAbstractButton* more = commandButton(
        &bar,
        QStringLiteral("FluentCommandBar.MoreButton"));
    ASSERT_NE(more, nullptr);

    const fluent::FluentElement::DesignLanguage languages[] = {
        fluent::FluentElement::DesignFluent,
        fluent::FluentElement::DesignMaterial,
        fluent::FluentElement::DesignCupertino,
    };
    const fluent::FluentElement::Theme themes[] = {
        fluent::FluentElement::Light,
        fluent::FluentElement::Dark,
    };
    for (auto language : languages) {
        for (auto theme : themes) {
            fluent::ThemeRegistry::instance().setDesignLanguage(
                language);
            fluent::FluentElement::setTheme(theme);
            bar.onThemeUpdated();
            processDeferredUiWork();
            const QImage image = bar.grab().toImage();
            ASSERT_FALSE(image.isNull())
                << "language=" << language
                << " theme=" << theme;
            bool painted = false;
            const QColor baseline = image.pixelColor(0, 0);
            for (int y = 0;
                 y < image.height() && !painted;
                 ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    if (image.pixelColor(x, y) != baseline) {
                        painted = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(painted)
                << "language=" << language
                << " theme=" << theme;
            EXPECT_TRUE(more->isVisible());
        }
    }
    fluent::ThemeRegistry::instance().resetToDefaults();
    fluent::FluentElement::setTheme(
        fluent::FluentElement::Light);
}
