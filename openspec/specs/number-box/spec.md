# NumberBox Specification

## Purpose

规范 textfields 模块中的 Fluent `NumberBox` 控件：它继承自 `LineEdit`，提供数字输入专用的 value、range、step、header、expression、spinner placement、spinner metrics、formatting 行为，以及对应的主题与测试要求。

## Requirements

### Requirement: NumberBox 公共 API
系统 SHALL 提供一个 `NumberBox` 组件，继承 `LineEdit`，并暴露数字输入所需的 value、range、step、header、expression、spinner placement、spinner metrics 和 formatting 属性。

#### Scenario: 默认构造
- **WHEN** 创建一个 `NumberBox`
- **THEN** `value` MUST 为 NaN、`minimum` MUST 为负无穷、`maximum` MUST 为正无穷、`smallChange` MUST 为 1、`largeChange` MUST 为 10、`acceptsExpression` MUST 为 false、`spinButtonPlacementMode` MUST 为 Hidden、`spinButtonSize` MUST 为 24x14、`inlineSpinButtonSize` MUST 为 24x20、`spinButtonRightMargin` MUST 为 4、`compactSpinButtonReservedWidth` MUST 为 14、`spinButtonSpacing` MUST 为 2、`spinButtonTextGap` MUST 为 2、`spinButtonIconSize` MUST 为 10、`displayPrecision` MUST 为 -1、`formatStep` MUST 为 0、`header` MUST 为空，并且输入框高度 MUST 为 32px

#### Scenario: 程序设置有效值
- **WHEN** 调用 `setValue(42.5)`
- **THEN** `value()` MUST 返回 42.5、输入文本 MUST 更新为该数值的显示文本，并且 `valueChanged` MUST 触发一次

#### Scenario: 程序设置 NaN
- **WHEN** 调用 `setValue(NaN)`
- **THEN** `value()` MUST 为 NaN、输入文本 MUST 清空，并且 `valueChanged` MUST 触发一次

#### Scenario: 重复设置相同值
- **WHEN** 调用任一属性 setter 且新值与旧值相同，或者重复设置 NaN 到 NaN
- **THEN** 组件 MUST 不触发对应 changed 信号且 MUST 不重排子控件

### Requirement: 数值解析与提交
系统 SHALL 在用户提交文本时解析数字输入，并根据 `acceptsExpression` 决定是否允许基础代数表达式。

#### Scenario: 提交普通数字
- **WHEN** 用户输入 `60` 并按 Enter 或让控件失焦
- **THEN** `value()` MUST 更新为 60，并且输入文本 MUST 按当前格式化设置显示

#### Scenario: 空文本提交
- **WHEN** 用户清空文本并提交
- **THEN** `value()` MUST 变为 NaN，并且控件 MUST 保留空文本

#### Scenario: expressions disabled 时提交表达式
- **WHEN** `acceptsExpression` 为 false，用户输入 `1 + 2^2` 并提交
- **THEN** `value()` MUST 变为 NaN，并且控件 MUST 保留用户输入文本以便修正

#### Scenario: expressions enabled 时提交表达式
- **WHEN** `acceptsExpression` 为 true，用户输入 `1 + 2^2` 并提交
- **THEN** `value()` MUST 更新为 5，并且输入文本 MUST 按当前格式化设置显示结果

#### Scenario: 表达式优先级
- **WHEN** `acceptsExpression` 为 true，用户提交包含括号、一元正负号、加减、乘除和幂运算的表达式
- **THEN** parser MUST 按括号最高、幂运算、乘除、加减的优先级计算，并正确处理一元正负号

#### Scenario: 无效表达式
- **WHEN** `acceptsExpression` 为 true，用户提交无法解析、括号不匹配或除零的表达式
- **THEN** `value()` MUST 变为 NaN，并且控件 MUST 保留用户输入文本以便修正

### Requirement: Range 与 step 行为
系统 SHALL 根据 `minimum`、`maximum`、`smallChange` 和 `largeChange` 管理有效数值范围、spinner 步进和键盘步进。

#### Scenario: value 低于 minimum
- **WHEN** `minimum` 为 10 且调用 `setValue(5)`
- **THEN** `value()` MUST clamp 到 10，并且显示文本 MUST 反映 clamp 后的值

#### Scenario: value 高于 maximum
- **WHEN** `maximum` 为 100 且调用 `setValue(120)`
- **THEN** `value()` MUST clamp 到 100，并且显示文本 MUST 反映 clamp 后的值

#### Scenario: range 无效时归一化
- **WHEN** 调用 range setter 导致 `maximum < minimum`
- **THEN** 组件 MUST 调整另一端边界以保持 `minimum <= maximum`，并且当前有效 value MUST 重新 clamp 到新范围

#### Scenario: smallChange 非正数
- **WHEN** 调用 `setSmallChange()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效 `smallChange` 不变

#### Scenario: largeChange 非正数
- **WHEN** 调用 `setLargeChange()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效 `largeChange` 不变

#### Scenario: spinner 增加数值
- **WHEN** 用户点击或按住上箭头 spinner
- **THEN** 组件 MUST 按 `smallChange` 增加当前 value，并 clamp 到 `maximum`

#### Scenario: spinner 减少数值
- **WHEN** 用户点击或按住下箭头 spinner
- **THEN** 组件 MUST 按 `smallChange` 减少当前 value，并 clamp 到 `minimum`

#### Scenario: spinner 到达 maximum
- **WHEN** `value` 已经等于 `maximum`
- **THEN** 上箭头 spinner MUST 进入 disabled 状态且下箭头 spinner MUST 保持可用

#### Scenario: spinner 到达 minimum
- **WHEN** `value` 已经等于 `minimum`
- **THEN** 下箭头 spinner MUST 进入 disabled 状态且上箭头 spinner MUST 保持可用

#### Scenario: NaN 状态下 spinner 起点
- **WHEN** 当前 `value` 为 NaN 且用户触发 spinner 步进
- **THEN** 组件 MUST 从 0 开始；如果 0 不在当前 range 内，则 MUST 从最接近 0 的 range 边界开始

#### Scenario: 键盘步进
- **WHEN** `NumberBox` 获得焦点，用户按 Up、Down、PageUp 或 PageDown
- **THEN** Up/Down MUST 按 `smallChange` 调整数值，PageUp/PageDown MUST 按 `largeChange` 调整数值，并且结果 MUST clamp 到 range

### Requirement: SpinButton placement
系统 SHALL 支持 Hidden、Inline 和 Compact 三种 `SpinButtonPlacementMode`，并保证 spinner 不遮挡文本、placeholder、selection 或 focus border。

#### Scenario: Hidden placement
- **WHEN** `spinButtonPlacementMode` 为 Hidden
- **THEN** 组件 MUST 不显示 spinner，并且 MAY 使用继承自 `LineEdit` 的 clear button 行为

#### Scenario: Inline placement
- **WHEN** `spinButtonPlacementMode` 为 Inline
- **THEN** 组件 MUST 在输入框右侧水平排列 down/up 两个 spinner 按钮，两个按钮 MUST 使用 `inlineSpinButtonSize`，两个按钮之间 MUST 按 `spinButtonSpacing` 留出间隔，文本右边距 MUST 永久预留两个按钮和间隔的完整空间，并且 clear button MUST 不显示

#### Scenario: Compact placement rest 状态
- **WHEN** `spinButtonPlacementMode` 为 Compact
- **THEN** 组件 MUST 在输入框右侧默认显示上下叠放的 spinner 按钮，上下按钮 MUST 使用 `spinButtonSize`，上下按钮整体 MUST 与输入框上下边框保持可见间隙，文本右边距 MUST 永久预留完整按钮空间，并且 clear button MUST 不显示

#### Scenario: Spinner button hover 状态
- **WHEN** `spinButtonPlacementMode` 为 Inline 或 Compact 且用户 hover spinner 按钮
- **THEN** spinner 按钮 MUST 保持可见边框，并只通过按钮自身的 Fluent hover 色更新反馈

#### Scenario: 配置 spinner 尺寸与间距
- **WHEN** 应用设置 `spinButtonSize`、`inlineSpinButtonSize`、`spinButtonRightMargin`、`compactSpinButtonReservedWidth`、`spinButtonSpacing`、`spinButtonTextGap` 或 `spinButtonIconSize`
- **THEN** 组件 MUST 更新 spinner 按钮几何、按钮 glyph 尺寸、公开属性值和文本右边距；无效的非正按钮尺寸或 icon 尺寸 MUST 被忽略

#### Scenario: 禁用状态
- **WHEN** `NumberBox` 被禁用
- **THEN** spinner MUST 不可交互，文本、placeholder、边框和 spinner glyph MUST 使用 disabled 语义色

### Requirement: 格式化与 rounder
系统 SHALL 支持固定小数位和指定增量 round half up 格式化，并且格式化 MUST 只影响显示文本，不改变已经 clamp 后的数值语义。

#### Scenario: 默认显示格式
- **WHEN** `displayPrecision` 为 -1 且 `formatStep` 为 0，并设置有效数值
- **THEN** 输入文本 MUST 使用紧凑十进制表示，不强制补零

#### Scenario: 固定小数位
- **WHEN** `displayPrecision` 为 2，并设置 value 为 3
- **THEN** 输入文本 MUST 显示为 `3.00`

#### Scenario: round to nearest step
- **WHEN** `formatStep` 为 0.25、`displayPrecision` 为 2，并提交 value 为 1.13
- **THEN** 输入文本 MUST 显示为 `1.25`，并且 `value()` MUST 反映 round 后的 1.25

#### Scenario: formatStep 非正数
- **WHEN** 调用 `setFormatStep()` 且传入 0 或负数
- **THEN** 组件 MUST 将 `formatStep` 视为禁用 rounder，不按增量舍入

#### Scenario: 编辑中不抢写文本
- **WHEN** 用户正在编辑尚未提交的文本
- **THEN** 组件 MUST 不因中间输入自动格式化而移动光标或替换文本

### Requirement: Fluent NumberBox 视觉结构
系统 SHALL 按 Figma Windows UI Kit Number Box 视觉结构绘制输入框、Header、状态颜色、聚焦底部高亮、clear button 和 spinner 状态。

#### Scenario: 无 Header 布局
- **WHEN** `header` 为空
- **THEN** 控件推荐高度 MUST 为 32px，输入框 MUST 占据整个控件高度，推荐最小宽度 MUST 不小于 124px

#### Scenario: 有 Header 布局
- **WHEN** `header` 非空
- **THEN** 控件推荐高度 MUST 为 60px，Header 文本 MUST 位于输入框上方，并且输入框高度 MUST 保持 32px

#### Scenario: 聚焦状态绘制
- **WHEN** `NumberBox` 获得焦点且处于启用、可编辑状态
- **THEN** 输入框 MUST 使用主题 accent 色绘制底部聚焦高亮

#### Scenario: Hover 与 Pressed 状态
- **WHEN** 用户 hover 或按下 `NumberBox` 输入区域、clear button 或 spinner
- **THEN** 组件 MUST 使用当前主题下的 Fluent hover/pressed control 填充和 stroke 色更新对应区域

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** `NumberBox`、clear button 和 spinner MUST 更新字体、文本颜色、placeholder、selection、输入框填充、边框、accent 高亮和 icon 颜色

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `NumberBox` 提供自动化测试和 VisualCheck，覆盖核心行为、Figma 关键变体和 WinUI Gallery 示例。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_number_box`
- **THEN** 测试 MUST 覆盖默认属性、属性信号、数值解析、表达式求值、无效输入 NaN、range clamp、small/large step、spinner visibility、Inline 水平排布及按钮间距、Inline/Compact 独立按钮尺寸、Compact 默认可见纵向排布及上下边框间隙、spinner metrics、keyboard step、format precision 和 0.25 rounder

#### Scenario: VisualCheck
- **WHEN** 运行 `test_number_box --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示表达式 NumberBox、Inline/Compact SpinButton placement、0.25 格式化金额 NumberBox、Header、disabled、Light/Dark 主题和 focus/hover 代表性状态，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫
