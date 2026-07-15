<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  面向 Qt Widgets 的跨平台 Fluent 风格 C++ UI 组件库。
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

| 范围 | 依赖 |
|---|---|
| FluentQt 组件库 | C++17、CMake 3.16+、Qt Widgets |
| Gallery | FluentQt、Qt Network、spdlog/fmt |
| 测试 | FluentQt、Qt Test/Network、GTest、spdlog/fmt |

## 🚀 快速开始

### 集成方式

| 集成方式 | CMake |
|---|---|
| `FetchContent` 集成 | `FetchContent_MakeAvailable(fluentqt)` |
| 源码集成 | `add_subdirectory(Fluent-Qt)` |
| 安装包集成 | `find_package(FluentQt CONFIG REQUIRED)` |

#### `FetchContent` 集成

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 源码集成

将 Fluent-Qt 源码放入项目的 `Fluent-Qt` 目录：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(Fluent-Qt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 安装包集成

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FluentQt CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

如果 FluentQt 不在系统搜索路径中，配置时传入 `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`。

### 最小示例

`main.cpp`：

```cpp
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QString>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    fluent::initializeResources();

    fluent::basicinput::Button button(QStringLiteral("Hello FluentQt"));
    button.show();
    return app.exec();
}
```

完整工程见 [`examples/hello_world`](examples/hello_world/)，IDE 中可直接运行 `fluentqt_hello_world` target。

## 🛠 构建

### 组件库

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/fluentqt --config Release --target FluentQt --parallel
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

### 源码包

生成用于离线或源码集成的精简组件库源码包：

```bash
cmake --build build/fluentqt --target fluent_qt_source_package
```

## 🖼 Gallery

Gallery 用于浏览、演示和验证 FluentQt 组件。

### 发布包

从 [GitHub Releases](https://github.com/calvinhxx/Fluent-Qt/releases/latest) 下载对应平台、Qt 版本和架构的 Gallery：

| 平台 | Qt 5 / x64 | Qt 6 / x64 | Qt 6 / ARM64 | 格式 |
|---|---|---|---|---|
| Windows | 5.15.2 | 6.2.4 | 6.9.3 | `.exe` |
| macOS | 5.15.2 | 6.9.3 | 6.9.3 | `.dmg` |
| Linux | 5.15 | 6.2.4 | 6.2.4 | `.deb` |

### 本地运行

| 平台 | x64 preset | ARM64 preset |
|---|---|---|
| Windows | `vcpkg-windows` | `vcpkg-windows-arm64` |
| macOS | `vcpkg-osx-x64` | `vcpkg-osx` |
| Linux | `vcpkg-linux` | `vcpkg-linux-arm64` |

将 `PRESET` 替换为表中的值：

```bash
cmake --preset PRESET
cmake --build --preset PRESET --target fluent_qt_gallery --parallel
```

### 本地打包

| 平台 | x64 打包 preset | ARM64 打包 preset | 格式 |
|---|---|---|---|
| Windows | `vcpkg-windows-installer` | `vcpkg-windows-arm64-installer` | `.exe` |
| macOS | `vcpkg-osx-x64-dmg` | `vcpkg-osx-dmg` | `.dmg` |
| Linux | `vcpkg-linux-deb` | `vcpkg-linux-arm64-deb` | `.deb` |

具体本地打包命令见[打包工作流](docs/development/packaging-workflow.md)。

## 📚 文档

- [开发工作流](docs/development/README.md)
- [测试与视觉验收](docs/development/testing-workflow.md)
- [打包工作流](docs/development/packaging-workflow.md)
- [发布治理](docs/development/release-governance.md)
- [架构约定](docs/architecture/README.md)
- [设计语言参考](docs/design-languages/README.md)

## 🔗 参考

| 来源 | 用途 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 视觉参考 |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS 风格参考 |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 风格参考 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 组件行为和示例页面参考 |

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
