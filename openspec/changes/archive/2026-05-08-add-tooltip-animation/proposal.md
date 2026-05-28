## Why

`ToolTip` 目前在 hover 场景中直接显示和隐藏，视觉上比较突兀。为显示、隐藏加入简约的 Fluent 风格动画，可以让提示浮层的出现和消失更轻盈，同时保持工具提示的即时反馈属性。

## What Changes

- 为 `ToolTip` 显示过程加入短时淡入动画。
- 为 `ToolTip` 隐藏过程加入短时淡出动画，并在动画结束后再真正隐藏窗口。
- 动画应复用现有 `FluentElement::themeAnimation()` 的时长和缓动曲线，保持与项目内其他 Fluent 组件一致。
- 保持现有 `show()` / `hide()` 调用方式和文本、边距、字体、主题行为兼容。

## Capabilities

### New Capabilities
- `tooltip-animation`: Defines the required motion behavior for ToolTip show and hide transitions.

### Modified Capabilities

## Impact

- Affected code: `src/view/status_info/ToolTip.h`, `src/view/status_info/ToolTip.cpp`.
- Affected tests: `tests/views/status_info/TestToolTip.cpp`.
- Public behavior: existing callers that use `show()` and `hide()` should receive animated transitions without changing their call sites.
- Dependencies: no new third-party dependency; implementation should use Qt animation primitives already used elsewhere in the project.
