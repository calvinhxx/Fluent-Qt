## Context

`src/view/scrolling/` 目前包含 `ScrollBar` 和 `ScrollView`。`ScrollBar` 已经提供 Fluent 风格的自绘滚动 thumb、hover/pressed 状态和淡入淡出动画；`ScrollView` 基于 `QScrollArea` 封装自定义滚动条、程序化滚动和缩放输入。现有能力可以滚动内容，但没有 WinUI `AnnotatedScrollBar` 这种面向长列表/长文档的“分组标签 + 当前位置 indicator + hover 详情”控制器。

Figma Windows UI Kit 节点 `101496:321564` 显示 Annotated Scroll Bar 的关键结构：首版只有 Vertical orientation，支持 Light/Dark 和 Rest/Hover/Pressed 状态；主体高度约 535px，上下各有 14x16 caret；标签列为右对齐 14px/20px Segoe UI 文本，示例标签为 2015-2023；indicator 使用约 29x3 的细 thumb；Hover/Pressed 时右侧出现 TeachingTip 风格详情浮层，Light 背景为 `#f9f9f9`、Dark 背景为 `#282828`，描边约 `rgba(117,117,117,0.4)`，内容示例为 `October 2019`。

WinUI Gallery 的 `AnnotatedScrollBarPage` 示例将 `ScrollView` 的垂直滚动条隐藏，把 `AnnotatedScrollBar` 放在右侧，并在加载后把 `scrollView.ScrollPresenter.VerticalScrollController` 连接到 `annotatedScrollBar.ScrollController`。示例在 `ItemsRepeater_SizeChanged` 时清空并重建 `annotatedScrollBar.Labels`，每个 `AnnotatedScrollBarLabel(text, offset)` 对应一个颜色分组的起始滚动 offset；`DetailLabelRequested` 根据当前 hover 的 `ScrollOffset` 返回 tooltip 内容；高度 slider 用来演示空间变小时标签会隐藏以避免碰撞。

参考来源：
- Figma: https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=101496-321564
- WinUI Gallery XAML: https://github.com/microsoft/WinUI-Gallery/blob/main/WinUIGallery/Samples/ControlPages/AnnotatedScrollBarPage.xaml
- WinUI Gallery code-behind: https://github.com/microsoft/WinUI-Gallery/blob/main/WinUIGallery/Samples/ControlPages/AnnotatedScrollBarPage.xaml.cs

## Goals / Non-Goals

**Goals:**

- 新增 `AnnotatedScrollBar : public QWidget, public FluentElement, public view::QMLPlus`，放在 `src/view/scrolling/`。
- 首版专注竖向 AnnotatedScrollBar，对齐 Figma 的 Vertical 组件和 WinUI Gallery 的垂直 `ScrollView` 示例。
- 提供范围、当前 offset、viewport/page step、标签集合、detail label provider 和与 `ScrollView` 同步的公共 API。
- 支持鼠标 hover、press、drag、track click、wheel/key 输入，把用户意图映射为滚动 offset。
- 按高度自动筛选可见标签，保持标签之间不重叠；当高度变化或标签变更时重新布局。
- 提供 Light/Dark、enabled/disabled、Rest/Hover/Pressed 视觉状态，以及 TeachingTip 风格详情浮层。
- 提供自动化测试和 VisualCheck，覆盖 Figma/WinUI Gallery 关键行为。

**Non-Goals:**

- 不实现水平 AnnotatedScrollBar；Figma 节点和 WinUI Gallery 示例均只覆盖 Vertical。
- 不替换 `ScrollView` 默认滚动条，也不修改 `ScrollView` 现有公共 API；联动由新控件通过辅助方法完成。
- 不实现 WinUI 内部 `IScrollController` 等接口的逐字移植；Qt 版本提供等价的范围/value 同步和信号。
- 不引入系统 toast、Popup 队列或复杂 TeachingTip 控件依赖；详情浮层由 `AnnotatedScrollBar` 自绘或轻量内部 widget 实现。
- 不在首版暴露完整自定义模板系统；颜色和 glyph 仍由主题 token 与内部实现驱动，但常用布局/视觉尺寸通过公开属性配置。

## Decisions

### 1. 使用 QWidget 自绘控件，而不是继承 QScrollBar

`AnnotatedScrollBar` 需要在同一控件内绘制标签列、上下 caret、indicator、hover/pressed detail tip，并维护标签碰撞隐藏逻辑。直接继承 `QScrollBar` 可以复用 range/value，但很难表达右侧详情浮层和标签命中区域，也会把实现绑到 Qt style 的 sub-control 几何。首版采用独立 `QWidget`，内部维护 `minimum`、`maximum`、`value`、`pageStep`，并通过信号或 `connectToScrollView()` 与真实滚动容器同步。

公共 API 形状：

- `minimum()` / `maximum()` / `setRange(int minimum, int maximum)`
- `value()` / `setValue(int value)`，自动 clamp 并发出 `valueChanged(int)`
- `pageStep()` / `setPageStep(int pageStep)`，用于 thumb/indicator 位置和键盘 PageUp/PageDown
- `labels()` / `setLabels(QVector<AnnotatedScrollBarLabel>)` / `addLabel()` / `clearLabels()`
- `setDetailLabelProvider(std::function<QString(int offset)>)`
- `connectToScrollView(ScrollView*)` / `disconnectScrollView()`
- 信号：`valueChanged(int)`、`scrollRequested(int)`、`labelActivated(int offset, QString text)`、`detailLabelRequested(int offset)`、`labelsChanged()`

`AnnotatedScrollBarLabel` 使用简单值类型，只包含 `QString text`、`int offset` 和 `QString detailText`。`detailText` 可作为 provider 未设置时的 fallback；不把标签结构注册为 metatype，也不通过信号槽传递复杂结构。

### 2. ScrollView 联动只同步垂直 range/value，不侵入 ScrollView

WinUI Gallery 示例的关键点是 `AnnotatedScrollBar` 提供滚动控制器，`ScrollView` 消费它。Qt 版不改 `ScrollView` 合约：`connectToScrollView(ScrollView*)` 读取 `ScrollView::verticalScrollBar()` 的 minimum/maximum/pageStep/value，并连接其 `rangeChanged`、`valueChanged`、`sliderMoved` 以及 `ScrollView::scrollPositionChanged`。当用户拖动或点击 `AnnotatedScrollBar` 时，控件发出 `scrollRequested(offset)`，已连接的 `ScrollView` 调用 `scrollTo(currentHorizontalOffset, offset, false)`。

是否隐藏原有垂直滚动条由调用方决定。VisualCheck 和示例会像 WinUI Gallery 一样把 `ScrollView` 的 vertical scrollbar visibility 设为 `Hidden`，然后把 `AnnotatedScrollBar` 放在右侧。

### 3. 标签按 offset 排序和碰撞过滤，保持布局可解释

标签 offset 映射到 track 可用高度：`ratio = (offset - minimum) / (maximum - minimum)`，然后换算为 y 坐标。标签绘制使用项目字体 token 中的 14px/20px 正文字体，右对齐在约 31px 标签列内。标签集合按 offset 升序布局；相同 offset 保持传入顺序。

为了复刻 WinUI Gallery 的“高度变小时隐藏标签避免碰撞”，布局阶段会保留最小垂直间距，默认使用 label line height 20px 加少量间隔。首版策略：

- 优先保留第一个和最后一个标签。
- 中间标签按 offset 顺序尝试放置，和已保留标签发生重叠时隐藏。
- hover/pressed 命中的标签可以临时参与 detail 计算，但不得造成文字重叠。
- `resizeEvent()`、`setLabels()`、`setRange()`、`setPageStep()`、字体/主题变化都会触发布局缓存刷新。

如果后续需要更接近 WinUI 的分层 label density，可以在不破坏公共 API 的情况下替换内部筛选策略。

### 4. Detail label 使用 provider 生成，视觉复用现有 ToolTip

WinUI 的 `DetailLabelRequested` 是按 scroll offset 请求字符串。Qt 版使用 `std::function<QString(int)>` 作为同步 provider，鼠标 hover 到轨道、indicator 或标签区域时计算当前 offset 并请求 detail 文本。provider 返回空字符串时不显示 detail；未设置 provider 时，优先使用命中标签的 `detailText`，再回退到标签 `text`。

详情浮层复用项目已有的 `view::status_info::ToolTip`，由 `AnnotatedScrollBar` 负责按 hover/drag offset 更新文本、定位和边界钳制。这样避免在控件内部再维护一套 Popup/ToolTip 绘制、动画和主题逻辑，也避免引入 modal、dim、close policy 等对 hover detail 过重的 flyout 行为。

### 5. Figma 默认尺寸可配置，视觉仍保持主题 token 驱动

默认 `sizeHint()` 使用接近 Figma 的 `97x535`，其中 97px 为 indicator/detail 可用宽度，535px 为设计稿主组件高度。标签列约 31px，右侧 caret hit area 约 14x16，上下 padding 约 17px，indicator thumb 约 29x3。实际嵌入时高度跟随布局，标签密度自动调整。

调用方可以通过 `preferredSize`、`minimumBarSize`、`verticalPadding`、`caretSize`、`labelColumnWidth`、`labelLineHeight`、`minimumLabelSpacing`、`indicatorWidth`、`indicatorThickness` 和 `lineStepFallback` 调整常见嵌入场景，不需要复制或修改控件内部绘制逻辑。属性变更会刷新标签布局、重绘，并在影响 size hint 时触发 `updateGeometry()`。

颜色由 `FluentElement` 的 theme token 派生：

- Light 标签文本使用 primary text，caret 使用约 45% 黑，indicator 使用 accent。
- Dark 标签文本使用 white/primary dark text，caret 使用约 54% 白，indicator 使用 accent 或高对比色。
- Disabled 状态降低标签、caret 和 indicator alpha，并停止交互。
- Hover/Pressed 只改变 indicator/detail 浮层状态，不改变标签布局。

caret glyph 使用 Segoe Fluent Icons 中的 8px solid caret：上/下分别对应 Figma 图标清单里的 `EDDB`、`EDDC`。若项目现有 icon font 代码点不同，实现在 `Typography` 中查找等价 glyph 或保留内部 fallback。

### 6. 测试以行为为主，视觉用渲染抽样和 VisualCheck 补足

自动化测试不要求精确到每个像素，但必须覆盖核心合同：默认 API、range clamp、label 增删与排序、标签碰撞隐藏、offset/y 映射、detail provider、hover/pressed 状态、drag/click 发出滚动请求、与 `ScrollView` value 同步、主题刷新、disabled 状态。渲染测试可以对 indicator、label、detail tip 的关键区域做非透明/颜色变化抽样。

VisualCheck 展示两个主场景：一个独立的 2015-2023 Figma 风格 AnnotatedScrollBar；一个 WinUI Gallery 风格的 `ScrollView + AnnotatedScrollBar` 色块分组示例，右侧提供高度 slider 或按钮以验证标签碰撞隐藏。

## Risks / Trade-offs

- [Risk] 独立 QWidget 不能完全复用 `QScrollBar` 的所有平台行为 → Mitigation: 公共 API 明确为 vertical annotated controller，保留 `valueChanged` / `scrollRequested` / keyboard/wheel 的常用滚动语义，并通过测试覆盖同步行为。
- [Risk] 标签碰撞过滤策略和 WinUI 内部算法可能有差异 → Mitigation: spec 约束“不重叠、首尾优先、空间变化刷新”，不锁死具体算法，后续可替换内部策略。
- [Risk] detail 浮层如果做成 child widget 可能被父布局裁剪，如果纯绘制又缺少真实窗口阴影 → Mitigation: 首版优先保证可见、命中和主题正确；VisualCheck 后再微调阴影或必要时改成轻量 top-level tool widget。
- [Risk] Figma 默认宽度包含 hover 浮层空间，实际业务可能只想占用窄轨道 → Mitigation: 默认遵循设计稿，后续可增加 `compactWidth` 或 `showInlineDetail` 配置；首版不为未验证场景扩大 API。
- [Risk] `connectToScrollView()` 自动滚动可能和外部手动连接重复 → Mitigation: 方法内部断开旧连接，文档和测试明确一个 AnnotatedScrollBar 同时只连接一个 ScrollView。

## Migration Plan

1. 新增 `AnnotatedScrollBar` 头/源文件，保持源码由现有 `src/**/*.cpp` CMake glob 纳入 `fluent_qt_lib`。
2. 新增并注册 `test_annotated_scrollbar`。
3. 先实现无 ScrollView 的 API、绘制、标签布局和交互测试。
4. 再接入 `ScrollView` 同步与 WinUI Gallery 风格 VisualCheck。
5. 使用 `ctest` 或单测目标验证自动化测试；手动运行 VisualCheck 检查 Light/Dark、hover detail 和标签碰撞。

## Open Questions

- 是否需要在后续版本中支持 horizontal orientation，取决于是否能在 Windows UI Kit 或实际业务中找到稳定参考。
- 是否需要把 detail 浮层抽成通用 TeachingTip 复用能力，取决于现有 `TeachingTip` 控件的弹出层行为是否适合滚动条 hover 高频更新。
