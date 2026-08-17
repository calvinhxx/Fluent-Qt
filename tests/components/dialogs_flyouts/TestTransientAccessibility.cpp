#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QVector>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/CoachMark.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/textfields/Label.h"

using fluent::basicinput::Button;
using fluent::dialogs_flyouts::CoachMark;
using fluent::dialogs_flyouts::Flyout;
using fluent::dialogs_flyouts::Popup;
using fluent::dialogs_flyouts::TeachingTip;
using fluent::textfields::Label;

namespace {

const QString& dismissAction()
{
    static const QString action = QStringLiteral("dismiss");
    return action;
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
    for (const auto& candidate : source->relations(QAccessible::AllRelations)) {
        if (candidate.first && candidate.first->object() == target
            && candidate.second == relation) {
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

class TransientAccessibilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window = new QWidget();
        window->resize(720, 520);
        window->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window));
    }

    void TearDown() override
    {
        delete window;
        window = nullptr;
    }

    Button* makeTarget(const QString& text = QStringLiteral("Open help"))
    {
        auto* target = new Button(text, window);
        target->setGeometry(40, 40, 140, 36);
        target->setAccessibleName(text);
        target->show();
        window->activateWindow();
        target->setFocus(Qt::OtherFocusReason);
        EXPECT_TRUE(QTest::qWaitFor(
            [target]() { return QApplication::focusWidget() == target; },
            1000));
        return target;
    }

    QWidget* window = nullptr;
};

} // namespace

TEST_F(TransientAccessibilityTest, Contract_AccessibilityPopupExposesLogicalStateDismissAndModal)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Button* invoker = makeTarget();
    Popup popup(window);
    popup.setAnimationEnabled(false);
    popup.setAccessibleName(QStringLiteral("Command surface"));
    popup.setAccessibleDescription(QStringLiteral("Choose an action"));

    QAccessibleInterface* root = accessible(&popup);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Pane);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Command surface"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Choose an action"));
    EXPECT_FALSE(root->state().active);
    EXPECT_TRUE(root->state().invisible);
    ASSERT_NE(root->actionInterface(), nullptr);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());

    ScopedAccessibilityEventCapture events;
    popup.open();
    EXPECT_TRUE(root->state().active);
    EXPECT_FALSE(root->state().invisible);
    EXPECT_TRUE(root->state().focused);
    EXPECT_FALSE(root->state().modal);
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        dismissAction()));
    EXPECT_EQ(root->actionInterface()->keyBindingsForAction(dismissAction()),
              QStringList{QStringLiteral("Escape")});
    EXPECT_EQ(events.count(&popup, QAccessible::ObjectShow), 1);

    events.clear();
    popup.setModal(true);
    popup.setModal(true);
    EXPECT_TRUE(root->state().modal);
    EXPECT_EQ(events.countModalState(&popup), 1);
    popup.setDim(true);
    EXPECT_EQ(events.countModalState(&popup), 1);

    QSignalSpy closing(&popup, &Popup::closing);
    events.clear();
    root->actionInterface()->doAction(dismissAction());
    ASSERT_EQ(closing.count(), 1);
    EXPECT_EQ(closing.first().first().toInt(),
              static_cast<int>(Popup::Escape));
    EXPECT_FALSE(root->state().active);
    EXPECT_TRUE(root->state().invisible);
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());
    EXPECT_EQ(events.count(&popup, QAccessible::ObjectHide), 1);
    EXPECT_EQ(QApplication::focusWidget(), invoker);

    events.clear();
    popup.close();
    EXPECT_EQ(events.count(&popup, QAccessible::ObjectHide), 0);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Command surface"));
#endif
}

TEST_F(TransientAccessibilityTest, Contract_AccessibilityFlyoutTracksAnchorRelationAndFocusReturn)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Button* first = makeTarget(QStringLiteral("First anchor"));
    auto* second = new Button(QStringLiteral("Second anchor"), window);
    second->setGeometry(220, 40, 140, 36);
    second->show();

    Flyout flyout(window);
    flyout.setAnimationEnabled(false);
    flyout.setAccessibleName(QStringLiteral("Anchor commands"));
    QAccessibleInterface* root = accessible(&flyout);
    ASSERT_NE(root, nullptr);

    ScopedAccessibilityEventCapture events;
    flyout.setAnchor(first);
    EXPECT_TRUE(hasRelation(root, first, QAccessible::Controlled));
    EXPECT_EQ(events.count(&flyout, QAccessible::ObjectReorder), 1);
    flyout.setAnchor(first);
    EXPECT_EQ(events.count(&flyout, QAccessible::ObjectReorder), 1);

    flyout.showAt(first);
    EXPECT_EQ(QApplication::focusWidget(), &flyout);
    root->actionInterface()->doAction(dismissAction());
    EXPECT_EQ(QApplication::focusWidget(), first);

    events.clear();
    flyout.setAnchor(second);
    EXPECT_TRUE(hasRelation(root, second, QAccessible::Controlled));
    EXPECT_FALSE(hasRelation(root, first, QAccessible::Controlled));
    EXPECT_EQ(events.count(&flyout, QAccessible::ObjectReorder), 1);
#endif
}

TEST_F(TransientAccessibilityTest, Contract_AccessibilityTeachingTipKeepsHelpTextChildrenAndTarget)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Button* target = makeTarget(QStringLiteral("Account settings"));
    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setAccessibleName(QStringLiteral("Account settings help"));
    tip.setAccessibleDescription(
        QStringLiteral("Review privacy before continuing"));

    auto* layout = new QVBoxLayout(tip.contentHost());
    auto* title = new Label(QStringLiteral("Privacy controls"),
                            tip.contentHost());
    auto* action = new Button(QStringLiteral("Review"), tip.contentHost());
    layout->addWidget(title);
    layout->addWidget(action);

    tip.setTarget(target);
    QAccessibleInterface* root = accessible(&tip);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::HelpBalloon);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Account settings help"));
    EXPECT_EQ(root->text(QAccessible::Description),
              QStringLiteral("Review privacy before continuing"));
    EXPECT_TRUE(hasRelation(root, target, QAccessible::DescriptionFor));
    EXPECT_TRUE(hasAccessibleAncestor(title, &tip));
    EXPECT_TRUE(hasAccessibleAncestor(action, &tip));

    tip.setLightDismissEnabled(false);
    tip.showAt(target);
    EXPECT_FALSE(root->actionInterface()->actionNames().contains(
        dismissAction()));
    tip.close();

    tip.setLightDismissEnabled(true);
    ScopedAccessibilityEventCapture events;
    QSignalSpy closing(&tip, &TeachingTip::closing);
    tip.showAt(target);
    EXPECT_EQ(events.count(&tip, QAccessible::ContextHelpStart), 1);
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        dismissAction()));
    root->actionInterface()->doAction(dismissAction());
    ASSERT_EQ(closing.count(), 1);
    EXPECT_EQ(closing.first().first().toInt(),
              static_cast<int>(TeachingTip::LightDismiss));
    EXPECT_EQ(events.count(&tip, QAccessible::ContextHelpEnd), 1);
    EXPECT_EQ(QApplication::focusWidget(), target);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Account settings help"));
#endif
}

TEST_F(TransientAccessibilityTest, Contract_AccessibilityCoachMarkAnnouncesRetainsFocusAndDismisses)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Button* target = makeTarget(QStringLiteral("Search"));
    CoachMark coach(window);
    coach.setAccessibleName(QStringLiteral("Search help"));
    coach.setAccessibleDescription(
        QStringLiteral("Find components. Step 2 of 4."));
    coach.setTarget(target);

    auto* layout = new QVBoxLayout(coach.contentHost());
    auto* next = new Button(QStringLiteral("Next"), coach.contentHost());
    layout->addWidget(next);

    QAccessibleInterface* root = accessible(&coach);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::HelpBalloon);
    EXPECT_TRUE(hasRelation(root, target, QAccessible::DescriptionFor));
    EXPECT_TRUE(hasAccessibleAncestor(next, &coach));

    ScopedAccessibilityEventCapture events;
    QSignalSpy opened(&coach, &CoachMark::opened);
    QSignalSpy closed(&coach, &CoachMark::closed);
    coach.open();
    EXPECT_TRUE(root->state().active);
    EXPECT_FALSE(root->state().focusable);
    EXPECT_EQ(QApplication::focusWidget(), target);
    EXPECT_TRUE(root->actionInterface()->actionNames().contains(
        dismissAction()));
    EXPECT_EQ(events.count(&coach, QAccessible::ContextHelpStart), 1);
    EXPECT_EQ(events.count(&coach, QAccessible::Alert), 1);
    ASSERT_TRUE(QTest::qWaitFor(
        [&]() { return opened.count() == 1; }, 1000));

    events.clear();
    QTest::keyClick(target, Qt::Key_Escape);
    EXPECT_FALSE(coach.isOpen());
    EXPECT_FALSE(root->state().active);
    EXPECT_EQ(QApplication::focusWidget(), target);
    EXPECT_EQ(events.count(&coach, QAccessible::ContextHelpEnd), 1);
    ASSERT_TRUE(QTest::qWaitFor(
        [&]() { return closed.count() == 1; }, 1000));
    EXPECT_TRUE(root->actionInterface()->actionNames().isEmpty());

    events.clear();
    coach.close();
    EXPECT_EQ(events.count(&coach, QAccessible::ContextHelpEnd), 0);
#endif
}

TEST_F(TransientAccessibilityTest, Contract_AccessibilityTransientRelationAndPolicyNoOpsStaySilent)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    Button* target = makeTarget();
    TeachingTip tip(window);
    tip.setAnimationEnabled(false);
    tip.setTarget(target);
    tip.setLightDismissEnabled(true);
    tip.showAt(target);

    ScopedAccessibilityEventCapture events;
    tip.setTarget(target);
    tip.setLightDismissEnabled(true);
    tip.setClosePolicy(tip.closePolicy());
    tip.setModal(tip.isModal());
    tip.setDim(tip.isDim());
    tip.setIsOpen(true);
    EXPECT_EQ(events.count(&tip, QAccessible::ObjectReorder), 0);
    EXPECT_EQ(events.count(&tip, QAccessible::ActionChanged), 0);
    EXPECT_EQ(events.count(&tip, QAccessible::ContextHelpStart), 0);
    EXPECT_EQ(events.count(&tip, QAccessible::StateChanged), 0);

    tip.close();
#endif
}
