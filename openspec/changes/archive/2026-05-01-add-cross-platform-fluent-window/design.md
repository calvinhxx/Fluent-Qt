## Context

`src/view/windowing/` 当前为空；库里已有 `FluentElement` 统一暴露颜色、字体、圆角、阴影、动画和材质 token，也已有 `Dialog`、`Popup`、`Menu` 等自绘浮层经验。已有实现里已经证明顶层窗口和普通 child widget 不同：顶层 window 不能依赖 `QGraphicsOpacityEffect`，frameless window 的阴影、透明、拖拽、关闭动画和平台窗口管理器行为都需要谨慎处理。

这次变更要补齐一个可作为应用根窗口使用的 Fluent Window。它不是普通控件，而是连接 Qt widget 树和 OS window manager 的边界。因此设计重点是把 Fluent UI 层和平台 chrome 行为拆开：

```text
src/view/windowing/                  src/compatibility/
┌──────────────────────────┐         ┌────────────────────────────┐
│ Window                   │         │ WindowChromeCompat          │
│  - Fluent layout/paint   │ delegates│  - native hit-test          │
│  - title/content slots   │────────▶│  - system move/resize       │
│  - titlebar content slot │         │  - platform window options  │
└──────────────────────────┘         └─────────────┬──────────────┘
                                                     │
                                      ┌──────────────┴──────────────┐
                                      │                             │
                                Windows behavior              macOS behavior
```

用户已明确方案 B 更合理：Windows 侧提供 Fluent/WinUI 风格自定义 chrome，macOS 侧保留原生窗口习惯和 traffic lights，只让内容区与可选标题区域使用 Fluent 视觉语言。

## Goals / Non-Goals

**Goals:**

- 在 `src/view/windowing/` 提供 `view::windowing::Window`、`TitleBar` 等基础类型；`TitleBar` 只提供系统保留区与外部内容槽。
- Window 可作为顶层应用窗口，提供标题栏 host、内容 host 和窗口状态操作；标题、图标、导航、搜索、caption controls 等具体 UI 由外部 QWidget 挂载到标题栏内容槽。
- Windows 上启用 Fluent custom chrome 和 native hit-test，保留系统移动、缩放、最大化、最小化、关闭和 Aero Snap 行为；caption controls 可由外部内容自行提供。
- macOS 上默认保留系统 traffic lights，并允许 Qt 自定义内容进入 unified titlebar 区域，避免把 Windows caption 交互强加给 macOS 用户。
- 所有 Win/mac/Qt native 差异集中在 `src/compatibility/`，`src/view/windowing/` 保持平台无关。
- 复用现有 Fluent token、Typography icons、QMLPlus、测试组织方式和 VisualCheck 约定。

**Non-Goals:**

- 不在第一期引入 Objective-C++ `.mm` 文件或 macOS AppKit 私有/半私有定制。
- 不实现完整 `QMainWindow` 替代品，例如 dock、toolbar area、status bar、MDI、save/restore layout。
- 不引入新的第三方窗口库。
- 不为 Dialog/Popup/Menu 做行为重构；只在确有复用价值时抽取小工具。
- 不保证 Linux 原生窗口管理器一比一体验；非 Win/mac 平台走 Qt fallback。

## Decisions

### Decision 1: `Window` 使用 QWidget 组合，而不是继承 QMainWindow

**选择**：`view::windowing::Window` 继承 `QWidget`、`FluentElement`、`view::QMLPlus`，内部组合 `TitleBar` 与 `contentHost`。业务内容通过 `setContentWidget(QWidget*)` 放入 content host，或通过 `contentHost()` 自行设置 layout。

```text
Window
├─ TitleBar
│  ├─ system reserved leading area
│  └─ external content slot
└─ ContentHost
   └─ user content
```

**理由**：
- 当前组件库是轻量 QWidget 控件库，不依赖 `QMainWindow` 的复杂 dock/menu/status 架构。
- `QWidget` 与现有测试、FluentElement、QMLPlus、AnchorLayout 模式一致。
- 自定义 chrome 的 hit-test 区域更容易由 `TitleBar` host 与外部控件 rect 推导，不需要绕过 `QMainWindow` 的内部 layout。

**备选**：
- 继承 `QMainWindow`：能复用 menu/status/dock，但会把本次目标扩大到应用 shell 框架，不符合当前库的轻量控件风格。
- 只提供 `TitleBar` 控件，不提供 `Window`：业务仍需自己处理 native hit-test 和平台差异，不能解决真正问题。

### Decision 2: 平台差异封装为 `WindowChromeCompat`

**选择**：新增 `src/compatibility/WindowChromeCompat.h/.cpp`，并按平台拆分实现文件，例如 `WindowChromeCompat_win.cpp`、`WindowChromeCompat_mac.cpp`。`Window` 只调用平台无关 API，不直接 include Windows/macOS 原生头，也不散落 `Q_OS_*` 分支。

建议 API 形态：

```cpp
namespace compatibility {

struct WindowChromeOptions {
    QRect titleBarRect;
    QVector<QRect> dragExclusionRects;
    int resizeBorderWidth = 8;
    bool useCustomWindowChrome = false;
    bool preferNativeMacControls = true;
};

class WindowChromeCompat {
public:
    explicit WindowChromeCompat(QWidget* window);
    void configure(const WindowChromeOptions& options);
    void applyPlatformWindowFlags();
    bool handleNativeEvent(const QByteArray& eventType, void* message, qintptr* result);
    bool beginSystemMove(const QPoint& globalPos);
    bool beginSystemResize(Qt::Edges edges, const QPoint& globalPos);
};

}
```

**理由**：
- nativeEvent、Windows hit-test 常量、DWM 属性、macOS 保留原生 controls 等都属于平台边界，不应出现在 view 层。
- 后续若需要 macOS `.mm` polish，可以只替换 compatibility 实现，不改 Window 公共 API。
- 现有 `QtCompat.h` 已建立“兼容差异集中封装”的项目惯例。

**备选**：
- 在 `Window.cpp` 中直接 `#ifdef Q_OS_WIN/Q_OS_MAC`：短期简单，但会让 view 层承担平台系统知识，后续很难测试和维护。
- 把所有差异继续塞进 `QtCompat.h`：该文件当前聚焦 Qt5/Qt6 API 差异，窗口 chrome 是平台行为差异，独立文件更清晰。

### Decision 3: TitleBar 是 host，caption/content 由外部挂载

**选择**：
- Windows：Window 使用 frameless/custom chrome，但 `TitleBar` 不内建 caption buttons。业务或示例可把 minimize/maximize/close buttons 作为普通 QWidget 放入 titlebar content slot，并连接到 `Window` slots。
- macOS：Window 默认保留系统 traffic lights，并通过 full-size/unified titlebar 策略让 titlebar content slot 与系统标题栏融合；不显示 Windows 风格 caption buttons。
- 其他平台：走 Qt 默认窗口框架，最多展示 Fluent 内容 header，不承诺 native custom chrome。

**理由**：
- Windows 用户期望 WinUI/Fluent caption 样式、Aero Snap、边缘 resize 和双击标题栏最大化，但具体 caption controls 不需要绑定在 `TitleBar` 类型里。
- macOS 用户期望左上角 traffic lights、系统全屏按钮、系统窗口移动和平台动画。强行右上角 Windows caption buttons 会破坏平台习惯。
- `TitleBar` 做成 host 后，平台分歧收敛到 chrome policy 与系统保留区宽度，具体 UI 由外部组合。

**备选**：
- 全平台统一右上角 Fluent caption buttons：视觉统一但 macOS 行为不自然，且会把产品层按钮固定进基础 host。
- 全平台都使用原生窗口框架：兼容成本低，但 Windows 上无法达到 Fluent custom chrome 的目标。

### Decision 4: Windows 移动/缩放依赖 native hit-test，不用手写 `move()`

**选择**：Windows 上 `Window::nativeEvent` 委托给 `WindowChromeCompat::handleNativeEvent()`。兼容层根据标题栏矩形、可拖拽排除区和 resize border 返回对应 hit-test 结果，使 OS 处理拖拽、边缘缩放、corner resize、双击最大化和 Snap。

```text
mouse over titlebar blank area -> HTCAPTION
mouse over caption button      -> HTCLIENT
mouse over custom widget       -> HTCLIENT
mouse over left resize border  -> HTLEFT
mouse over top-left corner     -> HTTOPLEFT
```

**理由**：
- 手写 `mouseMoveEvent + move()` 无法完整保留 Windows Snap、系统 resize cursor、边缘吸附和多显示器 DPI 行为。
- hit-test 是 Windows 自定义 chrome 的标准做法，且测试可以通过兼容层输入矩形做局部验证。

**备选**：
- 使用 `QWindow::startSystemMove()` 作为主路径：Qt 层 API 更简单，但对 resize/corner/Snap 覆盖不如 native hit-test 完整；它可以作为 fallback。
- 完全手写拖拽/缩放：实现快，但行为劣化明显，不适合作为顶层 Window 基座。

### Decision 5: macOS 使用 runtime bridge 做 unified titlebar，不引入 `.mm`

**选择**：macOS 第一阶段仍不引入 Objective-C++ `.mm` 文件，但 `WindowChromeCompat_mac.cpp` 可通过 Objective-C runtime bridge 对 `NSWindow` 做有限设置：保留 traffic lights，启用 full-size content view，隐藏系统标题文本，并让 Qt `TitleBar` content slot 融入原生标题栏区域。

**理由**：
- 当前 CMake 只启用 `CXX C`，源码 glob 只覆盖 `.h/.cpp`。引入 `.mm` 会扩大构建系统变更和 CI 风险。
- VSCode 类体验需要自定义 Qt 控件进入 macOS 标题栏区域，但不需要移动或重绘 traffic lights。
- 若后续需要 macOS vibrancy、toolbarStyle 或更精细的 traffic lights 布局，再通过新的设计变更引入 Objective-C++。

**备选**：
- 立即引入 `.mm` 操作 `NSWindow`：能更精细地融合 titlebar，但会让本次变更同时承担构建系统、AppKit 和窗口行为三类风险。
- 完全不触碰 `NSWindow`：实现最简单，但无法形成 macOS 上 traffic lights + 自定义 titlebar content 的 unified titlebar 视觉。

### Decision 6: 顶层 Window 不使用 QGraphicsOpacityEffect

**选择**：Window 本体不使用 `QGraphicsOpacityEffect` 做顶层透明/动画。Window 背景、标题栏、边框使用 `paintEvent` 与 Fluent token 绘制；如需顶层淡入淡出，必须使用 `setWindowOpacity`，且本次不默认开启窗口动画。

**理由**：
- 项目已有 Dialog 设计明确验证：Qt 不支持在 top-level window 上使用 `QGraphicsEffect`。
- 应用主窗口的打开/关闭动画由 OS/window manager 负责更自然，组件库不应默认干预。

**备选**：
- 给 Window 加统一 entrance 动画：视觉上活跃，但主窗口生命周期和 OS 动画容易冲突，且不属于第一期核心能力。

### Decision 7: 测试分层处理 native 行为

**选择**：
- 单元测试覆盖公共 API、默认属性、布局、主题切换、外部标题栏控件行为和兼容层纯逻辑。
- Windows-only 测试只验证 Windows 环境可观测的 hit-test/state 行为；非 Windows 环境跳过或验证 fallback。
- VisualCheck 展示 Windows custom chrome 和 macOS native frame 两种形态。

**理由**：
- CI/本地环境可能只有 macOS 或 Windows，不能让另一平台的 native 细节阻塞基础测试。
- native window manager 行为很多只能人工确认，VisualCheck 是项目已有模式。

**备选**：
- 所有 native 行为都做自动化断言：脆弱且平台依赖高，容易产生假失败。

## Risks / Trade-offs

- **[Risk] Windows frameless custom chrome 丢失系统阴影或圆角** -> Mitigation: 优先由 `WindowChromeCompat` 设置 DWM 相关窗口属性；无法启用时至少绘制 Fluent 边框并保留 resize/Snap。
- **[Risk] Qt5/Qt6 nativeEvent 签名或结果类型差异导致编译问题** -> Mitigation: 在 compatibility 层提供统一类型/宏或内联 helper，UI 层不直接写版本分支。
- **[Risk] macOS 视觉与 Windows 不完全一致** -> Mitigation: 这是方案 B 的有意取舍，保持平台窗口习惯优先；Fluent 视觉集中在内容区和可选 header。
- **[Risk] 自定义 titlebar 内嵌输入框/按钮时误触发窗口拖拽** -> Mitigation: `TitleBar` 向 compatibility 层提供 drag exclusion rects，外部交互控件默认排除。
- **[Risk] VisualCheck 无法覆盖所有 OS 组合** -> Mitigation: 将纯布局和 API 测试保持平台无关，native 行为通过平台条件测试和手动检查补充。

## Migration Plan

本次是新增能力，无现有 API 迁移。实现时按以下顺序落地：

1. 新增 compatibility 层的公共接口和 Qt fallback。
2. 新增 `Window`/`TitleBar` 的平台无关 UI 与布局，TitleBar 只提供系统保留区和外部内容槽。
3. 接入 Windows native hit-test/custom chrome。
4. 接入 macOS native-frame 保留策略。
5. 增加测试和 VisualCheck。

如果实现过程中 Windows native chrome 风险过高，可以先保留 Qt fallback 与 macOS 原生框架，将 Windows custom chrome 标记为未完成任务，不影响 spec 边界。

## Open Questions

- 是否需要第一版就暴露 `WindowMaterial`（Solid/Mica/Acrylic）枚举，还是先固定使用 `bgCanvas/bgLayer` token？建议第一版先不扩大材质 API。
- 是否需要支持运行时切换 macOS “原生 traffic lights”与“自绘 Fluent caption buttons”？建议不支持，避免平台行为复杂化。
