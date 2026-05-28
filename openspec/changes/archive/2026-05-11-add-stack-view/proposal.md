## Why

当前 `collections` 模块已有 `FlipView` 这类单页展示控件，但缺少一个面向导航栈的 Fluent `StackView`。业务在向导、设置页、详情钻取、面板式导航等场景中需要类似 QML `StackView` 的 push/pop/replace 栈语义和过渡动画；直接使用 `QStackedWidget` 只能切换 index，无法表达页面栈生命周期、返回导航和 Fluent 过渡效果。

## What Changes

- 新增 `view::collections::StackView` 组件，位于 `src/view/collections/`，从 Qt Widgets 的 `QStackedWidget` 派生并遵循 `FluentElement + view::QMLPlus` 的项目组件模式。
- 参考 QML `StackView` 提供页面栈 API：`push()`、`pop()`、`replace()`、`clear()`、`depth`、`currentItem`、`initialItem`、`busy`、`status` 和变更信号。
- 支持 Fluent 风格页面过渡：push enter/exit、pop enter/exit、replace enter/exit，默认使用水平 slide + fade，并支持禁用动画或配置动画时长。
- 支持返回导航能力：`canPop()`、`popToRoot()`、`popToItem()`、`goBack()`，并在栈顶变化时发出可绑定信号。
- 明确 QWidget 生命周期所有权：StackView 接管由栈管理的页面，并在 pop/remove/clear 时按策略隐藏、移除或销毁页面，避免悬空 widget。
- 增加 GTest 与 VisualCheck，覆盖栈语义、过渡状态、信号、禁用动画、键盘返回行为、主题刷新和 QML StackView 风格使用示例。

## Capabilities

### New Capabilities

- `stack-view`: 覆盖 Fluent Qt 组件库中的 QML StackView 风格页面栈组件，包括公共 API、页面生命周期、栈导航、过渡动画、输入行为和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/collections/StackView.h`、`src/view/collections/StackView.cpp`。
- 更新测试配置：`tests/views/collections/CMakeLists.txt` 注册 `test_stack_view`。
- 新增测试：`tests/views/collections/TestStackView.cpp`。
- 依赖 Qt Widgets `QStackedWidget`、`QPropertyAnimation`/`QParallelAnimationGroup`、现有 `FluentElement`、`QMLPlus` 和 Design Token。
- 不改变现有 `FlipView`、`ListView`、`GridView`、`TreeView` 公共 API；StackView 是新增组件，可与这些页面控件作为 stack item 组合使用。