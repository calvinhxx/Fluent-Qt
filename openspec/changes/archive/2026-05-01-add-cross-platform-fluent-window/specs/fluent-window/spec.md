## ADDED Requirements

### Requirement: Fluent Window 顶层容器

系统 SHALL 在 `src/view/windowing/` 提供 `view::windowing::Window` 作为 Fluent 顶层窗口容器。`Window` MUST 继承 `QWidget`、`FluentElement` 与 `view::QMLPlus`，并 MUST 提供标题栏区域与内容 host。默认最小尺寸 MUST 不小于 `Breakpoints::MinWindowWidth` 与 `Breakpoints::MinWindowHeight`。

#### Scenario: 默认构造生成标题栏与内容区域
- **WHEN** 创建一个 `view::windowing::Window`
- **THEN** 窗口 MUST 拥有非空标题栏对象、非空内容 host，且 `minimumWidth()` 与 `minimumHeight()` MUST 分别不小于项目 Breakpoints 中的最小窗口尺寸

#### Scenario: Window 可作为普通 QWidget 顶层显示
- **WHEN** 调用 `show()` 显示 `view::windowing::Window`
- **THEN** 窗口 MUST 作为 top-level widget 显示，且不要求业务代码额外创建 `QMainWindow`

### Requirement: 内容 host SHALL 承载业务 QWidget

`Window` MUST 提供 API 将业务 QWidget 放入内容 host，例如 `setContentWidget(QWidget*)` 与 `contentWidget()`；设置新内容时 MUST 从 host 布局中移除旧内容，且不得 delete 调用方仍拥有的旧内容，除非 API 文档明确转移所有权。

#### Scenario: 设置内容 widget
- **WHEN** 调用 `setContentWidget(content)` 设置业务内容
- **THEN** `content` MUST 成为 Window 内容 host 的子节点并填充内容区域

#### Scenario: 替换内容 widget
- **WHEN** Window 已有内容 `oldContent`，随后调用 `setContentWidget(newContent)`
- **THEN** `newContent` MUST 填充内容区域，`oldContent` MUST 不再位于内容 host 布局中

### Requirement: Fluent 标题栏 host SHALL 使用项目设计 token

`TitleBar` MUST 使用现有 Fluent token 绘制标题栏背景和分割线。`TitleBar` MUST expose 可配置的 `titleBarHeight` 属性，默认高度 MUST 等于 `TitleBar::defaultTitleBarHeight()`。`TitleBar` MUST expose 一个外部内容槽，并 MUST 支持为系统原生窗口控制预留 leading area。

#### Scenario: 标题栏尺寸稳定
- **WHEN** Window 使用默认标题栏
- **THEN** 标题栏 `sizeHint().height()` MUST 等于 `titleBarHeight()`，且布局变化不得导致标题栏高度抖动

#### Scenario: 配置标题栏高度
- **WHEN** 调用 `TitleBar::setTitleBarHeight(height)` 且 `height` 为正数
- **THEN** 标题栏 MUST 更新固定高度、`sizeHint()` 与 `minimumSizeHint()`，并 MUST 发射 `titleBarHeightChanged` 与 `chromeGeometryChanged`

#### Scenario: 主题切换刷新标题栏
- **WHEN** 调用 `FluentElement::setTheme()` 在 Light 与 Dark 之间切换
- **THEN** Window 与 TitleBar MUST 更新背景与描边颜色

### Requirement: 标题栏内容 SHALL 由外部 QWidget 提供

Window MUST 保留标准 `windowTitle`/`windowIcon` 语义供系统窗口管理器使用。标题栏内的标题、图标、导航按钮、搜索框和 action buttons SHOULD 由调用方组合为 QWidget 后挂载到 `TitleBar` 内容槽，Window MUST NOT 要求 `TitleBar` 内建这些业务 UI。

#### Scenario: 设置窗口标题
- **WHEN** 调用 `window.setWindowTitle("Settings")`
- **THEN** Window 的系统标题 MUST 更新为 `Settings`，且调用方 MAY 使用自己的标题栏内容 widget 展示该文本

#### Scenario: 修改窗口标题
- **WHEN** Window 已显示标题 `A`，随后调用 `setWindowTitle("B")`
- **THEN** Window 的系统标题 MUST 更新为 `B`，无需重建 Window

### Requirement: 外部标题栏控件 SHALL 能执行窗口状态操作

Windows custom chrome 模式下，Window MUST 提供可连接的最小化、最大化/还原、关闭 slots。调用方 MAY 将 caption buttons 作为外部 QWidget 挂载到 `TitleBar` 内容槽，并连接这些 slots。`TitleBar` MUST NOT 内建平台特定 caption buttons。

#### Scenario: 最小化按钮最小化窗口
- **WHEN** 用户点击最小化 caption button
- **THEN** Window MUST 进入 minimized 状态或向 Qt 发起最小化请求

#### Scenario: 最大化按钮切换状态
- **WHEN** 用户在 normal 状态点击最大化 caption button
- **THEN** Window MUST 请求最大化，且按钮图标 MUST 切换为 restore

#### Scenario: 还原按钮切回 normal
- **WHEN** 用户在 maximized 状态点击最大化/还原 caption button
- **THEN** Window MUST 请求恢复 normal 状态，且按钮图标 MUST 切换为 maximize

#### Scenario: 关闭按钮关闭窗口
- **WHEN** 用户点击关闭 caption button
- **THEN** Window MUST 调用 `close()` 并触发正常 Qt close event 流程

### Requirement: macOS SHALL 默认保留原生窗口控制

macOS 上 Window MUST 默认保留系统 traffic lights 与原生窗口框架。macOS 默认模式 MUST NOT 显示 Windows 风格的右上角 caption buttons。

#### Scenario: macOS 不显示自绘 Windows caption buttons
- **WHEN** 在 macOS 上创建默认 Window
- **THEN** Window MUST 使用系统窗口控制，且 Fluent TitleBar host MUST NOT 创建最小化/最大化/关闭三枚 Windows caption buttons

#### Scenario: macOS 内容区仍使用 Fluent token
- **WHEN** 在 macOS 上显示 Window 内容区
- **THEN** 内容 host 背景和可选标题区域 MUST 使用当前 Fluent 主题 token 绘制

### Requirement: 标题栏交互区域 SHALL 支持自定义内容并排除拖拽

Window SHOULD 支持在标题栏中放置自定义 QWidget，例如搜索框、菜单按钮或工具按钮。自定义内容中的交互控件 MUST 从窗口拖拽 hit-test 中排除，避免用户点击控件时触发窗口移动。

#### Scenario: 标题栏自定义控件不触发拖拽
- **WHEN** 标题栏包含一个自定义按钮，用户在该按钮区域按下鼠标
- **THEN** Window MUST 将该区域视为 client/control 区域，而不是 titlebar drag 区域

#### Scenario: 标题栏空白区域可拖拽
- **WHEN** 用户在标题栏空白区域按下并拖动
- **THEN** Window MUST 发起系统窗口移动或平台 fallback 移动

### Requirement: Window VisualCheck SHALL 展示跨平台外观

测试套件 MUST 提供 `tests/views/windowing/` VisualCheck。VisualCheck MUST 提供一个按钮用于创建并显示带自定义 titlebar content 的真实 Window。VisualCheck MUST 在自动化 `ctest` 中通过 `SKIP_VISUAL_TEST=1` 跳过。

#### Scenario: VisualCheck 可手动运行
- **WHEN** 手动运行 windowing 测试二进制并执行 VisualCheck
- **THEN** 用户 MUST 能点击按钮打开 Fluent Window 示例，并手动验证标题栏、内容区和平台窗口控制行为

#### Scenario: ctest 跳过 VisualCheck
- **WHEN** 使用项目默认 `ctest` 配置运行测试
- **THEN** Window VisualCheck MUST 被跳过，不得阻塞自动化测试
