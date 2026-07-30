#include <gtest/gtest.h>

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QKeySequence>
#include <QLineEdit>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>

#include "components/dialogs_flyouts/Flyout.h"
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/ThemeRegistry.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/menus_toolbars/CommandBar.h"
#include "components/menus_toolbars/CommandBarFlyout.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

using fluent::menus_toolbars::CommandBarFlyout;

static_assert(
    std::is_base_of<
        fluent::dialogs_flyouts::Flyout,
        CommandBarFlyout>::value,
    "CommandBarFlyout must remain a same-window Flyout");

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

QList<QAbstractButton*> rowsInVisualOrder(
    QWidget* root,
    const QString& objectName)
{
    QList<QAbstractButton*> rows =
        root->findChildren<QAbstractButton*>(objectName);
    std::sort(
        rows.begin(),
        rows.end(),
        [](QAbstractButton* first, QAbstractButton* second) {
            return first->mapToGlobal(QPoint()).y()
                < second->mapToGlobal(QPoint()).y();
        });
    return rows;
}

struct FlyoutFixture {
    QWidget window;
    QLineEdit editor{&window};
    QPushButton anchor{QStringLiteral("Anchor"), &window};
    CommandBarFlyout flyout{&window};
    QAction primary{QStringLiteral("Primary")};
    QAction secondary{QStringLiteral("Secondary")};

    FlyoutFixture()
    {
        window.resize(480, 320);
        editor.setGeometry(20, 20, 180, 32);
        anchor.setGeometry(20, 72, 120, 32);
        flyout.setAnimationEnabled(false);
        flyout.resize(240, 140);
        window.show();
        QApplication::processEvents();
    }

    ~FlyoutFixture()
    {
        flyout.close();
        QApplication::processEvents();
    }
};

class CommandSurfaceVisualWindow final
    : public QWidget,
      public fluent::FluentElement {
public:
    using QWidget::QWidget;

    void onThemeUpdated() override
    {
        QPalette next = palette();
        next.setColor(
            QPalette::Window, themeColorsRef().bgCanvas);
        setPalette(next);
        setAutoFillBackground(true);
    }
};

} // namespace

TEST(CommandBarFlyoutTest, DefaultsAndPropertiesNotifyOnlyOnChange)
{
    QWidget window;
    CommandBarFlyout flyout(&window);

    EXPECT_EQ(
        flyout.showMode(), CommandBarFlyout::ShowMode::Standard);
    EXPECT_FALSE(flyout.isExpanded());
    EXPECT_FALSE(flyout.isAlwaysExpanded());
    EXPECT_TRUE(flyout.primaryActions().isEmpty());
    EXPECT_TRUE(flyout.secondaryActions().isEmpty());

    QSignalSpy showModeSpy(
        &flyout, &CommandBarFlyout::showModeChanged);
    QSignalSpy expandedSpy(
        &flyout, &CommandBarFlyout::expandedChanged);
    QSignalSpy alwaysSpy(
        &flyout, &CommandBarFlyout::alwaysExpandedChanged);

    flyout.setShowMode(CommandBarFlyout::ShowMode::Transient);
    flyout.setShowMode(CommandBarFlyout::ShowMode::Transient);
    EXPECT_EQ(showModeSpy.count(), 1);

    flyout.setAlwaysExpanded(true);
    flyout.setAlwaysExpanded(true);
    EXPECT_EQ(alwaysSpy.count(), 1);

    flyout.setExpanded(true);
    EXPECT_FALSE(flyout.isExpanded());
    EXPECT_EQ(expandedSpy.count(), 0);

    QAction primary(QStringLiteral("Primary"));
    QAction secondary(QStringLiteral("Secondary"));
    QAction appended(QStringLiteral("Appended"));
    flyout.addAction(&primary);
    ASSERT_TRUE(flyout.addSecondaryAction(&secondary));
    flyout.insertAction(&secondary, &appended);
    EXPECT_EQ(
        flyout.primaryActions(),
        (QList<QAction*>{&primary, &appended}));
    EXPECT_EQ(
        flyout.secondaryActions(),
        (QList<QAction*>{&secondary}));
}

TEST(CommandBarFlyoutTest, ShowModesDriveFocusWithoutChangingPopupDefault)
{
    FlyoutFixture sample;
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&sample.primary));
    ASSERT_TRUE(sample.flyout.addSecondaryAction(&sample.secondary));

    sample.editor.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_EQ(QApplication::focusWidget(), &sample.editor);

    QSignalSpy focusSpy(
        qApp, &QApplication::focusChanged);
    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Transient);
    processDeferredUiWork();
    EXPECT_TRUE(sample.flyout.isOpen());
    EXPECT_EQ(QApplication::focusWidget(), &sample.editor);
    EXPECT_EQ(focusSpy.count(), 0)
        << "Transient open must not cause an intermediate focus transfer";
    EXPECT_FALSE(sample.flyout.isExpanded());
    QWidget* primaryMenuDivider =
        sample.flyout.findChild<QWidget*>(
            QStringLiteral(
                "FluentCommandBarFlyout.PrimaryMenuDivider"));
    ASSERT_NE(primaryMenuDivider, nullptr);
    EXPECT_FALSE(primaryMenuDivider->isVisible());

    sample.flyout.close();
    sample.editor.setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_EQ(QApplication::focusWidget(), &sample.editor);

    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();
    EXPECT_TRUE(sample.flyout.isOpen());
    QAbstractButton* primaryPresenter = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Primary"));
    ASSERT_NE(primaryPresenter, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), primaryPresenter);
    EXPECT_TRUE(sample.flyout.isExpanded());
    EXPECT_TRUE(primaryMenuDivider->isVisible());
    EXPECT_TRUE(primaryMenuDivider->isVisibleTo(&sample.flyout));

    QAbstractButton* secondaryRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Secondary"));
    ASSERT_NE(secondaryRow, nullptr);
    EXPECT_LT(
        primaryMenuDivider->mapToGlobal(QPoint()).y(),
        secondaryRow->mapToGlobal(QPoint()).y());
}

TEST(CommandBarFlyoutTest, ExpansionStateFollowsContentAndPreferences)
{
    FlyoutFixture sample;
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&sample.primary));
    ASSERT_TRUE(sample.flyout.addSecondaryAction(&sample.secondary));

    QSignalSpy expandedSpy(
        &sample.flyout, &CommandBarFlyout::expandedChanged);
    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Transient);
    EXPECT_FALSE(sample.flyout.isExpanded());

    sample.flyout.setExpanded(true);
    EXPECT_TRUE(sample.flyout.isExpanded());
    sample.flyout.setExpanded(false);
    EXPECT_FALSE(sample.flyout.isExpanded());

    sample.flyout.setAlwaysExpanded(true);
    EXPECT_TRUE(sample.flyout.isExpanded());
    QAbstractButton* more = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.MoreButton"));
    ASSERT_NE(more, nullptr);
    EXPECT_FALSE(more->isVisible());
    sample.flyout.setExpanded(false);
    EXPECT_TRUE(sample.flyout.isExpanded());

    sample.flyout.clearSecondaryActions();
    EXPECT_FALSE(sample.flyout.isExpanded());
    EXPECT_TRUE(sample.flyout.isAlwaysExpanded());
    EXPECT_GE(expandedSpy.count(), 3);

    sample.flyout.close();
    EXPECT_FALSE(sample.flyout.isExpanded());
}

TEST(CommandBarFlyoutTest, SecondaryOnlyTransientFlyoutExpands)
{
    FlyoutFixture sample;
    ASSERT_TRUE(sample.flyout.addSecondaryAction(&sample.secondary));

    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Transient);
    EXPECT_TRUE(sample.flyout.isOpen());
    EXPECT_TRUE(sample.flyout.isExpanded());
    QWidget* primaryMenuDivider =
        sample.flyout.findChild<QWidget*>(
            QStringLiteral(
                "FluentCommandBarFlyout.PrimaryMenuDivider"));
    ASSERT_NE(primaryMenuDivider, nullptr);
    EXPECT_FALSE(primaryMenuDivider->isVisible());
    sample.flyout.setExpanded(false);
    EXPECT_TRUE(sample.flyout.isExpanded());
}

TEST(CommandBarFlyoutTest, RejectsParentlessAndCrossWindowInvocation)
{
    QWidget firstWindow;
    QWidget secondWindow;
    QPushButton firstAnchor(&firstWindow);
    QPushButton secondAnchor(&secondWindow);
    firstWindow.show();
    secondWindow.show();
    QApplication::processEvents();

    CommandBarFlyout owned(&firstWindow);
    owned.setAnimationEnabled(false);
    owned.showAt(&secondAnchor);
    EXPECT_FALSE(owned.isOpen());
    EXPECT_FALSE(owned.isVisible());

    CommandBarFlyout parentless;
    parentless.setAnimationEnabled(false);
    parentless.showAt(&firstAnchor);
    EXPECT_FALSE(parentless.isOpen());
    EXPECT_FALSE(parentless.isVisible());
}

TEST(CommandBarFlyoutTest, PointAndAnchorInvocationCanBeRetargeted)
{
    FlyoutFixture sample;
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&sample.primary));

    sample.flyout.showAtPoint(
        &sample.window,
        QPoint(460, 300),
        CommandBarFlyout::ShowMode::Transient);
    QApplication::processEvents();
    ASSERT_TRUE(sample.flyout.isOpen());
    const QPoint pointPosition = sample.flyout.pos();

    sample.flyout.showAt(&sample.anchor);
    QApplication::processEvents();
    EXPECT_TRUE(sample.flyout.isOpen());
    EXPECT_NE(sample.flyout.pos(), pointPosition);
}

TEST(CommandBarFlyoutTest,
     Contract_PresentersTrackActionStateAndActivationRestoresFocus)
{
    FlyoutFixture sample;
    QAction primary(QStringLiteral("&Open"));
    primary.setShortcut(QKeySequence::Open);
    primary.setCheckable(true);
    QAction secondary(QStringLiteral("Properties"));
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&primary));
    ASSERT_TRUE(sample.flyout.addSecondaryAction(&secondary));

    sample.editor.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();
    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

    QAbstractButton* primaryPresenter = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Open"));
    QAbstractButton* secondaryRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Properties"));
    ASSERT_NE(primaryPresenter, nullptr);
    ASSERT_NE(secondaryRow, nullptr);
    EXPECT_GE(primaryPresenter->width(), 40);
    EXPECT_GE(primaryPresenter->height(), 40);
    EXPECT_EQ(
        primaryPresenter->property("commandShortcut").toString(),
        primary.shortcut().toString(QKeySequence::NativeText));

    primary.setChecked(true);
    primary.setEnabled(false);
    primary.setText(QStringLiteral("&Open document"));
    processDeferredUiWork();
    EXPECT_TRUE(primaryPresenter->isChecked());
    EXPECT_FALSE(primaryPresenter->isEnabled());
    EXPECT_EQ(
        primaryPresenter->accessibleName(),
        QStringLiteral("Open document"));

    QSignalSpy triggerSpy(&secondary, &QAction::triggered);
    QTest::mousePress(
        secondaryRow,
        Qt::LeftButton,
        Qt::NoModifier,
        secondaryRow->rect().center());
    QTest::mouseRelease(
        secondaryRow,
        Qt::LeftButton,
        Qt::NoModifier,
        QPoint(-4, -4));
    processDeferredUiWork();
    EXPECT_EQ(triggerSpy.count(), 0);
    EXPECT_TRUE(sample.flyout.isOpen());

    QTest::mouseClick(secondaryRow, Qt::RightButton);
    processDeferredUiWork();
    EXPECT_EQ(triggerSpy.count(), 0);
    EXPECT_TRUE(sample.flyout.isOpen());

    secondaryRow->click();
    processDeferredUiWork();
    EXPECT_EQ(triggerSpy.count(), 1);
    EXPECT_FALSE(sample.flyout.isOpen());
    EXPECT_EQ(QApplication::focusWidget(), &sample.editor);
}

TEST(CommandBarFlyoutTest,
     Contract_TriggerAndActionDestructionAreDeletionSafe)
{
    QWidget window;
    window.resize(520, 260);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    auto* flyout = new CommandBarFlyout(&window);
    flyout->setAnimationEnabled(false);
    auto* deleting = new QAction(QStringLiteral("Delete flyout"));
    QPointer<CommandBarFlyout> flyoutGuard = flyout;
    QSignalSpy triggerSpy(deleting, &QAction::triggered);
    ASSERT_TRUE(flyout->addPrimaryAction(deleting));
    QObject::connect(
        deleting,
        &QAction::triggered,
        &window,
        [flyout]() { delete flyout; });
    window.show();
    flyout->showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

    QAbstractButton* presenter = commandButton(
        flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Delete flyout"));
    ASSERT_NE(presenter, nullptr);
    presenter->click();
    EXPECT_EQ(triggerSpy.count(), 1);
    EXPECT_TRUE(flyoutGuard.isNull());
    delete deleting;

    CommandBarFlyout repairing(&window);
    repairing.setAnimationEnabled(false);
    auto* first = new QAction(QStringLiteral("First"));
    auto* second = new QAction(QStringLiteral("Second"));
    ASSERT_TRUE(repairing.addPrimaryAction(first));
    ASSERT_TRUE(repairing.addPrimaryAction(second));
    repairing.showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();
    QAbstractButton* secondPresenter = commandButton(
        &repairing,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Second"));
    ASSERT_NE(secondPresenter, nullptr);
    delete first;
    processDeferredUiWork();
    secondPresenter = commandButton(
        &repairing,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Second"));
    ASSERT_NE(secondPresenter, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), secondPresenter)
        << "actual focus="
        << (QApplication::focusWidget()
                ? QApplication::focusWidget()
                      ->objectName()
                      .toStdString()
                : std::string("<null>"));
    delete second;
    processDeferredUiWork();
    EXPECT_FALSE(repairing.isOpen());
}

TEST(CommandBarFlyoutTest,
     Contract_DestroyedActionsAreDroppedBeforeCloseLayout)
{
    QWidget window;
    window.resize(520, 260);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    CommandBarFlyout flyout(&window);
    flyout.setAnimationEnabled(false);
    auto* primary = new QAction(QStringLiteral("Primary"));
    auto* secondary = new QAction(QStringLiteral("Secondary"));
    ASSERT_TRUE(flyout.addPrimaryAction(primary));
    ASSERT_TRUE(flyout.addSecondaryAction(secondary));

    window.show();
    flyout.showAt(
        &anchor, CommandBarFlyout::ShowMode::Transient);
    processDeferredUiWork();
    ASSERT_TRUE(flyout.isOpen());

    delete secondary;
    delete primary;
    EXPECT_TRUE(flyout.primaryActions().isEmpty());
    EXPECT_TRUE(flyout.secondaryActions().isEmpty());

    flyout.close();
    QApplication::processEvents();
    EXPECT_FALSE(flyout.isOpen());
}

TEST(CommandBarFlyoutTest,
     Contract_TransientPointerMoreKeepsMenuRowsUnfocusedUntilKeyboardInput)
{
    FlyoutFixture sample;
    ASSERT_TRUE(
        sample.flyout.addPrimaryAction(&sample.primary));
    ASSERT_TRUE(
        sample.flyout.addSecondaryAction(&sample.secondary));

    sample.editor.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();
    sample.flyout.showAt(
        &sample.anchor,
        CommandBarFlyout::ShowMode::Transient);
    processDeferredUiWork();

    QAbstractButton* more = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.MoreButton"));
    QAbstractButton* secondaryRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Secondary"));
    ASSERT_NE(more, nullptr);
    ASSERT_NE(secondaryRow, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), &sample.editor);

    QTest::mouseClick(more, Qt::LeftButton);
    processDeferredUiWork();
    EXPECT_TRUE(sample.flyout.isExpanded());
    EXPECT_FALSE(secondaryRow->hasFocus());
    EXPECT_NE(
        QApplication::focusWidget(), secondaryRow);

    QTest::keyClick(more, Qt::Key_Down);
    processDeferredUiWork();
    EXPECT_EQ(
        QApplication::focusWidget(), secondaryRow);
}

TEST(CommandBarFlyoutTest,
     Contract_KeyboardCycleExpansionAndEscapeRestoreFocus)
{
    FlyoutFixture sample;
    QAction disabled(QStringLiteral("Disabled"));
    disabled.setEnabled(false);
    QAction primary(QStringLiteral("Primary"));
    QAction firstSecondary(QStringLiteral("First secondary"));
    QAction disabledSecondary(QStringLiteral("Disabled secondary"));
    disabledSecondary.setEnabled(false);
    QAction lastSecondary(QStringLiteral("Last secondary"));
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&disabled));
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&primary));
    ASSERT_TRUE(
        sample.flyout.addSecondaryAction(&firstSecondary));
    ASSERT_TRUE(
        sample.flyout.addSecondaryAction(&disabledSecondary));
    ASSERT_TRUE(
        sample.flyout.addSecondaryAction(&lastSecondary));

    sample.editor.setFocus(Qt::OtherFocusReason);
    processDeferredUiWork();
    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

    QAbstractButton* primaryPresenter = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Primary"));
    QAbstractButton* more = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.MoreButton"));
    QAbstractButton* firstRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("First secondary"));
    QAbstractButton* lastRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Last secondary"));
    ASSERT_NE(primaryPresenter, nullptr);
    ASSERT_NE(more, nullptr);
    ASSERT_NE(firstRow, nullptr);
    ASSERT_NE(lastRow, nullptr);
    EXPECT_EQ(QApplication::focusWidget(), primaryPresenter);

    QTest::keyClick(primaryPresenter, Qt::Key_Tab);
    EXPECT_EQ(QApplication::focusWidget(), more);
    QTest::keyClick(more, Qt::Key_Down);
    EXPECT_EQ(QApplication::focusWidget(), firstRow);
    QTest::keyClick(firstRow, Qt::Key_Up);
    EXPECT_EQ(QApplication::focusWidget(), lastRow);
    QTest::keyClick(lastRow, Qt::Key_Home);
    EXPECT_EQ(QApplication::focusWidget(), firstRow);
    QTest::keyClick(firstRow, Qt::Key_Backtab);
    EXPECT_EQ(QApplication::focusWidget(), more);

    more->click();
    processDeferredUiWork();
    EXPECT_FALSE(sample.flyout.isExpanded());
    EXPECT_EQ(QApplication::focusWidget(), more);
    QTest::keyClick(more, Qt::Key_Down);
    processDeferredUiWork();
    EXPECT_TRUE(sample.flyout.isExpanded());
    EXPECT_EQ(QApplication::focusWidget(), firstRow);

    QTest::keyClick(firstRow, Qt::Key_Escape);
    processDeferredUiWork();
    EXPECT_FALSE(sample.flyout.isOpen());
    EXPECT_EQ(QApplication::focusWidget(), &sample.editor);
}

TEST(CommandBarFlyoutTest,
     Contract_NarrowHostUsesStablePriorityOverflowAndRtl)
{
    QWidget window;
    window.resize(820, 320);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    CommandBarFlyout flyout(&window);
    flyout.setAnimationEnabled(false);
    QAction first(QStringLiteral("First command"));
    QAction lowFirst(QStringLiteral("Low first"));
    QAction lowTail(QStringLiteral("Low tail"));
    QAction high(QStringLiteral("High command"));
    QAction secondary(QStringLiteral("Secondary"));
    lowFirst.setPriority(QAction::LowPriority);
    lowTail.setPriority(QAction::LowPriority);
    high.setPriority(QAction::HighPriority);
    ASSERT_TRUE(flyout.addPrimaryAction(&first));
    ASSERT_TRUE(flyout.addPrimaryAction(&lowFirst));
    ASSERT_TRUE(flyout.addPrimaryAction(&lowTail));
    ASSERT_TRUE(flyout.addPrimaryAction(&high));
    ASSERT_TRUE(flyout.addSecondaryAction(&secondary));
    window.show();
    flyout.showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

    QList<QAbstractButton*> overflowRows =
        rowsInVisualOrder(
            &flyout,
            QStringLiteral(
                "FluentCommandBarFlyout.OverflowRow"));
    for (int width = window.width() - 4;
         overflowRows.isEmpty() && width > 180;
         width -= 4) {
        window.resize(width, window.height());
        processDeferredUiWork();
        overflowRows = rowsInVisualOrder(
            &flyout,
            QStringLiteral(
                "FluentCommandBarFlyout.OverflowRow"));
    }
    ASSERT_EQ(overflowRows.size(), 1);
    EXPECT_EQ(
        overflowRows.first()->accessibleName(),
        QStringLiteral("Low tail"));

    QAbstractButton* secondaryRow = commandButton(
        &flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Secondary"));
    ASSERT_NE(secondaryRow, nullptr);
    EXPECT_LT(
        overflowRows.first()->mapToGlobal(QPoint()).y(),
        secondaryRow->mapToGlobal(QPoint()).y());
    EXPECT_NE(
        flyout.findChild<QWidget*>(
            QStringLiteral(
                "FluentCommandBarFlyout.GroupSeparator")),
        nullptr);

    const QString overflowedName =
        overflowRows.first()->accessibleName();
    QAbstractButton* firstPresenter = commandButton(
        &flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("First command"));
    QAbstractButton* lowFirstPresenter = commandButton(
        &flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Low first"));
    ASSERT_NE(firstPresenter, nullptr);
    ASSERT_NE(lowFirstPresenter, nullptr);
    ASSERT_TRUE(firstPresenter->isVisible());
    ASSERT_TRUE(lowFirstPresenter->isVisible());
    EXPECT_LT(firstPresenter->x(), lowFirstPresenter->x());

    flyout.setLayoutDirection(Qt::RightToLeft);
    processDeferredUiWork();
    overflowRows = rowsInVisualOrder(
        &flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.OverflowRow"));
    ASSERT_EQ(overflowRows.size(), 1);
    EXPECT_EQ(
        overflowRows.first()->accessibleName(),
        overflowedName);
    EXPECT_GT(firstPresenter->x(), lowFirstPresenter->x());
}

TEST(CommandBarFlyoutTest,
     Contract_LongSecondaryListScrollsInsideHostCard)
{
    QWidget window;
    window.resize(360, 240);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    CommandBarFlyout flyout(&window);
    flyout.setAnimationEnabled(false);
    QList<QAction*> actions;
    for (int index = 0; index < 24; ++index) {
        auto* action = new QAction(
            QStringLiteral("Secondary command %1").arg(index),
            &window);
        actions.append(action);
        ASSERT_TRUE(flyout.addSecondaryAction(action));
    }
    window.show();
    flyout.showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();
    ASSERT_TRUE(flyout.isOpen());
    ASSERT_TRUE(flyout.isExpanded());

    auto* scrollView = qobject_cast<QScrollArea*>(
        flyout.findChild<QWidget*>(
            QStringLiteral(
                "FluentCommandBarFlyout.ScrollView")));
    ASSERT_NE(scrollView, nullptr);
    EXPECT_GT(scrollView->verticalScrollBar()->maximum(), 0)
        << "scrollView=" << scrollView->size().width()
        << "x" << scrollView->size().height()
        << " viewport=" << scrollView->viewport()->size().width()
        << "x" << scrollView->viewport()->size().height()
        << " content="
        << (scrollView->widget()
                ? scrollView->widget()->size().width()
                : -1)
        << "x"
        << (scrollView->widget()
                ? scrollView->widget()->size().height()
                : -1);
    EXPECT_EQ(scrollView->horizontalScrollBar()->maximum(), 0);

    const QRect visibleCard =
        fluent::overlay::visibleCardGeometry(
            flyout.geometry());
    EXPECT_TRUE(window.rect().contains(visibleCard));
    const QList<QAbstractButton*> rows =
        rowsInVisualOrder(
            &flyout,
            QStringLiteral(
                "FluentCommandBarFlyout.SecondaryRow"));
    ASSERT_EQ(rows.size(), actions.size());
    EXPECT_GE(rows.first()->height(), 40);
}

TEST(CommandBarFlyoutTest,
     Contract_AccessibleRolesNamesAcceleratorsAndExpansion)
{
#if QT_CONFIG(accessibility)
    FlyoutFixture sample;
    QAction primary(QStringLiteral("&Copy"));
    primary.setShortcut(QKeySequence::Copy);
    primary.setCheckable(true);
    primary.setChecked(true);
    QAction secondary(QStringLiteral("Select &All"));
    ASSERT_TRUE(sample.flyout.addPrimaryAction(&primary));
    ASSERT_TRUE(sample.flyout.addSecondaryAction(&secondary));
    sample.flyout.showAt(
        &sample.anchor, CommandBarFlyout::ShowMode::Transient);
    processDeferredUiWork();

    QWidget* primaryRow = sample.flyout.findChild<QWidget*>(
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryRow"));
    QAbstractButton* primaryPresenter = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.PrimaryPresenter"),
        QStringLiteral("Copy"));
    QAbstractButton* more = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.MoreButton"));
    ASSERT_NE(primaryRow, nullptr);
    ASSERT_NE(primaryPresenter, nullptr);
    ASSERT_NE(more, nullptr);

    QAccessibleInterface* rootInterface =
        QAccessible::queryAccessibleInterface(&sample.flyout);
    QAccessibleInterface* rowInterface =
        QAccessible::queryAccessibleInterface(primaryRow);
    QAccessibleInterface* primaryInterface =
        QAccessible::queryAccessibleInterface(primaryPresenter);
    QAccessibleInterface* moreInterface =
        QAccessible::queryAccessibleInterface(more);
    ASSERT_NE(rootInterface, nullptr);
    ASSERT_NE(rowInterface, nullptr);
    ASSERT_NE(primaryInterface, nullptr);
    ASSERT_NE(moreInterface, nullptr);
    EXPECT_EQ(rootInterface->role(), QAccessible::PopupMenu);
    EXPECT_EQ(rowInterface->role(), QAccessible::ToolBar);
    EXPECT_EQ(primaryInterface->role(), QAccessible::Button);
    EXPECT_EQ(
        primaryInterface->text(QAccessible::Name),
        QStringLiteral("Copy"));
    EXPECT_EQ(
        primaryInterface->text(QAccessible::Accelerator),
        primary.shortcut().toString(QKeySequence::NativeText));
    EXPECT_TRUE(primaryInterface->state().checkable);
    EXPECT_TRUE(primaryInterface->state().checked);
    EXPECT_TRUE(moreInterface->state().expandable);
    EXPECT_TRUE(moreInterface->state().collapsed);

    sample.flyout.setExpanded(true);
    processDeferredUiWork();
    EXPECT_TRUE(moreInterface->state().expanded);
    QAbstractButton* secondaryRow = commandButton(
        &sample.flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Select All"));
    ASSERT_NE(secondaryRow, nullptr);
    QAccessibleInterface* secondaryInterface =
        QAccessible::queryAccessibleInterface(secondaryRow);
    ASSERT_NE(secondaryInterface, nullptr);
    EXPECT_EQ(
        secondaryInterface->role(), QAccessible::MenuItem);
    EXPECT_EQ(
        secondaryInterface->text(QAccessible::Name),
        QStringLiteral("Select All"));
#else
    GTEST_SKIP() << "Qt accessibility support is disabled";
#endif
}

TEST(CommandBarFlyoutTest,
     Contract_ClosedThemeRefreshKeepsSecondaryRowsVisible)
{
    QWidget window;
    window.resize(520, 320);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    CommandBarFlyout flyout(&window);
    flyout.setAnimationEnabled(false);
    QAction primary(QStringLiteral("Primary"));
    QAction secondary(QStringLiteral("Secondary"));
    ASSERT_TRUE(flyout.addPrimaryAction(&primary));
    ASSERT_TRUE(flyout.addSecondaryAction(&secondary));
    window.show();

    fluent::FluentElement::setTheme(
        fluent::FluentElement::Dark);
    flyout.onThemeUpdated();
    flyout.showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

    QAbstractButton* secondaryRow = commandButton(
        &flyout,
        QStringLiteral(
            "FluentCommandBarFlyout.SecondaryRow"),
        QStringLiteral("Secondary"));
    EXPECT_NE(secondaryRow, nullptr);
    if (secondaryRow) {
        auto* scrollView = flyout.findChild<QScrollArea*>(
            QStringLiteral(
                "FluentCommandBarFlyout.ScrollView"));
        EXPECT_NE(scrollView, nullptr);
        EXPECT_TRUE(secondaryRow->isVisible());
        EXPECT_TRUE(secondaryRow->isVisibleTo(&flyout));
        EXPECT_FALSE(secondaryRow->geometry().isEmpty());
        if (scrollView) {
            EXPECT_FALSE(
                scrollView->viewport()->autoFillBackground());
            EXPECT_EQ(
                secondaryRow->width(),
                scrollView->viewport()->width());
        }
        EXPECT_GE(secondaryRow->width(), 160);

        const QPixmap popupPixmap = flyout.grab();
        const QImage popupImage = popupPixmap.toImage();
        const qreal dpr = popupPixmap.devicePixelRatio();
        const QRect logicalTextRect(
            secondaryRow->mapTo(&flyout, QPoint(52, 8)),
            QSize(
                qMax(0, secondaryRow->width() - 64),
                qMax(0, secondaryRow->height() - 16)));
        const QRect pixelRect(
            qRound(logicalTextRect.x() * dpr),
            qRound(logicalTextRect.y() * dpr),
            qRound(logicalTextRect.width() * dpr),
            qRound(logicalTextRect.height() * dpr));
        const QColor expectedText =
            flyout.themeColors().textPrimary;
        int textPixels = 0;
        for (int y = pixelRect.top();
             y <= pixelRect.bottom()
             && y < popupImage.height();
             ++y) {
            for (int x = pixelRect.left();
                 x <= pixelRect.right()
                 && x < popupImage.width();
                 ++x) {
                if (x < 0 || y < 0)
                    continue;
                const QColor pixel = popupImage.pixelColor(x, y);
                const int distance =
                    qAbs(pixel.red() - expectedText.red())
                    + qAbs(pixel.green() - expectedText.green())
                    + qAbs(pixel.blue() - expectedText.blue());
                if (pixel.alpha() > 128 && distance < 48)
                    ++textPixels;
            }
        }
        EXPECT_GT(textPixels, 4);
    }

    flyout.close();
    fluent::ThemeRegistry::instance().resetToDefaults();
    fluent::FluentElement::setTheme(
        fluent::FluentElement::Light);
}

TEST(CommandBarFlyoutTest,
     Contract_AllDesignLanguagesAndThemesRenderAndPreserveState)
{
    QWidget window;
    window.resize(520, 320);
    QPushButton anchor(QStringLiteral("Anchor"), &window);
    anchor.setGeometry(20, 20, 120, 32);
    CommandBarFlyout flyout(&window);
    flyout.setAnimationEnabled(false);
    QAction primary(QStringLiteral("Primary"));
    QAction secondary(QStringLiteral("A long translated secondary command"));
    ASSERT_TRUE(flyout.addPrimaryAction(&primary));
    ASSERT_TRUE(flyout.addSecondaryAction(&secondary));
    window.show();
    flyout.showAt(
        &anchor, CommandBarFlyout::ShowMode::Standard);
    processDeferredUiWork();

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
            flyout.onThemeUpdated();
            processDeferredUiWork();
            const QImage image = flyout.grab().toImage();
            ASSERT_FALSE(image.isNull())
                << "language=" << language
                << " theme=" << theme;
            bool hasOpaqueNonBlackPixel = false;
            for (int y = 0;
                 y < image.height() && !hasOpaqueNonBlackPixel;
                 ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor pixel = image.pixelColor(x, y);
                    if (pixel.alpha() > 0
                        && (pixel.red() > 0
                            || pixel.green() > 0
                            || pixel.blue() > 0)) {
                        hasOpaqueNonBlackPixel = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(hasOpaqueNonBlackPixel)
                << "opaque fallback artifact for language="
                << language << " theme=" << theme;
            EXPECT_TRUE(flyout.isOpen());
            EXPECT_TRUE(flyout.isExpanded());
            EXPECT_EQ(
                flyout.primaryActions(),
                (QList<QAction*>{&primary}));
            EXPECT_EQ(
                flyout.secondaryActions(),
                (QList<QAction*>{&secondary}));
        }
    }
    fluent::ThemeRegistry::instance().resetToDefaults();
    fluent::FluentElement::setTheme(
        fluent::FluentElement::Light);
}

TEST(CommandBarFlyoutTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
        GTEST_SKIP()
            << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
    }

    using fluent::AnchorLayout;
    using fluent::basicinput::Button;
    using fluent::menus_toolbars::CommandBar;
    using fluent::textfields::Label;
    using Edge = AnchorLayout::Edge;

    auto* window = new CommandSurfaceVisualWindow();
    window->resize(920, 600);
    window->setWindowTitle(
        QStringLiteral(
            "CommandBar and CommandBarFlyout VisualCheck"));
    auto* layout = new AnchorLayout(window);
    window->setLayout(layout);

    auto* title = new Label(
        QStringLiteral(
            "Responsive CommandBar / contextual CommandBarFlyout"),
        window);
    title->setFluentTypography(
        Typography::FontRole::Subtitle);
    title->anchors()->top = {window, Edge::Top, 24};
    title->anchors()->left = {window, Edge::Left, 32};
    layout->addWidget(title);

    auto* bar = new CommandBar(window);
    bar->setAccessibleName(
        QStringLiteral("Visual review commands"));
    bar->anchors()->top = {title, Edge::Bottom, 28};
    bar->anchors()->left = {title, Edge::Left, 0};
    bar->anchors()->right = {window, Edge::Right, -32};
    layout->addWidget(bar);

    const QStringList captions{
        QStringLiteral("New"),
        QStringLiteral("Save"),
        QStringLiteral("Synchronize translated content"),
        QStringLiteral("Favorite"),
    };
    for (int index = 0; index < captions.size(); ++index) {
        auto* action =
            new QAction(captions.at(index), window);
        if (index == 2)
            action->setPriority(QAction::LowPriority);
        if (index == 3) {
            action->setCheckable(true);
            action->setChecked(true);
        }
        bar->addPrimaryAction(action);
    }
    auto* separator = new QAction(window);
    separator->setSeparator(true);
    bar->insertPrimaryAction(
        bar->primaryActions().at(2), separator);
    auto* properties =
        new QAction(QStringLiteral("Properties"), window);
    auto* archive =
        new QAction(QStringLiteral("Archive"), window);
    bar->addSecondaryAction(properties);
    bar->addSecondaryAction(archive);

    auto* standard = new Button(
        QStringLiteral("Standard flyout"), window);
    standard->setFixedSize(144, 40);
    standard->anchors()->top = {bar, Edge::Bottom, 48};
    standard->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(standard);
    auto* transient = new Button(
        QStringLiteral("Transient flyout"), window);
    transient->setFixedSize(152, 40);
    transient->anchors()->top = {
        standard, Edge::Top, 0};
    transient->anchors()->left = {
        standard, Edge::Right, 12};
    layout->addWidget(transient);
    auto* always = new Button(
        QStringLiteral("Always expanded"), window);
    always->setFixedSize(152, 40);
    always->anchors()->top = {standard, Edge::Top, 0};
    always->anchors()->left = {
        transient, Edge::Right, 12};
    layout->addWidget(always);

    auto* flyout = new CommandBarFlyout(window);
    auto* copy =
        new QAction(QStringLiteral("Copy"), window);
    copy->setShortcut(QKeySequence::Copy);
    auto* pin =
        new QAction(QStringLiteral("Pin"), window);
    pin->setCheckable(true);
    flyout->addPrimaryAction(copy);
    flyout->addPrimaryAction(pin);
    flyout->addSecondaryAction(properties);
    flyout->addSecondaryAction(archive);

    QObject::connect(
        standard,
        &Button::clicked,
        flyout,
        [flyout, standard]() {
            flyout->setAlwaysExpanded(false);
            flyout->showAt(
                standard,
                CommandBarFlyout::ShowMode::Standard);
        });
    QObject::connect(
        transient,
        &Button::clicked,
        flyout,
        [flyout, transient]() {
            flyout->setAlwaysExpanded(false);
            flyout->showAt(
                transient,
                CommandBarFlyout::ShowMode::Transient);
        });
    QObject::connect(
        always,
        &Button::clicked,
        flyout,
        [flyout, always]() {
            flyout->setAlwaysExpanded(true);
            flyout->showAt(
                always,
                CommandBarFlyout::ShowMode::Standard);
        });

    auto* theme = new Button(
        QStringLiteral("Light / Dark"), window);
    theme->setFixedSize(120, 40);
    theme->anchors()->bottom = {
        window, Edge::Bottom, -28};
    theme->anchors()->left = {title, Edge::Left, 0};
    layout->addWidget(theme);
    auto* language = new Button(
        QStringLiteral("Design language"), window);
    language->setFixedSize(148, 40);
    language->anchors()->bottom = {
        theme, Edge::Bottom, 0};
    language->anchors()->left = {
        theme, Edge::Right, 12};
    layout->addWidget(language);
    auto* direction = new Button(
        QStringLiteral("LTR / RTL"), window);
    direction->setFixedSize(112, 40);
    direction->anchors()->bottom = {
        theme, Edge::Bottom, 0};
    direction->anchors()->left = {
        language, Edge::Right, 12};
    layout->addWidget(direction);

    QObject::connect(
        theme,
        &Button::clicked,
        window,
        [window, bar, flyout]() {
            fluent::FluentElement::setTheme(
                fluent::FluentElement::currentTheme()
                        == fluent::FluentElement::Light
                    ? fluent::FluentElement::Dark
                    : fluent::FluentElement::Light);
            window->onThemeUpdated();
            bar->onThemeUpdated();
            flyout->onThemeUpdated();
        });
    QObject::connect(
        language,
        &Button::clicked,
        window,
        [window, bar, flyout]() {
            auto& registry =
                fluent::ThemeRegistry::instance();
            const auto current = registry.designLanguage();
            const auto next =
                current == fluent::FluentElement::DesignFluent
                ? fluent::FluentElement::DesignMaterial
                : current
                        == fluent::FluentElement::DesignMaterial
                    ? fluent::FluentElement::DesignCupertino
                    : fluent::FluentElement::DesignFluent;
            registry.setDesignLanguage(next);
            window->onThemeUpdated();
            bar->onThemeUpdated();
            flyout->onThemeUpdated();
        });
    QObject::connect(
        direction,
        &Button::clicked,
        window,
        [bar, flyout]() {
            const Qt::LayoutDirection next =
                bar->layoutDirection() == Qt::LeftToRight
                ? Qt::RightToLeft
                : Qt::LeftToRight;
            bar->setLayoutDirection(next);
            flyout->setLayoutDirection(next);
        });

    window->onThemeUpdated();
    window->show();
    qApp->exec();
    fluent::ThemeRegistry::instance().resetToDefaults();
    fluent::FluentElement::setTheme(
        fluent::FluentElement::Light);
    delete window;
}
