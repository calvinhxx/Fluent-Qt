## Context

`src/view/status_info/` 目前只有 `ToolTip`，组件遵循 Qt Widgets 自绘和 `FluentElement` Design Token 访问模式。Figma Windows UI Kit 的 Progress 页面将 Ring 作为 Progress 组件的布局变体，覆盖 Light/Dark、Determinate/Indeterminate、16px/32px/64px ring size，并使用 3px 圆角描边；Running 在浅色主题为 `#005FB8`、深色主题为 `#60CDFF`，Paused/Error 使用 Caution/Critical 语义色。WinUI Gallery 的 `ProgressRingPage` 展示两个核心行为：indeterminate 示例由 `IsActive` 控制，determinate 示例由 `Value` 控制并通过 `NumberBox` 调节，背景示例在 Transparent 与 LightGray 之间切换。

## Goals / Non-Goals

**Goals:**

- 新增 `ProgressRing : public QWidget, public FluentElement, public view::QMLPlus`，保持状态信息组件的轻量自绘风格。
- 提供 WinUI 风格的 `isActive`、`isIndeterminate`、`minimum`、`maximum`、`value` 和 `backgroundVisible` 行为。
- 提供 Figma 对应的 16px、32px、64px 三档尺寸、3px 默认描边、Running/Paused/Error 状态色和 Light/Dark 主题响应。
- 使用 Qt 自绘和轻量 timer 实现 indeterminate 动画，不依赖 Figma 导出的图片资产。
- 提供 GTest 与 VisualCheck，覆盖属性、边界、动画启停、主题和尺寸状态。

**Non-Goals:**

- 不实现横向 `ProgressBar`；Figma 同页里的 Bar 变体另行提案。
- 不实现文本百分比、任务取消按钮、阻塞遮罩或对话框级 loading overlay。
- 不引入 Lottie、SVG 或其他动画资源格式。
- 不改造 `ToolTip`、`FluentElement` 或全局主题系统公共 API。

## Decisions

### 1. 直接继承 QWidget 并自绘 ring

`ProgressRing` 直接继承 `QWidget`，在 `paintEvent()` 中使用 `QPainter::Antialiasing`、圆帽 `QPen` 和 `drawArc()` 绘制轨道与进度弧。相比组合 `QProgressBar` 或使用图片资源，自绘更符合本项目控件模式，也能让尺寸、颜色和动画完全从 Design Token 派生。

### 2. API 使用枚举表达尺寸和状态

定义 `enum class ProgressRingSize { Small, Medium, Large }`，分别映射 16、32、64 px；默认 `Medium`。定义 `enum class ProgressRingStatus { Running, Paused, Error }`，颜色分别映射 `themeColors().accentDefault`、`systemCaution`、`systemCritical`。`Q_ENUM` 暴露给元对象系统，`Q_PROPERTY` 使用枚举而不是裸字符串，避免在实现和测试中散落 magic string。

### 3. 进度模型按 QProgressBar 习惯归一化

默认 `minimum=0`、`maximum=100`、`value=0`。setter 保持 `maximum > minimum`，`value` 始终 clamp 到有效范围。Determinate 模式用 `(value - minimum) / (maximum - minimum)` 得到 0 到 1 的比例，起点固定为 12 点方向，按顺时针绘制弧线。

### 4. IsActive 控制可见进度与动画生命周期

默认 `isActive=false`，避免构造控件就启动持续动画；业务显示长任务时显式设为 true。`isActive=false` 时控件保留 size hint，但不绘制指示弧且停止 timer。Indeterminate + Running + enabled + active 时启动 timer 推进相位；Determinate、Paused、Error、disabled 或 inactive 时停止 timer。

### 5. Indeterminate 动画使用 timer + phase，而不是 property animation

组件内部使用 `QTimer` 或 `QBasicTimer` 以约 60 FPS 更新 `m_animationPhase`，通过 `themeAnimation().normal` 推导转速基准。绘制时使用固定长度的圆帽弧段旋转，避免复杂 easing 状态机。这样能稳定跨平台运行，也便于测试通过 active 状态判断 timer 生命周期。

### 6. 背景轨道显式控制

`backgroundVisible=false` 表示 Transparent 背景语义，不绘制完整轨道；`true` 时绘制浅灰/语义弱化轨道，浅色主题使用 `strokeDivider`/中性灰透明色，深色主题使用 `strokeSurface`/白色透明色。这个属性覆盖 WinUI Gallery 中 Background 选择器的核心差异，而不把 `QBrush background` 暴露成首版必需 API。

## Risks / Trade-offs

- Indeterminate 动画与 WinUI Composition 的精确 easing 不完全一致 -> 首版以 Figma 尺寸、描边、颜色和运行感为准，后续可根据视觉审查微调弧长和转速。
- 默认 `isActive=false` 可能让直接创建的控件不显示 -> VisualCheck 和示例显式设置 active，API 文档/测试覆盖默认行为。
- `Paused`/`Error` 在 Figma Ring 变体中没有完整导出 -> 使用同一 Progress 组件里的 Caution/Critical token，作为静态语义 ring 状态。
- 60 FPS timer 对大量 ring 可能有成本 -> timer 只在 active indeterminate running 状态启动，控件隐藏、禁用或销毁时停止。

## Migration Plan

1. 新增 `ProgressRing` 源码并由现有源文件 glob 纳入 `fluent_qt_lib`。
2. 新增并注册 `test_progress_ring`。
3. 增量验证：构建并运行 `test_progress_ring`，VisualCheck 按需单独运行。
4. 若视觉审查发现动画与 WinUI 差异明显，在实现内调整弧长/转速，不扩大公共 API。

## Open Questions

- 是否需要在后续版本暴露完全自定义颜色/直径 API，取决于业务是否有超出 Figma 三档尺寸和语义状态的使用场景。
