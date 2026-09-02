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
  <a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#top"><img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery 预览"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#gallery">实时体验 C++ Web Gallery</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/zh-CN/#top">项目官网</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">提问与交流</a>
</p>

Fluent-Qt 是面向 Qt Widgets 的跨平台 Fluent UI 组件库，提供输入、导航、集合、数据表格、弹窗和窗口等原生控件。它保留 Qt Widgets 熟悉的对象模型和 CMake 工作流，支持 Windows、macOS、Linux、WebAssembly、浅色/深色/高对比度主题，以及 C++ 和可选 PySide6 接口，可直接接入现有项目。

## 🤖 使用 Agent 构建

在 Codex、Claude Code 或 Cursor 中调用 [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md)，直接描述要构建的桌面应用。[查看效果](https://calvinhxx.github.io/Fluent-Qt/zh-CN/#ai-build) · [安装与用法](docs/ai/README.md)

调整 Gallery 示例时，可以先在 Live Scene 里边改边看，再用编译后的 C++ 示例确认最终效果。[查看用法](docs/development/gallery-preview-workflow.md)

## 🧱 依赖

| 范围 | 依赖 |
|---|---|
| FluentQt C++ 组件库 | C++17、CMake 3.16+、Qt Widgets 5.15+ 或 6.2+ |
| C++ Gallery | FluentQt、Qt Network、spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`、Emscripten 3.1.70 |
| 测试 | FluentQt、Qt Test/Network、GTest、spdlog/fmt |
| 可选 PySide6 绑定 | Qt 6.2+；源码构建支持 Python 3.10+ |

## 🚀 快速开始

选择一种方式将 FluentQt 链接到 CMake 项目。新项目推荐使用 `FetchContent`。

### C++ 接入

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
    GIT_TAG v1.7.6
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

#### 安装包集成

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

先阅读 [Python 包使用指南](bindings/pyside6/PYPI.md)及其 [Hello World 示例](bindings/pyside6/examples/hello_world/README.md)。源码构建、兼容边界和维护者工作流统一收录在 [PySide6 绑定指南](bindings/pyside6/README.md)中。

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

本地仓库开发可使用[构建工作流](docs/development/build-workflow.md)提供的包装脚本。它根据当前可用 CPU 和内存余量选择并发数，不写死全局并发上限。

### 源码包

生成用于离线或源码集成的精简组件库源码包：

```bash
cmake --build build/fluentqt --target fluent_qt_source_package
```

### WebAssembly

评估项目时可直接使用[在线 WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/)。本地工具链、构建、浏览器冒烟测试和 Pages 部署统一见 [WebAssembly 工作流](docs/development/webassembly-workflow.md)。

## 🖼 Gallery

Gallery 用于浏览、演示和验证 FluentQt 组件。

### C++ Web Gallery

在线体验：[项目官网](https://calvinhxx.github.io/Fluent-Qt/zh-CN/#gallery) · [独立页面](https://calvinhxx.github.io/Fluent-Qt/gallery/)。

### C++ Gallery 安装包

从 [GitHub Releases](https://github.com/calvinhxx/Fluent-Qt/releases/latest) 下载当前 Windows、macOS 或 Linux Gallery 安装包。持续维护的构建与打包矩阵见[打包工作流](docs/development/packaging-workflow.md)。

### 本地运行 C++ Gallery

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

打包时使用对应的 `-release` preset。

### 本地打包 C++ Gallery

使用[打包工作流](docs/development/packaging-workflow.md)中的平台 preset 和验证步骤。

### Python 兼容 Gallery

Python Gallery 通过 [PyPI](https://pypi.org/project/FluentQt-Gallery/) 分发。

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

`FluentQt-Gallery` 会自动安装对应版本的 `FluentQt`。

## 📚 文档

可以从[文档导航](docs/README.md)开始，也可以按目标直接进入：

| 目标 | 入口 |
|---|---|
| 体验和查找控件 | [API Explorer](https://calvinhxx.github.io/Fluent-Qt/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) |
| 构建应用 | [AI 辅助开发](docs/ai/README.md) · [环境检查与项目模板](tools/onboarding/README.md) · [PySide6](bindings/pyside6/README.md) |
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
