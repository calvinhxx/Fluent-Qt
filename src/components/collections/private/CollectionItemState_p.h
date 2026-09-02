#pragma once

#include <QColor>
#include <QStyle>

#include "components/foundation/FluentElement.h"

namespace fluent::collections::detail {

enum class CollectionItemInteractionState {
    Normal,
    Hovered,
    Pressed,
    Selected,
    SelectedHovered,
    SelectedPressed,
    Disabled,
};

struct CollectionItemVisualStyle {
    QColor background = Qt::transparent;
    QColor foreground;
};

inline CollectionItemInteractionState collectionItemInteractionState(QStyle::State state) noexcept
{
    if (!(state & QStyle::State_Enabled))
        return CollectionItemInteractionState::Disabled;

    const bool selected = state & QStyle::State_Selected;
    const bool hovered = state & QStyle::State_MouseOver;
    const bool pressed = hovered && (state & QStyle::State_Sunken);

    if (pressed) {
        return selected ? CollectionItemInteractionState::SelectedPressed
                        : CollectionItemInteractionState::Pressed;
    }
    if (selected) {
        return hovered ? CollectionItemInteractionState::SelectedHovered
                       : CollectionItemInteractionState::Selected;
    }
    return hovered ? CollectionItemInteractionState::Hovered
                   : CollectionItemInteractionState::Normal;
}

inline CollectionItemVisualStyle collectionItemVisualStyle(QStyle::State state,
                                                           const FluentElement::Colors& colors)
{
    CollectionItemVisualStyle visual;
    visual.foreground = colors.textPrimary;

    switch (collectionItemInteractionState(state)) {
    case CollectionItemInteractionState::Disabled:
        visual.foreground = colors.textDisabled;
        break;
    case CollectionItemInteractionState::Pressed:
    case CollectionItemInteractionState::SelectedPressed:
        visual.background = colors.subtleTertiary;
        break;
    case CollectionItemInteractionState::Hovered:
    case CollectionItemInteractionState::Selected:
    case CollectionItemInteractionState::SelectedHovered:
        visual.background = colors.subtleSecondary;
        break;
    case CollectionItemInteractionState::Normal:
        break;
    }
    return visual;
}

// Preserve the state bits supplied by Qt and add only the item-specific bits
// that the collection view owns. Enabled is the sole normalized bit because
// both migrated call sites already set or clear it explicitly.
// zh_CN: 保留 Qt 提供的状态位，只补集合视图拥有的条目状态；Enabled 是唯一
// 会被规范化的状态位，因为两个迁移点原本就显式设置或清除它。
inline void applyCollectionItemState(QStyle::State& state, bool enabled, bool selected,
                                     bool hovered, bool pressed, bool active) noexcept
{
    state.setFlag(QStyle::State_Enabled, enabled);
    if (selected)
        state |= QStyle::State_Selected;
    if (hovered)
        state |= QStyle::State_MouseOver;
    if (pressed)
        state |= QStyle::State_Sunken;
    if (active)
        state |= QStyle::State_Active;
}

} // namespace fluent::collections::detail
