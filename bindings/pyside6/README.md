# PySide6 bindings

> **Status:** Current build, usage, and distribution guide

[Documentation home](../../docs/README.md) · [Contents](../../docs/SUMMARY.md)

FluentQt exposes the native C++ widget library to Python through one Shiboken6
extension. Use this page to choose the right entry point; detailed compatibility,
packaging, and publication rules live in the linked documents instead of being
repeated here.

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

Python uses the same native FluentQt implementation rather than a second widget
library. The package exposes Python-shaped adapters where raw C++ mixins or
runtime ownership overloads would produce unsafe Python semantics.

- Application models, selection models, and delegates remain caller-owned;
  the facade retains their wrappers while installed.
- Hosted widgets use explicit owned, borrowed, reparented, and take methods
  where the C++ runtime overload would be ambiguous in Python.
- Qt-owned children such as title bars and internal scroll bars are borrowed
  and must not be deleted or reparented by callers.
- `FluentWidget`, `bind()`, `StateGroup`, and `AnchorLayout` expose supported
  authoring capabilities without publishing implementation-only mixins or
  mutable registries.

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
