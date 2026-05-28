## Context

`src/view/scrolling/` 当前只包含 `ScrollBar`。`TestScrollBar.cpp` 中已经用局部 `FluentScrollArea` 手动 `setVerticalScrollBar(new ScrollBar(...))` 验证过集成方向，但项目还没有一个正式可复用的滚动容器。业务代码继续直接使用 `QScrollArea` 会导致默认 Qt 滚动条、样式同步和滚动 API 包装分散在各处。

Figma Windows UI kit 的 Scrolling 节点显示滚动体验以 Scroll Bar 为核心资产，覆盖 Light/Dark、vertical、horizontal、bidirectional、expanded/collapsed，以及 thumb/track 部件。WinUI Gallery 的 ScrollView 示例展示了基础内容滚动、水平/垂直滚动模式、滚动条可见性、程序化滚动和自定义滚动动画；ScrollViewer 示例还展示了 legacy ScrollViewer 的 zoom 和模式切换。

本项目是 Qt Widgets 自绘 Fluent 组件库，因此第一阶段应把 Qt 原生滚动行为和项目自绘滚动条稳定封装起来，而不是重新实现滚动物理或 Composition 动画系统。

## Goals / Non-Goals

**Goals:**

- 提供 `view::scrolling::ScrollView`，继承 `QScrollArea`、`FluentElement` 和 `view::QMLPlus`。
- 默认替换水平/垂直滚动条为项目自定义 `ScrollBar`，不得暴露 Qt 原生滚动条视觉。
- 支持垂直、水平、双向滚动使用场景，并提供滚动条可见性策略。
- 提供程序化滚动入口和 offset/scrollable range 查询，便于对齐 WinUI Gallery 的 `ScrollTo`/offset 示例。
- 响应主题切换，更新 viewport/content 背景和自定义 ScrollBar 视觉。
- 提供自动化测试和 VisualCheck，VisualCheck 应能展示主题切换、垂直/水平/双向内容滚动。

**Non-Goals:**

- 不承诺对任意 QWidget 做 WinUI UIElement 级别的真实 transform 缩放；Qt Widgets 没有统一的子控件缩放语义。
- 不实现 Composition 级别 keyframe 动画、惯性模型或 snap points；可用 Qt 滚动和可选 `QPropertyAnimation` 做基础程序化滚动/缩放。
- 不实现 AnnotatedScrollBar、外部 `IScrollController` 或页级滚动宿主抑制机制。
- 不重写现有 `ScrollBar` 视觉规格，除非集成 `ScrollView` 时发现必须修复的兼容问题。

## Decisions

### `ScrollView` 直接派生 `QScrollArea`

`QScrollArea` 已提供 QWidget 内容承载、viewport 裁剪、滚动范围计算、wheel/key/mouse drag 基础行为和 `ensureVisible()`。`ScrollView` 在此基础上做 Fluent 封装，比从 `QWidget` 重写滚动逻辑更稳，也符合用户指定的“通过 QScrollArea 派生”。

备选方案是用 `QWidget` + 自定义 viewport + 两个 ScrollBar 从零实现。该方案视觉控制更强，但会重复 Qt 已解决的布局、wheel、range、focus 和内容 resize 逻辑，风险不值得。

### 使用 `ScrollBar` 作为正式滚动条实现

构造阶段创建 `ScrollBar(Qt::Vertical, this)` 和 `ScrollBar(Qt::Horizontal, this)`，通过 `setVerticalScrollBar()`、`setHorizontalScrollBar()` 替换原生滚动条。`ScrollView` 负责滚动条策略、范围同步和主题触发，`ScrollBar` 继续负责 thumb/track 绘制和 hover/pressed/auto-hide 状态。

这样可以复用 Figma Scrolling 中已经映射到 `ScrollBar` 的视觉资产，并让 ScrollBar 的后续细节修正自然作用到 ScrollView。

### 显式暴露滚动模式和可见性策略

`ScrollView` 应提供 Qt 友好的枚举属性，例如 `horizontalScrollMode`、`verticalScrollMode`、`horizontalScrollBarVisibility`、`verticalScrollBarVisibility`。模式至少覆盖 `Disabled`、`Enabled`、`Auto`；可见性至少覆盖 `Disabled`、`Auto`、`Visible`、`Hidden`。内部映射到 `QAbstractScrollArea::ScrollBarPolicy` 和 `QScrollBar::setEnabled()/setVisible()`。

Qt 的 `ScrollBarPolicy` 不能完整区分 WinUI 的 “scroll enabled but scrollbar hidden”，因此需要组件级枚举，避免未来 API 被 Qt 原生策略限制。

### 程序化滚动使用 Qt 属性动画作为基础

`ScrollView` 提供 `scrollTo(int x, int y, bool animated = false)`、`scrollBy(int dx, int dy, bool animated = false)`、`horizontalOffset()`、`verticalOffset()`、`scrollableWidth()`、`scrollableHeight()` 等 API。动画开启时使用 `QPropertyAnimation` 驱动水平/垂直滚动条 value，并使用 `themeAnimation()` 的时长和曲线。

该设计覆盖 WinUI Gallery 的基础 `ScrollTo` 示例，同时不承诺 WinUI Composition 动画事件和 keyframe 替换能力。

### 缩放使用 widget 尺寸驱动并保留手势焦点

`ScrollView` 提供 `zoomMode`、`zoomFactor`、`minZoomFactor`、`maxZoomFactor`，以及 `zoomTo()`、`zoomBy()`、`resetZoom()`。缩放通过调整内容 widget 的固定尺寸来驱动 `QScrollArea` 重新计算滚动范围；原生 touchpad pinch、Smart Zoom 和 Ctrl+wheel 均映射到同一套 zoom factor 更新逻辑。

缩放时以触摸板/鼠标所在的 viewport 位置作为锚点，尽量保持指针下的内容坐标稳定。由于 Qt Widgets 不能对任意 QWidget 子树做统一 transform，内容如需精确视觉缩放，应实现 `ScrollViewZoomAware`，由 ScrollView 通知当前 zoom factor 并使用内容声明的 unscaled logical size 计算缩放尺寸。

macOS/Wayland native pinch 事件的 zoom value 是逐帧增量，按 `scale *= 1 + value` 应用。若平台同时投递高层 `QPinchGesture`、普通 wheel-scroll 尾流或 stray SmartZoom 事件，ScrollView 短暂抑制这些 duplicate/terminal 输入，避免 native pinch 已生效后又被滚动或智能缩放路径回滚。

### 主题和背景不通过 QSS 实现

实现应避免在组件内部使用 QSS。`onThemeUpdated()` 设置 frame shape、viewport auto fill、palette 或直接更新 viewport 背景；滚动条主题由 `ScrollBar::onThemeUpdated()` 和 FluentElement 全局主题机制处理。测试窗口可按现有测试模式设置背景，但组件本身遵循项目“不使用 QSS/QPalette 做 Design Token 替代”的约束：需要绘制或设置的颜色来自 `themeColors()`。

### VisualCheck 作为交互验证入口

自动化测试验证 API、滚动范围、滚动条类型、策略映射和主题切换不崩溃；VisualCheck 则展示三种内容：纵向长内容、横向宽内容、双向大画布。VisualCheck 必须使用 `SKIP_VISUAL_TEST` 守卫，并包含主题切换按钮。

## Risks / Trade-offs

- `QScrollArea` 的原生 scroll bar layout 会占用边缘空间，和 WinUI overlay scrollbar 不完全一致 -> 第一阶段优先稳定封装；如需要 overlay，可在后续通过覆盖层 ScrollBar 或 viewport margin 调整扩展。
- WinUI ScrollView 支持 zoom，而 Qt Widgets 对任意 QWidget zoom 没有统一 transform 模型 -> ScrollView 提供 resize-based zoom、zoom factor API、touchpad pinch 和 Ctrl+wheel 输入；内容若需要像 WinUI 一样精确缩放，应按当前 widget 尺寸或 zoom factor 重绘自身。
- `ScrollBar` 当前自动隐藏依赖 valueChanged/hover，静止时可能与 Figma collapsed 状态仍有差异 -> 通过 VisualCheck 暴露，再决定是否扩展 ScrollBar 状态。
- 隐藏滚动条但保留滚动能力需要区别于 Qt `ScrollBarAlwaysOff` -> 使用 ScrollView 自有 visibility 枚举处理。
- 内容 widget 主题不一定由 ScrollView 控制 -> ScrollView 只保证 viewport 和滚动条主题，业务内容自行响应主题。

## Migration Plan

新增组件不改变现有 `ScrollBar` 或其它滚动使用方式。实现后，业务侧可逐步把手写 `QScrollArea + setVerticalScrollBar(new ScrollBar)` 的组合迁移到 `ScrollView`。如实现发现 `ScrollBar` 集成问题，只做向后兼容修正，并继续保留单独 `ScrollBar` 使用能力。

## Open Questions

- `ScrollBar` 是否需要从当前占位式滚动条演进为真正 overlay 在 viewport 上方的 WinUI 风格滚动条？第一阶段不强制。
- 是否需要支持触控拖拽/pan 手势？Qt Widgets 的平台行为差异较大，可作为后续扩展。