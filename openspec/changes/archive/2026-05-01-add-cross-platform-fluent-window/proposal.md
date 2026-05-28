## Why

当前组件库已经具备 Fluent Design token、基础输入控件、菜单、弹窗与对话框，但 `src/view/windowing/` 仍为空，业务侧缺少一个可作为应用根窗口使用的 Fluent 风格顶层 Window。直接让业务代码用 `QWidget`/`QMainWindow` 再自行拼标题栏，会导致窗口外观、标题栏按钮、拖拽、缩放、主题刷新和 Win/mac 平台差异分散到各处，后续维护成本高。

这次变更引入跨平台 Fluent Window：Windows 上提供 Fluent/WinUI 风格自定义 chrome，macOS 上保留平台窗口习惯并融合 Fluent 内容区；所有平台差异集中封装在 `src/compatibility/`，避免 UI 层散落平台分支。

## What Changes

- 在 `src/view/windowing/` 下新增 Fluent Window 基础组件，作为应用顶层窗口容器使用。
- 提供 Fluent 标题栏 host、内容区 host、外部标题栏内容挂载槽、最小尺寸和主题自适应绘制。
- Windows 平台提供 custom chrome 能力，并保留系统窗口移动、缩放、最大化、还原、最小化、关闭和 Aero Snap 等原生行为；具体 caption controls 由外部内容自行挂载。
- macOS 平台默认保留系统 traffic lights 与窗口行为，Window 内容区和可选标题区域使用 Fluent token 绘制，避免强行复刻 Windows caption buttons。
- 在 `src/compatibility/` 新增窗口 chrome 兼容层，封装 Win/mac 的 native handle、hit-test、系统移动/缩放、标题栏区域和平台材质/窗口属性差异。
- 添加单元测试与 VisualCheck，覆盖默认属性、主题切换、外部标题栏控件行为、内容 host 布局、平台兼容层 fallback 行为和手动视觉检查。

## Capabilities

### New Capabilities

- `fluent-window`: 定义 Fluent 顶层 Window 的公共 API、布局结构、标题栏行为、caption controls、内容 host、主题刷新和可视化验收要求。
- `window-platform-compatibility`: 定义窗口 chrome 跨平台兼容层的职责，包括 Windows 自定义 chrome/native hit-test、macOS 原生窗口习惯保留、Qt fallback 行为，以及平台差异不得泄漏到 `windowing` UI 层。

### Modified Capabilities

- 无。

## Impact

- 新增 `src/view/windowing/` 下的 Window、TitleBar 等源码文件；TitleBar 只承担系统保留区与外部内容槽，不内建业务按钮。
- 新增 `src/compatibility/` 下的窗口 chrome 兼容层文件，可能包含按平台编译的 `.cpp` 文件；若 macOS 后续需要 Objective-C++ 原生 polish，需要在设计中明确是否引入 `.mm` 与 CMake 语言支持。
- 更新 CMake glob/编译配置以纳入新增源码和平台特定实现。
- 新增 `tests/views/windowing/` 测试目录、CMake 测试目标和 VisualCheck。
- 不引入新的第三方依赖；复用现有 FluentElement、QMLPlus、Typography icons、ThemeColors、Spacing、CornerRadius、Elevation 和 Animation token。
