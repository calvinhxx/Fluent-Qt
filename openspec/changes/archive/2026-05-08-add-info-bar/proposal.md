## Why

`status_info` 目录已有 `ToolTip`、`ProgressRing` 和 `ProgressBar`，但还缺少用于展示可关闭应用状态消息的 WinUI / Fluent `InfoBar`。业务侧如果临时组合 `QFrame`、`QLabel` 与按钮，很难稳定对齐 Windows UI Kit 的单行/多行布局、四种 severity、可关闭行为，以及 WinUI Gallery 中常见的消息长度、action button 和 icon visibility 示例。

## What Changes

- 新增自定义 `InfoBar` 状态信息组件，位于 `src/view/status_info/`，遵循 `QWidget + FluentElement + view::QMLPlus` 的本项目控件模式。
- 支持 WinUI 风格公共属性：`isOpen`、`title`、`message`、`severity`、`isClosable`、`isIconVisible`、`actionWidget`、`preferredWidth`、`singleLine` 等首版布局与行为配置。
- 对齐 Figma Windows UI Kit Info Bar 变体：默认 Light 单行 informational、600px 宽、50px 高，半透明 card 背景、1px card stroke、3px 圆角、16px severity badge、正文 14px Body/Body Strong；多行布局根据内容扩展到约 110px 或 158px 高。
- 覆盖 Informational、Success、Warning、Error/Critical 四种 severity 的图标与颜色语义，并支持 Light/Dark 主题刷新。
- 支持 close button 关闭行为：可关闭时显示 48x48 subtle icon button，点击后设置 `isOpen=false` 并发出关闭信号；不可关闭时隐藏 close button 并重排内容。
- 支持可选 action widget，允许业务放入现有 `Button` 或 `HyperlinkButton`，用于复刻 WinUI Gallery 的 Button / Hyperlink 示例。
- 增加 GTest 与 VisualCheck，覆盖默认属性、信号、open/close、severity、icon/close/action 可见性、单行/多行 size hint、主题切换和 WinUI Gallery 风格示例。

## Capabilities

### New Capabilities

- `info-bar`: 覆盖 Fluent Design Qt 组件库中的 WinUI 3 风格应用状态消息条控件行为、布局、severity 视觉语义和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/status_info/InfoBar.h`、`src/view/status_info/InfoBar.cpp`。
- 更新测试配置：`tests/views/status_info/CMakeLists.txt` 注册 `test_info_bar`。
- 新增测试：`tests/views/status_info/TestInfoBar.cpp`。
- 依赖现有 Design Token、`FluentElement`、`QMLPlus`、Qt painting、`Label`、`Button` / `HyperlinkButton` 组合能力。
- 不引入新的第三方依赖，不改变已有 `ToolTip`、`ProgressBar`、`ProgressRing` 或其他控件公共 API。