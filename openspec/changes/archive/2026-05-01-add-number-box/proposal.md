## Why

当前 Fluent Qt 组件库已有通用 `LineEdit`、`PasswordBox` 和 `AutoSuggestBox`，但缺少 WinUI 3 常用的数字输入控件。业务侧如果临时组合 `LineEdit` 与按钮，需要重复处理数值解析、范围约束、步进按钮、表达式计算、格式化和 Fluent 视觉状态，容易和 Figma Windows UI Kit / WinUI Gallery 的 NumberBox 行为不一致。

本变更新增自定义 `NumberBox`，参考 Figma Windows UI Kit 的 Number Box 组件结构、用户提供的 WinUI Gallery 截图，以及 WinUI Gallery 中表达式输入、SpinButton placement 和 0.25 格式化示例。

## What Changes

- 新增 `NumberBox` 文本字段组件，建议位于 `src/view/textfields/`，复用现有 `LineEdit` 的 Fluent 输入框底座并扩展数值语义。
- 支持 `value`、`minimum`、`maximum`、`smallChange`、`largeChange`、`header`、`placeholderText`、`acceptsExpression`、`spinButtonPlacementMode`、`displayPrecision`、`formatStep` 等首版属性。
- 支持用户输入普通数字和基础代数表达式，并在提交时转换为数值；表达式覆盖加、减、乘、除、括号和幂运算等 WinUI Gallery 示例期望。
- 支持 SpinButton `Inline` 与 `Compact` 两种布局：Inline 在输入框右侧常驻上下步进区域，Compact 在 hover/focus 时显示更紧凑的步进按钮。
- 支持按步长递增/递减、范围 clamp、无效输入恢复/保留策略、`valueChanged` 信号，以及格式化到固定小数位或指定增量。
- 新增 GTest 与 VisualCheck，覆盖默认属性、数值解析、表达式求值、范围/步进、SpinButton 布局、格式化、主题响应和截图中的三类示例。

## Capabilities

### New Capabilities

- `number-box`: 规范 Fluent Qt `NumberBox` 的公共 API、数值输入/表达式计算、步进按钮、格式化、视觉状态和测试要求。

### Modified Capabilities

无。

## Impact

- 新增源码：`src/view/textfields/NumberBox.h`、`src/view/textfields/NumberBox.cpp`。
- 更新测试配置：`tests/views/textfields/CMakeLists.txt` 注册 `test_number_box`。
- 新增测试：`tests/views/textfields/TestNumberBox.cpp`。
- 复用现有 `LineEdit`、`view::basicinput::Button`、`TextBlock`、`FluentElement`、`QMLPlus`、Design Token 和 Qt Validator/事件系统。
- 不引入新的第三方依赖，不改变现有 `LineEdit`、`PasswordBox`、`TextEdit` 或 `AutoSuggestBox` 的公共 API。