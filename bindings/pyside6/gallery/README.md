# Python Gallery package

> **Status:** Current package boundary

<!-- docs-nav:top:start -->
[Documentation](../../../docs/README.md) › [Python bindings](../README.md) › Get started and examples

[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:top:end -->

This directory owns the standalone Python Gallery application.

## Package boundary

- Distribution: `FluentQt-Gallery`
- Import package: `fluentqt_gallery`
- Entry point: `python -m fluentqt_gallery`
- Runtime dependency: the exact matching `FluentQt` distribution version

The Gallery may import only public `fluentqt` APIs. The reusable UILib never
imports `fluentqt_gallery`, and the core wheel never contains Gallery source or
artwork. This keeps applications that only need FluentQt from installing demo
code and large Gallery assets.

The Gallery remains in the same repository because its generated catalog
contract, live samples, and parity tests must evolve atomically with the C++
Gallery and binding manifest.

## Build and run

Configure the PySide6 bindings with
`FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`, then build both distributions:

```bash
cmake --build build/pyside6 --target fluentqt_pyside6_wheels --parallel
```

Install both generated wheels into a clean virtual environment and launch:

```bash
python -m pip install \
  build/pyside6/wheelhouse/fluentqt-*.whl \
  build/pyside6/gallery-wheelhouse/fluentqt_gallery-*.whl
python -m fluentqt_gallery
```

Use `--verify-catalog --walk-routes` for deterministic headless acceptance.

## QML+ foundation

**Foundation → QML+** contains twelve atomic examples for direct/computed
bindings, pause/resume, states, binding restoration, animation, and anchors.
Python Gallery displays Python source and executes the displayed statements.
The WebAssembly Gallery offers the matching C++ and Python source. The Python
helpers use the shared native QMLPlus runtime; see the
[QMLPlus contract](../../../docs/architecture/qmlplus.md).

CTest separates focused Gallery contracts (`test_pyside6_gallery`) from the
complete route and theme sweeps (`test_pyside6_gallery_routes`). Both groups
retain the same per-process timeout and run in fast CI. Run both with
`ctest --test-dir <build> -R '^test_pyside6_gallery(_routes)?$' --output-on-failure`.
Running `gallery/tests/test_gallery.py` directly still runs the complete suite;
standard unittest test-name and `-k` selection remain available.

## Spatial mode

Release wheels include Spatial starting with 1.9.1, so the installed Gallery
has the same 3D controls and examples as C++ and WebAssembly. Upgrade both
`FluentQt` and `FluentQt-Gallery` if using the 1.9.0 wheels, which omitted the
module. For a matched Python/Qt source build, enable Spatial explicitly:

```bash
cmake -S . -B build/pyside6 \
  -DFLUENT_QT_BUILD_SPATIAL=ON \
  -DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON
python3 tools/dev/fluent_qt_build.py build/pyside6 --target fluentqt_pyside6_wheels
PYTHONPATH=build/pyside6/python .venv-pyside/bin/python -m fluentqt_gallery
```

**Settings → 3D Gallery** controls both the navigation/content surfaces and all
Spatial examples. Open **Plugins → Spatial → SpatialView** or
**Plugins → Spatial → SpatialItem**. The component pages link back to that setting;
**Key usage / Full example** separates the integration calls from the complete
working Python example. The same widget instances and values survive mode changes.
With Spatial installed, a fresh Gallery starts in 3D when acceleration is available.
Turning it off saves a 2D preference for later launches. Failed initialization uses
2D without changing that preference.

[QmlBridge](../../../docs/architecture/qml-bridge.md) content joins the shared
3D mode through the native `QmlTextureLayer` OpenGL compositor. The existing QML
root, model, and state remain intact while its hidden Quick child is replaced
by a GPU texture in the projected panel. Unsupported composition or texture
failure uses native 2D without changing the 3D preference. Hidden cached QML
pages do not suspend the visible page's 3D mode.

**Plugins → QmlBridge → QmlView** contains six focused cards for loading,
Fluent theme tokens, context binding, QML actions, lifecycle, and load errors.
**Source code** shows Python host code. **Key usage** shows the integration calls;
**Full example** shows the complete `main.py`. The **File** selector displays the
QML page separately. Context and action cards include the
`LineEdit` or `ProgressBar` used by QML. The Gallery
packages the same `Basic.qml`, `Theme.qml`, `Context.qml`, and `Action.qml`
assets as C++ and WebAssembly. The standalone
[shared-model example](../examples/qml_bridge.py) retains its separate panel.

The shell uses cached widget surfaces, with the shared native Direct3D 11
compositor on Windows ARM64 (Qt 6.7+) and OpenGL on the other release platforms.
When QmlBridge is included, Quick follows the shell's selected graphics API
before the first window is created. Gallery preserves explicit backend
environment settings and uses 2D if they are incompatible. RHI hosts display
QML pages in native 2D and retain 3D on other pages; the QML texture helper does
not import GL textures into Direct3D or Metal. Pointer
input maps back to the live widgets, and native overlays remain above the canvas.
Splash and Intro retain their normal order. The material behind the canvas still
belongs to the Window's Mica/Acrylic setting. Particle effects remain visible in
both backends; Direct3D currently captures their CPU-painted contents. No separate
renderer preference is exposed. The native
`SpatialSession` activates the extension and tracks views and particle adapters;
deactivation returns those surfaces to native 2D without taking their ownership.
The Gallery retains its private RHI window preparation and the public
`SpatialRuntime` driver checks and texture limits. Splash prepares Home and
Settings only; cold component pages yield between samples and cache completed
pages for later visits.

Without the optional binding, the Gallery omits Spatial routes and imports no Qt
OpenGL modules. With the binding but no supported renderer, it retains those
examples in 2D, disables the switch and shows the red support badge. Use
`FLUENT_QT_GALLERY_DISABLE_3D=1` to exercise this fallback. Reduced motion and high
contrast also restore native 2D.

Run the focused regression under `QT_QPA_PLATFORM=offscreen`, or omit that
variable in a native desktop session to check GPU input, scrolling and overlays:

```bash
PYTHONPATH=build/pyside6/python .venv-pyside/bin/python \
  bindings/pyside6/gallery/tests/test_gallery_spatial.py
```

The native test saves snapshots under `build/spatial-validation/python-gallery`.
Offscreen results cover fallback behavior; they do not establish native visual
or performance parity. See [Spatial](../../../docs/architecture/spatial-view.md)
for supported content and platform review boundaries.

The background-texture teardown also has a dedicated native CTest entry:

```bash
ctest --test-dir build/pyside6 -R '^test_pyside6_gallery_spatial_backdrop_native$' --output-on-failure
```

Run it in a desktop session without `QT_QPA_PLATFORM=offscreen`. It is labeled
`pyside_native;local_desktop`, outside headless CI's `pyside` selection, and fails
rather than silently skipping when selected without a native platform.
Private QObject cleanup uses targeted Qt deferred-delete delivery;
it does not rely on Shiboken 6.2.4's uninitialized application-singleton flag or
change the application's normal ownership policy.

<!-- docs-nav:bottom:start -->
---
[← FluentQt PySide6 Hello World](../examples/hello_world/README.md) · [Contents](../../../docs/SUMMARY.md) · [Python bindings index](../README.md) · [Install FluentQt Gallery from PyPI →](PYPI.md)
<!-- docs-nav:bottom:end -->
