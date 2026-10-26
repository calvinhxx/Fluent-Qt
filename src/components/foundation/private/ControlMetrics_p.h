#pragma once

#include <QFontMetrics>
#include "compatibility/private/LineEditMetricsCompat_p.h"
#include "design/Spacing.h"

namespace fluent::detail {

// Single-line standard controls share a baseline; text can increase it.
// zh_CN: 标准单行控件共用基础高度，较大的文字可以自然撑高。
inline int standardControlHeight(const QFontMetrics& metrics)
{
    const int height = qMax(::Spacing::ControlHeight::Standard,
        compatibility::detail::singleLineEditMinimumTextHeight(metrics) + 2 * ::Spacing::Padding::TextFieldVertical);
    return ((height + ::Spacing::BaseUnit - 1) / ::Spacing::BaseUnit) * ::Spacing::BaseUnit;
}

} // namespace fluent::detail
