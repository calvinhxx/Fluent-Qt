## Context

`src/view/status_info/` 目前已有 `ToolTip` 与 `ProgressRing`。`ProgressRing` 已建立了状态信息控件的首选实现模式：直接继承 `QWidget`，混入 `FluentElement` 与 `view::QMLPlus`，通过 `QPainter` 自绘、缓存主题色，并用 `QBasicTimer` 管理 indeterminate 动画生命周期。

Figma Windows UI Kit 的 `Progress` 节点显示横向 Bar 与 Ring 共享同一 Progress 组件。Bar 变体包含 Light/Dark、Determinate/Indeterminate、Running/Paused/Error：默认容器为 220x32，rail 在垂直中心，rail 约 1px，filled/indeterminate track 为 3px 圆角线段；Running 色为 Light `#005FB8` / Dark `#60CDFF`，Paused 为 Light `#9D5D00` / Dark `#FCE100`，Error 为 Light `#C42B1C` / Dark `#FF99A4`。WinUI Gallery `ProgressBarPage` 展示两个核心行为：indeterminate 示例通过 `IsIndeterminate=True` 并绑定 `ShowPaused` / `ShowError`，determinate 示例通过 `NumberBox` 写入 `ProgressBar.Value`，UI test 验证控件可见、可用、有尺寸、ControlType 为 ProgressBar，且文本值随数值输入递增。

## Goals / Non-Goals

**Goals:**

- 新增 `ProgressBar : public QWidget, public FluentElement, public view::QMLPlus`，保持 `status_info` 组件的一致继承与自绘风格。
- 支持 WinUI 风格的 `isIndeterminate`、`minimum`、`maximum`、`value`、`showPaused`、`showError`、`barWidth`、`trackThickness`、`railVisible` 行为。
- 按 Figma Bar 变体绘制 220x32 默认尺寸、中心 rail、3px 圆角 track、Light/Dark 主题和 Running/Paused/Error 状态色。
- 用轻量 timer 实现 indeterminate 运动，并在隐藏、禁用、determinate、paused/error 或销毁时停止。
- 提供 GTest 与 VisualCheck，覆盖 WinUI Gallery 风格的 indeterminate 状态切换和 determinate 数值驱动示例。

**Non-Goals:**

- 不替换 Qt 原生 `QProgressBar` 的所有 API，如文本格式模板、反向布局、垂直方向或 busy overlay。
- 不修改 `ProgressRing` 的公共 API；Bar 与 Ring 共享概念，但分别维护独立组件与规格。
- 不引入 Figma 导出的图片资产、SVG、Lottie 或新的第三方动画依赖。
- 不实现对话框级阻塞遮罩、任务取消按钮或百分比标签组件；测试示例可以外部组合 `Label` / `NumberBox`。

## Decisions

### 1. 直接自绘横向 bar，而不是包装 QProgressBar

`ProgressBar` 使用 `QPainter` 绘制 rail 和 track。包装 `QProgressBar` 会受平台 style 与 Fusion style 差异影响，难以稳定匹配 Figma 的 1px rail、3px 圆角 track 和主题 token。直接自绘也与 `ProgressRing`、`ToolTip` 的项目模式一致，后续 VisualCheck 更可控。

### 2. API 保留 WinUI 语义，同时使用 Qt 属性命名

组件暴露 `Q_PROPERTY(bool isIndeterminate ...)`、`minimum`、`maximum`、`value`、`showPaused`、`showError`、`barWidth`、`trackThickness`、`railVisible`。`showError` 优先级高于 `showPaused`，与 WinUI 示例中两个布尔状态输入的使用方式兼容；默认 Running 状态由两个布尔值都为 false 表示，避免为首版增加额外状态枚举。`barWidth` 默认 220、`sizeHint()` 高度默认 32，测试可将宽度改为 WinUI Gallery 示例的 130。

### 3. Determinate 进度模型复用 QProgressBar 习惯

默认 `minimum=0`、`maximum=100`、`value=0`。`setRange()` 保证 `maximum > minimum`，`setValue()` clamp 到有效范围。`progressRatio()` 返回 0 到 1。Determinate track 从左侧开始，宽度为可用绘制宽度乘以比例；比例为 0 时不绘制 filled track，但可绘制 rail。

### 4. Indeterminate 绘制为移动线段

Indeterminate 模式使用约 60 FPS 的 `QBasicTimer` 推进 `m_animationPhase`，在 bar 宽度内绘制固定长度线段。Figma 静态帧的 running indeterminate segment 约占 Bar 宽度 44%，因此首版使用可用宽度的约 44% 作为线段长度，并让它在 rail 上循环移动。Paused/Error indeterminate 使用对应语义色绘制静态中心线段，不推进动画。

### 5. 主题色从 FluentElement 缓存派生

`updateThemeColors()` 缓存：

- Running: `themeColors().accentDefault`
- Paused: `themeColors().systemCaution`
- Error: `themeColors().systemCritical`
- Disabled: `themeColors().accentDisabled`
- Rail: Light 使用 `strokeStrong`/强中性色透明度，Dark 使用 `strokeStrong`/白色透明度，必要时设置最小 alpha 以保证 1px rail 可见

`onThemeUpdated()` 刷新缓存并重绘。绘制时 `!isEnabled()` 优先使用 disabled 色，并停止 indeterminate timer。

### 6. 可访问语义与测试接口不依赖平台私有 API

首版设置 accessible name/description 时保留 Qt 默认机制，测试层直接验证对象属性、`progressText()` 或 `displayValue()` 辅助 getter，而不依赖 Windows UI Automation。`progressText()` 返回当前 determinate value 的整数文本，覆盖 WinUI Gallery UI test 里文本随 `NumberBox` 递增的核心行为。

## Risks / Trade-offs

- Indeterminate 动画不会完全复刻 WinUI Composition easing → 以 Figma 静态比例、方向和运行感为基准，VisualCheck 后微调 segment 长度和周期。
- 使用 `showPaused` / `showError` 两个布尔值可能表达出同时为 true 的状态 → 明确 `showError` 优先，测试覆盖优先级。
- Figma rail 色在导出代码中表现为较强透明中性色，和真实 WinUI 主题资源可能存在细微差异 → 首版从现有 Fluent token 派生，必要时只调整内部颜色计算，不扩大 API。
- 很多 indeterminate bar 同时运行可能带来 timer 成本 → timer 只在 visible、enabled、indeterminate、running 状态启动，隐藏或销毁时停止。

## Migration Plan

1. 新增 `ProgressBar` 源码，源文件由现有 CMake glob 或项目源列表纳入构建。
2. 新增并注册 `test_progress_bar`。
3. 构建并运行 `test_progress_bar`，默认跳过 VisualCheck。
4. 按需运行 VisualCheck，对比 Figma Bar 变体与 WinUI Gallery 示例布局。
5. 若视觉审查不通过，优先调整内部绘制常量和 token 映射，不变更公共 API。

## Open Questions

- 后续是否需要公开完整自定义颜色 API，取决于业务是否有超出 Running/Paused/Error/Disabled 的品牌化进度状态。
