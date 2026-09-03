#include <gtest/gtest.h>

#include <QMetaType>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>

#include "components/foundation/MotionPolicy.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "design/Animation.h"

namespace {

using fluent::MotionPolicy;

class MotionPolicyTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // Qt 5 records the nested signal argument as the short name "Mode".
        // Register that alias so QSignalSpy can preserve the enum value.
        qRegisterMetaType<MotionPolicy::Mode>("Mode");
    }

    void SetUp() override { MotionPolicy::instance().setMode(MotionPolicy::Mode::Full); }

    void TearDown() override { MotionPolicy::instance().setMode(MotionPolicy::Mode::Full); }
};

TEST_F(MotionPolicyTest, Contract_FullPreservesExistingDurationsAndMotion)
{
    MotionPolicy& policy = MotionPolicy::instance();

    EXPECT_EQ(static_cast<int>(MotionPolicy::Mode::Full), 0);
    EXPECT_EQ(static_cast<int>(MotionPolicy::Mode::Reduced), 1);
    EXPECT_EQ(static_cast<int>(MotionPolicy::Mode::Disabled), 2);
    EXPECT_EQ(static_cast<int>(MotionPolicy::Kind::Transition), 0);
    EXPECT_EQ(static_cast<int>(MotionPolicy::Kind::Continuous), 1);
    EXPECT_EQ(policy.mode(), MotionPolicy::Mode::Full);
    EXPECT_TRUE(policy.shouldAnimate(true, MotionPolicy::Kind::Transition));
    EXPECT_TRUE(policy.shouldAnimate(true, MotionPolicy::Kind::Continuous));
    EXPECT_EQ(policy.resolvedDuration(Animation::Duration::Fast), Animation::Duration::Fast);
    EXPECT_EQ(policy.resolvedDuration(Animation::Duration::Normal), Animation::Duration::Normal);
}

TEST_F(MotionPolicyTest, Contract_ReducedShortensTransitionsAndStopsContinuousMotion)
{
    MotionPolicy& policy = MotionPolicy::instance();
    policy.setMode(MotionPolicy::Mode::Reduced);

    EXPECT_TRUE(policy.shouldAnimate(true, MotionPolicy::Kind::Transition));
    EXPECT_FALSE(policy.shouldAnimate(true, MotionPolicy::Kind::Continuous));
    EXPECT_GT(policy.resolvedDuration(Animation::Duration::Normal), 0);
    EXPECT_LT(policy.resolvedDuration(Animation::Duration::Normal), Animation::Duration::Normal);
    EXPECT_EQ(policy.resolvedDuration(30), 30);
}

TEST_F(MotionPolicyTest, Contract_DisabledAndLocalSwitchSuppressAllMotion)
{
    MotionPolicy& policy = MotionPolicy::instance();

    EXPECT_FALSE(policy.shouldAnimate(false, MotionPolicy::Kind::Transition));
    EXPECT_FALSE(policy.shouldAnimate(false, MotionPolicy::Kind::Continuous));
    EXPECT_EQ(policy.resolvedDuration(Animation::Duration::Normal, false), 0);

    policy.setMode(MotionPolicy::Mode::Disabled);
    EXPECT_FALSE(policy.shouldAnimate(true, MotionPolicy::Kind::Transition));
    EXPECT_FALSE(policy.shouldAnimate(true, MotionPolicy::Kind::Continuous));
    EXPECT_EQ(policy.resolvedDuration(Animation::Duration::Normal), 0);
}

TEST_F(MotionPolicyTest, Contract_ModeSetterIsNoOpSafe)
{
    MotionPolicy& policy = MotionPolicy::instance();
    QSignalSpy modeSpy(&policy, &MotionPolicy::modeChanged);

    policy.setMode(MotionPolicy::Mode::Full);
    EXPECT_EQ(modeSpy.count(), 0);

    policy.setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(modeSpy.at(0).at(0).value<MotionPolicy::Mode>(), MotionPolicy::Mode::Reduced);

    policy.setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(modeSpy.count(), 1);
}

TEST_F(MotionPolicyTest, Contract_InvalidModeIsIgnored)
{
    MotionPolicy& policy = MotionPolicy::instance();
    QSignalSpy modeSpy(&policy, &MotionPolicy::modeChanged);

    policy.setMode(static_cast<MotionPolicy::Mode>(99));

    EXPECT_EQ(policy.mode(), MotionPolicy::Mode::Full);
    EXPECT_EQ(modeSpy.count(), 0);
}

TEST_F(MotionPolicyTest, Contract_ReceiverDestructionDisconnectsSafely)
{
    MotionPolicy& policy = MotionPolicy::instance();
    QPointer<QObject> receiver = new QObject;
    int notifications = 0;
    QObject::connect(&policy, &MotionPolicy::modeChanged, receiver,
                     [&notifications](MotionPolicy::Mode) { ++notifications; });

    delete receiver;
    EXPECT_TRUE(receiver.isNull());

    policy.setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(notifications, 0);
}

TEST_F(MotionPolicyTest, Contract_TransitionHelperAppliesReducedAndDisabledFinalStates)
{
    QVariantAnimation animation;
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);
    QSignalSpy finishedSpy(&animation, &QVariantAnimation::finished);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    fluent::detail::startMotionTransition(&animation, Animation::Duration::Normal);
    EXPECT_EQ(animation.duration(), 50);
    QTRY_COMPARE(finishedSpy.count(), 1);
    EXPECT_DOUBLE_EQ(animation.currentValue().toDouble(), 1.0);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    fluent::detail::startMotionTransition(&animation, Animation::Duration::Normal);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(finishedSpy.count(), 2);
    EXPECT_DOUBLE_EQ(animation.currentValue().toDouble(), 1.0);
}

TEST_F(MotionPolicyTest, Contract_ActiveTransitionConvergesWhenMotionIsDisabled)
{
    QVariantAnimation animation;
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);
    QSignalSpy finishedSpy(&animation, &QVariantAnimation::finished);

    fluent::detail::startMotionTransition(&animation, Animation::Duration::Normal);
    ASSERT_EQ(animation.state(), QAbstractAnimation::Running);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(finishedSpy.count(), 1);
    EXPECT_DOUBLE_EQ(animation.currentValue().toDouble(), 1.0);
}

TEST_F(MotionPolicyTest, Contract_ActiveTransitionUsesReducedBudgetAndRestoresFullTiming)
{
    QVariantAnimation animation;
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);

    fluent::detail::startMotionTransition(&animation, 1000);
    ASSERT_EQ(animation.state(), QAbstractAnimation::Running);
    animation.setCurrentTime(200);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Running);
    EXPECT_GT(animation.duration(), animation.currentTime());
    EXPECT_LE(animation.duration() - animation.currentTime(), 50);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    EXPECT_EQ(animation.duration(), 1000);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Running);
    animation.stop();
}

TEST_F(MotionPolicyTest, Contract_ReusedTransitionRefreshesItsLocalAnimationPreference)
{
    QVariantAnimation animation;
    animation.setStartValue(0.0);
    animation.setEndValue(1.0);

    fluent::detail::startMotionTransition(&animation, Animation::Duration::Normal, false);
    ASSERT_EQ(animation.state(), QAbstractAnimation::Stopped);

    fluent::detail::startMotionTransition(&animation, Animation::Duration::Normal, true);
    ASSERT_EQ(animation.state(), QAbstractAnimation::Running);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(animation.duration(), 50);
    EXPECT_EQ(animation.state(), QAbstractAnimation::Running);
    QTRY_COMPARE(animation.state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(animation.currentValue().toDouble(), 1.0);
}

TEST_F(MotionPolicyTest, Contract_TransitionGroupResolvesEveryChild)
{
    QObject firstTarget;
    QObject secondTarget;
    firstTarget.setProperty("value", 0.0);
    secondTarget.setProperty("value", 0.0);

    QParallelAnimationGroup group;
    auto* first = new QPropertyAnimation(&firstTarget, "value", &group);
    first->setStartValue(0.0);
    first->setEndValue(1.0);
    first->setDuration(Animation::Duration::Normal);
    auto* second = new QPropertyAnimation(&secondTarget, "value", &group);
    second->setStartValue(0.0);
    second->setEndValue(1.0);
    second->setDuration(Animation::Duration::Slow);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    fluent::detail::startMotionTransitionGroup(&group);
    EXPECT_EQ(first->duration(), 50);
    EXPECT_EQ(second->duration(), 50);
    QTRY_COMPARE(group.state(), QAbstractAnimation::Stopped);
    EXPECT_DOUBLE_EQ(firstTarget.property("value").toDouble(), 1.0);
    EXPECT_DOUBLE_EQ(secondTarget.property("value").toDouble(), 1.0);
}

TEST_F(MotionPolicyTest, Contract_ActiveTransitionGroupRestoresEveryFullTiming)
{
    QObject firstTarget;
    QObject secondTarget;
    firstTarget.setProperty("value", 0.0);
    secondTarget.setProperty("value", 0.0);

    QParallelAnimationGroup group;
    auto* first = new QPropertyAnimation(&firstTarget, "value", &group);
    first->setStartValue(0.0);
    first->setEndValue(1.0);
    first->setDuration(400);
    auto* second = new QPropertyAnimation(&secondTarget, "value", &group);
    second->setStartValue(0.0);
    second->setEndValue(1.0);
    second->setDuration(700);

    fluent::detail::startMotionTransitionGroup(&group);
    ASSERT_EQ(group.state(), QAbstractAnimation::Running);
    group.setCurrentTime(200);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    EXPECT_LE(first->duration() - first->currentTime(), 50);
    EXPECT_LE(second->duration() - second->currentTime(), 50);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    EXPECT_EQ(first->duration(), 400);
    EXPECT_EQ(second->duration(), 700);
    EXPECT_EQ(group.state(), QAbstractAnimation::Running);
    group.stop();
}

} // namespace
