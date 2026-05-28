## Why

`scrolling` 目录目前已有 Fluent 风格 `ScrollBar` 和 `ScrollView`，但缺少用于长内容快速定位的 WinUI `AnnotatedScrollBar`。业务如果手动组合滚动条、标签和 hover 提示，很难稳定复刻 Windows UI Kit 的年份/分组标签、详情提示、碰撞隐藏和与 `ScrollView` 联动的行为。

## What Changes

- 新增自定义 `AnnotatedScrollBar` 组件，位于 `src/view/scrolling/`，遵循本项目 `QWidget + FluentElement + view::QMLPlus` 的控件实现模式。
- 支持竖向 annotated scroll bar：显示分组标签、顶部/底部 caret、当前位置 indicator，并在 Light/Dark、Rest/Hover/Pressed 状态下刷新视觉。
- 提供标签数据 API，允许调用方按滚动 offset 注册 `AnnotatedScrollBarLabel(text, offset)`；当可用高度不足时自动隐藏部分标签，避免文字重叠。
- 提供滚动范围、viewport、offset 同步 API，以及 `connectToScrollView(ScrollView*)` 或等价辅助能力，使 `AnnotatedScrollBar` 可以作为 `ScrollView` 的外置垂直滚动控制器。
- 支持 hover/drag 时显示详情提示：通过 detail label provider 或信号按当前 offset 生成内容，视觉对齐 Figma 的 TeachingTip 风格小浮层。
- 增加 GTest 与 VisualCheck，覆盖标签布局、offset 映射、拖动/点击滚动、详情提示、主题切换、与 `ScrollView` 联动，以及 WinUI Gallery 风格示例。

## Capabilities

### New Capabilities

- `annotated-scrollbar`: 覆盖 Fluent Design Qt 组件库中的 WinUI 风格 AnnotatedScrollBar 公共 API、标签/详情模型、滚动交互、主题视觉和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/scrolling/AnnotatedScrollBar.h`、`src/view/scrolling/AnnotatedScrollBar.cpp`。
- 更新测试配置：`tests/views/scrolling/CMakeLists.txt` 注册 `test_annotated_scrollbar`。
- 新增测试：`tests/views/scrolling/TestAnnotatedScrollBar.cpp`。
- 依赖现有 `ScrollView`、`ScrollBar`、`FluentElement`、`QMLPlus`、Design Token、Qt painting、mouse/keyboard event 和定时/动画能力。
- 不引入新的第三方依赖，不改变现有 `ScrollBar`、`ScrollView` 公共 API；`ScrollView` 联动通过新控件的辅助 API 完成。
