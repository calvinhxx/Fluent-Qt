# Fluent-QT

A gallery of WinUI-style Qt Widgets.

Fluent-QT 是基于 Qt Widgets 的 Fluent / WinUI 风格组件库，支持 **Qt 5.15+** 和
**Qt 6.2+**。CMake 会自动发现本机 Qt，并在版本不足时停止配置。

## 参考资料

- [Win UI Kit](https://aka.ms/WinUI/3.0-figma-toolkit)
- [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery)

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

## 开发文档

可复用开发规则集中在 [docs/development/README.md](docs/development/README.md):

- [Testing Workflow](docs/development/testing-workflow.md)
- [Qt Component Test Conventions](docs/development/qt-component-test-conventions.md)
- [Visual Review](docs/development/visual-review.md)
- [Logging Workflow](docs/development/logging-workflow.md)
- [Component API Conventions](docs/development/component-api-conventions.md)
- [Component API Audit](docs/development/component-api-audit.md)
- [Source Comment Style](docs/development/comment-style.md)

架构约定见 [docs/architecture/README.md](docs/architecture/README.md)。涉及 Popup、Flyout、
ComboBox dropdown、DrawerView 等 same-window overlay 行为时，优先看
[Overlay Behavior Contract](docs/architecture/overlay-behavior.md)。

OpenSpec artifacts 位于 [openspec/](openspec/)。OpenSpec 驱动的工作应先遵循对应的
proposal、apply 或 archive workflow，再进入实现改动。
