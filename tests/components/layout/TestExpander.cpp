#include <gtest/gtest.h>

#include <QApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/layout/Divider.h"
#include "components/layout/Expander.h"
#include "components/textfields/Label.h"
#include "design/Animation.h"

using fluent::WidgetOwnership;
using fluent::layout::Expander;

namespace {

QWidget* makeBody(QWidget* parent = nullptr)
{
    auto* body = new QWidget(parent);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(12, 10, 12, 14);
    auto* child = new QWidget(body);
    child->setFixedHeight(36);
    layout->addWidget(child);
    return body;
}

class FullMotionScope {
public:
    FullMotionScope()
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    }

    ~FullMotionScope()
    {
        fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    }
};

} // namespace

TEST(ExpanderTest, Contract_DefaultsAndHeaderText)
{
    static_assert(std::is_base_of<fluent::layout::Card, Expander>::value,
                  "Expander must reuse the public Card surface");

    Expander expander;
    EXPECT_TRUE(expander.headerText().isEmpty());
    EXPECT_EQ(expander.contentWidget(), nullptr);
    EXPECT_EQ(expander.contentOwnership(), WidgetOwnership::Borrowed);
    EXPECT_FALSE(expander.isExpanded());
    EXPECT_TRUE(expander.isAnimationEnabled());
    EXPECT_EQ(expander.height(), 44);

    QSignalSpy headerSpy(&expander, &Expander::headerTextChanged);
    expander.setHeaderText(QStringLiteral("Details"));
    expander.setHeaderText(QStringLiteral("Details"));
    EXPECT_EQ(headerSpy.count(), 1);
    EXPECT_EQ(expander.headerButton()->accessibleName(), QStringLiteral("Details"));
    auto* headerText =
        expander.findChild<fluent::textfields::Label*>(QStringLiteral("fluentExpanderHeaderText"));
    ASSERT_NE(headerText, nullptr);
    EXPECT_EQ(headerText->textColorRole(), fluent::textfields::Label::TextColorRole::Primary);
}

TEST(ExpanderTest, Contract_ExpandedHandlerCanSynchronouslyDeleteExpander)
{
    auto* expander = new Expander;
    QPointer<Expander> guard(expander);
    QObject::connect(expander, &Expander::expandedChanged, qApp,
                     [expander](bool) { delete expander; });

    expander->setExpandedAnimated(true, false);

    EXPECT_TRUE(guard.isNull());
}

TEST(ExpanderTest, Contract_LayoutHeightHandlerCanSynchronouslyDeleteExpander)
{
    auto* expander = new Expander;
    expander->resize(360, 44);
    expander->setContentWidget(makeBody(), WidgetOwnership::Owned);
    QPointer<Expander> guard(expander);
    QObject::connect(expander, &Expander::layoutHeightChanged, qApp,
                     [expander](int) { delete expander; });

    expander->setExpandedAnimated(true, false);

    EXPECT_TRUE(guard.isNull());
}

TEST(ExpanderTest, Contract_ExpandedStateAndSignals)
{
    Expander expander;
    expander.resize(360, 44);
    expander.setContentWidget(makeBody(), WidgetOwnership::Owned);
    expander.setAnimationEnabled(false);

    QSignalSpy expandedSpy(&expander, &Expander::expandedChanged);
    QSignalSpy startedSpy(&expander, &Expander::expansionTransitionStarted);
    QSignalSpy finishedSpy(&expander, &Expander::expansionTransitionFinished);

    expander.setExpandedAnimated(true, true);
    EXPECT_TRUE(expander.isExpanded());
    EXPECT_GT(expander.height(), 44);
    auto* divider =
        expander.findChild<fluent::layout::Divider*>(QStringLiteral("fluentExpanderDivider"));
    auto* clip = expander.findChild<QWidget*>(QStringLiteral("fluentExpanderClip"));
    ASSERT_NE(divider, nullptr);
    ASSERT_NE(clip, nullptr);
    EXPECT_EQ(divider->geometry(), QRect(0, 44, expander.width(), 1));
    EXPECT_EQ(clip->geometry().top(), divider->geometry().bottom() + 1);
    EXPECT_EQ(clip->geometry().bottom(), expander.rect().bottom());
    EXPECT_EQ(expandedSpy.count(), 1);
    EXPECT_EQ(startedSpy.count(), 1);
    EXPECT_EQ(finishedSpy.count(), 1);

    expander.setExpanded(true);
    EXPECT_EQ(expandedSpy.count(), 1);

    expander.setExpanded(false);
    EXPECT_FALSE(expander.isExpanded());
    EXPECT_EQ(expander.height(), 44);
    EXPECT_EQ(expandedSpy.count(), 2);
    EXPECT_EQ(startedSpy.count(), 2);
    EXPECT_EQ(finishedSpy.count(), 2);
}

TEST(ExpanderTest, Contract_GlobalMotionConvergesActiveTransitions)
{
    FullMotionScope motionScope;
    Expander expander;
    expander.resize(360, 44);
    expander.setContentWidget(makeBody(), WidgetOwnership::Owned);

    auto* animation = expander.findChild<QVariantAnimation*>();
    ASSERT_NE(animation, nullptr);
    QSignalSpy finishedSpy(&expander, &Expander::expansionTransitionFinished);

    expander.setExpanded(true);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(animation->duration(), Animation::Duration::Normal);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_GT(animation->duration(), 0);
    EXPECT_LT(animation->duration(), Animation::Duration::Normal);
    QTRY_COMPARE(finishedSpy.count(), 1);
    EXPECT_TRUE(expander.isExpanded());
    EXPECT_GT(expander.height(), 44);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    expander.setExpanded(false);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_FALSE(expander.isExpanded());
    EXPECT_EQ(expander.height(), 44);
    EXPECT_EQ(finishedSpy.count(), 2);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    expander.setExpanded(true);
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(animation->duration(), Animation::Duration::Normal);

    expander.setAnimationEnabled(false);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_TRUE(expander.isExpanded());
    EXPECT_EQ(finishedSpy.count(), 3);

    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Reduced);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    expander.setExpanded(false);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(expander.height(), 44);
    EXPECT_EQ(finishedSpy.count(), 4);
}

TEST(ExpanderTest, Contract_BorrowedContentIsDetached)
{
    QPointer<QWidget> body = makeBody();
    {
        Expander expander;
        ASSERT_TRUE(expander.setContentWidget(body, WidgetOwnership::Borrowed));
        EXPECT_EQ(body->parentWidget(), expander.contentWidget()->parentWidget());
    }

    ASSERT_FALSE(body.isNull());
    EXPECT_EQ(body->parentWidget(), nullptr);
    delete body;
}

TEST(ExpanderTest, Contract_ReparentedContentReturnsToOriginalParent)
{
    QWidget owner;
    QWidget* body = makeBody(&owner);
    {
        Expander expander;
        ASSERT_TRUE(expander.setContentWidget(body, WidgetOwnership::Reparented));
        EXPECT_NE(body->parentWidget(), &owner);
    }
    EXPECT_EQ(body->parentWidget(), &owner);
}

TEST(ExpanderTest, Contract_OwnedContentIsDestroyed)
{
    QPointer<QWidget> body = makeBody();
    auto* expander = new Expander;
    ASSERT_TRUE(expander->setContentWidget(body, WidgetOwnership::Owned));
    delete expander;
    EXPECT_TRUE(body.isNull());
}

TEST(ExpanderTest, Contract_TakeContentTransfersWithoutDeleting)
{
    Expander expander;
    QWidget* body = makeBody();
    ASSERT_TRUE(expander.setContentWidget(body, WidgetOwnership::Owned));

    QWidget* taken = expander.takeContentWidget();
    EXPECT_EQ(taken, body);
    EXPECT_EQ(taken->parentWidget(), nullptr);
    EXPECT_EQ(expander.contentWidget(), nullptr);
    EXPECT_EQ(expander.height(), 44);
    delete taken;
}
