# PySide6 bindings

> **Status:** Current build, usage, and distribution guide

[Documentation home](../../docs/README.md) · [Contents](../../docs/SUMMARY.md)

FluentQt wraps the C++ widget library for Python in one Shiboken6 extension. Use
the table below to jump to installation, examples, or packaging rules without
repeating details across pages.

## Choose a path

| Goal | Start here |
|---|---|
| Install and use FluentQt | [Python package guide](PYPI.md) |
| Run a minimal application | [Hello World](examples/hello_world/README.md) |
| Explore the complete component set | [Python Gallery package](gallery/README.md) |
| Build the bindings from source | [Build from source](#build-from-source) |
| Check supported Python, Qt, platforms, and APIs | [Compatibility and coverage](ROADMAP.md) |
| Publish wheels | [Publishing runbook](PUBLISHING.md) |

The Python deliverables stay separate:

- `FluentQt` / `import fluentqt` is the reusable native UILib.
- `FluentQt-Gallery` / `import fluentqt_gallery` is an example application that
  depends on the exact matching `FluentQt` version.

## Compatibility boundary

- The C++ library supports Qt Widgets 5.15+ and 6.2+.
- The optional PySide6 target is Qt 6-only and disabled by default.
- Source builds retain a CPython 3.10 plus Qt/PySide6/Shiboken6 6.2.4 minimum
  compatibility gate.
- PySide6, Shiboken6 runtime, Shiboken6 generator, and the C++ Qt SDK must use
  the same version and architecture.
- Published wheels support only the combinations declared in
  [`wheel-matrix.json`](wheel-matrix.json). A source-compatible combination is
  not automatically a prebuilt-wheel promise.
- PySide2, Qt 5 Python, PyPy, and 32-bit x86 are outside the supported binding
  contract.

Use machine-readable sources for changing facts:

| Fact | Canonical source |
|---|---|
| Public Python names and required methods | [`api-manifest.json`](api-manifest.json) |
| CPython, platform, architecture, Qt, and wheel tags | [`wheel-matrix.json`](wheel-matrix.json) |
| API versioning and deprecations | [API compatibility policy](API_COMPATIBILITY.md) |
| Linux repair and audit rules | [manylinux policy](MANYLINUX.md) |

## Install from PyPI

```bash
python -m pip install FluentQt
```

The wheel installs the matching PySide6 and Shiboken6 runtime dependencies.
Follow the [package guide](PYPI.md) for the minimal application and current
published compatibility table.

Install the standalone Gallery when you want runnable examples:

```bash
python -m pip install FluentQt-Gallery
python -m fluentqt_gallery
```

The Gallery imports only public `fluentqt` APIs. Its source and artwork are not
included in the reusable UILib wheel.

## Build from source

Create a matched Qt for Python environment. The minimum compatibility example
uses Python 3.10 and Qt/PySide/Shiboken 6.2.4:

```bash
python3.10 -m venv .venv-pyside
.venv-pyside/bin/python -m pip install PySide6==6.2.4
.venv-pyside/bin/python -m pip install \
  --index-url https://download.qt.io/official_releases/QtForPython/ \
  shiboken6_generator==6.2.4
```

Configure with the matching C++ Qt SDK and the same interpreter:

```bash
cmake -S . -B build/pyside6 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.2.4 \
  -DPython_EXECUTABLE=/path/to/.venv-pyside/bin/python \
  -DFLUENT_QT_BUILD_PYSIDE6_BINDINGS=ON \
  -DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON \
  -DFLUENT_QT_BUILD_EXAMPLES=OFF \
  -DFLUENT_QT_INSTALL=OFF \
  -DBUILD_TESTING=ON
cmake --build build/pyside6 --target fluentqt_pyside6_stubs --parallel
ctest --test-dir build/pyside6 -L '^pyside$' --output-on-failure
```

`fluentqt_pyside6_stubs` builds the native extension and generates the typed
package facade. The manifest check rejects missing public classes, enums,
functions, variables, or required methods before wheel creation.

Build both distributions when a clean-wheel test or release rehearsal is
required:

```bash
cmake --build build/pyside6 --target fluentqt_pyside6_wheels --parallel
```

The UILib wheel is written to `build/pyside6/wheelhouse/`; the Gallery wheel is
written to `build/pyside6/gallery-wheelhouse/`. Use the
[publishing runbook](PUBLISHING.md) for clean-install, TestPyPI, approval, and
artifact-verification steps.

## Run representative examples

### Optional Plugins module

Enable `FLUENT_QT_BUILD_PLUGINS=ON` in the matched source-build environment and
build `fluentqt_pyside6_stubs` to include `fluentqt.plugins`. Spatial includes
this dependency automatically. The Python module uses the native plugin
manager for metadata, dependency ordering, activation, rollback, and cleanup.
See the [Plugins contract](../../docs/architecture/plugins.md) for extension
authoring and caller ownership.

The [plugin extension example](examples/plugin_extension.py) demonstrates
registration, activation, widget creation, and deactivation. With the generated
package on `PYTHONPATH`, run it normally or pass `--smoke-test` for an automated
lifecycle run. That run does not establish native visual acceptance.
The [example entry table](../../docs/architecture/plugins.md#minimal-extension-example)
identifies the matching C++ executable and browser app.

`PluginManager` borrows plugins and the optional host `QObject`. Keep a Python
reference to any unparented plugin while it is registered. A plugin with a Qt
parent keeps that parent. Unregistering leaves the plugin alive. If a registered
plugin is destroyed, the manager removes its registration and stops its active
dependents. Widgets returned by a factory keep the parent you pass in and can
outlive both the manager and the plugin.

Optional modules are compiled during source builds; published wheel coverage is
governed by the [wheel matrix](wheel-matrix.json). In the browser, WebAssembly
compiles the shared C++ modules directly rather than running Python.

### Optional Spatial module

The default binding build keeps the base 2D API. To include `SpatialView`,
`SpatialItem`, and `ParticleLayer`, enable Spatial in the same matched source-build environment:

```bash
cmake -S . -B build/pyside6 -DFLUENT_QT_BUILD_SPATIAL=ON
python3 tools/dev/fluent_qt_build.py build/pyside6 --target fluentqt_pyside6_stubs
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/spatial.py
```

Import from `fluentqt.spatial`. Rendering and pointer animation use the native
C++ implementation; Python handles configuration and application signals.
The [Spatial Hello World guide](examples/hello_world/README.md) describes the
Card-and-Button layout and matching native C++ and browser entries.
Activate a `SpatialSession` before creating or attaching its surfaces:

```python
from fluentqt.spatial import SpatialSession
from PySide6.QtWidgets import QWidget

session = SpatialSession(window, window)
active = session.activate()
if active:
    session.prepareWindow(window)  # Best-effort while the window is hidden.
view = session.createView(window) if active else None
if view is None:
    session.deactivate()
    content = QWidget()  # Populate an ordinary 2D surface with application controls.
else:
    content = view
window.setContentWidget(content)
```

Always check `activate()` and `createView()` before using the returned view. In
Hello World, any failure cleanly falls back to centering the Card and Button in
a plain 2D layout (`prepareWindow()` is optional best-effort setup).

The session shares the C++ plugin lifecycle (you can also register
`fluentqt.spatial.SpatialPlugin` manually with `fluentqt.plugins.PluginManager`).
Activation creates no GPU context, and deactivation switches tracked views back
to 2D Raster mode while leaving widgets and models with their configured cleanup
mode (`addOwnedWidget()`, `addBorrowedWidget()`, `addReparentedWidget()`). Use
`attachView(view)` to track an existing `SpatialView` subclass.

`fluentqt.particles` provides `ParticleBackdrop`, `ParticlesPlugin`, and
`ParticlesSession`. Backdrops work in ordinary Widgets layouts with CPU rendering
and do not require Spatial. See the [Particles guide](../../docs/architecture/particles.md).

`ParticleLayer` is a lower-level adapter for driving an existing OpenGL
compositor from a `ParticleBackdrop` (which otherwise draws on the CPU):

```python
from fluentqt.spatial import SpatialSession

session = SpatialSession(canvas, canvas)
layer = session.createParticleLayer(backdrop, canvas) if session.activate() else None
if layer is not None:
    layer.frameRequested.connect(canvas.update)

# In canvas.paintGL(), with its context current and an aggregate byte allowance:
if layer is not None and layer.render(canvas.devicePixelRatioF(), remaining_bytes):
    texture = layer.textureId()  # Premultiplied RGBA, bottom-up GL coordinates.
    # Sample this texture in the compositor, preserving widget clipping/stacking.

# On 2D mode or compositor teardown, resume CPU at the same particle phase:
if layer is not None:
    layer.release()
# At the end of the extension's scope, release every tracked adapter:
session.deactivate()
```

Only a successful `render()` delegates painting and frame requests to the
compositor; otherwise `ParticleBackdrop` continues rendering on the CPU. Pass a
valid active OpenGL context and preserve widget clipping when drawing. See the
Python Gallery [particle compositor](gallery/src/fluentqt_gallery/particle_compositor.py)
for a full example.

This build adds Qt OpenGL/OpenGLWidgets to the same extension, sharing theme
and motion state with the base controls. A 2D-only source build does not link
these modules. Release wheels include Spatial on every platform starting with
1.9.1, correcting its omission from the 1.9.0 wheels. With
`FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`, the Python Gallery
uses **Settings → 3D Gallery** for its shell and every Spatial example, matching
the C++ and WebAssembly Gallery. See the [Gallery guide](gallery/README.md#spatial-mode).
See [Spatial](../../docs/architecture/spatial-view.md) for supported content and fallback behavior.

### Optional QmlBridge module

Release wheel builds enable `fluentqt.qmlbridge` for the Gallery's QmlView page.
The published 1.12.1 wheels omitted this module. For a source build, enable
`FLUENT_QT_BUILD_QML_BRIDGE=ON` and build `fluentqt_pyside6_stubs`. The module
exports the shared native `QmlPlugin`, `QmlSession`, `QmlTheme`,
`QmlTextureLayer`, and `QmlView`.
Source builds can omit Qt QuickWidgets by disabling QmlBridge. Import
`fluentqt.qmlbridge` to register the Quick converters used by returned QML root
objects.

Call `QmlSession.prepareApplication()` once at startup before creating the
first Qt Quick view when sharing a window with Spatial or `QOpenGLWidget`. Then
activate a session, create a view, register context objects, and load an
absolute `QUrl`. Check `QmlView.lastError()` or `errorOccurred` if `load()`
returns `False`. Keep Python references to unparented model objects or assign
them a Qt parent.

The [shared-model example](examples/qml_bridge.py) runs the same
[`Preview.qml`](../../examples/qml_bridge/qml/Preview.qml) and model setup as C++
and WebAssembly:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/qml_bridge.py --smoke-test
```

Deactivating the session clears the QML scene immediately and deletes the
internal Quick widget and engine on the next event-loop tick, leaving your
`QmlView` and models intact (reactivating requires calling `load()` again). QML
code can bind to the read-only `fluentTheme` object for theme colors,
typography, radii, and motion settings. Python Gallery provides six cards under
**Plugins → QmlBridge → QmlView** covering loading, theme tokens, context
objects, actions, lifecycle, and errors; see the [Gallery cards guide](../../docs/architecture/qml-bridge.md#gallery-example-cards).

`QmlTextureLayer` and `QmlView` also support offscreen texture rendering when an
OpenGL compositor renders projected Widgets panels, using standard Qt Quick items
with no Qt Quick 3D dependency. See the
[composition guide](../../docs/architecture/qml-bridge.md#composition-and-gallery-mode)
for cache limits, 2D fallback, and Qt 5.15/6.2 scaling notes.

### Base examples

Run build-tree examples with the generated package on `PYTHONPATH`:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/main.py
```

| Scenario | Example |
|---|---|
| Theme, controls, and signals | [`compatibility_showcase.py`](examples/compatibility_showcase.py) |
| Native title bar and backdrop | [`window_chrome.py`](examples/window_chrome.py) |
| Models, delegates, and selection | [`list_view_model.py`](examples/list_view_model.py), [`grid_view_model.py`](examples/grid_view_model.py), [`tree_view_model.py`](examples/tree_view_model.py) |
| Same-window overlays | [`popup_overlay.py`](examples/popup_overlay.py), [`flyout_overlay.py`](examples/flyout_overlay.py), [`drawer_view_ownership.py`](examples/drawer_view_ownership.py) |
| Date and time controls | [`date_time_pickers.py`](examples/date_time_pickers.py) |
| Nullable start/end date-time range | [`date_time_range_picker.py`](examples/date_time_range_picker.py) |
| Navigation and page ownership | [`navigation_view_ownership.py`](examples/navigation_view_ownership.py), [`tab_view_navigation.py`](examples/tab_view_navigation.py) |
| Menus and command surfaces | [`command_surfaces.py`](examples/command_surfaces.py) |

The complete Python Gallery is the preferred way to browse every generated
route and live sample. Its deterministic acceptance mode is:

```bash
QT_QPA_PLATFORM=offscreen \
  .venv-pyside/bin/python -m fluentqt_gallery \
  --verify-catalog --walk-routes
```

Native backdrop, title-bar, drag, and compositor behavior still require a real
Windows, macOS, X11, or Wayland session.

Theme and motion preferences use the same native runtime as C++:

```python
import fluentqt

fluentqt.set_theme(fluentqt.Theme.HighContrast)
fluentqt.set_motion_mode(fluentqt.MotionMode.Reduced)
```

`Reduced` keeps short transitions while stopping continuous motion;
`Disabled` resolves all motion to its final state. A component whose local
animation switch is off never animates in any global mode.

Application-owned animations can use the same native decisions and observe
runtime preference changes through the process-wide policy:

```python
policy = fluentqt.motion_policy()
policy.modeChanged.connect(lambda mode: print("motion mode:", mode))
duration = policy.resolvedDuration(250)  # 50 ms in Reduced mode
animate_spinner = policy.shouldAnimate(
    True, fluentqt.MotionKind.Continuous
)
```

## Public API and ownership

Python uses the native FluentQt C++ implementation, adding explicit Python
helpers only where C++ mixins or ownership overloads would be ambiguous:

- Models, selection models, and delegates stay managed by your app; keep a Python
  reference if they have no Qt parent.
- Container widgets provide explicit `Borrowed`, `Owned`, `Reparented`, and `take*`
  methods (such as `setBorrowedEditor()` / `setOwnedContentWidget()`).
- Internal Qt-owned children (such as title bars and scroll bars) must not be
  deleted or reparented by user code.
- `FluentWidget`, `bind()`, `StateGroup`, and `AnchorLayout` provide safe
  Python equivalents of C++ mixins without exposing internal registries.

The [compatibility and coverage guide](ROADMAP.md) records intentional boundary
decisions. Exact exports remain defined by `api-manifest.json`.

## Maintainer references

| Change | Required reference |
|---|---|
| Add or change a public Python API | [API compatibility policy](API_COMPATIBILITY.md) |
| Change a supported runtime or wheel | [Compatibility and coverage](ROADMAP.md) and [`wheel-matrix.json`](wheel-matrix.json) |
| Change Linux wheel repair | [manylinux policy](MANYLINUX.md) |
| Build or publish a release bundle | [Publishing runbook](PUBLISHING.md) |
| Change Gallery packaging | [Gallery package boundary](gallery/README.md) |

Run the policy gate after changing the public surface:

```bash
python3 bindings/pyside6/tools/verify_api_policy.py
```

Do not copy generated API counts or wheel totals into prose. Update the owning
manifest or matrix and let its validators describe the current state.
