# FluentQt Library Source Package

This archive contains the reusable FluentQt UI component library, its optional
PySide6 binding sources, the optional WebAssembly adapter target, and minimal
integration material. The native C++
Gallery, top-level C++ tests, and application logging support are not part of
this package. The standalone `FluentQt-Gallery` application is intentionally
excluded because it depends on the native Gallery catalogs and artwork from
the complete repository. This archive still builds the reusable `FluentQt`
Python wheel; use a full checkout to build the separate Gallery wheel.

Requirements:

- C++17
- CMake 3.16+
- Qt Widgets 5.15+ or 6.2+

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

Top-level development builds include `FluentQt` and the
`fluentqt_hello_world` executable example. Source-subproject builds keep the
example disabled and build only the library. The included
`examples/hello_world` project demonstrates both in-tree and installed-package
integration.

For AI-assisted integration, start with `llms.txt` and
`docs/ai/README.md`. The source package includes the generated component and
integration catalog, its JSON Schemas, the cross-agent `build-fluentqt-gui`
Skill, its product-reference, differentiation, component-selection, theme, and
visual-evidence contracts under `.agents/skills/`, plus its proportional
`lite`/`full` routing and performance/lifecycle contract. The Skill is
self-contained: its scripts read the bundled catalog snapshot and the same
directory can be installed in any compatible agent. The package also includes
the Skill packager, catalog compatibility query, and deterministic evaluation
tools under `tools/ai/`. It intentionally omits the catalog generator because
regeneration depends on Gallery sources from a full checkout. Catalog source
and focused-test paths point back to that checkout; sample code remains
embedded in the catalog and is available with `--json`.

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
