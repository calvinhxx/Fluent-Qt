#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_RANGESLIDERACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_RANGESLIDERACCESSIBILITY_P_H

#include "components/basicinput/RangeSlider.h"

namespace fluent::basicinput::detail {

// RangeSlider exposes a grouping root with one logical Slider child per thumb.
// zh_CN: RangeSlider 暴露一个分组根节点，每个手柄对应一个逻辑 Slider 子项。
void ensureRangeSliderAccessibilityFactory();
void notifyRangeSliderValueChanged(RangeSlider* slider, RangeSlider::Thumb thumb);
void notifyRangeSliderFocusChanged(RangeSlider* slider);

} // namespace fluent::basicinput::detail

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_RANGESLIDERACCESSIBILITY_P_H
