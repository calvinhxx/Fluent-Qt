<p align="center">
  English | <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-Qt logo">
</p>

<h1 align="center">Fluent-Qt</h1>

<p align="center">
  FluentQt uilib based on Qt Widgets, with a Gallery demo app.
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
  <img src="docs/assets/readme/hero.png" alt="Fluent-Qt Gallery preview">
</p>

## 🧱 Dependencies

| Category | Requirement |
|---|---|
| Language | C++17 |
| UI runtime | Qt Widgets, Qt 5.15+ or Qt 6.2+ |
| Build | CMake presets, vcpkg manifest mode |
| Library dependency | spdlog |
| Tests | GTest |

## 🛠 Build FluentQt uilib

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

## 🔌 Use FluentQt uilib

See [`examples/hello_world/`](examples/hello_world/) for a minimal consumer example.

| Integration mode | When to use |
|---|---|
| Installed package | Fluent-Qt is installed or provided by a package manager. |
| Source subproject | Fluent-Qt source is vendored and built with your app. |

Installed package:

```cmake
find_package(FluentQt CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

Source subproject:

```cmake
set(FLUENT_QT_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_INSTALL OFF CACHE BOOL "" FORCE)
set(FLUENT_QT_ENABLE_GALLERY_PACKAGING OFF CACHE BOOL "" FORCE)

add_subdirectory(external/Fluent-Qt)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

Application code:

```cpp
#include <FluentQt/FluentQt.h>

fluent::initializeResources();

auto* button = new fluent::basicinput::Button("Save", this);
button->setFluentStyle(fluent::basicinput::Button::Accent);
```

## 🖼 Gallery

Gallery is the demo application for this repository. It is useful for browsing the available controls, checking states, and comparing theme settings.

Build the Gallery target after configuring the project:

```bash
cmake --preset vcpkg-osx -DFLUENT_QT_BUILD_GALLERY=ON
cmake --build --preset vcpkg-osx --target fluent_qt_gallery
```

```powershell
cmake --preset vcpkg-windows -DFLUENT_QT_BUILD_GALLERY=ON
cmake --build --preset vcpkg-windows --target fluent_qt_gallery
```

## 📦 Package Gallery

| Platform | Architecture | Packaging preset |
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

Windows x64 installer:

```powershell
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer
```

## 📚 Documentation

| Development | Testing | Architecture | Design |
|---|---|---|---|
| [Development workflow](docs/development/README.md) | [Testing workflow](docs/development/testing-workflow.md) | [Architecture contracts](docs/architecture/README.md) | [Design language references](docs/design-languages/README.md) |
| [Release governance](docs/development/release-governance.md) | [Visual review](docs/development/visual-review.md) | [Overlay behavior](docs/architecture/overlay-behavior.md) | [Figma sources](docs/design-languages/figma-sources.md) |
| [Packaging workflow](docs/development/packaging-workflow.md) |  |  |  |

## 🔗 References

| Entry | Purpose |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows visual reference |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS style reference |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 style reference |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | Component behavior and sample page reference |

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

Fluent-Qt is released under the [MIT License](LICENSE).
