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
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20WebAssembly-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
  <a href="https://pypi.org/project/FluentQt/"><img alt="PyPI" src="https://img.shields.io/pypi/v/FluentQt?color=3776AB"></a>
</p>

<p align="center">
  <a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#top"><img src="docs/assets/readme/hero.png" alt="Windows Mica 效果下开启 3D 模式的 Fluent-Qt Gallery Home 首页"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#gallery">实时体验 C++ Web Gallery</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#top">项目官网</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">提问与交流</a>
</p>

Fluent-Qt（FluentQt）是基于 Qt Widgets 的跨平台 Fluent 风格 C++ UI 组件库，提供输入、导航、集合、数据表格、[图表](docs/architecture/charts.md)、弹窗和窗口等原生控件。组件库采用 C++17 开发，可通过 CMake 接入现有 Qt 项目，并提供可选的 PySide6 绑定。

支持 Windows、macOS、Linux 和 WebAssembly，内置浅色、深色和高对比度主题，动效可减弱或关闭。可选的 [Spatial](docs/architecture/spatial-view.md) 插件为 Widgets 界面提供 3D 展示能力，[QmlBridge](docs/architecture/qml-bridge.md) 插件提供 QML 兼容支持，可加载 QML 页面并共用业务数据和 Fluent 主题。

## 🧩 模块概览

### Core 基础库

`FluentQt::FluentQt` 提供原生控件、主题、设计参数与公共控件基础能力。普通 2D 应用链接此 target 即可。

### Plugins 插件

可选的 [Plugins](docs/architecture/plugins.md) 模块负责插件注册、依赖管理、激活和清理。C++ 应用链接 `FluentQt::Plugins` 后即可使用，各插件通过自己的 target 引入额外依赖。

C++ 直接调用库，Python 使用配套的 PySide6 绑定。WebAssembly 版本由同一份 C++ 代码编译，在浏览器中运行。

#### Spatial

[Spatial](docs/architecture/spatial-view.md) 为现有 Qt Widgets 界面提供 3D 展示能力。`SpatialSession` 负责激活插件、创建视图和停用时的清理。

链接 `FluentQt::Spatial` 时会自动引入 `FluentQt::Plugins` 和 OpenGL 依赖。3D 不可用时可回退到原生 2D。

在 Gallery 的 Plugins → Spatial 中查看 SpatialView 和 SpatialItem 示例。3D 开关在 Settings → 3D Gallery。

#### QmlBridge

[QmlBridge](docs/architecture/qml-bridge.md) 是 FluentQt 的 QML 兼容插件，可在 Widgets 界面中加载 QML 页面，共用业务数据和 Fluent 主题。

启用 `FLUENT_QT_BUILD_QML_BRIDGE=ON` 并链接 `FluentQt::QmlBridge`。Qt Qml、Quick 和 QuickWidgets 依赖随这个可选模块引入。

在 Gallery 的 Plugins → QmlBridge → QmlView 中体验，或构建[最小示例](examples/qml_bridge/)。初始化与页面加载方法见[接入指南](docs/architecture/qml-bridge.md)。

## 🖼 Gallery

Gallery 用于浏览组件、体验交互和查看示例代码。

| 体验方式 | 入口 |
|---|---|
| 浏览器 | [在线 Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) · [项目官网](https://calvinhxx.github.io/Fluent-Qt/zh-CN/#gallery) |
| Windows、macOS、Linux | [下载 Gallery 安装包](https://github.com/calvinhxx/Fluent-Qt/releases/latest) |

### Python 兼容 Gallery

Python Gallery 通过 [PyPI](https://pypi.org/project/FluentQt-Gallery/) 分发。

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

`FluentQt-Gallery` 会自动安装对应版本的 `FluentQt`。

## 🤖 使用 Agent 构建

在 Codex、Claude Code 或 Cursor 中使用 [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md)，创建桌面应用、给现有工程添加 GUI 或修复界面。[查看效果](https://calvinhxx.github.io/Fluent-Qt/zh-CN/#ai-build) · [安装与用法](docs/ai/README.md)

调整 Gallery 示例时，可以先在 Live Scene 里边改边看，再用编译后的 C++ 示例确认最终效果。[查看用法](docs/development/gallery-preview-workflow.md)

## 🚀 快速开始

C++ 接入需要 C++17、CMake 3.16+ 和 Qt Widgets 5.15+ 或 6.2+。新项目推荐使用 `FetchContent`。

### C++ 接入

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
    GIT_TAG v1.10.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 源码集成

定义好应用目标后，加入 Fluent-Qt 源码目录并链接导出目标：

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### 已安装组件库集成

使用已安装的 FluentQt 开发头文件和 CMake 包文件，构建与安装方式见[组件库构建与安装指南](docs/development/build-workflow.md#install-the-library-or-create-a-source-package)。

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

如果 FluentQt 不在系统搜索路径中，配置时传入 `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`。

### C++ 最小示例

`main.cpp`：

```cpp
#include <FluentQt/FluentQt.h>

#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    app.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    fluent::windowing::Window window;
    window.setWindowTitle(QStringLiteral("FluentQt Hello World"));
    window.resize(480, 320);

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 32, 32, 32);

    auto* button = new fluent::basicinput::Button(
        QStringLiteral("Hello from FluentQt"), content);
    button->setFluentStyle(fluent::basicinput::Button::Accent);
    layout->addStretch();
    layout->addWidget(button, 0, Qt::AlignCenter);
    layout->addStretch();

    window.setContentWidget(content);
    window.show();
    return app.exec();
}
```

完整工程见 [`examples/hello_world`](examples/hello_world/)，IDE 中可直接运行 `fluentqt_hello_world` target。

### 可选 Python 兼容

PySide6 兼容层通过 Shiboken6 将 Fluent-Qt 的原生 C++ 控件提供给 Python 使用。

```bash
python -m pip install FluentQt
```

安装、示例、兼容信息和源码构建见 [Python 指南](bindings/pyside6/README.md)。

## 🛠 构建

### 依赖

| 范围 | 依赖 |
|---|---|
| FluentQt C++ 组件库 | C++17、CMake 3.16+、Qt Widgets 5.15+ 或 6.2+ |
| C++ Gallery | FluentQt、Qt Network、spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`、Emscripten 3.1.70 |
| 测试 | FluentQt、Qt Test/Network、GTest、spdlog/fmt |
| 可选 PySide6 绑定 | Qt/PySide6/Shiboken6 6.2.4+；源码构建支持 Python 3.10+ |

### 组件库

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

构建脚本根据可用 CPU 和内存选择并行任务数，配置方法见[构建工作流](docs/development/build-workflow.md)。

### 可选扩展

配置 FluentQt 时启用 Plugins 公共基础设施或其下的插件模块，并在应用中链接对应 target。

| 扩展层级 | 构建选项 | CMake target | 接入示例 |
|---|---|---|---|
| Plugins 公共基础设施 | `-DFLUENT_QT_BUILD_PLUGINS=ON` | `FluentQt::Plugins` | [最小插件示例](docs/architecture/plugins.md#minimal-extension-example) |
| Plugins → Spatial | `-DFLUENT_QT_BUILD_SPATIAL=ON` | `FluentQt::Spatial`（自动包含 Plugins） | [Spatial Hello World](examples/hello_world/README.md) |
| Plugins → QmlBridge | `-DFLUENT_QT_BUILD_QML_BRIDGE=ON` | `FluentQt::QmlBridge`（自动包含 Plugins） | [QML 宿主示例](examples/qml_bridge/) |

源码集成时，在 `add_subdirectory()` 或 `FetchContent_MakeAvailable()` 前设置选项；安装包集成方式见对应模块指南。匹配的 Python 构建通过 `fluentqt.plugins`、`fluentqt.spatial` 和 `fluentqt.qmlbridge` 提供已启用模块，WebAssembly 复用 C++ 实现。

### 本地构建 Gallery

仓库预设需要 CMake 3.25+ 和 `VCPKG_ROOT`。本机 Qt kit 的配置方法见[首次配置](docs/development/build-workflow.md#first-use-setup)。

先查看当前平台可用的构建配置：

```bash
cmake --list-presets
```

Apple Silicon Mac 使用 `vcpkg-osx`：

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

其他平台使用对应的 preset：

| 平台 | preset |
|---|---|
| Apple Silicon Mac | `vcpkg-osx` |
| Intel Mac 或 Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

打包时使用对应的 `-release` preset，并遵循[打包工作流](docs/development/packaging-workflow.md)中的验证步骤。

### WebAssembly

浏览器版本的工具链、本地构建、验证和 Pages 部署见 [WebAssembly 工作流](docs/development/webassembly-workflow.md)。

### 源码包

生成用于离线或源码集成的精简组件库源码包：

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

## 📚 文档

可以从[文档导航](docs/README.md)开始，也可以按目标直接进入：

| 目标 | 入口 |
|---|---|
| 体验和查找控件 | [API Explorer](https://calvinhxx.github.io/Fluent-Qt/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) |
| 构建应用 | [AI 辅助开发](docs/ai/README.md) · [环境检查与项目模板](tools/onboarding/README.md) |
| 参与 FluentQt 开发 | [开发文档树](docs/development/README.md) · [架构约定](docs/architecture/README.md) · [Fluent 设计](docs/design-languages/README.md) |
| 打包或发布 | [打包工作流](docs/development/packaging-workflow.md) · [发布治理](docs/development/release-governance.md) · [版本记录](docs/releases/README.md) |
| 提问或报告问题 | [社区入口](docs/community/README.md) · [支持](SUPPORT.md) · [安全报告](SECURITY.md) |
| 中文即时交流 | QQ 群 `1109997685` |

QQ群用于中文即时交流、作品展示与贡献协作；需要持续追踪的问题请继续使用 GitHub Discussions 或 Issues。

<p align="center">
  <img src="docs/assets/community/qq-group-1109997685.png" width="280" alt="Fluent-Qt QQ 群入群二维码">
</p>

提交改动前请阅读[参与贡献](CONTRIBUTING.md)和[社区行为准则](CODE_OF_CONDUCT.md)。

## 🔗 参考

| 来源 | 用途 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 视觉参考 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 组件行为和示例页面参考 |

## 许可证

Fluent-Qt 项目自身的源代码使用 [MIT License](LICENSE) 发布。项目中捆绑的资源以及发布包中的运行时依赖继续适用各自的上游许可；具体版本、来源、对应源码提供规则和许可证位置见[第三方声明](THIRD_PARTY_NOTICES.md)。产品名称、徽标及外部设计参考的相关说明见[商标与外部引用声明](TRADEMARKS.md)。
