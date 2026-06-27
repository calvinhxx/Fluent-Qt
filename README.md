# Fluent-QT

A gallery of WinUI-style Qt Widgets.

Fluent-QT 是基于 Qt Widgets 的 Fluent / WinUI 风格组件库，支持 **Qt 5.15+** 和
**Qt 6.2+**。CMake 会自动发现本机 Qt，并在版本不足时停止配置。

## 参考资料

- [Win UI Kit](https://aka.ms/WinUI/3.0-figma-toolkit)
- [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery)
- [Fluent UI 2 Web](https://developer.microsoft.com/en-us/fluentui#/controls/web)

多设计语言（Fluent / Material 3 / macOS）的样式主题规范见
[docs/design-languages/README.md](docs/design-languages/README.md)，其中 Fluent 为基线
（值取自本仓库 `src/design/*.h`），Material 3 与 macOS 的颜色 token、形状与组件规格均通过
Figma MCP 从官方设计套件实证抽取，并以 Fluent 基线为差量：

- [Fluent（Windows）设计参考](docs/design-languages/fluent.md)（基线，另两者为其差量）
- [Material 3（Google）设计参考](docs/design-languages/material-3.md)
- [macOS（Apple）设计参考](docs/design-languages/macos.md)
- [Figma 来源与抽取方式](docs/design-languages/figma-sources.md)

## 快速开始

项目使用 C++17、vcpkg manifest 依赖、GTest 和 spdlog。公共 CMake presets 默认开启测试，
并要求先设置 `VCPKG_ROOT`。

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx
ctest --preset vcpkg-osx --output-on-failure
```

Windows PowerShell:

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
```

可用公共 presets:

- `vcpkg-osx`: macOS arm64 vcpkg build.
- `vcpkg-osx-x64`: macOS x64 / Rosetta vcpkg build.
- `vcpkg-windows`: Windows x64 vcpkg build.

## 本机工具链

日常开发可以直接用 Qt Creator、Visual Studio 或 Xcode 打开 CMake 项目，并选择本机已有的
Qt Kit、编译器、generator 或 SDK。

公共 `CMakePresets.json` 不写死 Qt、Ninja、MSVC、Xcode 或其它机器路径。本机路径请通过
IDE Kit、shell 环境，或已被 git 忽略的 `CMakeUserPresets.json` 提供。如果 Qt 不在 CMake
默认搜索路径中，把 `CMAKE_PREFIX_PATH` 或 `Qt6_DIR` 放进本机 user preset。

## 项目结构

- [src/components/](src/components/)：可复用 Fluent Qt 组件与 component foundation。
- [tests/components/](tests/components/)：与组件分类对应的 GTest / VisualCheck 测试。
- [app/](app/)：Gallery 应用层代码，按 model / view / viewmodel 组织。
- [tests/gallery/](tests/gallery/)：Gallery 应用层测试，可使用 app-only 几何验证辅助规则。

## 日志

Gallery 应用默认以 Info 级别将日志持久化到平台日志目录，并把 Qt 的
qWarning/qDebug 汇入同一日志文件：

- macOS: `~/Library/Application Support/Fluent-QT/WinUI 3 Gallery/logs/`
- Windows: `%LOCALAPPDATA%\Fluent-QT\WinUI 3 Gallery\logs\`

单文件写满 5MB 轮转为编号备份，磁盘最多保留 3 个文件。组件库 `fluent_qt_lib`
保持中立默认值（Warn、仅控制台、无文件输出），第三方独立使用时通过
`utils::logging::InitializationOptions` 自行配置。`SPDLOG_LEVEL` 环境变量可临时
覆盖日志级别。规则详见 [Logging Workflow](docs/development/logging-workflow.md)。

## 开发文档

可复用开发规则集中在 [docs/development/README.md](docs/development/README.md):

- [Testing Workflow](docs/development/testing-workflow.md)
- [App Visual Geometry Verification](docs/development/app-visual-geometry-verification.md)
- [Qt Component Test Conventions](docs/development/qt-component-test-conventions.md)
- [Visual Review](docs/development/visual-review.md)
- [Tooltip Usage](docs/development/tooltip-usage.md)
- [Logging Workflow](docs/development/logging-workflow.md)
- [Component API Conventions](docs/development/component-api-conventions.md)
- [Component API Audit](docs/development/component-api-audit.md)
- [Source Comment Style](docs/development/comment-style.md)

架构约定见 [docs/architecture/README.md](docs/architecture/README.md)。涉及 Popup、Flyout、
ComboBox dropdown、DrawerView 等 same-window overlay 行为时，优先看
[Overlay Behavior Contract](docs/architecture/overlay-behavior.md)。

OpenSpec artifacts 位于 [openspec/](openspec/)。OpenSpec 驱动的工作应先遵循对应的
proposal、apply 或 archive workflow，再进入实现改动。
