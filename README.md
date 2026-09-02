<p align="center">
  English | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  A cross-platform Fluent-style C++ UI component library for Qt Widgets.
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
  <a href="https://calvinhxx.github.io/Fluent-Qt/#top"><img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery preview"></a>
</p>

<p align="center">
  <strong><a href="https://calvinhxx.github.io/Fluent-Qt/#gallery">Try the live C++ Web Gallery</a></strong>
  ·
  <a href="https://calvinhxx.github.io/Fluent-Qt/#top">Project website</a>
  ·
  <a href="https://github.com/calvinhxx/Fluent-Qt/discussions">Questions &amp; community</a>
</p>

Fluent-Qt is a cross-platform Fluent UI component library for Qt Widgets. It provides native controls for input, navigation, collections, data grids, overlays, and windows while preserving Qt's object model and CMake workflow. It supports Windows, macOS, Linux, WebAssembly, Light/Dark/High Contrast themes, an application-wide Full/Reduced/Disabled motion policy, C++, and optional PySide6 bindings, and can be added directly to existing projects.

## 🤖 Build with AI

Use [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) in Codex, Claude Code, or Cursor, then describe the desktop app you want. [Example](https://calvinhxx.github.io/Fluent-Qt/#ai-build) · [Install and use](docs/ai/README.md)

When tuning a Gallery sample, use Live Scene to see each saved change, then check the result in the compiled C++ sample. [How it works](docs/development/gallery-preview-workflow.md)

## 🧱 Dependencies

| Scope | Dependencies |
|---|---|
| FluentQt C++ library | C++17, CMake 3.16+, Qt Widgets 5.15+ or 6.2+ |
| C++ Gallery | FluentQt, Qt Network, spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| Tests | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| Optional PySide6 bindings | Qt 6.2+; Python 3.10+ for source builds |

## 🚀 Quick Start

Link FluentQt to a CMake project using one of the following methods. `FetchContent` is recommended for new projects.

### C++ setup

| Integration | CMake |
|---|---|
| `FetchContent` integration | `FetchContent_MakeAvailable(fluentqt)` |
| Source integration | `add_subdirectory(Fluent-Qt)` |
| Installed package integration | `find_package(FluentQt CONFIG REQUIRED)` |

#### `FetchContent` integration

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    fluentqt
    GIT_REPOSITORY https://github.com/calvinhxx/Fluent-Qt.git
    GIT_TAG v1.8.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Source integration

After defining your application target, add the Fluent-Qt source directory and
link the exported target:

```cmake
add_subdirectory(Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Installed package integration

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

If FluentQt is outside the system search path, configure with `-DCMAKE_PREFIX_PATH=/path/to/fluentqt`.

### C++ minimal example

`main.cpp`:

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

See [`examples/hello_world`](examples/hello_world/) for the complete project, or run the `fluentqt_hello_world` target directly from an IDE.

### Optional Python compatibility

The PySide6 compatibility layer exposes Fluent-Qt's native C++ widgets to
Python through Shiboken6.

```bash
python -m pip install FluentQt
```

Start with the [Python package guide](bindings/pyside6/PYPI.md) and its
[Hello World example](bindings/pyside6/examples/hello_world/README.md). Source
builds, compatibility boundaries, and maintainer workflows are indexed by the
[PySide6 binding guide](bindings/pyside6/README.md).

## 🛠 Build

### Library

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/fluentqt --config Release --target FluentQt --parallel
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

For local repository development, the [build workflow](docs/development/build-workflow.md)
provides a wrapper that selects parallelism from current CPU and memory
headroom instead of using a fixed job count.

### Source package

Create the reduced library source package for offline or source integration:

```bash
cmake --build build/fluentqt --target fluent_qt_source_package
```

### WebAssembly

Use the [live WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/)
for evaluation. Local toolchain setup, builds, browser smoke tests, and Pages
deployment are documented in the
[WebAssembly workflow](docs/development/webassembly-workflow.md).

## 🖼 Gallery

Gallery is used to browse, demonstrate, and validate FluentQt components.

### C++ Web Gallery

Online: [project website](https://calvinhxx.github.io/Fluent-Qt/#gallery) · [standalone page](https://calvinhxx.github.io/Fluent-Qt/gallery/).

### C++ Gallery packages

Download the current Windows, macOS, or Linux Gallery package from
[GitHub Releases](https://github.com/calvinhxx/Fluent-Qt/releases/latest).
The maintained build and package matrix lives in the
[packaging workflow](docs/development/packaging-workflow.md).

### Run the C++ Gallery locally

List the presets available on the current host:

```bash
cmake --list-presets
```

On Apple Silicon, use `vcpkg-osx`:

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery
```

Use the matching preset on other platforms:

| Platform | Preset |
|---|---|
| Apple Silicon Mac | `vcpkg-osx` |
| Intel Mac or Rosetta | `vcpkg-osx-x64` |
| Linux x64 | `vcpkg-linux` |
| Linux ARM64 | `vcpkg-linux-arm64` |
| Windows x64 | `vcpkg-windows` |
| Windows ARM64 | `vcpkg-windows-arm64` |

Use the matching `-release` preset when packaging.

### Package the C++ Gallery locally

Use the platform preset and verification steps in the
[Packaging Workflow](docs/development/packaging-workflow.md).

### Python compatibility Gallery

The Python Gallery is available from
[PyPI](https://pypi.org/project/FluentQt-Gallery/).

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

`FluentQt-Gallery` installs the matching `FluentQt` version automatically.

## 📚 Documentation

Start with the [documentation map](docs/README.md), or choose a path:

| Goal | Entry point |
|---|---|
| Evaluate controls | [API Explorer](https://calvinhxx.github.io/Fluent-Qt/api/) · [WebAssembly Gallery](https://calvinhxx.github.io/Fluent-Qt/gallery/) |
| Build an application | [AI-assisted development](docs/ai/README.md) · [Onboarding tools](tools/onboarding/README.md) · [PySide6](bindings/pyside6/README.md) |
| Contribute to FluentQt | [Development tree](docs/development/README.md) · [Architecture](docs/architecture/README.md) · [Fluent design](docs/design-languages/README.md) |
| Package or release | [Packaging](docs/development/packaging-workflow.md) · [Release governance](docs/development/release-governance.md) · [Release notes](docs/releases/README.md) |
| Get help or report a problem | [Community](docs/community/README.md) · [Support](SUPPORT.md) · [Security](SECURITY.md) |

See [Contributing](CONTRIBUTING.md) and the
[Code of Conduct](CODE_OF_CONDUCT.md) before opening a change.

## 🔗 References

| Entry | Purpose |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows visual reference |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Component behavior and sample page reference |

## License

Fluent-Qt's own source code is released under the [MIT License](LICENSE). Bundled assets and packaged runtime dependencies retain their upstream terms; versions, provenance, source-availability rules, and license locations are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Product names, logos, and external design references are addressed in [TRADEMARKS.md](TRADEMARKS.md).
