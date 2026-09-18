#ifndef FLUENTQT_DESIGN_TOASTTOKENS_P_H
#define FLUENTQT_DESIGN_TOASTTOKENS_P_H

#include "design/Elevation.h"
#include "design/Spacing.h"

namespace fluent::toast_tokens {

// Toast refinement, Figma 59:250 / 60:348. Component-local tokens leave other
// surfaces unchanged. zh_CN: Toast 专用设计 token，不改变其它表面。
constexpr int minimumWidth = 220;
constexpr int maximumWidth = 380;
constexpr int cornerRadius = 12;
constexpr int statusSize = 28;
constexpr int statusRadius = 8;
constexpr int horizontalPadding = Spacing::Standard;
constexpr int compactPadding = Spacing::Medium;
constexpr int detailPadding = Spacing::Standard;
constexpr int columnGap = Spacing::Medium;
constexpr int textGap = Spacing::XSmall;
constexpr int actionGap = Spacing::Medium;
constexpr int controlHeight = Spacing::ControlHeight::Standard;
constexpr int closeSize = Spacing::ControlHeight::Small;
constexpr int closeInset = Spacing::Small;
constexpr int closeGap = Spacing::Small;
constexpr int enterDuration = 180;
constexpr int exitDuration = 140;
constexpr int stackDuration = 220;
constexpr int enterDistance = 8;

inline QColor informationalTint(bool dark)
{
    return QColor(dark ? "#253D48" : "#EFF6FB");
}

inline QColor actionHover(bool dark)
{
    return QColor(dark ? "#404040" : "#ECECEC");
}

inline Elevation::ShadowParams ambientShadow()
{
    return {0, 8, 24, -8, QColor(Qt::black), 0.18};
}

inline Elevation::ShadowParams contactShadow()
{
    return {0, 2, 6, -1, QColor(Qt::black), 0.10};
}

} // namespace fluent::toast_tokens

#endif // FLUENTQT_DESIGN_TOASTTOKENS_P_H
