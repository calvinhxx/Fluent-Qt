<p align="center">
  简体中文 | <a href="README.en.md">English</a>
</p>

<p align="center">
  <img src="app/assets/app-icon.png" width="88" alt="Fluent-QT logo">
</p>

<h1 align="center">Fluent-QT</h1>

<p align="center">
  面向 Qt Widgets 的现代 Fluent 组件库。
  <br>
  用 C++17 构建原生桌面体验，覆盖组件、设计 token、主题系统与 Gallery 示例应用。
</p>

<p align="center">
  <a href="https://github.com/calvinhxx/Fluent-QT/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/calvinhxx/Fluent-QT/actions/workflows/ci.yml/badge.svg"></a>
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-111827.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS-111827.svg">
  <img alt="Qt Widgets" src="https://img.shields.io/badge/UI-Qt%20Widgets-41CD52.svg">
  <img alt="Qt" src="https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-41CD52.svg">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C.svg">
</p>

<p align="center">
  <img src="docs/assets/readme/hero.png" alt="Fluent-QT Gallery 预览">
</p>

## ✨ 项目定位

Fluent-QT 为传统 Qt Widgets 应用补齐现代桌面界面能力。它不要求迁移到 QML，而是在原生 Widgets 栈上提供可复用组件、设计规范、主题基础设施和可运行的 Gallery。

| 原生 Widgets | 设计系统 | Gallery 验证 |
|---|---|---|
| 保持 C++/Qt Widgets 工程形态。 | 以 Fluent 为基线，兼顾 Material 3 与 macOS 风格分支。 | 用真实应用展示组件状态、主题切换和示例代码。 |

## 🧭 支持范围

| 平台 | 架构 | 交付 |
|---|---|---|
| Windows | x64 / ARM64 | Debug、Release、installer |
| macOS | arm64 / x64 | Debug、Release、DMG |

## 🧱 依赖

| 类别 | 要求 |
|---|---|
| Language | C++17 |
| UI runtime | Qt 5.15+ 或 Qt 6.2+ |
| Build | CMake、vcpkg |
| Test / logging | GTest、spdlog |

## 🧩 组件能力

README 只保留能力概览，完整组件索引放在 Gallery 和后续官网中展示。

覆盖范围包括：基础输入、集合视图、导航、弹层、文本输入、日期时间、菜单工具栏、滚动、状态反馈与窗口系统。

## 🚀 快速开始

macOS:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx
ctest --preset vcpkg-osx --output-on-failure
```

Windows:

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
cmake --preset vcpkg-windows
cmake --build --preset vcpkg-windows
ctest --preset vcpkg-windows --output-on-failure
```

## 📦 打包

macOS:

```bash
cmake --preset vcpkg-osx-release
cmake --build --preset vcpkg-osx-release
cpack --preset vcpkg-osx-dmg
```

Windows:

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

## 🔗 引用

| 入口 | 用途 |
|---|---|
| [Windows UI Kit (Community)](https://www.figma.com/design/qpecbg7hOfos9DcHWeKlfw/Windows-UI-kit--Community-?node-id=2434-129659) | Fluent / Windows 视觉基线 |
| [macOS 27 UI Kit (Community)](https://www.figma.com/design/W0PjLoNXuQyLACYlAE3QKi/macOS-27--Community-?node-id=131-8996) | macOS 风格分支参考 |
| [Material 3 Design Kit (Community)](https://www.figma.com/design/sfn7GB1zXX6Lu8hfhYqhbA/Material-3-Design-Kit--Community-?node-id=49823-12141) | Material 3 风格分支参考 |
| [WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) | 组件语义与示例体验参考 |

## License

Fluent-QT is released under the [MIT License](LICENSE).
