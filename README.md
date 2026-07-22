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
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
</p>

<p align="center">
  <img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery preview">
</p>

## 🧱 Dependencies

| Scope | Dependencies |
|---|---|
| FluentQt library | C++17, CMake 3.16+, Qt Widgets |
| Gallery | FluentQt, Qt Network, spdlog/fmt |
| Tests | FluentQt, Qt Test/Network, GTest, spdlog/fmt |

## 🚀 Quick Start

### Integration

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
    GIT_TAG main
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

### Minimal example

`main.cpp`:

```cpp
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QString>

int main(int argc, char* argv[])
{
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();

    fluent::basicinput::Button button(QStringLiteral("Hello FluentQt"));
    button.show();
    return app.exec();
}
```

FluentQt follows the operating system display scale through Qt. Keep application
geometry in device-independent coordinates and do not add a second app-level multiplier.

See [`examples/hello_world`](examples/hello_world/) for the complete project, or run the `fluentqt_hello_world` target directly from an IDE.

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

## 🖼 Gallery

Gallery is used to browse, demonstrate, and validate FluentQt components.

### Release packages

Download the Gallery for the required platform, Qt version, and architecture from [GitHub Releases](https://github.com/calvinhxx/Fluent-Qt/releases/latest):

| Platform | Qt 5 / x64 | Qt 6 / x64 | Qt 6 / ARM64 | Format |
|---|---|---|---|---|
| Windows | 5.15.2 | 6.2.4 | 6.9.3 | `.exe` |
| macOS | 5.15.2 | 6.9.3 | 6.9.3 | `.dmg` |
| Linux | 5.15 | 6.2.4 | 6.2.4 | `.deb` |

### Run locally

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

### Package locally

| Platform | x64 packaging preset | ARM64 packaging preset | Format |
|---|---|---|---|
| Windows | `vcpkg-windows-installer` | `vcpkg-windows-arm64-installer` | `.exe` |
| macOS | `vcpkg-osx-x64-dmg` | `vcpkg-osx-dmg` | `.dmg` |
| Linux | `vcpkg-linux-deb` | `vcpkg-linux-arm64-deb` | `.deb` |

See the [Packaging Workflow](docs/development/packaging-workflow.md) for exact local packaging commands.

## 📚 Documentation

- [Development workflow](docs/development/README.md)
- [Testing and visual review](docs/development/testing-workflow.md)
- [Packaging workflow](docs/development/packaging-workflow.md)
- [Release governance](docs/development/release-governance.md)
- [Architecture contracts](docs/architecture/README.md)
- [Design language references](docs/design-languages/README.md)

## 🔗 References

| Entry | Purpose |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows visual reference |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS style reference |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 style reference |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Component behavior and sample page reference |

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

Fluent-Qt's own source code is released under the [MIT License](LICENSE).
Bundled assets and packaged runtime dependencies retain their upstream terms;
versions, provenance, source-availability rules, and license locations are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Product names,
logos, and external design references are addressed in
[TRADEMARKS.md](TRADEMARKS.md).
