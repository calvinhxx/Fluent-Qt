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

Collection controls include ListView, GridView, FlowView, TreeView, and DataGrid. DataGrid keeps Qt's caller-owned model/delegate contract and stays viewport-bound for large tables.

Fluent is the project's only visual language. Version 1.7 removes the former
Material/Cupertino enums, presets, paint branches, Gallery choices, and PySide6
entry points; product branding stays within Fluent Light/Dark semantic tokens.

## 🧱 Dependencies

| Scope | Dependencies |
|---|---|
| FluentQt C++ library | C++17, CMake 3.16+, Qt Widgets 5.15+ or 6.2+ |
| C++ Gallery | FluentQt, Qt Network, spdlog/fmt |
| C++ WebAssembly | Qt 6.9.3 `wasm_singlethread`, Emscripten 3.1.70 |
| Tests | FluentQt, Qt Test/Network, GTest, spdlog/fmt |
| Optional PySide6 bindings | Qt 6.2+; Python 3.10+ for source builds |

## 🤖 AI-assisted development

Codex, Claude Code, Cursor, and other agents can use [`build-fluentqt-gui`](.agents/skills/build-fluentqt-gui/SKILL.md) to build native C++ or PySide6 GUIs. [Example](https://calvinhxx.github.io/Fluent-Qt/#ai-build) · [Docs](docs/ai/README.md)

## 🚀 Quick Start

### C++ integration

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
    GIT_TAG v1.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fluentqt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Source integration

Place the Fluent-Qt source in the project's `Fluent-Qt` directory:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(Fluent-Qt)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

#### Installed package integration

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FluentQt CONFIG REQUIRED)

add_executable(my_app main.cpp)
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

```python
import sys

import fluentqt
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget
from fluentqt.basicinput import Button
from fluentqt.windowing import Window

def main() -> int:
    fluentqt.prepare_high_dpi_application()
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = Window()
    window.setWindowTitle("FluentQt Hello World")
    window.resize(480, 320)

    content = QWidget()
    layout = QVBoxLayout(content)
    layout.setContentsMargins(32, 32, 32, 32)

    button = Button("Hello from FluentQt", content)
    button.setFluentStyle(Button.ButtonStyle.Accent)
    layout.addStretch()
    layout.addWidget(button, 0, Qt.AlignCenter)
    layout.addStretch()

    window.setContentWidget(content)
    window.show()
    return app.exec()

if __name__ == "__main__":
    sys.exit(main())
```

See [`bindings/pyside6/examples/hello_world`](bindings/pyside6/examples/hello_world/)
for the complete Python example.

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

### Source package

Create the reduced library source package for offline or source integration:

```bash
cmake --build build/fluentqt --target fluent_qt_source_package
```

### WebAssembly

Build the WebAssembly Gallery with Qt 6.9.3 `wasm_singlethread` and Emscripten 3.1.70:

```bash
source "$HOME/Qt/Tools/emsdk/emsdk_env.sh"
export QT_WASM_ROOT="$HOME/Qt/6.9.3/wasm_singlethread"
export QT_HOST_ROOT="$HOME/Qt/6.9.3/macos"
cmake --preset wasm
cmake --build --preset wasm --parallel
python3 -m http.server 4173 --bind 127.0.0.1 --directory build/wasm
```

Open `http://127.0.0.1:4173/app/index.html`. See the [WebAssembly workflow](docs/development/webassembly-workflow.md) for setup, testing, CI, deployment, and platform notes.

## 🖼 Gallery

Gallery is used to browse, demonstrate, and validate FluentQt components.

### C++ Web Gallery

Online: [project website](https://calvinhxx.github.io/Fluent-Qt/#gallery) · [standalone page](https://calvinhxx.github.io/Fluent-Qt/gallery/).

### C++ Gallery packages

Download the Gallery for the required platform, Qt version, and architecture from [GitHub Releases](https://github.com/calvinhxx/Fluent-Qt/releases/latest):

| Platform | Qt 5 / x64 | Qt 6 / x64 | Qt 6 / ARM64 | Format |
|---|---|---|---|---|
| Windows | 5.15.2 | 6.2.4 | 6.9.3 | `.exe` |
| macOS | 5.15.2 | 6.9.3 | 6.9.3 | `.dmg` |
| Linux | 5.15 | 6.2.4 | 6.2.4 | `.deb` |

### Run the C++ Gallery locally

| Platform | x64 preset | ARM64 preset |
|---|---|---|
| Windows | `vcpkg-windows` | `vcpkg-windows-arm64` |
| macOS | `vcpkg-osx-x64` | `vcpkg-osx` |
| Linux | `vcpkg-linux` | `vcpkg-linux-arm64` |

Replace `PRESET` with a value from the table:

```bash
cmake --preset PRESET
cmake --build --preset PRESET --target fluent_qt_gallery --parallel
```

### Package the C++ Gallery locally

| Platform | x64 packaging preset | ARM64 packaging preset | Format |
|---|---|---|---|
| Windows | `vcpkg-windows-installer` | `vcpkg-windows-arm64-installer` | `.exe` |
| macOS | `vcpkg-osx-x64-dmg` | `vcpkg-osx-dmg` | `.dmg` |
| Linux | `vcpkg-linux-deb` | `vcpkg-linux-arm64-deb` | `.deb` |

See the [Packaging Workflow](docs/development/packaging-workflow.md) for exact local packaging commands.

### Python compatibility Gallery

The Python Gallery is available from
[PyPI](https://pypi.org/project/FluentQt-Gallery/).

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

`FluentQt-Gallery` installs the matching `FluentQt` version automatically.

## 📚 Documentation

- [Support and community](SUPPORT.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Code of Conduct](CODE_OF_CONDUCT.md)
- [AI-assisted app development](docs/ai/README.md)
- [Development workflow](docs/development/README.md)
- [Testing and visual review](docs/development/testing-workflow.md)
- [Packaging workflow](docs/development/packaging-workflow.md)
- [Release governance](docs/development/release-governance.md)
- [Architecture contracts](docs/architecture/README.md)
- [Fluent design reference and legacy-theme migration](docs/design-languages/README.md)
- [WebAssembly compatibility](docs/development/webassembly-workflow.md)
- [Python compatibility](bindings/pyside6/README.md)

## 🔗 References

| Entry | Purpose |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows visual reference |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Component behavior and sample page reference |

## License

Fluent-Qt's own source code is released under the [MIT License](LICENSE).
Bundled assets and packaged runtime dependencies retain their upstream terms;
versions, provenance, source-availability rules, and license locations are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Product names,
logos, and external design references are addressed in
[TRADEMARKS.md](TRADEMARKS.md).
