## Why

当前文本输入组件库已有 `LineEdit` 和 `AutoSuggestBox`，但缺少 WinUI 3 常见表单场景里的密码输入控件。业务侧如果直接使用 `LineEdit` 组合按钮，需要重复处理密码遮罩、显示/隐藏密码按钮、Header 高度、边距和状态绘制，容易与 Fluent Text Fields 设计不一致。

本变更新增基于 `LineEdit` 派生的 `PasswordBox`，对齐 Figma Windows UI Kit 中 Password Box 的 32px 输入框、可选 Header、Reveal 按钮状态，以及 WinUI Gallery 中 `Password`、`PasswordChanged`、`PasswordRevealMode` 的核心行为。

## What Changes

- 新增 `PasswordBox` 文本字段组件，继承 `LineEdit`，作为密码输入专用控件。
- 提供独立的 `password` 属性和 `passwordChanged` 信号，避免业务代码依赖被遮罩后的显示文本。
- 支持 `PasswordRevealMode`，覆盖隐藏、持续显示、按住显示三类常用密码可见性策略。
- 支持可选 Header、placeholder、disabled/read-only/focus/hover/pressed 等 Fluent 状态，并复用 `LineEdit` 的主题字体、选择色、底部聚焦高亮和内容边距机制。
- 增加右侧 Reveal 按钮，按钮尺寸和交互状态对齐 Password Box 设计，不与文本、清除按钮或边框重叠。
- 增加单元测试和 VisualCheck，覆盖属性、信号、显示模式、键鼠交互、主题状态和 Header 布局。

## Capabilities

### New Capabilities

- `password-box`: 覆盖 Fluent Design Qt 组件库中的 WinUI 3 风格密码输入控件行为、视觉状态和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/textfields/PasswordBox.h`、`src/view/textfields/PasswordBox.cpp`。
- 更新构建配置：根或 textfields 相关 CMake 目标需要纳入新组件源码，测试 CMake 需要注册 `test_password_box`。
- 新增测试：`tests/views/textfields/TestPasswordBox.cpp`。
- 依赖现有组件：`LineEdit`、`Button`、`FluentElement`、`QMLPlus`、Design Token。
- 不引入新的第三方依赖，不改变已有 `LineEdit` / `AutoSuggestBox` 公共 API。