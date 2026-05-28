## Context

`src/view/textfields/` 已有 `LineEdit`、`PasswordBox`、`TextEdit`、`TextBlock` 和 `AutoSuggestBox`。其中 `LineEdit` 已实现 WinUI 3 风格的 32px 单行输入框、底部 accent focus 高亮、hover/disabled/read-only 状态、clear button、主题字体和文本 palette；`PasswordBox` 进一步展示了在 `LineEdit` 基础上绘制 header、预留右侧按钮空间、用 `view::basicinput::Button` 组合内嵌按钮的模式。

Figma Windows UI Kit 的 Number Box 主组件包含 48 个变体，布局分为 `Equation`、`Inline Spinner` 和 `Compact Spinner`，覆盖 Light/Dark、Rest/Hover/Pressed/Disabled、Focus、Text Entered、Clear hover/pressed 与 Spinner hover/pressed；控件本体为 124x32，Body 14/20 文本，Control radius 约 4px，focus 状态使用底部 2px accent 高亮。WinUI Gallery 的 NumberBox 页面展示三类首版必须覆盖的用例：表达式输入、带 SpinButton 的整数输入、按 0.25 round half up 的格式化金额输入。

## Goals / Non-Goals

**Goals:**

- 新增 `NumberBox : public LineEdit`，保持 textfields 模块现有三重继承能力来自 `LineEdit`。
- 对齐 WinUI NumberBox 的核心语义：`value`、`minimum`、`maximum`、`smallChange`、`largeChange`、`acceptsExpression`、`spinButtonPlacementMode`、格式化和 `valueChanged`。
- 覆盖 Figma 的三种布局：无 spinner 的 equation/clear-button 输入、Inline spinner、Compact spinner。
- 支持基础表达式解析和提交行为，避免业务侧自己计算 `1 + 2^2` 这类输入。
- 支持 header + 32px 输入框视觉结构，并沿用 `LineEdit` 的主题、字体、selection、placeholder 和 focus 绘制。
- 提供自动化测试和 VisualCheck，VisualCheck 布局参考用户截图中的三个 WinUI Gallery 示例区块。

**Non-Goals:**

- 不实现完整数学函数库、变量、单位换算、locale-aware 表达式语法或任意脚本执行。
- 不引入 QtQml/QJSEngine、第三方表达式库或新的运行时依赖。
- 不实现 WinUI 的完整 automation peer / event args 类型；Qt 首版使用 Qt signal 和 widget API 表达同等行为。
- 不修改 `LineEdit`、`PasswordBox`、`TextEdit`、`AutoSuggestBox` 的现有公共 API。

## Decisions

### 1. 继承 LineEdit，而不是包装 QDoubleSpinBox

`NumberBox` 继承 `LineEdit`，复用现有 Fluent 输入框绘制、clear button、placeholder、selection、字体和主题逻辑。右侧 spinner、header 和数值提交逻辑在 `NumberBox` 中扩展。

备选方案是包装 `QDoubleSpinBox`，但 Qt 原生 spin box 的绘制、子控件结构、文本验证和按钮布局会绕开项目已有的 Fluent 自绘模式，也难以匹配 Figma 的 Equation/Inline/Compact 三种变体。

### 2. 使用 WinUI 风格的 value = NaN 表达无效输入

`value` 使用 `double`，默认值为 `NaN`，这与 WinUI Gallery 示例中对 `double.IsNaN(sender.Value)` 的处理一致。空文本、无法解析的输入或除零结果会让 `value` 进入 `NaN` 状态；有效数值则根据 range 进行 clamp。

这样比强制回退到上一个有效值更透明，业务可以决定无效输入时是提示、清空还是重置。控件仍保留 `lastValidValue` 供 spinner 操作使用：当当前值为 `NaN` 且用户点击 spinner 时，从 range 内的 0 或 `minimum` 开始步进。

### 3. 表达式解析使用受限递归下降 parser

当 `acceptsExpression=true` 时，提交文本时用组件内部的受限 parser 解析数字、空白、括号、一元正负号、`+`、`-`、`*`、`/`、`^`。parser 仅接受这些 token，并返回 success/value/error，不执行脚本、不访问外部状态。

备选方案是 `QJSEngine`，但会引入 QtQml 依赖和脚本语义风险；简单字符串替换或 `QRegularExpression` 拼接也不够稳健。受限 parser 更符合首版需求，测试边界也清晰。

### 4. SpinButton 使用 RepeatButton 子控件组合

Inline 和 Compact spinner 由两个 `view::basicinput::RepeatButton` 组成，分别使用 `Typography::Icons::ChevronUp` / `ChevronDown`，`Button::Standard` + `IconOnly` + `Small`，并开启按住重复触发。`Standard` 样式让两种模式下按钮边框默认可见，hover 反馈交给按钮自身状态色处理。

`Inline` 模式在输入框右侧常驻水平排列的 down/up 按钮区，两个按钮使用独立的 `inlineSpinButtonSize`，默认高度高于 Compact 按钮，按钮之间通过 `spinButtonSpacing` 留出间隔，并始终预留文本右边距；`Compact` 模式在输入框右侧常驻上下叠放按钮，继续使用 `spinButtonSize`，默认按钮高度小于输入框半高，使按钮整体与上下边框保持间隙；`Hidden` 模式不显示 spinner，允许沿用 `LineEdit` clear button。spinner 可见时禁用 clear button，避免右侧 clear button 与 spinner 互相遮挡。

Spinner 的按钮尺寸和相关间距不作为不可变常量写死：`spinButtonSize`、`inlineSpinButtonSize`、`spinButtonRightMargin`、`compactSpinButtonReservedWidth`、`spinButtonSpacing`、`spinButtonTextGap` 和 `spinButtonIconSize` 都暴露为 Qt 属性。默认值仍对齐 Figma/WinUI 视觉（Compact 24x14、Inline 24x20、4px 右边距、14px compact 预留宽度、2px spinner 按钮间距、2px 文本间距、10px glyph），但调用方可按产品密度或视觉需求调整；Compact 当前常驻完整按钮，文本边距按完整按钮宽度计算。

### 5. 格式化拆成小数位和增量 rounder

首版提供 `displayPrecision` 和 `formatStep`：`displayPrecision >= 0` 时提交/失焦后按固定小数位显示；`formatStep > 0` 时先按 round half up 贴合指定增量，再进行小数位格式化。WinUI Gallery 的金额示例可配置为 `displayPrecision=2`、`formatStep=0.25`。

数值本身保持 double，格式化只影响显示文本。正在编辑时不强制重排文本，避免用户输入中途被打断。

### 6. Header 和视觉状态沿用 PasswordBox 模式

`NumberBox` 使用 `header` 属性时，推荐高度为 `20 + 8 + 32 = 60px`；无 header 时推荐高度为 32px。绘制 header 的字体、颜色和 elide 行为参考 `PasswordBox`，输入框状态色委托给 `LineEdit`/现有 token，必要时在 `NumberBox` 中关闭 `LineEdit` 内置 frame 并绘制组合 frame。

## Risks / Trade-offs

- 表达式 parser 与 WinUI NumberBox 的边界语义可能不完全一致 -> 首版限定支持运算符集合，并用测试固定优先级、括号、幂、一元符号和错误场景。
- `NaN` 默认值需要业务侧理解 -> 测试和 VisualCheck 覆盖空值/无效输入，示例中展示有效设置初值的用法。
- Compact spinner 的显示时机在不同平台 hover/focus 事件上可能有差异 -> 使用控件内部状态统一驱动，测试验证 visibility 和文本边距，视觉微调用 VisualCheck 完成。
- `formatStep` 的浮点误差可能影响 0.25 这类增量 -> 使用 `qRound64(value / step)` 风格的 round half up 并在格式化前归一到小数位，测试覆盖正负值和边界。

## Migration Plan

1. 新增 `NumberBox` 源码，根 CMake 的 `src/*.h` / `src/*.cpp` glob 会纳入 `fluent_qt_lib`。
2. 新增并注册 `test_number_box`。
3. 增量验证：`cmake --build build --target test_number_box && SKIP_VISUAL_TEST=1 ./build/tests/views/textfields/test_number_box`。
4. 按需运行 VisualCheck，对照用户截图和 Figma 变体微调右侧按钮间距、focus 高亮和 header 布局。
5. 若实现出现回归，移除新增源码和测试注册即可回滚，不影响现有组件 API。

## Open Questions

- 是否需要后续增加 locale-specific decimal separator / thousands separator；首版按 C locale / `QLocale::c()` 解析，避免表达式语法被系统区域设置改变。
- 是否需要暴露 `commitOnFocusOut` 或 `invalidInputBehavior`；首版固定为 Enter/失焦提交，解析失败时 `value=NaN` 并保留用户文本。