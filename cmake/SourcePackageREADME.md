# FluentQt Library Source Package

This archive contains the FluentQt UI component library, optional PySide6
binding sources, the optional WebAssembly adapter target, and minimal
integration material. It can also build the `FluentQt` Python wheel.

The native C++ Gallery, top-level C++ tests, and application logging support
are excluded. Building the separate `FluentQt-Gallery` application wheel
requires a full checkout with the native Gallery catalogs and artwork.

The archive preserves the CMake module tree: `uilib/` owns the Widgets library
and its minimal example; `plugin/` owns the lifecycle framework and the
`plugin_extension`, `Spatial`, and `QmlBridge` subdirectories. Library
implementations remain under `src/`, and example sources and standalone
projects remain under `examples/`. The full checkout also has `app/` for
Gallery, which is excluded here.

Requirements:

- C++17
- CMake 3.16+
- Qt Widgets 5.15+ or 6.2+
- Optional Plugins: enable `FLUENT_QT_BUILD_PLUGINS=ON` and link `FluentQt::Plugins`; uses the existing Qt Widgets dependencies
- Optional Spatial: enable `FLUENT_QT_BUILD_SPATIAL=ON` and link `FluentQt::Spatial`; includes Plugins automatically; Qt 6 requires OpenGLWidgets, Qt 5 requires QOpenGLWidget support
- Optional QmlBridge: enable `FLUENT_QT_BUILD_QML_BRIDGE=ON` and link `FluentQt::QmlBridge`; includes Plugins automatically and requires Qt Qml, Quick, and QuickWidgets

WebAssembly consumers use the narrower browser baseline: Qt 6.9.3
`wasm_singlethread` with Emscripten 3.1.70. Configure with
`-DFLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT=ON` to export
`FluentQt::WebAssembly` and `FluentQt::WasmAsyncify`. Every windowed browser
application links the runtime target; add the Asyncify target only when the
executable retains synchronous nested dialog or menu event loops. Native builds
leave both optional targets absent.

The optional PySide6 binding target requires Python 3.10+ and matching Qt,
PySide6, Shiboken6, and Shiboken6 generator versions from 6.2 onward. See
`bindings/pyside6/README.md` for setup, wheel, and validation commands,
`bindings/pyside6/API_COMPATIBILITY.md` for API version governance, and
`bindings/pyside6/MANYLINUX.md` for the publishable Linux wheel boundary.

Top-level development builds include `FluentQt` and the minimal
`fluentqt_hello_world` executable example. Source-subproject builds keep examples
disabled by default. The included
`examples/hello_world` project demonstrates both in-tree and installed-package
integration. Interactive component demonstrations are maintained in the Gallery
in the full repository.

The optional Plugins module manages linked C++ extension objects, including
registration, dependency-ordered activation, cleanup, and an optional QWidget
factory interface. It does not load shared libraries. Matched PySide6 builds
expose `fluentqt.plugins`; WebAssembly compiles the same C++ lifecycle into the
application. Its public `<FluentQt/Plugins.h>` header and the included
`examples/plugin_extension` project describe the lifecycle and in-tree or
installed-package integration. With `FLUENT_QT_BUILD_PLUGINS=ON` and examples
enabled, build the `fluentqt_plugin_extension` target. Future concrete
extensions keep their additional dependencies in separate targets.

Spatial uses that plugin lifecycle through `SpatialSession`, available in C++
and Spatial-enabled PySide6 builds. Activate the session before creating or
attaching views. Deactivation returns tracked views to native 2D and releases
Spatial resources while retaining caller widgets and models. Requesting the
installed SDK's `Spatial` component loads Plugins automatically; a base-package
request loads neither Plugins nor the additional OpenGL or Quick modules. The
included `examples/hello_world` Spatial variant demonstrates the session entry.

QmlBridge hosts application QML through `QmlSession` and `QmlView`, without
adding Qt Quick to the Widgets core. Call `QmlSession::prepareApplication()`
before the first Quick surface, check activation and view creation, and bind
borrowed context models before loading. Deactivation clears page state while
retaining caller-owned hosts and models; Quick resources are released through
deferred deletion after QML callbacks return. Allow that delivery before
application teardown. Reactivation requires an explicit load.
Request `COMPONENTS QmlBridge` from the installed SDK to discover its Quick
dependencies. Its public `<FluentQt/QmlBridge.h>` header and the included
`examples/qml_bridge` project describe in-tree and installed integration.
With the module and examples enabled, build `fluentqt_qml_bridge`.

Before changing a consumer, run the bundled local preflight. It performs no
network requests and configures its Qt Widgets probe in a temporary directory:

```bash
python3 tools/onboarding/fluentqt_doctor.py --profile cpp
```

Use `--profile python` for the optional installed Python distribution and
`--format json` when an agent consumes the result.

Create a maintained C++ starter without adding repository-specific paths:

```bash
python3 tools/onboarding/fluentqt create my-app \
  --language cpp --starter workbench
```

Use `--starter existing-qt` for an embeddable integration slice or
`--language pyside6` for the Python counterpart. See
`tools/onboarding/README.md` for the full contract.

For AI-assisted integration, start with `llms.txt` and `docs/ai/README.md`.
The package includes the generated component and integration catalog, its
JSON Schemas, and the cross-agent `build-fluentqt-gui` Skill under
`.agents/skills/`. The Skill covers product references, differentiation,
component selection, themes, visual evidence, and performance/lifecycle rules.
Its `lite`/`full` routing matches the workflow to the task's scope.

The Skill is self-contained: its scripts read the bundled catalog snapshot,
and its directory can be installed in any compatible agent. The package also
includes the Skill packager, catalog compatibility query, and deterministic
evaluation tools under `tools/ai/`.

Regenerating the catalog requires Gallery sources from a full checkout, so
the generator is excluded. Catalog source and focused-test paths refer to
that checkout. Sample code is embedded in the catalog and available with
`--json`.

The project's own source is MIT licensed. Bundled assets retain the licenses
and notices included in `THIRD_PARTY_NOTICES.md` and `third_party/`. Qt is a
consumer-supplied dynamic dependency of this source package and is not covered
by the FluentQt MIT license. See `TRADEMARKS.md` for name and design-reference
disclaimers.

Minimal source integration:

```cmake
add_subdirectory(path/to/FluentQt-source)
target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
```

For a WebAssembly executable, use `qt_add_executable()` and follow Qt's normal
browser deployment flow. Enable `FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` before
adding the source tree, link `FluentQt::WebAssembly`, call `configureRuntime()`
before constructing Fluent widgets, and use `showWindow()` for application
windows. Add `FluentQt::WasmAsyncify` separately only when required by the
application's nested event-loop behavior.
