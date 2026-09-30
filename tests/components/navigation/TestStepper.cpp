#include <gtest/gtest.h>
#include <algorithm>
#include <QAccessible>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/navigation/Stepper.h"

using fluent::navigation::Stepper;
using fluent::navigation::StepperItem;
using fluent::basicinput::Button;

class StepperTest : public ::testing::Test {
protected:
    fluent::MotionPolicy::Mode previous;
    void SetUp() override
    {
        previous = fluent::MotionPolicy::instance().mode();
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    }
    void TearDown() override { fluent::MotionPolicy::instance().setMode(previous); }
    void populate(Stepper& view)
    {
        for (int i = 0; i < 4; ++i)
            view.addItem(
                StepperItem(QStringLiteral("Step %1").arg(i + 1), QStringLiteral("Description")));
        view.resize(560, 120);
    }
    Button* button(Stepper& view, int index)
    {
        return view.findChildren<Button*>(QStringLiteral("stepperStep")).at(index);
    }
};

TEST_F(StepperTest, Contract_ResizeAndDirectionChangesPreserveMarkerTransition)
{
    Stepper view;
    populate(view);
    view.show();
    QTest::qWait(30);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    view.setCurrentIndex(1);
    QTest::qWait(20);
    auto* motion =
        button(view, 1)->findChild<QVariantAnimation*>(QStringLiteral("stepperMarkerTransition"));
    ASSERT_NE(motion, nullptr);
    const int elapsed = motion->currentTime();
    view.resize(240, 140);
    view.setLayoutDirection(Qt::RightToLeft);
    view.setOrientation(Qt::Vertical);
    QTest::qWait(10);
    EXPECT_EQ(motion->state(), QAbstractAnimation::Running);
    EXPECT_GE(motion->currentTime(), elapsed);
    EXPECT_EQ(view.currentIndex(), 1);
}

TEST_F(StepperTest, Contract_AccessibleFocusUsesTheSameStepAsTheNativeButton)
{
    Stepper view;
    populate(view);
    view.setCurrentIndex(1);
    view.show();
    view.activateWindow();
    QTest::qWait(20);
    button(view, 1)->setFocus(Qt::TabFocusReason);
    auto* root = QAccessible::queryAccessibleInterface(&view);
    auto* focused = QAccessible::queryAccessibleInterface(button(view, 1));
    ASSERT_NE(root, nullptr);
    ASSERT_NE(focused, nullptr);
    EXPECT_EQ(focused->role(), QAccessible::PageTab);
    EXPECT_EQ(root->child(1), focused);
    EXPECT_EQ(root->focusChild(), focused);
    EXPECT_EQ(focused->parent(), root);
    EXPECT_TRUE(focused->state().focused);
    EXPECT_TRUE(focused->state().selected);
    EXPECT_EQ(focused->childCount(), 0);
    EXPECT_TRUE(focused->text(QAccessible::Description).contains(QStringLiteral("Current step")));
    view.insertItem(0, StepperItem(QStringLiteral("Inserted")));
    EXPECT_EQ(root->child(2), focused);
    EXPECT_EQ(root->indexOfChild(focused), 2);
    EXPECT_EQ(focused->text(QAccessible::Name), QStringLiteral("Step 2"));
}

TEST_F(StepperTest, Contract_TabEntrySkipsDisabledCurrentAndEmptyEligibility)
{
    QWidget host;
    QVBoxLayout layout(&host);
    Button before(QStringLiteral("Before"), &host);
    Stepper view(&host);
    Button after(QStringLiteral("After"), &host);
    populate(view);
    layout.addWidget(&before);
    layout.addWidget(&view);
    layout.addWidget(&after);
    view.setCurrentIndex(2);
    view.setItemEnabled(2, false);
    QWidget::setTabOrder(&before, &view);
    QWidget::setTabOrder(&view, &after);
    host.show();
    host.activateWindow();
    QTest::qWait(20);
    before.setFocus();
    QTest::keyClick(&before, Qt::Key_Tab);
    EXPECT_TRUE(button(view, 0)->hasFocus());
    EXPECT_EQ(view.focusProxy(), button(view, 0));
    EXPECT_EQ(view.currentIndex(), 2);
    view.setItemEnabled(0, false);
    before.setFocus();
    QTest::keyClick(&before, Qt::Key_Tab);
    EXPECT_TRUE(button(view, 1)->hasFocus());
    view.setItemEnabled(1, false);
    view.setItemEnabled(3, false);
    before.setFocus();
    QTest::keyClick(&before, Qt::Key_Tab);
    EXPECT_TRUE(after.hasFocus());
    EXPECT_EQ(view.focusProxy(), nullptr);
    EXPECT_EQ(view.currentIndex(), 2);
    view.setItemEnabled(1, true);
    before.setFocus();
    QTest::keyClick(&before, Qt::Key_Tab);
    EXPECT_TRUE(button(view, 1)->hasFocus());
    EXPECT_EQ(view.currentIndex(), 2);
}

TEST_F(StepperTest, Contract_CurrentIndexAndPropertiesAreApplicationControlled)
{
    Stepper view;
    populate(view);
    EXPECT_EQ(view.currentIndex(), -1);
    QSignalSpy changed(&view, &Stepper::currentIndexChanged);
    view.setCurrentIndex(1);
    view.setCurrentIndex(1);
    view.setCurrentIndex(99);
    view.setCurrentIndex(-2);
    EXPECT_EQ(view.currentIndex(), 1);
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(view.itemAt(0).state, Stepper::Pending);
    view.setItemEnabled(2, false);
    view.setCurrentIndex(2);
    EXPECT_EQ(view.currentIndex(), 2);
    view.setCurrentIndex(-1);
    EXPECT_EQ(changed.count(), 3);
    QSignalSpy orientation(&view, &Stepper::orientationChanged);
    view.setOrientation(Qt::Vertical);
    view.setOrientation(Qt::Vertical);
    EXPECT_EQ(orientation.count(), 1);
    QSignalSpy motion(&view, &Stepper::animationEnabledChanged);
    view.setAnimationEnabled(false);
    view.setAnimationEnabled(false);
    EXPECT_EQ(motion.count(), 1);
}

TEST_F(StepperTest, Contract_ActivationRequestsDoNotAdvanceOrCompleteSteps)
{
    Stepper view;
    populate(view);
    view.setCurrentIndex(1);
    view.show();
    QTest::qWait(20);
    QSignalSpy requested(&view, &Stepper::stepRequested);
    QSignalSpy nativeClick(button(view, 2), &Button::clicked);
    QTest::mouseClick(button(view, 2), Qt::LeftButton);
    EXPECT_EQ(requested.count(), 1);
    EXPECT_EQ(requested.at(0).at(0).toInt(), 2);
    EXPECT_EQ(nativeClick.count(), 1);
    EXPECT_EQ(view.currentIndex(), 1);
    EXPECT_FALSE(button(view, 2)->isChecked());
    EXPECT_TRUE(button(view, 1)->isChecked());
    view.setItemEnabled(2, false);
    button(view, 2)->click();
    EXPECT_EQ(requested.count(), 1);
    QObject::connect(&view, &Stepper::stepRequested, &view, &Stepper::setCurrentIndex);
    button(view, 0)->click();
    EXPECT_EQ(view.currentIndex(), 0);
    EXPECT_EQ(view.itemAt(1).state, Stepper::Pending);
}

TEST_F(StepperTest, Contract_MutationsPreserveIdentityAndNoOpSignals)
{
    Stepper view;
    populate(view);
    view.setCurrentIndex(2);
    QSignalSpy changed(&view, &Stepper::currentIndexChanged);
    QSignalSpy items(&view, &Stepper::itemsChanged);
    EXPECT_FALSE(view.setItem(2, view.itemAt(2)));
    EXPECT_FALSE(view.setItemState(2, Stepper::Pending));
    EXPECT_EQ(items.count(), 0);
    view.insertItem(0, StepperItem(QStringLiteral("Inserted")));
    EXPECT_EQ(view.currentIndex(), 3);
    EXPECT_EQ(view.itemAt(3).text, QStringLiteral("Step 3"));
    view.removeItem(0);
    EXPECT_EQ(view.currentIndex(), 2);
    view.removeItem(2);
    EXPECT_EQ(view.currentIndex(), -1);
    EXPECT_EQ(changed.count(), 3);
    view.clearItems();
    const int count = items.count();
    view.clearItems();
    EXPECT_EQ(items.count(), count);
}

TEST_F(StepperTest, Contract_KeyboardSkipsDisabledStepsWithoutImplicitNavigation)
{
    Stepper view;
    populate(view);
    view.setItemEnabled(1, false);
    view.show();
    view.activateWindow();
    QTest::qWait(20);
    button(view, 0)->setFocus();
    QTest::keyClick(button(view, 0), Qt::Key_Right);
    EXPECT_TRUE(button(view, 2)->hasFocus());
    EXPECT_EQ(view.currentIndex(), -1);
    QSignalSpy requested(&view, &Stepper::stepRequested);
    QTest::keyClick(button(view, 2), Qt::Key_Return);
    EXPECT_EQ(requested.count(), 1);
    QTest::keyClick(button(view, 2), Qt::Key_Space);
    EXPECT_EQ(requested.count(), 2);
    view.setLayoutDirection(Qt::RightToLeft);
    button(view, 0)->setFocus();
    QTest::keyClick(button(view, 0), Qt::Key_Left);
    EXPECT_TRUE(button(view, 2)->hasFocus());
    view.setOrientation(Qt::Vertical);
    button(view, 0)->setFocus();
    QTest::keyClick(button(view, 0), Qt::Key_Down);
    EXPECT_TRUE(button(view, 2)->hasFocus());
}

TEST_F(StepperTest, Contract_OverflowAndMirroredGeometryRemainReachable)
{
    Stepper view;
    populate(view);
    view.resize(160, 120);
    view.show();
    QTest::qWait(20);
    EXPECT_GE(view.itemGeometry(0).width(), 96);
    view.setCurrentIndex(3);
    QTest::qWait(20);
    EXPECT_TRUE(view.rect().intersects(view.itemGeometry(3)));
    view.resize(600, 120);
    view.setLayoutDirection(Qt::RightToLeft);
    QTest::qWait(20);
    EXPECT_GT(view.itemGeometry(0).left(), view.itemGeometry(3).left());
    view.setOrientation(Qt::Vertical);
    QTest::qWait(20);
    EXPECT_LT(view.itemGeometry(0).top(), view.itemGeometry(3).top());
}

TEST_F(StepperTest, Contract_AccessibilityPreservesNamesAndRequestBoundary)
{
    Stepper view;
    populate(view);
    view.setAccessibleName(QStringLiteral("Caller navigation"));
    view.setCurrentIndex(1);
    auto item = view.itemAt(2);
    item.accessibleName = QStringLiteral("Custom step");
    item.state = Stepper::Error;
    view.setItem(2, item);
    auto* root = QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::PageTabList);
    EXPECT_EQ(root->childCount(), 4);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Caller navigation"));
    EXPECT_TRUE(root->child(1)->state().selected);
    EXPECT_EQ(root->child(2)->text(QAccessible::Name), QStringLiteral("Custom step"));
    EXPECT_TRUE(root->child(2)->text(QAccessible::Description).contains(QStringLiteral("Error")));
    QSignalSpy requested(&view, &Stepper::stepRequested);
    root->child(2)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_EQ(requested.count(), 1);
    EXPECT_EQ(view.currentIndex(), 1);
    view.setItemEnabled(2, false);
    EXPECT_TRUE(root->child(2)->state().disabled);
    EXPECT_FALSE(root->child(2)->actionInterface()->actionNames().contains(
        QAccessibleActionInterface::pressAction()));
}

TEST_F(StepperTest, Contract_InterruptedMotionSettlesWhenPolicyChangesOrWidgetHides)
{
    Stepper view;
    populate(view);
    view.show();
    QTest::qWait(20);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    view.setCurrentIndex(0);
    QTest::qWait(30);
    view.setCurrentIndex(2);
    QTest::qWait(20);
    view.setCurrentIndex(1);
    auto animations =
        view.findChildren<QVariantAnimation*>(QStringLiteral("stepperMarkerTransition"));
    ASSERT_EQ(animations.size(), 4);
    EXPECT_TRUE(std::any_of(animations.cbegin(), animations.cend(),
                            [](auto* a) { return a->state() == QAbstractAnimation::Running; }));
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    for (auto* animation : animations)
        EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(button(view, 1)->isChecked());
    EXPECT_FALSE(button(view, 0)->isChecked());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    view.setCurrentIndex(2);
    for (auto* animation : animations)
        EXPECT_LE(animation->duration(), 50);
    view.hide();
    for (auto* animation : animations)
        EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
}

TEST_F(StepperTest, Contract_RequestCallbackMayDestroyTheComponent)
{
    auto* view = new Stepper;
    populate(*view);
    QPointer<Stepper> guarded(view);
    auto* action = button(*view, 0);
    QObject::connect(view, &Stepper::stepRequested, view, [view] { delete view; });
    action->click();
    EXPECT_TRUE(guarded.isNull());
    view = new Stepper;
    populate(*view);
    guarded = view;
    QObject::connect(view, &Stepper::stepRequested, view, [view] { delete view; });
    auto* child = QAccessible::queryAccessibleInterface(view)->child(0);
    ASSERT_NE(child, nullptr);
    child->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT_TRUE(guarded.isNull());
}

TEST_F(StepperTest, Contract_MarkerTransitionRendersAnIntermediateFrame)
{
    Stepper view;
    populate(view);
    view.show();
    QTest::qWait(30);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    const auto before = button(view, 1)->grab().toImage();
    view.setCurrentIndex(1);
    QTest::qWait(35);
    const auto intermediate = button(view, 1)->grab().toImage();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    const auto final = button(view, 1)->grab().toImage();
    EXPECT_NE(before, intermediate);
    EXPECT_NE(intermediate, final);
}
