## ADDED Requirements

### Requirement: AnnotatedScrollBar 公共 API
系统 SHALL 在 `src/view/scrolling/` 提供 `AnnotatedScrollBar` 公共组件，用于以 WinUI 风格显示带标签和详情提示的竖向滚动控制器。组件 MUST 继承 `QWidget`、`FluentElement` 和 `view::QMLPlus`，并暴露滚动 range、当前 value、page step、标签集合和 detail label provider。

#### Scenario: 默认构造
- **WHEN** 创建一个 `AnnotatedScrollBar`
- **THEN** `minimum()` MUST 为 0、`maximum()` MUST 为 0、`value()` MUST 为 0、`pageStep()` MUST 为 0、`labels()` MUST 为空、detail label provider MUST 未设置，且 `sizeHint()` MUST 返回可用于显示竖向 annotated scroll bar 的非空尺寸

#### Scenario: 组件符合项目控件继承模式
- **WHEN** 编译包含 `src/view/scrolling/AnnotatedScrollBar.h` 的测试或业务代码
- **THEN** `AnnotatedScrollBar` MUST 同时具备 QWidget 绘制能力、`FluentElement` 主题能力和 `view::QMLPlus` anchors/binding 能力

#### Scenario: 配置布局尺寸属性
- **WHEN** 调用方设置 `preferredSize`、`minimumBarSize`、`verticalPadding`、`caretSize`、`labelColumnWidth`、`labelLineHeight`、`minimumLabelSpacing`、`indicatorWidth`、`indicatorThickness` 或 `lineStepFallback`
- **THEN** 组件 MUST 规范化非法尺寸、发出 `layoutMetricsChanged()`、刷新相关布局或绘制，并在影响 size hint 的属性变化时更新几何

#### Scenario: 设置 range 和 value
- **WHEN** 调用 `setRange(minimum, maximum)` 后调用 `setValue(value)`
- **THEN** `AnnotatedScrollBar` MUST 将 value clamp 到 `[minimum, maximum]`，并在有效值变化时发出一次 `valueChanged(int)` 信号

#### Scenario: 重复设置相同滚动状态
- **WHEN** 调用 `setRange()`、`setValue()` 或 `setPageStep()` 且新状态与旧状态等价
- **THEN** 组件 MUST 不发出冗余 changed 信号，且 MUST 不产生不必要的布局重算

#### Scenario: 标签集合 API
- **WHEN** 调用 `setLabels()`、`addLabel()` 或 `clearLabels()`
- **THEN** 组件 MUST 更新内部标签集合、重新计算标签布局、触发一次 `labelsChanged()` 信号，并请求重绘

### Requirement: 标签 offset 映射与碰撞隐藏
系统 SHALL 根据滚动 range 将每个 `AnnotatedScrollBarLabel(text, offset)` 映射到竖向轨道位置，并在可用高度不足时隐藏部分标签，确保可见标签文本不重叠。

#### Scenario: 标签按 offset 定位
- **WHEN** range 为 `[0, 1000]` 且标签 offset 分别位于 range 起点、中点和终点
- **THEN** 标签 MUST 分别布局到轨道顶部、中部和底部附近，并保持文本在标签列内右对齐显示

#### Scenario: 标签顺序稳定
- **WHEN** 调用方传入未排序的标签集合
- **THEN** 布局 MUST 按 offset 升序计算标签位置，相同 offset 的标签 MUST 保持传入顺序

#### Scenario: 高度不足时隐藏重叠标签
- **WHEN** 控件高度不足以同时显示所有标签
- **THEN** `AnnotatedScrollBar` MUST 隐藏会与已保留标签重叠的标签，并且可见标签之间 MUST 保持至少一个文本行高的垂直间距

#### Scenario: 尺寸变化刷新标签布局
- **WHEN** 控件高度、字体、range 或标签集合发生变化
- **THEN** 组件 MUST 重新计算可见标签，确保后续绘制和命中测试使用最新布局

### Requirement: 详情提示与交互状态
系统 SHALL 在 hover 或 pressed 交互期间根据当前 scroll offset 显示 detail label，并在鼠标离开、禁用或 provider 返回空内容时隐藏提示。

#### Scenario: detail label provider 返回内容
- **WHEN** 调用方设置 detail label provider 且用户 hover 到轨道、indicator 或标签区域
- **THEN** 组件 MUST 将指针位置映射为 scroll offset，调用 provider 取得文本，并显示 TeachingTip 风格详情提示

#### Scenario: provider 返回空内容
- **WHEN** detail label provider 对当前 offset 返回空字符串
- **THEN** 组件 MUST 不显示 detail label 浮层

#### Scenario: 标签 fallback 详情
- **WHEN** 未设置 detail label provider 且用户 hover 到某个可命中的标签
- **THEN** 组件 MUST 使用该标签的 `detailText` 作为详情内容；若 `detailText` 为空，MUST 使用标签 `text`

#### Scenario: 鼠标离开隐藏详情
- **WHEN** 鼠标离开 `AnnotatedScrollBar`
- **THEN** 组件 MUST 清除 hover 状态并隐藏 detail label 浮层

#### Scenario: pressed 状态保持详情
- **WHEN** 用户按住 indicator 或轨道并拖动
- **THEN** 组件 MUST 保持 pressed 视觉状态，并随当前拖动 offset 更新 detail label 内容和位置

### Requirement: 滚动输入与信号
系统 SHALL 将鼠标、滚轮和键盘输入转换为竖向滚动 offset，并通过 value 与信号向外通知滚动请求。

#### Scenario: 点击轨道请求滚动
- **WHEN** 用户点击竖向轨道上的某个位置
- **THEN** 组件 MUST 将点击位置映射为有效 offset，更新 `value()`，并发出 `scrollRequested(offset)` 信号

#### Scenario: 点击标签请求滚动
- **WHEN** 用户点击某个标签文本或标签命中区域
- **THEN** 组件 MUST 滚动到该标签 offset，发出 `labelActivated(offset, text)` 和 `scrollRequested(offset)` 信号

#### Scenario: 拖动 indicator
- **WHEN** 用户拖动 indicator
- **THEN** 组件 MUST 连续更新 `value()` 到拖动位置对应的有效 offset，并发出对应 `valueChanged(int)` 和 `scrollRequested(int)` 信号

#### Scenario: 滚轮和键盘滚动
- **WHEN** 控件获得焦点后接收 wheel、Up、Down、PageUp、PageDown、Home 或 End 输入
- **THEN** 组件 MUST 按小步、`pageStep()` 或 range 端点更新 value，并将结果 clamp 到有效 range

#### Scenario: 禁用状态忽略用户输入
- **WHEN** `AnnotatedScrollBar` 被禁用
- **THEN** 组件 MUST 保持当前 value 不变，MUST 不响应 hover/pressed/drag/click/wheel/key 滚动输入，且 MUST 不显示 detail label

### Requirement: ScrollView 联动
系统 SHALL 提供与 `view::scrolling::ScrollView` 的垂直滚动联动能力，使 `AnnotatedScrollBar` 能作为外置滚动控制器驱动和反映 ScrollView 的垂直 offset。

#### Scenario: 连接 ScrollView 后同步 range
- **WHEN** 调用 `connectToScrollView(scrollView)`
- **THEN** `AnnotatedScrollBar` MUST 从 `scrollView->verticalScrollBar()` 同步 minimum、maximum、pageStep 和当前 value，并连接后续 range/value 变化

#### Scenario: AnnotatedScrollBar 输入驱动 ScrollView
- **WHEN** 已连接的 `AnnotatedScrollBar` 因点击、拖动、滚轮或键盘输入发出滚动请求
- **THEN** 对应 `ScrollView` MUST 滚动到请求的垂直 offset，并保持当前水平 offset 不被重置

#### Scenario: ScrollView 外部滚动反向同步
- **WHEN** 已连接的 `ScrollView` 因鼠标滚轮、程序化 `scrollTo()` 或内容尺寸变化更新垂直滚动条 value/range
- **THEN** `AnnotatedScrollBar` MUST 更新自身 value/range/pageStep 并请求重绘

#### Scenario: 断开旧连接
- **WHEN** `AnnotatedScrollBar` 已连接一个 ScrollView 后再次连接其他 ScrollView 或调用 `disconnectScrollView()`
- **THEN** 组件 MUST 断开旧 ScrollView 的信号连接，后续旧 ScrollView 滚动 MUST 不再改变当前 `AnnotatedScrollBar`

#### Scenario: 不强制隐藏 ScrollView 原滚动条
- **WHEN** 调用 `connectToScrollView(scrollView)`
- **THEN** 组件 MUST 不修改 `ScrollView` 的滚动条可见性策略；调用方可按需要自行设置 vertical scrollbar visibility 为 hidden

### Requirement: Fluent 视觉与主题
系统 SHALL 按 Figma Windows UI Kit 的 Annotated Scroll Bar 竖向组件绘制标签、caret、indicator 和 detail label，并响应 Light/Dark 主题与 enabled 状态变化。

#### Scenario: 默认竖向视觉结构
- **WHEN** `AnnotatedScrollBar` 以默认尺寸显示
- **THEN** 组件 MUST 显示顶部和底部 caret、右对齐标签列、当前位置 indicator，并保持接近 Figma 的 97px 默认宽度、535px 默认高度、17px 上下 padding、14x16 caret 和 29x3 indicator 视觉语义

#### Scenario: Light 主题
- **WHEN** 当前 Fluent 主题为 Light
- **THEN** 标签文本 MUST 使用 Light primary text 色，caret MUST 使用弱化前景色，indicator MUST 使用 accent 色，detail label MUST 使用浅色背景、1px stroke 和可读深色文本

#### Scenario: Dark 主题
- **WHEN** 当前 Fluent 主题为 Dark
- **THEN** 标签文本、caret、indicator 和 detail label MUST 切换为 Dark 主题可读颜色，detail label MUST 使用深色背景和浅色文本

#### Scenario: 主题切换
- **WHEN** 全局 Fluent 主题在 Light 和 Dark 之间切换
- **THEN** `AnnotatedScrollBar` MUST 在 `onThemeUpdated()` 中刷新缓存颜色、字体或 glyph 资源，并请求重绘

#### Scenario: disabled 视觉
- **WHEN** 组件被禁用
- **THEN** 标签、caret 和 indicator MUST 使用 disabled 语义色或降低 alpha，并且 hover/pressed/detail 视觉 MUST 被清除

### Requirement: 测试与可视化覆盖
系统 SHALL 为 `AnnotatedScrollBar` 提供自动化测试和 VisualCheck，覆盖公共 API、标签布局、滚动交互、主题和与 `ScrollView` 的联动。

#### Scenario: 测试目标注册
- **WHEN** 配置 CMake 后构建测试
- **THEN** `tests/views/scrolling/CMakeLists.txt` MUST 注册 `test_annotated_scrollbar`，并且目标 MUST 能链接 `fluent_qt_lib`

#### Scenario: 自动化测试
- **WHEN** 使用 `SKIP_VISUAL_TEST=1` 运行 `test_annotated_scrollbar`
- **THEN** 非 VisualCheck 测试 MUST 覆盖默认属性、range/value clamp、标签集合、标签碰撞隐藏、detail provider、点击/拖动/wheel/key 滚动、disabled 状态、Light/Dark 主题刷新和 `ScrollView` 联动

#### Scenario: VisualCheck 展示 Figma 风格样例
- **WHEN** 手动运行 `test_annotated_scrollbar --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示 2015-2023 标签、hover/pressed detail label、Light/Dark 状态和高度变化导致的标签隐藏行为

#### Scenario: VisualCheck 展示 WinUI Gallery 风格联动
- **WHEN** 手动运行 `test_annotated_scrollbar --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 展示一个隐藏原垂直滚动条的 `ScrollView` 与右侧 `AnnotatedScrollBar` 联动的长内容示例，并允许用户通过 AnnotatedScrollBar 滚动内容

#### Scenario: VisualCheck 使用高度滑块展示标签密度变化
- **WHEN** 手动运行 `test_annotated_scrollbar --gtest_filter="*VisualCheck*"`
- **THEN** VisualCheck MUST 提供可交互 slider 调节 linked `AnnotatedScrollBar` 高度，并在高度变化时刷新可见标签布局以展示碰撞隐藏行为
