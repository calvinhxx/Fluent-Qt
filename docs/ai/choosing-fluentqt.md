# Choosing and migrating to FluentQt

> **Status:** Current guide
>
> **Audience:** C++ and PySide6 application developers evaluating UI libraries or modernizing existing Qt Widgets applications

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [AI-assisted development](README.md) › Workflow

[Contents](../SUMMARY.md) · [AI-assisted development index](README.md) · [FluentQt Onboarding Tools →](../../tools/onboarding/README.md)
<!-- docs-nav:top:end -->

FluentQt (also named Fluent-Qt in CMake, GitHub, and packaging) is an MIT-licensed native C++17 component library built directly on **Qt Widgets** (`Qt 5.15+` and `Qt 6.2+`). One C++17 codebase drives native desktop C++, official **PySide6** bindings (via Shiboken6), Qt Designer plugins, and **WebAssembly** builds.

## Comparing Qt UI approaches

Choose the Qt UI stack that matches your existing architecture, licensing constraints, and interaction model:

| Option | License | Core architecture | C++ & Python reach | Built-in data & visual stack | Visual designer | Incremental adoption in existing QWidget apps |
|---|---|---|---|---|---|---|
| **Plain Qt Widgets + QSS** | LGPLv3 / Commercial (Qt) | Native OS styles or global CSS-like stylesheets | C++ and PySide6 / PyQt | Standard Qt views; `QtCharts` is a separate GPLv3/commercial module | Qt Designer (built-in widgets) | Baseline; QSS cascades often break custom metrics, DPI scaling, and focus states across widgets |
| **Qt Quick / QML** | LGPLv3 / Commercial (Qt) | Separate scene-graph UI runtime (QML + JS/C++ models) | C++ and PySide6 / PyQt | QML Controls, custom shaders, Qt Graphs / Qt Charts | Qt Design Studio | High rewrite cost for pure `QWidget` apps unless isolated inside `QQuickWidget` (or FluentQt `QmlBridge`) |
| **FluentQt (Qt Widgets)** | **MIT** (library, PySide6 bindings, Designer plugin, starters) | Native C++17 `QWidget` subclasses + semantic tokens | **One C++17 core**: C++, official `PySide6`, and WebAssembly | Included `DataGrid`, 9 pure-Widgets charts (no QtCharts), optional `Particles`, `Spatial`, `QmlBridge` | **Browser Page Studio** (WASM C++ preview, exports C++ / PySide6 / `.ui`) + **Qt Designer** plugin | Designed for page-by-page or control-by-control drop-in replacement inside existing `QApplication` event loops |

Qt licenses apply separately from the FluentQt library license. See the [Qt Charts licensing terms](https://doc.qt.io/qt-6/qtcharts-index.html#licenses).

Check current supported component counts and platform matrices in the generated [AI component catalog](generated/fluentqt-ai-catalog.json) and [PySide6 wheel matrix](../../bindings/pyside6/wheel-matrix.json) rather than relying on static summaries.

## When another stack fits better

Do not migrate to FluentQt simply for styling if another architecture fits your constraints better:

- **Mobile-first or GPU-heavy fluid transitions**: Use **Qt Quick / QML** directly if your primary UI is touch-driven animations and custom shader transitions. If you have a mature Qt Widgets shell and only need one QML viewport, host that view selectively through FluentQt's optional [QmlBridge](../architecture/qml-bridge.md) module.
- **Codebases locked to PyQt5 or PyQt6**: FluentQt's Python bindings target official **PySide6** via Shiboken6. If your application cannot migrate from PyQt5/PyQt6 bindings to PySide6, staying with Qt Widgets tooling built for PyQt avoids a binding migration.
- **Web-first teams shipping browser software**: Use a web UI framework unless you need shared C++/Qt native widgets alongside a browser evaluation target (`WebAssembly`).

## Adoption and migration paths

### Path 1: Incremental upgrade in an existing C++ Qt Widgets app

Existing CMake/Qt Widgets applications do not need to rewrite their domain model, `QApplication` event loop, or legacy dialogs all at once:

1. **Check toolchain readiness**: run `python3 tools/onboarding/fluentqt doctor --profile cpp` or review [CMake setup](../../README.md#c-setup) (`FetchContent`, installed `find_package(FluentQt CONFIG REQUIRED)`, vcpkg overlay, or Conan 2).
2. **Initialize theme tokens at startup**: call `fluent::prepareHighDpiApplication()` before `QApplication`, call `fluent::initializeResources()`, apply `Typography::fontStyle(Typography::FontRole::Body).toQFont()`, and apply `fluent::UserTheme::apply()`.
3. **Migrate one panel or page first**: scaffold a bounded slice with `python3 tools/onboarding/fluentqt create panel-slice --language cpp --starter existing-qt` or construct FluentQt widgets directly inside existing `QLayout` containers. Existing business signals and `QAbstractItemModel` models stay caller-owned.
4. **Add `.ui` forms visually when needed**: load the [Qt Designer plugin](../../tools/designer/README.md) into Qt Creator or export `.ui`, C++, or PySide6 snippets from [Page Studio](https://calvinhxx.github.io/Fluent-Qt/studio/).
5. **Verify geometry and accessibility**: run `fluent::diagnostics::Inspector::report(window)` on migrated surfaces to catch clipped layouts, missing accessible names, or nested scroll boundary issues (see [Inspector contract](../architecture/inspector-report.md)).

### Path 2: Adopting FluentQt in an existing PySide6 (or migrating PyQt) project

1. **Install binaries**: `python -m pip install FluentQt` (add `FluentQt-Gallery` to inspect examples locally).
2. **Swap PyQt imports if coming from PyQt6**: map `PyQt6.*` imports to `PySide6.*`, `pyqtSignal` to `Signal`, and `pyqtSlot` to `Slot`; Qt Widgets layouts and `QAbstractTableModel` / `QStandardItemModel` implementations carry over cleanly.
3. **Upgrade screen by screen**: test an isolated slice via `python3 tools/onboarding/fluentqt create py-slice --language pyside6 --starter existing-qt` and reuse your existing `QWidget` parents and models.
4. **Share `.ui` forms with C++ teammates**: compile `.ui` files generated by `pyside6-designer` or Page Studio via [`tools/designer/compile_ui.py`](../../tools/designer/compile_ui.py) so `QStringList` properties work identically across Qt 6.2 through Qt 6.9+.
5. **Automate headless UI checks**: run `fluentqt.inspect_widget(window)` in offscreen `unittest` / CI checks alongside unit tests.

### Path 3: Starting a new desktop application

Scaffold a layered desktop project with explicit `domain`, `application`, `infrastructure`, and `ui` separation (and `--quality-report` CI checks already wired):

- **Operations / telemetry / instrument UI**: `python3 tools/onboarding/fluentqt create my-monitor --language cpp --starter monitor` (or `--language pyside6`) — includes KPI cards, a two-series `LineChart`, and a filterable `DataGrid` inside `ScrollView`.
- **General desktop workbench shell**: `python3 tools/onboarding/fluentqt create my-app --language cpp --starter workbench` — includes a responsive navigation/content shell.
- **Browser-first layout prototyping**: open one of the [Page Studio templates](https://calvinhxx.github.io/Fluent-Qt/studio/) (`device-monitor`, `ai-workbench`, `serial-console`, `analytics`, `settings`), tweak Light/Dark themes and properties on live WebAssembly widgets, and export C++, PySide6, or `.ui`. Exported pages inherit the host application’s theme and accent.

<!-- docs-nav:bottom:start -->
---
[Contents](../SUMMARY.md) · [AI-assisted development index](README.md) · [FluentQt Onboarding Tools →](../../tools/onboarding/README.md)
<!-- docs-nav:bottom:end -->
