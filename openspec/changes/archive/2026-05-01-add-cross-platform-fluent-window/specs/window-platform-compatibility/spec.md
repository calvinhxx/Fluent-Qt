## ADDED Requirements

### Requirement: 窗口平台差异 SHALL 集中在 compatibility 层

系统 MUST 在 `src/compatibility/` 提供窗口 chrome 兼容层，封装 Win/mac/native Qt 的平台差异。`src/view/windowing/` 下的 UI 源码 MUST NOT 直接 include Windows SDK、AppKit/Objective-C 头，也 MUST NOT 散落 `Q_OS_WIN`、`Q_OS_MAC` 或 `QT_VERSION_CHECK` 条件分支；必要分支 MUST 位于 compatibility 层。

#### Scenario: windowing UI 层没有平台宏
- **WHEN** 搜索 `src/view/windowing/` 下源码中的 `Q_OS_WIN`、`Q_OS_MAC`、`_WIN32`、`__APPLE__`、`QT_VERSION_CHECK`
- **THEN** MUST 没有命中，平台与 Qt 版本差异 MUST 由 `src/compatibility/` 封装

#### Scenario: 原生平台头不泄漏到 windowing
- **WHEN** 搜索 `src/view/windowing/` 下源码中的 `windows.h`、`dwmapi.h`、`Cocoa`、`AppKit`
- **THEN** MUST 没有命中

### Requirement: WindowChromeCompat SHALL 提供统一 chrome 操作

兼容层 MUST 提供统一的窗口 chrome API，至少覆盖平台窗口 flags 初始化、native event 处理、系统移动、系统缩放、标题栏矩形、resize border 和拖拽排除区配置。Window UI 层 MUST 通过该 API 与平台窗口管理器交互。

#### Scenario: Window 配置标题栏 hit-test 数据
- **WHEN** Window 布局标题栏和外部标题栏交互控件后
- **THEN** Window MUST 将 titlebar rect、resize border width 和 drag exclusion rects 传递给 WindowChromeCompat

#### Scenario: Window 委托 native event
- **WHEN** Qt 向 Window 派发 native event
- **THEN** Window MUST 将 eventType、message 和 result 委托给 WindowChromeCompat，并使用其返回值决定事件是否已处理

### Requirement: Windows 自定义 chrome SHALL 保留系统移动缩放行为

Windows 平台 custom chrome 模式下，兼容层 MUST 通过 native hit-test 或等价系统 API 保留窗口移动、边缘缩放、corner 缩放、双击标题栏最大化/还原和 Aero Snap。实现 MUST 避免仅通过 `QWidget::move()` 手写拖拽作为主路径。

#### Scenario: 标题栏空白区域返回 caption hit-test
- **WHEN** Windows native hit-test 坐标位于标题栏空白区域
- **THEN** 兼容层 MUST 将该位置识别为系统 caption 区域，使 OS 接管拖拽、双击和 Snap

#### Scenario: 外部控件区域返回 client hit-test
- **WHEN** Windows native hit-test 坐标位于标题栏外部交互控件区域
- **THEN** 兼容层 MUST 将该位置识别为 client/control 区域，使控件正常接收 Qt 鼠标事件

#### Scenario: resize border 返回边缘 hit-test
- **WHEN** Windows native hit-test 坐标位于窗口左、右、上、下或角落 resize border
- **THEN** 兼容层 MUST 返回对应 resize hit-test，使 OS 接管缩放

### Requirement: macOS SHALL 使用原生窗口控制策略

macOS 平台默认 MUST 不启用 Windows custom caption chrome。兼容层 MUST 保留 Qt/macOS 原生窗口控制、traffic lights、系统全屏按钮和平台窗口移动行为。第一阶段实现 MUST NOT 要求 Objective-C++ `.mm` 文件。

#### Scenario: macOS 默认不启用 frameless Windows chrome
- **WHEN** 在 macOS 上创建默认 Window
- **THEN** 兼容层 MUST 不强制启用 Windows custom caption buttons 或 Windows hit-test 逻辑

#### Scenario: macOS 保留 traffic lights
- **WHEN** 在 macOS 上显示默认 Window
- **THEN** 用户 MUST 能使用系统 traffic lights 执行关闭、最小化和缩放/全屏相关窗口操作

#### Scenario: macOS traffic lights 随标题栏高度居中
- **WHEN** Window 配置或更新 TitleBar 高度
- **THEN** compatibility 层 MUST 将原生 traffic lights 与当前 titlebar rect 垂直中心线对齐

### Requirement: fallback 平台 SHALL 安全降级

在非 Windows/macOS 平台，或 native chrome 功能不可用时，兼容层 MUST 安全降级到 Qt 默认窗口行为。fallback MUST 不崩溃，且 MUST 保持 Window 内容显示、主题刷新和基本关闭行为可用。

#### Scenario: unsupported platform 构建成功
- **WHEN** 在既不是 Windows 也不是 macOS 的 Qt Widgets 平台构建项目
- **THEN** compatibility 层 MUST 编译通过，Window MUST 能显示内容并关闭

#### Scenario: system move API 不可用
- **WHEN** 平台无法执行 native system move 或 resize
- **THEN** 兼容层 MUST 返回失败状态，Window MUST 保持可用并避免崩溃

### Requirement: Qt5/Qt6 native event 差异 SHALL 被封装

如果 Window 实现需要重写 `nativeEvent` 或处理 native event result 类型，Qt5/Qt6 签名和类型差异 MUST 通过 `src/compatibility/` 封装。Window UI 层 MUST NOT 直接写 `#if QT_VERSION >= QT_VERSION_CHECK(...)`。

#### Scenario: Qt5 构建 native event 路径
- **WHEN** 使用 Qt 5.15 构建包含 Window 的项目
- **THEN** native event 相关代码 MUST 编译通过，且 Window UI 源码中 MUST 不存在 Qt 版本条件分支

#### Scenario: Qt6 构建 native event 路径
- **WHEN** 使用 Qt 6.2+ 构建包含 Window 的项目
- **THEN** native event 相关代码 MUST 编译通过，且 compatibility 层 MUST 处理结果类型与签名差异

### Requirement: 顶层 Window SHALL NOT 使用 QGraphicsOpacityEffect

Window 本体作为 top-level window 时 MUST NOT 使用 `QGraphicsOpacityEffect` 实现整体透明或动画。需要顶层透明时 MUST 使用 Qt 支持的窗口透明度机制或平台原生能力。

#### Scenario: Window 构造不安装 graphics effect
- **WHEN** 创建默认 Window
- **THEN** `graphicsEffect()` MUST 为 null，且 Window MUST 不输出顶层窗口 graphics effect 不受支持的 Qt 警告
