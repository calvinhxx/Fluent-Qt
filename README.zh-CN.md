<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  基于 Qt Widgets 的 FluentQt uilib，附带 Gallery 演示应用。
</p>

<p align="center">
  <a href="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/calvinhxx/Fluent-Qt/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/calvinhxx/Fluent-Qt/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/calvinhxx/Fluent-Qt?style=flat&color=111827"></a>
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-111827.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
</p>

<p align="center">
  <img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery 预览">
</p>

## 🧱 依赖

| 类别 | 要求 |
|---|---|
| 语言 | C++17 |
| UI 运行时 | Qt Widgets，Qt 5.15+ 或 Qt 6.2+ |
| 构建 | CMake presets、vcpkg manifest mode |
| 库依赖 | spdlog |
| 测试 | GTest |

## 🛠 FluentQt uilib 编译

macOS arm64:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset vcpkg-osx -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=OFF
cmake --build --preset vcpkg-osx --target FluentQt
```

Windows x64:

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
cmake --preset vcpkg-windows -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=OFF
cmake --build --preset vcpkg-windows --target FluentQt
```

## 🔌 FluentQt uilib 使用

最小消费示例见 [`examples/hello_world/`](examples/hello_world/)。

| 集成方式 | 适用场景 |
|---|---|
| 已安装 CMake 包 | Fluent-Qt 已安装，或由包管理器提供。 |
| 源码子项目 | 将 Fluent-Qt 源码放进业务仓库一起编译。 |

已安装 CMake 包：

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

源码子项目：

```cmake
set(FLUENT_QT_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_INSTALL OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_ENABLE_GALLERY_PACKAGING OFF CACHE BOOL "" FORCE)

add_subdirectory(external/Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

业务代码：

```cpp
#include <FluentQt/FluentQt.h>

fluent::initializeResources();

auto* button = new fluent::basicinput::Button("Save", this);
button->setFluentStyle(fluent::basicinput::Button::Accent);
```

## 🖼 Gallery

Gallery 是仓库里的演示应用，用来浏览控件、检查状态、对照主题设置。

配置项目后，可以单独构建 Gallery target：

```bash
cmake --preset vcpkg-osx -DFLUENT_QT_BUILD_GALLERY=ON
cmake --build --preset vcpkg-osx --target fluent_qt_gallery
```

```powershell
cmake --preset vcpkg-windows -DFLUENT_QT_BUILD_GALLERY=ON
cmake --build --preset vcpkg-windows --target fluent_qt_gallery
```

## 📦 Gallery 打包

| 平台 | 架构 | 打包 preset |
|---|---|---|
| Windows | x64 | `vcpkg-windows-installer` |
| Windows | ARM64 | `vcpkg-windows-arm64-installer` |
| macOS | arm64 | `vcpkg-osx-dmg` |
| macOS | x64 | `vcpkg-osx-x64-dmg` |

macOS arm64 DMG:

```bash
cmake --preset vcpkg-osx-release
cmake --build --preset vcpkg-osx-release
cpack --preset vcpkg-osx-dmg
```

Windows x64 安装包：

```powershell
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer
```

## 📚 文档

| 开发 | 测试 | 架构 | 设计 |
|---|---|---|---|
| [开发工作流](docs/development/README.md) | [测试工作流](docs/development/testing-workflow.md) | [架构约定](docs/architecture/README.md) | [设计语言参考](docs/design-languages/README.md) |
| [发布治理](docs/development/release-governance.md) | [视觉验收](docs/development/visual-review.md) | [Overlay 行为](docs/architecture/overlay-behavior.md) | [Figma 来源](docs/design-languages/figma-sources.md) |
| [打包工作流](docs/development/packaging-workflow.md) |  |  |  |

## 🔗 参考

| 来源 | 用途 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 视觉参考 |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS 风格参考 |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 风格参考 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 控件行为和示例页面参考 |

## ⭐ Star History

<p align="center">
  <a href="https://www.star-history.com/#calvinhxx/Fluent-Qt&Date">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=calvinhxx/Fluent-Qt&amp;type=Date&amp;theme=dark">
      <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=calvinhxx/Fluent-Qt&amp;type=Date">
      <img alt="Fluent-Qt Star History Chart" src="https://api.star-history.com/svg?repos=calvinhxx/Fluent-Qt&amp;type=Date">
    </picture>
  </a>
</p>

## License

Fluent-Qt 使用 [MIT License](LICENSE) 发布。
