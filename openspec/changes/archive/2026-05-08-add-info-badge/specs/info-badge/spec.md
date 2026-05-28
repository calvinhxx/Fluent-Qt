## ADDED Requirements

### Requirement: InfoBadge 公共 API
系统 SHALL 提供一个 `InfoBadge` 组件，继承 `QWidget`、`FluentElement` 和 `view::QMLPlus`，并暴露 InfoBadge 所需的 value、icon、display mode、status、opacity 和自定义颜色属性。

#### Scenario: 默认构造
- **WHEN** 创建一个 `InfoBadge`
- **THEN** `value` MUST 为 `-1`、`displayMode` MUST 为 `Auto`、`status` MUST 为 `Attention`、`iconGlyph` MUST 为空、`badgeOpacity` MUST 为 `1.0`、custom colors MUST 为 invalid，并且控件 MUST 显示为 dot/beacon 形态

#### Scenario: 属性变更信号
- **WHEN** 调用任一属性 setter 且新值与旧值不同
- **THEN** 组件 MUST 更新绘制状态、更新几何（如尺寸受影响）并触发对应 changed 信号一次

#### Scenario: 重复设置相同值
- **WHEN** 调用任一属性 setter 且新值与旧值相同
- **THEN** 组件 MUST 不触发 changed 信号

### Requirement: DisplayMode 与 value/icon 形态选择
系统 SHALL 支持 `Auto`、`Dot`、`Icon`、`Value` 四种显示模式，并在 Auto 模式下根据 value 与 icon 自动选择可见形态。

#### Scenario: Auto value 形态
- **WHEN** `displayMode=Auto` 且 `value >= 0`
- **THEN** InfoBadge MUST 显示数值文本，并使用 value badge 尺寸

#### Scenario: Auto icon 形态
- **WHEN** `displayMode=Auto`、`value < 0` 且 `iconGlyph` 非空
- **THEN** InfoBadge MUST 显示 icon glyph，并使用 icon badge 尺寸

#### Scenario: Auto dot 形态
- **WHEN** `displayMode=Auto`、`value < 0` 且 `iconGlyph` 为空
- **THEN** InfoBadge MUST 显示 dot/beacon，并使用 beacon 尺寸

#### Scenario: 显式 Value 模式
- **WHEN** `displayMode=Value` 且 `value < 0`
- **THEN** InfoBadge MUST 不绘制数值文本，并保留最小 value badge 尺寸以避免布局跳动

### Requirement: InfoBadge 尺寸与绘制
系统 SHALL 按 Figma Windows UI Kit Badge 变体绘制 dot、icon、number 和 wide number badge。

#### Scenario: Dot 尺寸
- **WHEN** InfoBadge 有效形态为 Dot
- **THEN** `sizeHint()` MUST 返回 `4x4`，并在该矩形内绘制圆形 beacon

#### Scenario: Icon 尺寸
- **WHEN** InfoBadge 有效形态为 Icon
- **THEN** `sizeHint()` MUST 返回 `16x16`，并绘制圆形背景和居中 icon glyph

#### Scenario: 单字符 Value 尺寸
- **WHEN** InfoBadge 有效形态为 Value 且 value 文本宽度可放入 16px 圆形
- **THEN** `sizeHint()` MUST 至少返回 `16x16`，并绘制圆形背景和居中文本

#### Scenario: 宽数字 Value 尺寸
- **WHEN** InfoBadge 有效形态为 Value 且文本宽度超过 16px 圆形可用宽度
- **THEN** `sizeHint()` MUST 保持高度 16px，并按文本宽度加水平内边距扩展宽度，绘制胶囊形背景

#### Scenario: 透明度绘制
- **WHEN** `badgeOpacity` 为 0.0
- **THEN** InfoBadge MUST 保留 `sizeHint()` 但不绘制可见内容

### Requirement: 动态 Value 模型
系统 SHALL 支持动态数值更新，并使用 `-1` 作为无数值 sentinel。

#### Scenario: 设置有效 value
- **WHEN** 调用 `setValue(10)`
- **THEN** `value()` MUST 返回 10，Auto 模式 MUST 显示数值 `10`，并按宽数字尺寸更新几何

#### Scenario: 设置 sentinel value
- **WHEN** 调用 `setValue(-1)`
- **THEN** `value()` MUST 返回 -1，Auto 模式 MUST 回退到 icon 或 dot 形态

#### Scenario: value 低于 sentinel
- **WHEN** 调用 `setValue()` 且传入值小于 -1
- **THEN** 组件 MUST clamp 到 -1，并只触发一次 value changed 信号

### Requirement: 状态色与自定义颜色
系统 SHALL 根据当前 Fluent 主题和 `InfoBadgeStatus` 选择背景与前景色，并允许自定义颜色覆盖默认 token。

#### Scenario: Attention 状态色
- **WHEN** `status=Attention`
- **THEN** 背景 MUST 使用当前主题 accent/attention 语义色，浅色主题对应 Figma Attention `#005FB7/#005FB8` 附近色值，深色主题对应 `#60CDFF`

#### Scenario: Informational 状态色
- **WHEN** `status=Informational`
- **THEN** 背景 MUST 使用 `themeColors().systemInfo`

#### Scenario: Caution 状态色
- **WHEN** `status=Caution`
- **THEN** 背景 MUST 使用 `themeColors().systemCaution`

#### Scenario: Success 状态色
- **WHEN** `status=Success`
- **THEN** 背景 MUST 使用 `themeColors().systemSuccess`

#### Scenario: Critical 状态色
- **WHEN** `status=Critical`
- **THEN** 背景 MUST 使用 `themeColors().systemCritical`

#### Scenario: 自定义背景和前景色
- **WHEN** `customBackgroundColor` 或 `customTextColor` 为 valid QColor
- **THEN** 组件 MUST 优先使用自定义颜色绘制对应背景或文本/icon，而不是状态 token

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** InfoBadge MUST 在 `onThemeUpdated()` 中刷新背景、前景、disabled 和字体相关状态，并请求重绘

### Requirement: 嵌入其它控件
系统 SHALL 允许 InfoBadge 作为普通 QWidget 嵌入任意父控件或布局，且不依赖 NavigationView 专用 API。

#### Scenario: 嵌入按钮角落
- **WHEN** 调用方把 InfoBadge 放入按钮内部的右上角 overlay 布局
- **THEN** InfoBadge MUST 维持自身 sizeHint 和透明背景，不绘制额外窗口或遮挡父控件其它区域

#### Scenario: 嵌入导航行右侧
- **WHEN** 调用方把 InfoBadge 放入导航行右侧
- **THEN** InfoBadge MUST 能以 value/icon/dot 任一形态显示，且 opacity 为 0 时不触发父行重新布局

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `InfoBadge` 提供自动化测试和 VisualCheck，覆盖行为、主题和 Figma/WinUI Gallery 关键视觉变体。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_info_badge`
- **THEN** 测试 MUST 覆盖默认属性、属性信号、displayMode 选择、value clamp、尺寸映射、opacity、自定义颜色、状态切换和主题刷新行为

#### Scenario: VisualCheck
- **WHEN** 运行 `test_info_badge --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Light/Dark、Dot/Icon/Value/Wide Value、Informational/Attention/Caution/Success/Critical、按钮角标、导航行角标和动态 value 代表状态，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫