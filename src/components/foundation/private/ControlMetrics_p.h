#pragma once

#include <QFontMetrics>
#include "compatibility/private/LineEditMetricsCompat_p.h"
#include "design/Spacing.h"

namespace fluent::detail {

// Baseline height for standard single-line controls; larger fonts expand on a 4px grid.
inline int standardControlHeight(const QFontMetrics& metrics)
{
    const int height = qMax(::Spacing::ControlHeight::Standard,
        compatibility::detail::singleLineEditMinimumTextHeight(metrics) + 2 * ::Spacing::Padding::TextFieldVertical);
    return ((height + ::Spacing::BaseUnit - 1) / ::Spacing::BaseUnit) * ::Spacing::BaseUnit;
}

} // namespace fluent::detail
