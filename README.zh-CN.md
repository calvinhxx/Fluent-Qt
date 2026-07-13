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
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-111827.svg">
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

Linux 通过 Qt 的 X11（`xcb`）与 Wayland 平台插件面向通用桌面环境。
Ubuntu 22.04 x64 与 ARM64 + Qt 6.2.x 是 CI/参考基线，official Qt 5.15.2
`gcc_64` 用于 x64 Qt 5 验证。兼容范围、Qt 5/Qt 6 命令、桌面测试和可选
WSL2/Lima 开发说明见
[Linux 工作流](docs/development/linux-workflow.md)。

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

Windows ARM64（使用原生 Qt `msvc2022_arm64` kit）：

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
cmake --preset vcpkg-windows-arm64 -D "CMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_arm64" -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=OFF
cmake --build --preset vcpkg-windows-arm64 --target FluentQt
```

Linux x64:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset vcpkg-linux -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=OFF
cmake --build --preset vcpkg-linux --target FluentQt
```

Linux ARM64:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset vcpkg-linux-arm64 -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=OFF
cmake --build --preset vcpkg-linux-arm64 --target FluentQt
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

```bash
cmake --preset vcpkg-linux -DFLUENT_QT_BUILD_GALLERY=ON
cmake --build --preset vcpkg-linux --target fluent_qt_gallery
```

## 📦 Gallery 打包

| Qt | 平台 | 架构 | 打包 preset |
|---|---|---|---|
| 5.15 | Windows | x64 | `vcpkg-windows-installer` |
| 5.15 | macOS | x64 | `vcpkg-osx-x64-dmg` |
| 5.15 | Linux | x64（`.deb`） | `vcpkg-linux-deb` |
| 6.2 | Windows | x64 | `vcpkg-windows-installer` |
| 6.9.3 | Windows | ARM64 | `vcpkg-windows-arm64-installer` |
| 6.2 | macOS | x64 | `vcpkg-osx-x64-dmg` |
| 6.2 | macOS | arm64 | `vcpkg-osx-dmg` |
| 6.2 | Linux | x64（`.deb`） | `vcpkg-linux-deb` |
| 6.2 | Linux | ARM64（`.deb`） | `vcpkg-linux-arm64-deb` |

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

Linux x64 Debian 包：

```bash
cmake --preset vcpkg-linux-release
cmake --build --preset vcpkg-linux-release
cpack --preset vcpkg-linux-deb
```

Linux ARM64 Debian 包：

```bash
cmake --preset vcpkg-linux-arm64-release
cmake --build --preset vcpkg-linux-arm64-release
cpack --preset vcpkg-linux-arm64-deb
```

## 📚 文档

| 开发 | 测试 | 架构 | 设计 |
|---|---|---|---|
| [开发工作流](docs/development/README.md) | [测试工作流](docs/development/testing-workflow.md) | [架构约定](docs/architecture/README.md) | [设计语言参考](docs/design-languages/README.md) |
| [发布治理](docs/development/release-governance.md) | [视觉验收](docs/development/visual-review.md) | [Overlay 行为](docs/architecture/overlay-behavior.md) | [Figma 来源](docs/design-languages/figma-sources.md) |
| [打包工作流](docs/development/packaging-workflow.md) | [Linux 工作流](docs/development/linux-workflow.md) |  |  |

## 🔗 参考

| 来源 | 用途 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 视觉参考 |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS 风格参考 |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 风格参考 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 控件行为和示例页面参考 |

## ⭐ Star History

<p align="center">
  <a href="https://www.star-history.com/?type=date&amp;repos=calvinhxx%2FFluent-Qt">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=calvinhxx/Fluent-Qt&amp;type=date&amp;theme=dark&amp;legend=top-left&amp;sealed_token=5uu92EgOlUC9-xsiTo-jtgWIXlpuwyqUZv6T-OkI3i98eXHmX-twtcffxg0vsJQXdrSnhu52jkCJzQppzhC2AYUlcT3Hz4FZ-Li9V9ymhCzitOQIVPP5lZyhAdBK1BVDbLtDVH3GbLHt_AotSiZFR-MMOamnvCxTC9CiYzgQQs8vqXNFLzt2cgYSPEjk">
      <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=calvinhxx/Fluent-Qt&amp;type=date&amp;legend=top-left&amp;sealed_token=5uu92EgOlUC9-xsiTo-jtgWIXlpuwyqUZv6T-OkI3i98eXHmX-twtcffxg0vsJQXdrSnhu52jkCJzQppzhC2AYUlcT3Hz4FZ-Li9V9ymhCzitOQIVPP5lZyhAdBK1BVDbLtDVH3GbLHt_AotSiZFR-MMOamnvCxTC9CiYzgQQs8vqXNFLzt2cgYSPEjk">
      <img alt="Fluent-Qt Star History Chart" src="https://api.star-history.com/chart?repos=calvinhxx/Fluent-Qt&amp;type=date&amp;legend=top-left&amp;sealed_token=5uu92EgOlUC9-xsiTo-jtgWIXlpuwyqUZv6T-OkI3i98eXHmX-twtcffxg0vsJQXdrSnhu52jkCJzQppzhC2AYUlcT3Hz4FZ-Li9V9ymhCzitOQIVPP5lZyhAdBK1BVDbLtDVH3GbLHt_AotSiZFR-MMOamnvCxTC9CiYzgQQs8vqXNFLzt2cgYSPEjk">
    </picture>
  </a>
</p>

## License

Fluent-Qt 使用 [MIT License](LICENSE) 发布。
