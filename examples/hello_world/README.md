# FluentQt Hello World

Two independent executables demonstrate base and Spatial integration. Each has
its own window implementation; only application startup is shared. Click the
button to change its text.

| Executable target | UILib dependency | Presentation |
| --- | --- | --- |
| `fluentqt_hello_world` | `FluentQt::FluentQt` | A Window with one centered Button |
| `fluentqt_hello_world_spatial` | `FluentQt::Spatial` | A Button on a Card in SpatialView, with rotation, shadow and pointer tilt |

The 2D executable does not link Spatial or its Qt OpenGL modules, even when both
targets are built together. Spatial includes Plugins and the base library.

The Spatial window requests a 720 × 520 logical size. It centers a 320 × 200
Card containing the accent Button. The item uses rotation `(6, -12, 0)`, surface
intensity `0.6`, and maximum pointer tilt `(3, 5)`. The
[Python counterpart](../../bindings/pyside6/examples/hello_world/README.md)
uses this composition and configuration; the browser executable compiles the
same C++ window construction.

The Spatial variant checks activation and the nullable view factory before
using the view. If either fails, it deactivates the session and keeps the same
Card and Button centered in an ordinary QWidget layout. Window preparation is
best-effort after activation; a false preparation result alone does not select
the fallback. Successful activation does not establish GPU availability.

## Read the code

- [CMakeLists.txt](CMakeLists.txt) selects each executable's library dependency.
- [Desktop initialization](platform/desktop/HelloWorldApplication.cpp) creates
  QApplication, initializes resources and shows the window.
- [HelloWorldWindow.cpp](HelloWorldWindow.cpp) creates the Window and Button,
  using a plain QWidget and layout to center the button.
- [HelloWorldSpatialWindow.cpp](HelloWorldSpatialWindow.cpp) creates the Spatial
  variant. The 2D target does not compile this file.

The 3D variant keeps the component's built-in 2D fallback for reduced motion,
high contrast and keyboard navigation. Interactive parameter controls and
component combinations belong in Gallery's Spatial pages.

## Build inside this repository

Open the root project in Qt Creator and select either executable target. The
3D target exists when `FLUENT_QT_BUILD_SPATIAL=ON`; the development presets
enable it. With Spatial off, only the 2D target is created.

On the configured macOS preset:

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx \
  --target fluentqt_hello_world fluentqt_hello_world_spatial
./build/vcpkg-osx/examples/hello_world/fluentqt_hello_world
./build/vcpkg-osx/examples/hello_world/fluentqt_hello_world_spatial
```

Release presets disable executable examples for Gallery packaging.

## Build against an installed SDK

The standalone project builds only the 2D executable by default:

```bash
cmake -S examples/hello_world -B build/examples/hello_world \
  -DCMAKE_PREFIX_PATH="/path/to/fluentqt-sdk;/path/to/Qt/kit"
cmake --build build/examples/hello_world --config Release
```

To build both, use an SDK built with Spatial and add
`-DFLUENT_QT_HELLO_WORLD_SPATIAL=ON` to the configure command. This requests
`find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)` explicitly.

See the repository [README](../../README.md) for `add_subdirectory`,
`FetchContent`, Qt selection, and deployment guidance.

## WebAssembly

Both executable targets also build for WebAssembly. Activate Emscripten 3.1.70,
set `QT_WASM_ROOT` and `QT_HOST_ROOT`, then build the public `wasm` preset.
Serve `build/wasm` over HTTP and use the matching entry:

| Variant | Standalone build output | Staged beside Gallery |
| --- | --- | --- |
| Base 2D | `examples/hello_world/hello-world/index.html` | `app/hello-world/index.html` |
| Spatial | `examples/hello_world/hello-world-spatial/index.html` | `app/hello-world-spatial/index.html` |

The browser target links
`FluentQt::WebAssembly`; its selected launcher calls `configureRuntime()` and
`showWindow()` so the shared window is hosted in the same movable/resizable Qt
desktop surface as the Gallery. Hello World does not use nested modal loops and
therefore does not link `FluentQt::WasmAsyncify`. Shared window construction
contains no WebAssembly conditionals.

The Spatial minimal app is separate from the Gallery's `?showcase=spatial`
embed, which includes interactive scene controls. Use the minimal entry when
comparing the C++, Python, and browser Hello World layouts. Compare logical
widget geometry and matching states; platform fonts, device pixel ratios, and
the surrounding browser stage can change raw screenshot pixels.
