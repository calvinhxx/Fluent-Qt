#include <gtest/gtest.h>

#include <QAccessible>
#include <QApplication>
#include <QTest>
#include <QVector>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "components/collections/FlipView.h"
#include "components/collections/SplitView.h"
#include "QtTestEnvironment.h"

using fluent::collections::FlipView;
using fluent::collections::SplitView;
using fluent::collections::SplitViewPaneOptions;
using fluent::basicinput::Button;

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
};

QVector<AccessibleEventRecord> g_accessibilityEvents;

void captureAccessibilityEvent(QAccessibleEvent* event)
{
    if (event) {
        g_accessibilityEvents.append(
            AccessibleEventRecord{event->object(), event->type()});
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

    void clear() { g_accessibilityEvents.clear(); }

private:
    QAccessible::UpdateHandler m_previous = nullptr;
};

QAccessibleInterface* accessible(QWidget* widget)
{
    return widget ? QAccessible::queryAccessibleInterface(widget) : nullptr;
}

QList<QAccessibleInterface*> childrenWithRole(
    QAccessibleInterface* root, QAccessible::Role role)
{
    QList<QAccessibleInterface*> result;
    if (!root)
        return result;
    for (int index = 0; index < root->childCount(); ++index) {
        QAccessibleInterface* child = root->child(index);
        if (child && child->role() == role)
            result.append(child);
    }
    return result;
}

QAccessibleInterface* childForObject(
    QAccessibleInterface* root, QObject* object)
{
    if (!root || !object)
        return nullptr;
    for (int index = 0; index < root->childCount(); ++index) {
        QAccessibleInterface* child = root->child(index);
        if (child && child->object() == object)
            return child;
    }
    return nullptr;
}

#endif

SplitViewPaneOptions paneOptions(int minimumSize, int preferredSize,
                                 int maximumSize, bool fill = false)
{
    SplitViewPaneOptions options;
    options.minimumSize = minimumSize;
    options.preferredSize = preferredSize;
    options.maximumSize = maximumSize;
    options.fill = fill;
    return options;
}

} // namespace

TEST(CollectionSurfaceAccessibilityTest, Contract_AccessibilityFlipViewExposesCurrentPageValueContentAndActions)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(640, 420));

    FlipView view(&window);
    view.setAccessibleName(QStringLiteral("Featured projects"));
    view.setGeometry(20, 20, 480, 300);
    auto* alpha = new QWidget;
    auto* beta = new QWidget;
    auto* gamma = new QWidget;
    alpha->setAccessibleName(QStringLiteral("Alpha"));
    beta->setAccessibleName(QStringLiteral("Beta"));
    gamma->setAccessibleName(QStringLiteral("Gamma"));
    ASSERT_TRUE(view.addPage(alpha, fluent::WidgetOwnership::Owned));
    ASSERT_TRUE(view.addPage(beta, fluent::WidgetOwnership::Owned));
    ASSERT_TRUE(view.addPage(gamma, fluent::WidgetOwnership::Owned));
    view.show();
    QApplication::processEvents();

    QAccessibleInterface* root = accessible(&view);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::LayeredPane);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Featured projects"));
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("Page 1 of 3: Alpha"));
    ASSERT_EQ(root->childCount(), 3);
    EXPECT_EQ(root->child(0)->object(), alpha);
    EXPECT_EQ(root->child(1)->object(), beta);
    EXPECT_EQ(root->child(2)->object(), gamma);
    EXPECT_FALSE(root->child(0)->state().invisible);
    EXPECT_TRUE(root->child(1)->state().invisible);

    QAccessibleValueInterface* value = root->valueInterface();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->currentValue().toInt(), 1);
    EXPECT_EQ(value->minimumValue().toInt(), 1);
    EXPECT_EQ(value->maximumValue().toInt(), 3);
    EXPECT_EQ(value->minimumStepSize().toInt(), 1);

    QAccessibleActionInterface* actions = root->actionInterface();
    ASSERT_NE(actions, nullptr);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::decreaseAction()));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::increaseAction()),
              QStringList{QStringLiteral("Right")});

    actions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_EQ(view.currentIndex(), 1);
    EXPECT_EQ(value->currentValue().toInt(), 2);
    EXPECT_EQ(root->text(QAccessible::Value),
              QStringLiteral("Page 2 of 3: Beta"));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::decreaseAction()));

    view.setOrientation(Qt::Vertical);
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::increaseAction()),
              QStringList{QStringLiteral("Down")});
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::decreaseAction()),
              QStringList{QStringLiteral("Up")});
    value->setCurrentValue(3);
    EXPECT_EQ(view.currentIndex(), 2);
    EXPECT_FALSE(actions->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));

    view.setEnabled(false);
    EXPECT_TRUE(actions->actionNames().isEmpty());
    EXPECT_TRUE(root->state().disabled);
#endif
}

TEST(CollectionSurfaceAccessibilityTest, Contract_AccessibilityFlipViewEventsFollowEffectivePageChanges)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    FlipView view;
    auto* first = new QWidget;
    first->setAccessibleName(QStringLiteral("First"));
    ASSERT_TRUE(view.addPage(first, fluent::WidgetOwnership::Owned));
    showAndProcess(view, QSize(400, 260));
    ASSERT_NE(accessible(&view), nullptr);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    view.setCurrentIndex(0);
    EXPECT_EQ(events.count(&view, QAccessible::ValueChanged), 0);

    auto* second = new QWidget;
    second->setAccessibleName(QStringLiteral("Second"));
    ASSERT_TRUE(view.addPage(second, fluent::WidgetOwnership::Owned));
    EXPECT_EQ(events.count(&view, QAccessible::ObjectReorder), 1);
    EXPECT_EQ(events.count(&view, QAccessible::ValueChanged), 1);
    EXPECT_EQ(events.count(&view, QAccessible::ActionChanged), 1);

    events.clear();
    view.setCurrentIndex(1);
    EXPECT_EQ(events.count(&view, QAccessible::ValueChanged), 1);
    EXPECT_EQ(events.count(&view, QAccessible::ActionChanged), 1);
    view.setCurrentIndex(1);
    EXPECT_EQ(events.count(&view, QAccessible::ValueChanged), 1);
    EXPECT_EQ(events.count(&view, QAccessible::ActionChanged), 1);

    events.clear();
    view.setOrientation(Qt::Vertical);
    view.setOrientation(Qt::Vertical);
    EXPECT_EQ(events.count(&view, QAccessible::ActionChanged), 1);

    view.setAccessibleName(QStringLiteral("Caller carousel"));
    view.setCurrentIndex(0);
    EXPECT_EQ(accessible(&view)->text(QAccessible::Name),
              QStringLiteral("Caller carousel"));
#endif
}

TEST(CollectionSurfaceAccessibilityTest, Contract_AccessibilitySplitViewExposesPanesAndKeyboardResizableGrips)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    QWidget window;
    showAndProcess(window, QSize(760, 420));

    Button before(QStringLiteral("Before splitter"), &window);
    before.setGeometry(20, 320, 140, 36);
    before.show();
    SplitView split(&window);
    split.setAccessibleName(QStringLiteral("Workspace layout"));
    split.setGeometry(20, 20, 640, 280);
    auto* navigation = new QWidget;
    auto* content = new QWidget;
    auto* inspector = new QWidget;
    navigation->setAccessibleName(QStringLiteral("Navigation"));
    content->setAccessibleName(QStringLiteral("Content"));
    inspector->setAccessibleName(QStringLiteral("Inspector"));
    ASSERT_EQ(split.addPane(navigation, fluent::WidgetOwnership::Owned,
                            paneOptions(80, 160, 300)), 0);
    ASSERT_EQ(split.addPane(content, fluent::WidgetOwnership::Owned,
                            paneOptions(80, 160, 300)), 1);
    ASSERT_EQ(split.addPane(inspector, fluent::WidgetOwnership::Owned,
                            paneOptions(80, 160, 600, true)), 2);
    split.show();
    QApplication::processEvents();

    QAccessibleInterface* root = accessible(&split);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::Splitter);
    EXPECT_EQ(root->text(QAccessible::Name),
              QStringLiteral("Workspace layout"));
    EXPECT_NE(childForObject(root, navigation), nullptr);
    EXPECT_NE(childForObject(root, content), nullptr);
    EXPECT_NE(childForObject(root, inspector), nullptr);

    QList<QAccessibleInterface*> grips =
        childrenWithRole(root, QAccessible::Grip);
    ASSERT_EQ(grips.size(), 2);
    QAccessibleInterface* firstGrip = grips.at(0);
    EXPECT_EQ(firstGrip->text(QAccessible::Name),
              QStringLiteral("Resize Navigation and Content"));
    EXPECT_TRUE(firstGrip->state().focusable);
    EXPECT_TRUE(firstGrip->state().sizeable);
    ASSERT_NE(firstGrip->valueInterface(), nullptr);
    ASSERT_NE(firstGrip->actionInterface(), nullptr);
    QAccessibleValueInterface* value = firstGrip->valueInterface();
    QAccessibleActionInterface* actions = firstGrip->actionInterface();
    EXPECT_EQ(value->currentValue().toInt(), 160);
    EXPECT_EQ(value->minimumValue().toInt(), 80);
    EXPECT_EQ(value->maximumValue().toInt(), 240);
    EXPECT_EQ(value->minimumStepSize().toInt(), 1);
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::increaseAction()));
    EXPECT_TRUE(actions->actionNames().contains(
        QAccessibleActionInterface::decreaseAction()));
    EXPECT_EQ(actions->keyBindingsForAction(
                  QAccessibleActionInterface::increaseAction()),
              (QStringList{QStringLiteral("Right"),
                           QStringLiteral("Shift+Right")}));

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    QObject* firstGripObject = firstGrip->object();
    ASSERT_NE(firstGripObject, nullptr);
    actions->doAction(QAccessibleActionInterface::increaseAction());
    EXPECT_EQ(value->currentValue().toInt(), 168);
    EXPECT_EQ(events.count(firstGripObject, QAccessible::ValueChanged), 1);

    auto* handleWidget = qobject_cast<QWidget*>(firstGripObject);
    ASSERT_NE(handleWidget, nullptr);
    before.setFocus(Qt::OtherFocusReason);
    QTest::keyClick(&before, Qt::Key_Tab);
    EXPECT_EQ(QApplication::focusWidget(), handleWidget);
    handleWidget->setFocus(Qt::TabFocusReason);
    EXPECT_TRUE(handleWidget->hasFocus());
    ASSERT_NE(root->focusChild(), nullptr);
    EXPECT_EQ(root->focusChild()->object(), firstGripObject);
    QTest::keyClick(handleWidget, Qt::Key_Left, Qt::ShiftModifier);
    EXPECT_EQ(value->currentValue().toInt(), 167);

    split.setEnabled(false);
    EXPECT_TRUE(firstGrip->state().disabled);
    EXPECT_TRUE(actions->actionNames().isEmpty());
#endif
}

TEST(CollectionSurfaceAccessibilityTest, Contract_AccessibilitySplitViewStructureTracksVisiblePanesWithoutNoOpNoise)
{
#if !QT_CONFIG(accessibility)
    GTEST_SKIP() << "Qt accessibility support is disabled";
#else
    SplitView split;
    auto* first = new QWidget;
    auto* second = new QWidget;
    auto* third = new QWidget;
    first->setAccessibleName(QStringLiteral("First"));
    second->setAccessibleName(QStringLiteral("Second"));
    third->setAccessibleName(QStringLiteral("Third"));
    split.addPane(first, fluent::WidgetOwnership::Owned,
                  paneOptions(40, 120, 300));
    split.addPane(second, fluent::WidgetOwnership::Owned,
                  paneOptions(40, 120, 300));
    split.addPane(third, fluent::WidgetOwnership::Owned,
                  paneOptions(40, 120, 500, true));
    showAndProcess(split, QSize(560, 260));

    QAccessibleInterface* root = accessible(&split);
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(childrenWithRole(root, QAccessible::Grip).size(), 2);

    FLUENT_REQUIRE_ACCESSIBLE_EVENT_CAPTURE();
    ScopedAccessibilityEventCapture events;
    second->hide();
    QApplication::processEvents();
    EXPECT_EQ(childrenWithRole(root, QAccessible::Grip).size(), 1);
    ASSERT_NE(childForObject(root, second), nullptr);
    EXPECT_TRUE(childForObject(root, second)->state().invisible);
    EXPECT_EQ(events.count(&split, QAccessible::ObjectReorder), 1);

    second->hide();
    QApplication::processEvents();
    EXPECT_EQ(events.count(&split, QAccessible::ObjectReorder), 1);

    events.clear();
    second->show();
    QApplication::processEvents();
    EXPECT_EQ(childrenWithRole(root, QAccessible::Grip).size(), 2);
    EXPECT_EQ(events.count(&split, QAccessible::ObjectReorder), 1);

    events.clear();
    split.setPanePreferredSize(0, split.panePreferredSize(0));
    EXPECT_EQ(events.count(&split, QAccessible::ValueChanged), 0);
#endif
}
