## ADDED Requirements

### Requirement: ScrollView 公共组件 API
系统 SHALL 在 `src/view/scrolling/` 提供 `ScrollView` 公共组件，用于承载可滚动 QWidget 内容。组件 MUST 继承 `QScrollArea`、`FluentElement` 和 `view::QMLPlus`，并且调用方 MUST 能像普通 `QScrollArea` 一样设置内容 widget。

#### Scenario: 创建 ScrollView 并设置内容
- **WHEN** 调用方创建 `ScrollView` 并调用 `setWidget(content)` 设置比 viewport 更大的 QWidget
- **THEN** ScrollView MUST 正常显示内容并允许通过滚动偏移查看被裁剪区域

#### Scenario: 组件符合 Fluent 继承模式
- **WHEN** 编译包含 `src/view/scrolling/ScrollView.h` 的测试或业务代码
- **THEN** `ScrollView` MUST 同时具备 `QScrollArea` 滚动能力、`FluentElement` 主题能力和 `view::QMLPlus` anchors/binding 能力

### Requirement: 自定义 ScrollBar 集成
ScrollView SHALL 默认使用项目自定义 `view::scrolling::ScrollBar` 作为水平和垂直滚动条。组件 MUST NOT 显示 Qt 原生滚动条视觉。

#### Scenario: 默认滚动条类型
- **WHEN** 创建 ScrollView
- **THEN** `verticalScrollBar()` 和 `horizontalScrollBar()` MUST 返回 `view::scrolling::ScrollBar` 类型实例

#### Scenario: 内容需要双向滚动
- **WHEN** ScrollView 内容宽度和高度都大于 viewport
- **THEN** ScrollView MUST 能通过自定义水平 ScrollBar 和自定义垂直 ScrollBar 分别滚动对应方向

#### Scenario: 主题切换更新滚动条
- **WHEN** 全局 Fluent 主题在 Light 和 Dark 之间切换
- **THEN** ScrollView 的自定义 ScrollBar MUST 使用对应主题重新绘制

### Requirement: 滚动模式与滚动条可见性
ScrollView SHALL 提供水平和垂直方向的滚动模式与滚动条可见性策略。模式 MUST 至少表达 disabled、enabled 和 auto；可见性 MUST 至少表达 disabled、auto、visible 和 hidden。

#### Scenario: 自动显示滚动条
- **WHEN** 某方向滚动条可见性为 auto 且内容超出 viewport
- **THEN** 该方向自定义 ScrollBar MUST 可用并可显示

#### Scenario: 内容未超出时自动隐藏滚动条
- **WHEN** 某方向滚动条可见性为 auto 且内容未超出 viewport
- **THEN** 该方向自定义 ScrollBar MUST 不占用可见交互状态

#### Scenario: 隐藏滚动条但保留滚动能力
- **WHEN** 某方向滚动模式为 enabled 且滚动条可见性为 hidden
- **THEN** ScrollView MUST 保留该方向滚动能力，但 MUST 不显示该方向滚动条

#### Scenario: 禁用某方向滚动
- **WHEN** 某方向滚动模式为 disabled
- **THEN** ScrollView MUST 禁止该方向滚动，并且该方向 offset MUST 保持在有效范围内

### Requirement: 程序化滚动与状态查询
ScrollView SHALL 提供程序化滚动 API 和滚动状态查询 API。调用方 MUST 能查询当前水平/垂直 offset、可滚动宽高，并能按绝对位置或相对距离滚动。

#### Scenario: 绝对滚动到指定位置
- **WHEN** 调用方调用 `scrollTo(x, y, false)`
- **THEN** ScrollView MUST 将水平和垂直滚动条移动到有效范围内最接近的目标值

#### Scenario: 相对滚动指定距离
- **WHEN** 调用方调用 `scrollBy(dx, dy, false)`
- **THEN** ScrollView MUST 基于当前 offset 增量滚动并裁剪到有效范围

#### Scenario: 动画滚动
- **WHEN** 调用方调用带动画参数的程序化滚动 API
- **THEN** ScrollView MUST 使用项目 Fluent 动画 token 驱动滚动条 value 变化，且滚动结束后 offset MUST 到达目标值

#### Scenario: 查询滚动范围
- **WHEN** 内容尺寸大于 viewport
- **THEN** `scrollableWidth()` 和 `scrollableHeight()` MUST 返回对应方向最大可滚动距离

### Requirement: 缩放模式与手势缩放
ScrollView SHALL provide a zoom mode and zoom factor API aligned with WinUI ScrollView zoom scenarios. Zoom MUST be disabled by default for compatibility, and callers MUST be able to enable touchpad pinch/Ctrl+wheel zoom explicitly.

#### Scenario: 程序化缩放到指定比例
- **WHEN** 调用方设置 `minZoomFactor`、`maxZoomFactor` 并调用 `zoomTo(factor, false)`
- **THEN** ScrollView MUST clamp the target to the valid range, update `zoomFactor`, resize the content widget, and update scrollable ranges

#### Scenario: zoom-aware 内容收到缩放状态
- **WHEN** content widget implements the ScrollView zoom-aware content interface and ScrollView zoom factor changes
- **THEN** ScrollView MUST notify the content of the current zoom factor and use the content's unscaled logical size for range calculation

#### Scenario: 触控板捏合缩放
- **WHEN** `zoomMode` 为 enabled 且平台发送 native pinch/zoom gesture
- **THEN** ScrollView MUST update `zoomFactor` using the gesture delta and keep the gesture position as the zoom anchor when possible

#### Scenario: 触控板缩放结束后保持比例
- **WHEN** native pinch/zoom gesture updates `zoomFactor` and then the gesture stream ends
- **THEN** ScrollView MUST preserve the resulting zoom factor instead of reverting because of terminal high-level pinch events

#### Scenario: 触控板缩放期间不触发滚动冲突
- **WHEN** native pinch/zoom gesture is active or has just ended and Qt also emits wheel-scroll tail events or SmartZoom events
- **THEN** ScrollView MUST keep the native zoom result stable and MUST NOT let those events scroll or reset the zoom state

#### Scenario: Ctrl wheel 缩放
- **WHEN** `zoomMode` 为 enabled 且用户使用 Ctrl+wheel 或等价触控板输入
- **THEN** ScrollView MUST zoom in or out around the pointer position instead of treating the input as a normal scroll

#### Scenario: 禁用缩放模式
- **WHEN** `zoomMode` 为 disabled
- **THEN** ScrollView MUST ignore user gesture zoom input while preserving programmatic zoom APIs

### Requirement: 主题与视觉边界
ScrollView SHALL 响应 Fluent 主题变化并保持 viewport 背景、边框和自定义滚动条视觉一致。组件实现 MUST 避免使用 QSS 来绘制 Fluent 视觉。

#### Scenario: Light 主题
- **WHEN** 当前主题为 Light
- **THEN** ScrollView viewport 背景 MUST 使用 Fluent Light 主题语义色，并且自定义 ScrollBar MUST 使用 Light 视觉状态

#### Scenario: Dark 主题
- **WHEN** 当前主题为 Dark
- **THEN** ScrollView viewport 背景 MUST 使用 Fluent Dark 主题语义色，并且自定义 ScrollBar MUST 使用 Dark 视觉状态

#### Scenario: 无原生 frame 干扰
- **WHEN** ScrollView 显示在测试窗口中
- **THEN** 组件 MUST 不显示 Qt 原生 `QScrollArea` frame 或原生滚动条边框破坏 Fluent 视觉

#### Scenario: 双向滚动角落无原生视觉泄露
- **WHEN** 水平和垂直自定义 ScrollBar 同时显示
- **THEN** ScrollView MUST hide the scrollbar intersection corner by using a transparent non-painting corner widget instead of exposing Qt native corner artifacts

### Requirement: ScrollView 测试与 VisualCheck
系统 SHALL 为 ScrollView 提供自动化测试和 VisualCheck。自动化测试 MUST 覆盖核心 API 与滚动条集成；VisualCheck MUST 用于人工验证滚动体验和主题视觉。

#### Scenario: 自动化测试不阻塞
- **WHEN** 使用 `SKIP_VISUAL_TEST=1` 运行 `test_scroll_view`
- **THEN** 非 VisualCheck 测试 MUST 自动完成且不打开交互窗口

#### Scenario: VisualCheck 展示多方向滚动
- **WHEN** 手动运行 ScrollView VisualCheck
- **THEN** 窗口 MUST 展示垂直滚动、水平滚动和双向滚动示例，并允许用户拖动自定义 ScrollBar

#### Scenario: VisualCheck 提供主题切换
- **WHEN** 用户在 VisualCheck 中点击主题切换按钮
- **THEN** ScrollView 和自定义 ScrollBar MUST 在 Light/Dark 主题之间更新视觉

#### Scenario: VisualCheck 提供缩放入口
- **WHEN** 手动运行 ScrollView VisualCheck
- **THEN** 窗口 MUST allow users to trigger zoom and verify touchpad pinch/Ctrl+wheel zoom on enabled examples

### Requirement: 构建注册
系统 SHALL 将 ScrollView 源文件纳入 `fluent_qt_lib` 构建，并注册对应测试目标。

#### Scenario: 源文件参与构建
- **WHEN** 构建 `fluent_qt_lib`
- **THEN** `src/view/scrolling/ScrollView.cpp` 和 `src/view/scrolling/ScrollView.h` MUST 参与编译

#### Scenario: 测试目标注册
- **WHEN** 配置 CMake 后构建测试
- **THEN** `tests/views/scrolling/CMakeLists.txt` MUST 注册 `test_scroll_view`，并且目标 MUST 能链接 `fluent_qt_lib`