## ADDED Requirements

### Requirement: ProgressBar 公共 API
系统 SHALL 提供一个 `ProgressBar` 组件，继承 `QWidget`、`FluentElement` 和 `view::QMLPlus`，并暴露横向进度条所需的 mode、range、value、state、size 和 rail 属性。

#### Scenario: 默认构造
- **WHEN** 创建一个 `ProgressBar`
- **THEN** `isIndeterminate` MUST 为 false、`minimum` MUST 为 0、`maximum` MUST 为 100、`value` MUST 为 0、`showPaused` MUST 为 false、`showError` MUST 为 false、`barWidth` MUST 为 220、`trackThickness` MUST 为 3px、`railVisible` MUST 为 true，并且控件 MUST 不启动动画 timer

#### Scenario: 属性变更信号
- **WHEN** 调用任一属性 setter 且新值与旧值不同
- **THEN** 组件 MUST 更新绘制状态并触发对应的 changed 信号一次

#### Scenario: 重复设置相同值
- **WHEN** 调用任一属性 setter 且新值与旧值相同
- **THEN** 组件 MUST 不触发 changed 信号且 MUST 不重启动画 timer

#### Scenario: Error 优先于 Paused
- **WHEN** `showPaused` 和 `showError` 同时为 true
- **THEN** 组件 MUST 使用 Error 状态色与 Error 状态语义

### Requirement: Determinate 进度模型
系统 SHALL 在 determinate 模式下根据 `minimum`、`maximum` 和 `value` 计算进度比例，并从左到右绘制对应长度的 filled track。

#### Scenario: 设置有效进度值
- **WHEN** `isIndeterminate` 为 false、`minimum` 为 0、`maximum` 为 100，并调用 `setValue(40)`
- **THEN** `value()` MUST 返回 40，并且 `progressRatio()` MUST 返回 0.4

#### Scenario: value 低于范围
- **WHEN** 调用 `setValue()` 且传入值小于 `minimum`
- **THEN** `value()` MUST clamp 到 `minimum`，并触发一次 value changed 信号

#### Scenario: value 高于范围
- **WHEN** 调用 `setValue()` 且传入值大于 `maximum`
- **THEN** `value()` MUST clamp 到 `maximum`，并触发一次 value changed 信号

#### Scenario: range 无效时归一化
- **WHEN** 调用 range setter 导致 `maximum` 小于或等于 `minimum`
- **THEN** 组件 MUST 调整边界以保持 `maximum > minimum`，并保证 `value` 仍处于有效范围内

#### Scenario: 0 进度绘制
- **WHEN** `isIndeterminate` 为 false 且 `progressRatio()` 为 0
- **THEN** 组件 MUST 允许绘制 rail，但 MUST 不绘制 filled track

### Requirement: Indeterminate 动画行为
系统 SHALL 在 indeterminate running 状态下绘制移动线段，并按控件可见性、启用状态和状态属性管理动画生命周期。

#### Scenario: indeterminate running 启动动画
- **WHEN** `isIndeterminate` 为 true、`showPaused` 为 false、`showError` 为 false、控件启用且可见
- **THEN** 组件 MUST 启动内部 timer 推进动画相位，并周期性请求重绘

#### Scenario: 切换到 determinate 停止动画
- **WHEN** `isIndeterminate` 从 true 变为 false
- **THEN** 组件 MUST 停止内部 timer，并按当前 `value` 绘制静态 filled track

#### Scenario: Paused 或 Error 停止动画
- **WHEN** `showPaused` 或 `showError` 为 true
- **THEN** 组件 MUST 停止 indeterminate timer，并使用对应语义色绘制静态 track

#### Scenario: disabled 停止动画
- **WHEN** 控件被禁用
- **THEN** 组件 MUST 停止内部 timer，并使用 disabled 语义色绘制或隐藏进度 track

#### Scenario: hide 停止动画
- **WHEN** 控件隐藏或销毁
- **THEN** 组件 MUST 停止内部 timer，且后续 MUST 不再请求动画 tick

### Requirement: 尺寸与绘制
系统 SHALL 按 Figma Windows UI Kit 的 Bar 变体绘制默认 220x32 尺寸、中心 rail 和 3px 圆角 track。

#### Scenario: 默认尺寸
- **WHEN** 创建一个默认 `ProgressBar`
- **THEN** `sizeHint()` MUST 返回宽 220、高 32 的尺寸，并且 `minimumSizeHint()` MUST 至少保留 32px 高度

#### Scenario: 自定义 bar 宽度
- **WHEN** 调用 `setBarWidth()` 且传入正数
- **THEN** `sizeHint().width()` MUST 使用该宽度，并触发一次 bar width changed 信号

#### Scenario: 非正 bar 宽度
- **WHEN** 调用 `setBarWidth()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效 bar width 不变

#### Scenario: 自定义 track 厚度
- **WHEN** 调用 `setTrackThickness()` 且传入正数
- **THEN** 组件 MUST 使用该厚度绘制 determinate filled track 和 indeterminate segment，并将圆角半径设置为厚度的一半

#### Scenario: 非正 track 厚度
- **WHEN** 调用 `setTrackThickness()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效 track thickness 不变

#### Scenario: rail 显示
- **WHEN** `railVisible` 为 true
- **THEN** 组件 MUST 在控件垂直中心绘制 1px rail，并在其上方绘制 progress track

#### Scenario: rail 隐藏
- **WHEN** `railVisible` 为 false
- **THEN** 组件 MUST 不绘制 rail，但 MUST 保持 progress track 的位置与尺寸语义不变

### Requirement: 主题色与状态色
系统 SHALL 根据当前 Fluent 主题、启用状态、`showPaused` 和 `showError` 选择 progress track 与 rail 颜色。

#### Scenario: Running 状态色
- **WHEN** `showPaused` 为 false 且 `showError` 为 false
- **THEN** progress track MUST 使用 `themeColors().accentDefault`，浅色主题对应 Figma 的 `#005FB8`，深色主题对应 `#60CDFF`

#### Scenario: Paused 状态色
- **WHEN** `showPaused` 为 true 且 `showError` 为 false
- **THEN** progress track MUST 使用 `themeColors().systemCaution`，浅色主题对应 Figma 的 `#9D5D00`，深色主题对应 `#FCE100`

#### Scenario: Error 状态色
- **WHEN** `showError` 为 true
- **THEN** progress track MUST 使用 `themeColors().systemCritical`，浅色主题对应 Figma 的 `#C42B1C`，深色主题对应 `#FF99A4`

#### Scenario: Disabled 状态色
- **WHEN** 控件被禁用
- **THEN** progress track MUST 使用 disabled 语义色，并且 indeterminate animation MUST 停止

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** `ProgressBar` MUST 在 `onThemeUpdated()` 中刷新 rail、track 和 disabled 颜色，并请求重绘

### Requirement: WinUI Gallery 风格交互语义
系统 SHALL 支持 WinUI Gallery `ProgressBarPage` 中的 indeterminate 状态切换和 determinate 数值驱动示例。

#### Scenario: Indeterminate state controls
- **WHEN** 一个 indeterminate `ProgressBar` 绑定到 Running、Paused、Error 三个状态输入
- **THEN** Running MUST 显示活动 indeterminate segment，Paused MUST 显示 caution 静态 track，Error MUST 显示 critical 静态 track

#### Scenario: NumberBox drives determinate value
- **WHEN** 外部数值输入调用 `setValue()` 并传入 1、2、3
- **THEN** `ProgressBar` 的 `value()` 与 `progressText()` MUST 分别反映这些数值，并且 determinate filled track MUST 随比例增长

#### Scenario: NaN 输入防护
- **WHEN** 外部数值输入提供非有限数值
- **THEN** 组件的公开 setter MUST 不产生无效绘制状态，并且测试示例 MUST 能将输入恢复到 0

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `ProgressBar` 提供自动化测试和 VisualCheck，覆盖行为、主题和 Figma / WinUI Gallery 关键视觉变体。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_progress_bar`
- **THEN** 测试 MUST 覆盖默认属性、属性信号、range/value clamp、progress ratio、bar width、track thickness、railVisible、showPaused/showError 优先级、动画启停和主题响应

#### Scenario: VisualCheck
- **WHEN** 运行 `test_progress_bar --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Light/Dark、Determinate/Indeterminate、Running/Paused/Error、disabled、rail visible/hidden、130px WinUI Gallery 示例宽度和 220px Figma 默认宽度，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫
