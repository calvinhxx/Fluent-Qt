#pragma once

#include "design/Elevation.h"
#include "design/Spacing.h"

namespace fluent::menu_tokens {

// Shared menu proportions; keep short cascades readable as command lists.
constexpr int minimumWidth = 160;
constexpr int cascadeGap = Spacing::Gap::Tight;
constexpr Elevation::Level shadowLevel = Elevation::Medium;
constexpr qreal shadowIntensity = 0.18;

} // namespace fluent::menu_tokens
