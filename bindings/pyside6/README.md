# PySide6 bindings

The optional binding target supports PySide6, Shiboken6, and Qt 6.2 or newer.
The three Qt for Python packages and the C++ Qt SDK must use the same version;
mixing Qt runtimes in one process is unsupported.
This does not change the C++ library's Qt 5.15 support; PySide2/Shiboken2 are
outside the scope of this binding target.
See the compatibility roadmap
([English](ROADMAP.md) · [简体中文](ROADMAP.zh-CN.md)) for the risk-ordered
component, ownership, platform-validation, and wheel-release milestones.

Qt/PySide 6.2.4 with Python 3.10 on Ubuntu 22.04 and Windows Server 2022 is the
minimum CI baseline. macOS arm64 additionally uses the repository's Qt/PySide
6.9.3 release toolchain with Python 3.11 because Shiboken 6.2's embedded parser
cannot parse the current Xcode SDK. This does not raise the binding's Qt 6.2
minimum. The build intentionally invokes the Shiboken generator directly
instead of requiring newer Shiboken CMake helpers, so the same path works
across the 6.2+ release line. Use Python 3.10 for the 6.2.4 toolchain:

```bash
python3.10 -m venv .venv-pyside
.venv-pyside/bin/python -m pip install PySide6==6.2.4
.venv-pyside/bin/python -m pip install \
  --index-url https://download.qt.io/official_releases/QtForPython/ \
  shiboken6_generator==6.2.4
```

Configure with a matching Qt SDK and the same Python interpreter:

```bash
cmake -S . -B build/pyside6 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.2.4 \
  -DPython_EXECUTABLE=/path/to/.venv-pyside/bin/python \
  -DFLUENT_QT_BUILD_PYSIDE6_BINDINGS=ON \
  -DFLUENT_QT_BUILD_EXAMPLES=OFF \
  -DFLUENT_QT_INSTALL=OFF \
  -DBUILD_TESTING=ON
cmake --build build/pyside6 --target _fluentqt --parallel
ctest --test-dir build/pyside6 -L '^pyside$' --output-on-failure
```

For a Qt 6.9 macOS wheel, also configure with
`-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`. Qt 6.9 supports macOS 12 and newer; this
prevents the build host's newer macOS version from unnecessarily narrowing the
wheel's deployment target. The macOS CI lane enforces this value on the
generated extension.

Build a platform- and Python-specific wheel:

```bash
cmake --build build/pyside6 \
  --target fluentqt_pyside6_wheel \
  --parallel
```

The wheel is written to `build/pyside6/wheelhouse/`. It contains FluentQt and
the Python modules, but does not bundle Qt, PySide6, or Shiboken6. Its metadata
pins the exact matching PySide6 runtime distribution (`PySide6` for 6.2.x,
`PySide6-Essentials` for 6.3+) and Shiboken6 version. The native extension uses
relative runtime paths so installation is not tied to the build virtual
environment.

Validate the wheel in a fresh environment without `PYTHONPATH`:

```bash
python3.10 -m venv .venv-fluentqt-wheel
.venv-fluentqt-wheel/bin/python -m pip install \
  build/pyside6/wheelhouse/fluentqt-*.whl
QT_QPA_PLATFORM=offscreen \
FLUENTQT_EXPECTED_VERSION="$(
  .venv-fluentqt-wheel/bin/python -c \
    'from importlib.metadata import version; print(version("FluentQt"))'
)" \
  .venv-fluentqt-wheel/bin/python \
  bindings/pyside6/tests/test_wheel_smoke.py
.venv-fluentqt-wheel/bin/python -m pip check
```

Run the example from the build tree:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world.py
```

Run the controls, progress, and theme-switching example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/controls.py
```

Run the interactive compatibility acceptance window:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/compatibility_showcase.py
```

The window prints the loaded package/native-extension paths and exact
FluentQt/PySide6/Qt versions. Use its controls to review Light/Dark,
Fluent/Material/macOS, accent switching, signals, values, and press-and-hold
behavior. The `Animate shimmer` checkbox exercises the Shimmer timer, while
the deterministic snapshot keeps it at a fixed progress. Save the same view
for review or CI:

```bash
PYTHONPATH=build/pyside6/python \
QT_QPA_PLATFORM=offscreen \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/compatibility_showcase.py \
  --snapshot build/pyside6/pyside6-compatibility-showcase.png
```

The current binding phase exports `Button`, `CheckBox`, `HyperlinkButton`,
`RadioButton`, `RepeatButton`, `Slider`, `ToggleButton`, `ToggleSwitch`,
`Divider`, `Label`, `LineEdit`, `NumberBox`, `PasswordBox`, `ProgressBar`,
`ProgressRing`, `InfoBadge`, `Shimmer`, and `Window`, together with their enums
and backdrop value types. It also supports Light/Dark mode,
Fluent/Material/macOS style presets, in-memory accent overrides, typography
scaling, Qt properties and signals, Python subclassing, and explicit Window
child-parent tracking.

```python
from PySide6.QtGui import QColor
import fluentqt

fluentqt.set_theme(fluentqt.Theme.Dark)
fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
fluentqt.set_accent_color(QColor("#7f52ff"))
fluentqt.set_font_scale(1.1)
```

The Python Hello World mirrors the complete C++ example: both use the Fluent
window, application font, content layout, and accent button.
Importing `fluentqt` has no application-creation or theme side effects.
`Window.nativeEvent()` follows PySide's safe two-argument override contract:
Python returns a `(handled, result)` tuple and never receives the result pointer.
`api-manifest.json` records the required public surface and is checked by the
binding tests so generator upgrades cannot silently remove required APIs.
The private native extension preserves the C++ namespace hierarchy to work
across Shiboken releases; the `fluentqt` package and its category modules
re-export the stable public Python API shown above.

This phase intentionally does not publish the C++ `FluentElement` or
`QMLPlus` mixins. Python controls therefore do not expose `anchors()`, `bind()`,
or `setState()`; use Qt layouts, Python signal handlers, and QObject properties.
`Shimmer` exposes its built-in templates, active/animation state, duration, and
progress. Its `Custom` template value is reserved for now:
`ShimmerPainter::Element` collections and their getter/mutator API remain C++
only until a stable Python value-type contract is designed.
The wheel target is validated on Linux x64 and Windows x64 with Qt 6.2.4, plus
macOS arm64 with Qt 6.9.3. Clean-environment smoke tests verify that the loaded
Qt, PySide6, and Shiboken6 libraries come from the installed wheel environment.
A complete wheel release/publishing matrix has not yet been added.

Passing the build-tree tests proves the declared API contract; passing the
clean-wheel smoke proves installation/runtime isolation; the interactive
showcase proves the visible controls and signal-driven behavior. Native
Window/TitleBar/backdrop behavior still requires a real desktop and cannot be
accepted solely from the offscreen snapshot.

Very old Shiboken generators embed an older Clang parser. If Shiboken 6.2
cannot parse the C++ standard-library headers from a much newer host compiler,
use the Ubuntu 22.04 baseline or a compiler/SDK contemporary with Qt 6.2. This
does not require raising FluentQt's Qt minimum version.
