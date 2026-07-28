#include <gtest/gtest.h>

#include <QApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/foundation/WidgetOwnership.h"
#include "components/layout/Accordion.h"
#include "components/layout/Expander.h"
#include "components/textfields/Label.h"

using fluent::WidgetOwnership;
using fluent::layout::Accordion;
using fluent::layout::Expander;

namespace {

Expander* makeItem(const QString& title, QWidget* parent = nullptr)
{
    auto* item = new Expander(parent);
    item->setHeaderText(title);
    item->setAnimationEnabled(false);

    auto* body = new QWidget;
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(12, 8, 12, 12);
    layout->addWidget(new fluent::textfields::Label(
        QStringLiteral("Body for %1").arg(title), body));
    item->setContentWidget(body, WidgetOwnership::Owned);
    return item;
}

} // namespace

class AccordionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<Accordion::ExpansionMode>("ExpansionMode");
    }
};

TEST_F(AccordionTest, Contract_DefaultsAndSetterNoOps)
{
    static_assert(std::is_base_of<QWidget, Accordion>::value,
                  "Accordion remains a QWidget container");
    static_assert(std::is_base_of<fluent::FluentElement, Accordion>::value,
                  "Accordion participates in Fluent theming");

    Accordion accordion;
    EXPECT_EQ(accordion.expansionMode(),
              Accordion::ExpansionMode::Single);
    EXPECT_EQ(accordion.count(), 0);
    EXPECT_EQ(accordion.itemAt(0), nullptr);

    QSignalSpy modeSpy(&accordion, &Accordion::expansionModeChanged);
    accordion.setExpansionMode(Accordion::ExpansionMode::Single);
    EXPECT_EQ(modeSpy.count(), 0);
    accordion.setExpansionMode(Accordion::ExpansionMode::Multiple);
    accordion.setExpansionMode(Accordion::ExpansionMode::Multiple);
    EXPECT_EQ(modeSpy.count(), 1);
    EXPECT_EQ(
        modeSpy.at(0).at(0).value<Accordion::ExpansionMode>(),
        Accordion::ExpansionMode::Multiple);
}

TEST_F(AccordionTest, Contract_SingleModeCollapsesPeerAndMultipleModeDoesNot)
{
    Accordion accordion;
    auto* first = makeItem(QStringLiteral("First"));
    auto* second = makeItem(QStringLiteral("Second"));
    ASSERT_TRUE(accordion.addItem(first, WidgetOwnership::Owned));
    ASSERT_TRUE(accordion.addItem(second, WidgetOwnership::Owned));

    QSignalSpy expansionSpy(
        &accordion, &Accordion::itemExpansionChanged);
    first->setExpanded(true);
    EXPECT_TRUE(first->isExpanded());
    EXPECT_FALSE(second->isExpanded());

    second->setExpanded(true);
    EXPECT_FALSE(first->isExpanded());
    EXPECT_TRUE(second->isExpanded());
    EXPECT_GE(expansionSpy.count(), 3);

    accordion.setExpansionMode(Accordion::ExpansionMode::Multiple);
    first->setExpanded(true);
    EXPECT_TRUE(first->isExpanded());
    EXPECT_TRUE(second->isExpanded());

    accordion.setExpansionMode(Accordion::ExpansionMode::Single);
    EXPECT_TRUE(first->isExpanded());
    EXPECT_FALSE(second->isExpanded());
}

TEST_F(AccordionTest, Contract_DefaultBorrowingAndTakeTransferLifetime)
{
    QPointer<Expander> borrowed = makeItem(QStringLiteral("Borrowed"));
    {
        Accordion accordion;
        ASSERT_TRUE(accordion.addItem(borrowed));
        EXPECT_EQ(accordion.itemOwnershipAt(0),
                  WidgetOwnership::Borrowed);
        EXPECT_EQ(borrowed->parentWidget(), &accordion);
    }
    ASSERT_FALSE(borrowed.isNull());
    EXPECT_EQ(borrowed->parentWidget(), nullptr);
    delete borrowed;

    Accordion accordion;
    auto* owned = makeItem(QStringLiteral("Transferred"));
    ASSERT_TRUE(accordion.addItem(owned, WidgetOwnership::Owned));
    Expander* taken = accordion.takeItem(0);
    EXPECT_EQ(taken, owned);
    EXPECT_EQ(taken->parentWidget(), nullptr);
    EXPECT_EQ(accordion.count(), 0);
    delete taken;
}

TEST_F(AccordionTest, Contract_ReparentedAndOwnedItemsFollowPolicy)
{
    QWidget originalOwner;
    auto* reparented = makeItem(QStringLiteral("Reparented"), &originalOwner);
    {
        Accordion accordion;
        ASSERT_TRUE(accordion.addItem(
            reparented, WidgetOwnership::Reparented));
        EXPECT_EQ(reparented->parentWidget(), &accordion);
    }
    EXPECT_EQ(reparented->parentWidget(), &originalOwner);

    QPointer<Expander> owned = makeItem(QStringLiteral("Owned"));
    auto* accordion = new Accordion;
    ASSERT_TRUE(accordion->addItem(owned, WidgetOwnership::Owned));
    delete accordion;
    EXPECT_TRUE(owned.isNull());
}

TEST_F(AccordionTest, Contract_CountTracksInsertRemoveAndExternalDestruction)
{
    Accordion accordion;
    auto* first = makeItem(QStringLiteral("First"));
    auto* second = makeItem(QStringLiteral("Second"));
    QSignalSpy countSpy(&accordion, &Accordion::countChanged);

    EXPECT_FALSE(accordion.addItem(nullptr));
    ASSERT_TRUE(accordion.addItem(first));
    EXPECT_FALSE(accordion.addItem(first));
    ASSERT_TRUE(accordion.insertItem(0, second));
    EXPECT_EQ(accordion.itemAt(0), second);
    EXPECT_EQ(accordion.indexOf(first), 1);
    EXPECT_EQ(accordion.count(), 2);

    delete second;
    EXPECT_EQ(accordion.count(), 1);
    EXPECT_EQ(accordion.itemAt(0), first);

    EXPECT_TRUE(accordion.removeItem(0));
    EXPECT_EQ(first->parentWidget(), nullptr);
    EXPECT_EQ(accordion.count(), 0);
    EXPECT_FALSE(accordion.removeItem(0));
    EXPECT_EQ(countSpy.count(), 4);
    delete first;
}

TEST_F(AccordionTest, Contract_ArrowHomeAndEndMoveHeaderFocus)
{
    Accordion accordion;
    auto* first = makeItem(QStringLiteral("First"));
    auto* second = makeItem(QStringLiteral("Second"));
    auto* third = makeItem(QStringLiteral("Third"));
    ASSERT_TRUE(accordion.addItem(first, WidgetOwnership::Owned));
    ASSERT_TRUE(accordion.addItem(second, WidgetOwnership::Owned));
    ASSERT_TRUE(accordion.addItem(third, WidgetOwnership::Owned));
    accordion.resize(360, accordion.sizeHint().height());
    accordion.show();
    QApplication::processEvents();

    first->headerButton()->setFocus();
    ASSERT_TRUE(first->headerButton()->hasFocus());

    QTest::keyClick(first->headerButton(), Qt::Key_Down);
    EXPECT_TRUE(second->headerButton()->hasFocus());
    QTest::keyClick(second->headerButton(), Qt::Key_End);
    EXPECT_TRUE(third->headerButton()->hasFocus());
    QTest::keyClick(third->headerButton(), Qt::Key_Down);
    EXPECT_TRUE(first->headerButton()->hasFocus());
    QTest::keyClick(first->headerButton(), Qt::Key_Up);
    EXPECT_TRUE(third->headerButton()->hasFocus());
    QTest::keyClick(third->headerButton(), Qt::Key_Home);
    EXPECT_TRUE(first->headerButton()->hasFocus());
}
