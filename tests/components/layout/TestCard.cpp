#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QVariant>

#include "components/layout/Card.h"

using fluent::layout::Card;

TEST(CardTest, Contract_DefaultsAndInheritance)
{
    static_assert(std::is_base_of<QFrame, Card>::value, "Card must remain a QFrame");
    static_assert(std::is_base_of<fluent::FluentElement, Card>::value,
                  "Card must expose FluentElement");
    static_assert(std::is_base_of<fluent::QMLPlus, Card>::value,
                  "Card must expose QMLPlus");

    Card card;
    EXPECT_EQ(card.appearance(), Card::Layer);
    EXPECT_TRUE(card.isBorderVisible());
    EXPECT_TRUE(card.property("fluentSurfaceColor").value<QColor>().isValid());
}

TEST(CardTest, Contract_SettersAreNoOpsForRepeatedValues)
{
    Card card;
    QSignalSpy appearanceSpy(&card, &Card::appearanceChanged);
    QSignalSpy borderSpy(&card, &Card::borderVisibleChanged);

    card.setAppearance(Card::LayerAlt);
    card.setAppearance(Card::LayerAlt);
    EXPECT_EQ(appearanceSpy.count(), 1);
    EXPECT_EQ(card.appearance(), Card::LayerAlt);

    card.setBorderVisible(false);
    card.setBorderVisible(false);
    EXPECT_EQ(borderSpy.count(), 1);
    EXPECT_FALSE(card.isBorderVisible());
}

TEST(CardTest, Contract_SurfacePropertyTracksAppearance)
{
    Card card;
    card.setAppearance(Card::Layer);
    const QColor layer = card.property("fluentSurfaceColor").value<QColor>();

    card.setAppearance(Card::LayerAlt);
    const QColor layerAlt = card.property("fluentSurfaceColor").value<QColor>();

    EXPECT_TRUE(layer.isValid());
    EXPECT_TRUE(layerAlt.isValid());
    EXPECT_NE(layer, layerAlt);
}
