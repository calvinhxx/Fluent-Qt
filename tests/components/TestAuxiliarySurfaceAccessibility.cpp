#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QVector>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/basicinput/DropDownButton.h"
#include "components/collections/DrawerView.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "compatibility/QtCompat.h"
#include "QtTestEnvironment.h"

using fluent::basicinput::Button;
using fluent::basicinput::DropDownButton;
using fluent::collections::DrawerView;
using fluent::status_info::ToolTip;
using fluent::textfields::Label;

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
}

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
    if (!event)
        return;
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

    int countModalState(QObject* object) const
    {
        int result = 0;
        for (const AccessibleEventRecord& event : g_accessibilityEvents) {
            if (event.object == object
                && event.type == QAccessible::StateChanged
                && event.changedState.modal) {
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

bool hasRelation(QAccessibleInterface* source, QWidget* target,
                 QAccessible::Relation relation)
{
    if (!source || !target)
        return false;
    QAccessibleInterface* expected = accessible(target);
    for (const auto& candidate : source->relations(QAccessible::AllRelations)) {
        if (candidate.first == expected
            && (candidate.second & relation) == relation) {
            return true;
        }
    }
    return false;
}

bool hasAccessibleAncestor(QWidget* child, QWidget* ancestor)
{
    QAccessibleInterface* current = accessible(child);
    for (int depth = 0; current && depth < 16; ++depth) {
        if (current->object() == ancestor)
            return true;
        current = current->parent();
    }
    return false;
}

#endif

} // namespace

TEST(AuxiliarySurfaceAccessibilityTest, Contract_AccessibilityDropDownButtonExposesOneMenuActionAndKeyboardPath)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(420, 240));
    DropDownButton button(QStringLiteral("&Options"), &window);
    button.setGeometry(24, 24, 140, 36);
    button.show();
    button.setFocus(Qt::OtherFocusReason);

    QMenu first(QStringLiteral("Options"), &button);
    first.addAction(QStringLiteral("Open"));
    QMenu second(QStringLiteral("More options"), &button);
    second.addAction(QStringLiteral("Save"));

    QAccessibleInterface* root = accessible(&button);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ButtonMenu);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Options"));
    EXPECT_FALSE(root->state().hasPopup);
    ASSERT_NE(root->actionInterface(), nullptr);
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::pressAction()));

    QSignalSpy clicked(&button, &QPushButton::clicked);
    QTest::keyClick(&button, Qt::Key_Space);
    EXPECT_EQ(clicked.count(), 1);
    clicked.clear();

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    button.setMenu(&first);
    EXPECT_TRUE(root->state().hasPopup);
    EXPECT_TRUE(root->state().expandable);
    EXPECT_TRUE(root->state().collapsed);
    EXPECT_FALSE(root->state().expanded);
    EXPECT_EQ(events.countPopupState(&button), 1);
    EXPECT_EQ(events.count(&button, QAccessible::ActionChanged), 1);
    button.setMenu(&first);
    EXPECT_EQ(events.countPopupState(&button), 1);
    EXPECT_EQ(events.count(&button, QAccessible::ActionChanged), 1);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::showMenuAction()));
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::showMenuAction()),
              (QStringList{QStringLiteral("Space"),
                           QStringLiteral("Enter"),
                           QStringLiteral("Alt+Down"),
                           QStringLiteral("F4")}));

    actions->doAction(QAccessibleActionInterface::showMenuAction());
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    EXPECT_TRUE(root->state().expanded);
    EXPECT_EQ(events.countExpandedState(&button), 2);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    EXPECT_TRUE(button.hasFocus());
    EXPECT_EQ(events.countExpandedState(&button), 3);

    QTest::keyClick(&button, Qt::Key_Space);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    QTest::keyClick(&button, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    QTest::keyClick(&button, Qt::Key_Enter);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    QTest::keyClick(&button, Qt::Key_Down, Qt::AltModifier);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    QTest::keyClick(&button, Qt::Key_F4);
    QTRY_VERIFY_WITH_TIMEOUT(button.isOpen(), 1000);
    QTest::keyClick(&first, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!button.isOpen(), 1000);
    EXPECT_EQ(clicked.count(), 0);

    events.clear();
    button.setMenu(&second);
    EXPECT_EQ(events.countPopupState(&button), 0);
    EXPECT_EQ(events.countExpandedState(&button), 0);
    EXPECT_EQ(events.count(&button, QAccessible::ActionChanged), 1);
    button.setMenu(nullptr);
    EXPECT_FALSE(root->state().hasPopup);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
    EXPECT_EQ(events.countPopupState(&button), 1);

    button.setAccessibleName(QStringLiteral("Project options"));
    button.setText(QStringLiteral("Changed"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Project options"));
#endif
}

TEST(AuxiliarySurfaceAccessibilityTest, Contract_AccessibilityDrawerExposesPaneStateDismissAndFocusReturn)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(720, 520));
    Button invoker(QStringLiteral("Open settings"), &window);
    invoker.setGeometry(24, 24, 140, 36);
    invoker.show();
    invoker.setFocus(Qt::OtherFocusReason);
    ASSERT_EQ(QApplication::focusWidget(), &invoker);

    DrawerView drawer(&window);
    drawer.setAccessibleName(QStringLiteral("Quick settings"));
    drawer.setAnimationEnabled(false);
    drawer.setModal(false);
    drawer.setDim(false);
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* heading = new Label(QStringLiteral("Quick settings"), content);
    auto* apply = new Button(QStringLiteral("Apply"), content);
    layout->addWidget(heading);
    layout->addWidget(apply);
    ASSERT_TRUE(drawer.setContentWidget(content,
                                        fluent::WidgetOwnership::Owned));

    QAccessibleInterface* root = accessible(&drawer);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Pane);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Quick settings"));
    EXPECT_FALSE(root->state().active);
    EXPECT_TRUE(root->state().invisible);
    EXPECT_TRUE(root->state().collapsed);
    EXPECT_FALSE(root->state().expanded);
    EXPECT_FALSE(root->state().modal);
    ASSERT_NE(root->actionInterface(), nullptr);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    drawer.open();
    EXPECT_TRUE(root->state().active);
    EXPECT_FALSE(root->state().invisible);
    EXPECT_TRUE(root->state().expanded);
    EXPECT_FALSE(root->state().collapsed);
    EXPECT_EQ(QApplication::focusWidget(), &drawer);
    EXPECT_TRUE(hasAccessibleAncestor(heading, &drawer));
    EXPECT_TRUE(hasAccessibleAncestor(apply, &drawer));
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        dismissAction()));
    EXPECT_EQ(root->actionInterface()->keyBindingsForAction(dismissAction()),
              QStringList{QStringLiteral("Escape")});
    EXPECT_EQ(events.countExpandedState(&drawer), 1);

    events.clear();
    drawer.setModal(true);
    drawer.setModal(true);
    EXPECT_TRUE(root->state().modal);
    EXPECT_EQ(events.countModalState(&drawer), 1);

    events.clear();
    QTest::keyClick(&drawer, Qt::Key_Escape);
    EXPECT_FALSE(drawer.isOpen());
    EXPECT_TRUE(root->state().invisible);
    EXPECT_TRUE(root->state().collapsed);
    EXPECT_EQ(QApplication::focusWidget(), &invoker);
    EXPECT_EQ(events.countExpandedState(&drawer), 1);

    drawer.setClosePolicy(DrawerView::ClosePolicy(
        DrawerView::NoAutoClose));
    drawer.open();
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());
    drawer.close();
#endif
}

TEST(AuxiliarySurfaceAccessibilityTest, Contract_AccessibilityToolTipExposesTextOwnerAndLogicalLifecycle)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(420, 240));
    Button target(QStringLiteral("Save"), &window);
    target.setGeometry(24, 24, 100, 36);
    target.show();

    ToolTip* tip = ToolTip::attach(
        &target, QStringLiteral("Save current document"));
    ASSERT_NE(tip, nullptr);
    tip->setAnimationEnabled(false);
    QAccessibleInterface* root = accessible(tip);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::ToolTip);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Save current document"));
    EXPECT_TRUE(root->state().invisible);
    EXPECT_FALSE(root->state().focusable);
#if FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION
    EXPECT_TRUE(hasRelation(root, &target,
                            QAccessible::DescriptionFor));
#endif

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    tip->setText(QStringLiteral("Save all changes"));
    tip->setText(QStringLiteral("Save all changes"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Save all changes"));
    EXPECT_EQ(events.count(tip, QAccessible::NameChanged), 1);

    events.clear();
    tip->show();
    QApplication::processEvents();
    EXPECT_TRUE(root->state().active);
    EXPECT_FALSE(root->state().invisible);
    EXPECT_EQ(events.count(tip, QAccessible::StateChanged), 1);

    events.clear();
    tip->hide();
    QApplication::processEvents();
    EXPECT_FALSE(root->state().active);
    EXPECT_TRUE(root->state().invisible);
    EXPECT_EQ(events.count(tip, QAccessible::StateChanged), 1);

    tip->setAccessibleName(QStringLiteral("Document save help"));
    events.clear();
    tip->setText(QStringLiteral("Changed visible help"));
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Document save help"));
    EXPECT_EQ(events.count(tip, QAccessible::NameChanged), 0);

    events.clear();
    EXPECT_EQ(ToolTip::attach(
                  &target, QStringLiteral("Changed visible help")),
              tip);
#if FLUENT_HAS_ACCESSIBLE_DESCRIPTION_RELATION
    EXPECT_TRUE(hasRelation(root, &target,
                            QAccessible::DescriptionFor));
#endif
    EXPECT_EQ(events.count(tip, QAccessible::ObjectReorder), 0);
#endif
}
