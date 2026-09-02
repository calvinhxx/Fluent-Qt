#include <gtest/gtest.h>

#include "components/collections/private/CollectionItemState_p.h"

namespace {

using fluent::collections::detail::CollectionItemInteractionState;
using fluent::collections::detail::applyCollectionItemState;
using fluent::collections::detail::collectionItemInteractionState;
using fluent::collections::detail::collectionItemVisualStyle;

QStyle::State itemState(bool enabled, bool selected = false, bool hovered = false,
                        bool pressed = false)
{
    QStyle::State state = QStyle::State_None;
    state.setFlag(QStyle::State_Enabled, enabled);
    state.setFlag(QStyle::State_Selected, selected);
    state.setFlag(QStyle::State_MouseOver, hovered);
    state.setFlag(QStyle::State_Sunken, pressed);
    return state;
}

TEST(CollectionItemStateTest, ResolvesInteractionPrecedence)
{
    EXPECT_EQ(collectionItemInteractionState(itemState(true)),
              CollectionItemInteractionState::Normal);
    EXPECT_EQ(collectionItemInteractionState(itemState(true, false, true)),
              CollectionItemInteractionState::Hovered);
    EXPECT_EQ(collectionItemInteractionState(itemState(true, false, true, true)),
              CollectionItemInteractionState::Pressed);
    EXPECT_EQ(collectionItemInteractionState(itemState(true, true)),
              CollectionItemInteractionState::Selected);
    EXPECT_EQ(collectionItemInteractionState(itemState(true, true, true)),
              CollectionItemInteractionState::SelectedHovered);
    EXPECT_EQ(collectionItemInteractionState(itemState(true, true, true, true)),
              CollectionItemInteractionState::SelectedPressed);

    EXPECT_EQ(collectionItemInteractionState(itemState(true, false, false, true)),
              CollectionItemInteractionState::Normal)
        << "Pressed feedback remains gated by hover, matching the existing delegates";
    EXPECT_EQ(collectionItemInteractionState(itemState(false, true, true, true)),
              CollectionItemInteractionState::Disabled);
}

TEST(CollectionItemStateTest, MapsExistingSemanticColorsWithoutChangingPixels)
{
    fluent::FluentElement::Colors colors{};
    colors.textPrimary = QColor(1, 2, 3, 255);
    colors.textDisabled = QColor(4, 5, 6, 120);
    colors.subtleSecondary = QColor(7, 8, 9, 80);
    colors.subtleTertiary = QColor(10, 11, 12, 60);

    for (int mask = 0; mask < 16; ++mask) {
        const bool enabled = mask & 1;
        const bool selected = mask & 2;
        const bool hovered = mask & 4;
        const bool pressed = mask & 8;
        const auto actual =
            collectionItemVisualStyle(itemState(enabled, selected, hovered, pressed), colors);

        QColor expectedBackground = Qt::transparent;
        const QColor expectedForeground = enabled ? colors.textPrimary : colors.textDisabled;
        if (enabled && hovered && pressed)
            expectedBackground = colors.subtleTertiary;
        else if (enabled && (selected || hovered))
            expectedBackground = colors.subtleSecondary;

        EXPECT_EQ(actual.background, expectedBackground) << "mask=" << mask;
        EXPECT_EQ(actual.foreground, expectedForeground) << "mask=" << mask;
    }
}

TEST(CollectionItemStateTest, AppliesEveryInputLikeThePreviousCallSites)
{
    const QStyle::State baseStates[] = {
        QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange,
        QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange | QStyle::State_Enabled |
            QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_Sunken |
            QStyle::State_Active,
    };

    for (const QStyle::State base : baseStates) {
        for (int mask = 0; mask < 32; ++mask) {
            const bool enabled = mask & 1;
            const bool selected = mask & 2;
            const bool hovered = mask & 4;
            const bool pressed = mask & 8;
            const bool active = mask & 16;

            QStyle::State expected = base;
            expected.setFlag(QStyle::State_Enabled, enabled);
            if (selected)
                expected |= QStyle::State_Selected;
            if (hovered)
                expected |= QStyle::State_MouseOver;
            if (pressed)
                expected |= QStyle::State_Sunken;
            if (active)
                expected |= QStyle::State_Active;

            QStyle::State actual = base;
            applyCollectionItemState(actual, enabled, selected, hovered, pressed, active);
            EXPECT_EQ(actual, expected) << "base=" << int(base) << " mask=" << mask;
        }
    }
}

} // namespace
