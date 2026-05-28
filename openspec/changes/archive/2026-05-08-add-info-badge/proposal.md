## Why

当前 `status_info` 模块已有 `ToolTip`、`ProgressRing` 等状态提示控件，但缺少 WinUI 3 常用的 `InfoBadge`：业务侧需要在导航项、按钮或任意控件角落展示非侵入式通知数量、状态点或图标提示时，只能临时自绘，容易和 Figma Windows UI Kit 以及 WinUI Gallery 的尺寸、颜色和行为不一致。

本变更新增 `InfoBadge` 自绘控件，对齐 Figma InfoBadge/Badge 设计节点中的 Dot/Beacon、Icon、Number、Numbers Wide、Light/Dark 和语义状态变体，并覆盖 WinUI Gallery 中 `Value`、`IconSource`、NavigationView 嵌入、样式切换、动态 value 与自定义背景示例的核心行为。

## What Changes

- 在 `src/view/status_info/` 新增 `InfoBadge` 组件，遵循 `QWidget + FluentElement + view::QMLPlus` 三重继承和 `paintEvent()` 自绘模式。
- 支持三种内容形态：点状 beacon、图标徽章、数值徽章；数值根据 `value` 自动显示，宽数值按内容扩展宽度。
- 暴露 `value`、`iconGlyph`、`displayMode`、`status`、`customBackgroundColor`、`customTextColor`、`badgeOpacity` 等 Qt 属性和变更信号。
- 支持 Figma/WinUI 语义状态：Informational、Attention、Caution、Success、Critical，并在 Light/Dark 主题下从 Design Token 派生背景与前景色。
- 支持 `value=-1` 或 equivalent sentinel 表示无数值内容，允许动态 value 更新为 dot/icon/number 的不同显示结果。
- 提供可嵌入任意 QWidget 的轻量 badge，VisualCheck 展示独立样式、按钮角标、导航列表项角标和动态 value 状态。
- 增加自动化测试，覆盖默认属性、尺寸映射、value clamp/sentinel、状态颜色选择、opacity、custom color、icon/value/dot 显示模式、主题刷新和 VisualCheck 守卫。

## Capabilities

### New Capabilities

- `info-badge`: 覆盖 Fluent Design Qt 组件库中的 WinUI 3 风格 InfoBadge 行为、视觉状态、尺寸、主题色、动态数值和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/status_info/InfoBadge.h`、`src/view/status_info/InfoBadge.cpp`。
- 更新测试配置：`tests/views/status_info/CMakeLists.txt` 注册 `test_info_badge`。
- 新增测试：`tests/views/status_info/TestInfoBadge.cpp`。
- 依赖现有 Design Token、`FluentElement`、`QMLPlus`、Qt painting 和 Segoe Fluent Icons 字体，不引入第三方依赖。
- 不修改已有 `ToolTip`、`ProgressRing` 或其它 Status & Info 控件公共 API。