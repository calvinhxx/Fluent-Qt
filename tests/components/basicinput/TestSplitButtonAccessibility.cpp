#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include "components/basicinput/SplitButton.h"
#include "components/basicinput/ToggleSplitButton.h"
#include "QtTestEnvironment.h"

using fluent::basicinput::SplitButton;
using fluent::basicinput::ToggleSplitButton;

namespace {

void showAndProcess(QWidget& widget, const QSize& size)
{
    widget.resize(size);
    widget.show();
    QApplication::processEvents();
}

#if QT_CONFIG(accessibility)

struct AccessibleEventRecord {
    QObject* object = nullptr;
    QAccessible::Event type = QAccessible::InvalidEvent;
    QAccessible::State changedState{};
};

QVector<AccessibleEventRecord> g_accessibilityEvents;

void captureAccessibilityEvent(QAccessibleEvent* event)
{
    if (event) {
        AccessibleEventRecord record;
        record.object = event->object();
        record.type = event->type();
        if (event->type() == QAccessible::StateChanged) {
            record.changedState =
                static_cast<QAccessibleStateChangeEvent*>(event)
                    ->changedStates();
        }
        g_accessibilityEvents.append(record);
    }
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

    int count(QObject* object, QAccessible::Event type) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object && event.type == type)
                ++result;
        }
        return result;
    }

    int countPopupState(QObject* object) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object
                && event.type == QAccessible::StateChanged
                && (event.changedState.hasPopup
                    || event.changedState.expandable)) {
                ++result;
            }
        }
        return result;
    }

    int countExpandedState(QObject* object) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object
                && event.type == QAccessible::StateChanged
                && (event.changedState.expanded
                    || event.changedState.collapsed)) {
                ++result;
            }
        }
        return result;
    }

    int countCheckedState(QObject* object) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object
                && event.type == QAccessible::StateChanged
                && event.changedState.checked) {
                ++result;
            }
        }
        return result;
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

TEST(SplitButtonAccessibilityTest, Contract_AccessibilitySplitButtonExposesPrimaryAndMenuActions)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    SplitButton button(QStringLiteral("&Run"));
    QMenu menu(QStringLiteral("More"), &button);
    menu.addAction(QStringLiteral("Run later"));
    button.setMenu(&menu);
    showAndProcess(button, QSize(160, 36));

    QAccessibleInterface* root = accessible(&button);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ButtonMenu);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Run"));
    EXPECT_TRUE(root->state().hasPopup);
    EXPECT_TRUE(root->state().expandable);
    EXPECT_TRUE(root->state().collapsed);
    EXPECT_FALSE(root->state().expanded);
    EXPECT_FALSE(root->state().checkable);
    EXPECT_EQ(root->childCount(), 0);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::toggleAction()));
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::pressAction()),
              QStringList{QStringLiteral("Space")});
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::showMenuAction()),
              (QStringList{QStringLiteral("Alt+Down"),
                           QStringLiteral("F4")}));

    QSignalSpy clicked(&button, &SplitButton::clicked);
    QSignalSpy menuShown(&menu, &QMenu::aboutToShow);
    actions->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(menuShown.count(), 0);

    actions->doAction(QAccessibleActionInterface::showMenuAction());
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(menuShown.count(), 1);
    EXPECT_TRUE(root->state().expanded);
    EXPECT_FALSE(root->state().collapsed);
    menu.close();
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);

    button.setAccessibleName(QStringLiteral("Build project"));
    button.setAccessibleDescription(QStringLiteral("Default build command"));
    button.setText(QStringLiteral("Changed text"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Build project"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Default build command"));
#endif
}

TEST(SplitButtonAccessibilityTest, Contract_AccessibilityToggleSplitButtonKeepsToggleAndMenuDistinct)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    ToggleSplitButton button(QStringLiteral("Pin"));
    QMenu menu(QStringLiteral("Pin options"), &button);
    menu.addAction(QStringLiteral("Pin to top"));
    button.setMenu(&menu);
    showAndProcess(button, QSize(160, 36));

    QAccessibleInterface* root = accessible(&button);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ButtonMenu);
    EXPECT_TRUE(root->state().checkable);
    EXPECT_FALSE(root->state().checked);
    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::toggleAction()));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::pressAction()));

    QSignalSpy clicked(&button, &ToggleSplitButton::clicked);
    QSignalSpy toggled(&button, &ToggleSplitButton::toggled);
    actions->doAction(QAccessibleActionInterface::toggleAction());
    EXPECT_TRUE(button.isChecked());
    EXPECT_TRUE(root->state().checked);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(toggled.count(), 1);

    actions->doAction(QAccessibleActionInterface::showMenuAction());
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    EXPECT_TRUE(button.isChecked());
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(toggled.count(), 1);
    menu.close();
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
#endif
}

TEST(SplitButtonAccessibilityTest, Contract_AccessibilityMenuAvailabilityDisabledStateAndKeyboardStayAligned)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    SplitButton button(QStringLiteral("Export"));
    showAndProcess(button, QSize(160, 36));
    QAccessibleInterface* root = accessible(&button);
    ASSERT_NE(root, nullptr);
    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_FALSE(root->state().hasPopup);
    EXPECT_FALSE(root->state().expandable);
    EXPECT_FALSE(root->state().collapsed);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));

    auto* menu = new QMenu(QStringLiteral("Formats"), &button);
    menu->addAction(QStringLiteral("PDF"));
    button.setMenu(menu);
    EXPECT_TRUE(root->state().hasPopup);
    EXPECT_TRUE(root->state().collapsed);

    button.setEnabled(false);
    EXPECT_TRUE(actions->actionNames().isEmpty());
    button.setEnabled(true);
    button.setFocus(Qt::OtherFocusReason);
    QTest::keyClick(&button, Qt::Key_Down, Qt::AltModifier);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    menu->close();
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);

    QTest::keyClick(&button, Qt::Key_F4);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    menu->close();
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);

    delete menu;
    EXPECT_EQ(button.menu(), nullptr);
    EXPECT_FALSE(root->state().hasPopup);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
#endif
}

TEST(SplitButtonAccessibilityTest, Contract_AccessibilitySplitStateEventsFollowEffectiveChangesAndNoOps)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    SplitButton button(QStringLiteral("Run"));
    QMenu first(QStringLiteral("First"), &button);
    QMenu second(QStringLiteral("Second"), &button);
    first.addAction(QStringLiteral("First action"));
    second.addAction(QStringLiteral("Second action"));
    showAndProcess(button, QSize(160, 36));
    QAccessibleInterface* root = accessible(&button);
    ASSERT_NE(root, nullptr);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture capture;
    button.setMenu(&first);
    EXPECT_EQ(capture.countPopupState(&button), 1);
    EXPECT_EQ(capture.countExpandedState(&button), 1);
    EXPECT_EQ(capture.count(&button, QAccessible::ActionChanged), 1);
    button.setMenu(&first);
    EXPECT_EQ(capture.countPopupState(&button), 1);
    EXPECT_EQ(capture.countExpandedState(&button), 1);
    EXPECT_EQ(capture.count(&button, QAccessible::ActionChanged), 1);

    root->actionInterface()->doAction(
        QAccessibleActionInterface::showMenuAction());
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    EXPECT_EQ(capture.countExpandedState(&button), 2);
    root->actionInterface()->doAction(
        QAccessibleActionInterface::showMenuAction());
    QApplication::processEvents();
    EXPECT_EQ(capture.countExpandedState(&button), 2);
    first.hide();
    QApplication::processEvents();
    EXPECT_EQ(capture.countExpandedState(&button), 3);
    first.hide();
    QApplication::processEvents();
    EXPECT_EQ(capture.countExpandedState(&button), 3);

    capture.clear();
    button.setMenu(&second);
    EXPECT_EQ(capture.countPopupState(&button), 0);
    EXPECT_EQ(capture.countExpandedState(&button), 0);
    EXPECT_EQ(capture.count(&button, QAccessible::ActionChanged), 1);
    button.setMenu(nullptr);
    EXPECT_EQ(capture.countPopupState(&button), 1);
    EXPECT_EQ(capture.countExpandedState(&button), 1);
    EXPECT_EQ(capture.count(&button, QAccessible::ActionChanged), 2);

    ToggleSplitButton toggle(QStringLiteral("Pin"));
    ASSERT_NE(accessible(&toggle), nullptr);
    capture.clear();
    toggle.setChecked(true);
    EXPECT_EQ(capture.countCheckedState(&toggle), 1);
    toggle.setChecked(true);
    EXPECT_EQ(capture.countCheckedState(&toggle), 1);
#endif
}
