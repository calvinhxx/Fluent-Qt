# FluentQt Hello World

Two independent executables demonstrate base and Spatial integration. Each has
its own window implementation; only application startup is shared. Click the
button to change its text.

| Executable target | UILib dependency | Presentation |
| --- | --- | --- |
| `fluentqt_hello_world` | `FluentQt::FluentQt` | A Window with one centered Button |
| `fluentqt_hello_world_spatial` | `FluentQt::Spatial` | A Button on a Card in SpatialView, with rotation, shadow and pointer tilt |

The 2D executable does not link Spatial or its Qt OpenGL modules, even when both
targets are built together. Spatial already links the base library.

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

The 2D executable is also the minimal WebAssembly smoke target. Activate
Emscripten 3.1.70, set `QT_WASM_ROOT` and `QT_HOST_ROOT`, then build the public
`wasm` preset. Serve `build/wasm` over HTTP and open
`examples/hello_world/fluentqt_hello_world.html`. The browser target links
`FluentQt::WebAssembly`; its selected launcher calls `configureRuntime()` and
`showWindow()` so the shared window is hosted in the same movable/resizable Qt
desktop surface as the Gallery. Hello World does not use nested modal loops and
therefore does not link `FluentQt::WasmAsyncify`. Shared window construction
contains no WebAssembly conditionals.
