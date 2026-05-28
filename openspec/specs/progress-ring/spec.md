# ProgressRing Specification

## Purpose

规范 status_info 模块中的 Fluent `ProgressRing` 控件：它继承自 `QWidget`、`FluentElement` 和 `view::QMLPlus`，提供 indeterminate 与 determinate 环形进度、active 动画生命周期、range/value 模型、尺寸、描边、状态色、背景轨道，以及对应的主题与测试要求。

## Requirements

### Requirement: ProgressRing 公共 API
系统 SHALL 提供一个 `ProgressRing` 组件，继承 `QWidget`、`FluentElement` 和 `view::QMLPlus`，并暴露环形进度所需的 active、mode、range、value、size、stroke、status 和 background 属性。

#### Scenario: 默认构造
- **WHEN** 创建一个 `ProgressRing`
- **THEN** `isActive` MUST 为 false、`isIndeterminate` MUST 为 true、`minimum` MUST 为 0、`maximum` MUST 为 100、`value` MUST 为 0、`ringSize` MUST 为 Medium、`strokeWidth` MUST 为 3px、`status` MUST 为 Running，并且控件 MUST 不启动动画 timer

#### Scenario: 属性变更信号
- **WHEN** 调用任一属性 setter 且新值与旧值不同
- **THEN** 组件 MUST 更新绘制状态并触发对应的 changed 信号一次

#### Scenario: 重复设置相同值
- **WHEN** 调用任一属性 setter 且新值与旧值相同
- **THEN** 组件 MUST 不触发 changed 信号且 MUST 不重启动画 timer

### Requirement: Determinate 进度模型
系统 SHALL 在 determinate 模式下根据 `minimum`、`maximum` 和 `value` 计算进度比例，并以固定起点绘制对应弧长。

#### Scenario: 设置有效进度值
- **WHEN** `isIndeterminate` 为 false、`minimum` 为 0、`maximum` 为 100，并调用 `setValue(40)`
- **THEN** `value()` MUST 返回 40，并且绘制比例 MUST 为 0.4

#### Scenario: value 低于范围
- **WHEN** 调用 `setValue()` 且传入值小于 `minimum`
- **THEN** `value()` MUST clamp 到 `minimum`，并触发一次 value changed 信号

#### Scenario: value 高于范围
- **WHEN** 调用 `setValue()` 且传入值大于 `maximum`
- **THEN** `value()` MUST clamp 到 `maximum`，并触发一次 value changed 信号

#### Scenario: range 无效时归一化
- **WHEN** 调用 range setter 导致 `maximum` 小于或等于 `minimum`
- **THEN** 组件 MUST 调整边界以保持 `maximum > minimum`，并保证 `value` 仍处于有效范围内

### Requirement: Indeterminate 与 active 行为
系统 SHALL 在 indeterminate 模式下通过 active 状态控制动画生命周期，并在非运行状态停止动画。

#### Scenario: active indeterminate running 启动动画
- **WHEN** `isActive` 为 true、`isIndeterminate` 为 true、`status` 为 Running 且控件启用
- **THEN** 组件 MUST 启动内部 timer 推进动画相位，并周期性请求重绘

#### Scenario: inactive 停止动画
- **WHEN** `isActive` 从 true 变为 false
- **THEN** 组件 MUST 停止内部 timer，并且 MUST 不绘制进度指示弧

#### Scenario: 切换到 determinate 停止 indeterminate 动画
- **WHEN** `isIndeterminate` 从 true 变为 false
- **THEN** 组件 MUST 停止 indeterminate timer，并按当前 `value` 绘制静态进度弧

#### Scenario: Paused 或 Error 为静态状态
- **WHEN** `status` 为 Paused 或 Error
- **THEN** 组件 MUST 使用对应语义色绘制当前弧线，并且 MUST 不推进 indeterminate 动画相位

#### Scenario: disabled 停止动画
- **WHEN** 控件被禁用
- **THEN** 组件 MUST 停止内部 timer，并使用 disabled 语义颜色绘制或隐藏指示弧

### Requirement: ProgressRing 尺寸与绘制
系统 SHALL 按 Figma Windows UI Kit 的 Ring 变体绘制 16px、32px、64px 三档尺寸和 3px 默认圆帽描边。

#### Scenario: Small 尺寸
- **WHEN** `ringSize` 设置为 Small
- **THEN** `sizeHint()` MUST 返回 16x16，并且绘制区域 MUST 约束在 16px 正方形内

#### Scenario: Medium 尺寸
- **WHEN** `ringSize` 设置为 Medium
- **THEN** `sizeHint()` MUST 返回 32x32，并且绘制区域 MUST 约束在 32px 正方形内

#### Scenario: Large 尺寸
- **WHEN** `ringSize` 设置为 Large
- **THEN** `sizeHint()` MUST 返回 64x64，并且绘制区域 MUST 约束在 64px 正方形内

#### Scenario: 自定义描边宽度
- **WHEN** 调用 `setStrokeWidth()` 且传入正数
- **THEN** 组件 MUST 使用该描边宽度绘制轨道和进度弧，并将绘制 rect 按半个描边宽度内缩，避免边缘被裁剪

#### Scenario: 非正描边宽度
- **WHEN** 调用 `setStrokeWidth()` 且传入 0 或负数
- **THEN** 组件 MUST 保持上一个有效描边宽度不变

### Requirement: 主题色与状态色
系统 SHALL 根据当前 Fluent 主题和 `ProgressRingStatus` 选择指示弧与轨道颜色。

#### Scenario: Running 状态色
- **WHEN** `status` 为 Running
- **THEN** 指示弧 MUST 使用 `themeColors().accentDefault`，浅色主题对应 Windows UI Kit 的 `#005FB8`，深色主题对应 `#60CDFF`

#### Scenario: Paused 状态色
- **WHEN** `status` 为 Paused
- **THEN** 指示弧 MUST 使用 `themeColors().systemCaution`

#### Scenario: Error 状态色
- **WHEN** `status` 为 Error
- **THEN** 指示弧 MUST 使用 `themeColors().systemCritical`

#### Scenario: 主题切换
- **WHEN** 全局主题在 Light 和 Dark 之间切换
- **THEN** `ProgressRing` MUST 在 `onThemeUpdated()` 中刷新轨道、指示弧和 disabled 颜色，并请求重绘

### Requirement: 背景轨道
系统 SHALL 支持透明背景和可见轨道两种背景语义，以覆盖 WinUI Gallery 示例中的 Background 切换。

#### Scenario: 默认透明背景
- **WHEN** `backgroundVisible` 为 false
- **THEN** 组件 MUST 不绘制完整圆形轨道，只绘制 active 状态下的指示弧

#### Scenario: 显示背景轨道
- **WHEN** `backgroundVisible` 为 true
- **THEN** 组件 MUST 绘制完整圆形轨道，轨道颜色 MUST 使用当前主题下弱化的 stroke 或 neutral 语义色

#### Scenario: active false 时背景轨道
- **WHEN** `isActive` 为 false 且 `backgroundVisible` 为 true
- **THEN** 组件 MUST 允许绘制背景轨道，但 MUST 不绘制进度指示弧

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `ProgressRing` 提供自动化测试和 VisualCheck，覆盖行为、主题和 Figma 关键视觉变体。

#### Scenario: 自动化测试
- **WHEN** 构建并运行 `test_progress_ring`
- **THEN** 测试 MUST 覆盖默认属性、属性信号、range/value clamp、active 动画启停、determinate/indeterminate 切换、尺寸映射、stroke width 和 backgroundVisible 行为

#### Scenario: VisualCheck
- **WHEN** 运行 `test_progress_ring --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 Light/Dark、Small/Medium/Large、Determinate/Indeterminate、Running/Paused/Error、透明/可见背景轨道等代表性状态，包含可通过 `NumberBox` 调节 determinate ring value 的 WinUI Gallery 风格示例，并遵循 `SKIP_VISUAL_TEST` 环境变量守卫
