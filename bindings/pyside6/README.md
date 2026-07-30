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

Run the dedicated native CalendarView acceptance example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/calendar_view_showcase.py
```

Pass `--snapshot build/pyside6/pyside6-calendar-view-showcase.png` to render
the same window without leaving an interactive process running.

Run the AnnotatedScrollBar label and linked-ScrollView example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/annotated_scroll_bar_showcase.py
```

Pass `--snapshot build/pyside6/pyside6-annotated-scroll-bar-showcase.png` to
render its initial linked state without leaving an interactive process running.

Run the Accordion ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/accordion_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-accordion-ownership.png` to render its
initial owned, borrowed, and reparented item state without leaving an
interactive process running.

Run the StackView navigation and ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/stack_view_navigation.py
```

Pass `--snapshot build/pyside6/pyside6-stack-view-navigation.png` to render
the initial native page stack without leaving an interactive process running.

Run the dedicated native ColorPicker acceptance example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/color_picker_showcase.py
```

Pass `--snapshot build/pyside6/pyside6-color-picker-showcase.png` to render the
same window without leaving an interactive process running.

Run the ScrollView ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/scroll_view_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-scroll-view-ownership.png` to save its
initial owned-content state without opening an interactive window.

Run the interactive compatibility acceptance window:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/compatibility_showcase.py
```

The window prints the loaded package/native-extension paths and exact
FluentQt/PySide6/Qt versions. Use its controls to review Light/Dark,
Fluent/Material/macOS, accent switching, signals, values, multiline text, and
press-and-hold behavior. The `Animate shimmer` checkbox exercises the Shimmer
timer, while the deterministic snapshot keeps it at a fixed progress. Save the
same view for review or CI:

```bash
PYTHONPATH=build/pyside6/python \
QT_QPA_PLATFORM=offscreen \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/compatibility_showcase.py \
  --snapshot build/pyside6/pyside6-compatibility-showcase.png
```

The current binding phase exports `Accordion`, `AnnotatedScrollBar`,
`AnnotatedScrollBarLabel`, `Avatar`, `Button`, `CalendarView`, `CheckBox`,
`ColorPicker`, `CompoundButton`, `HyperlinkButton`, `RadioButton`,
`RatingControl`, `RepeatButton`, `Slider`, `ToggleButton`, `ToggleSwitch`,
`Card`, `Divider`, `Expander`, `FontIcon`, `Label`, `LineEdit`, `NumberBox`,
`PasswordBox`, `TextEdit`, `ProgressBar`, `ProgressRing`, `InfoBadge`,
`InfoBar`, `Shimmer`, `PipsPager`, `ScrollBar`, `ScrollView`, `StackView`, and
`Window`,
together with their enums and value types. It also supports Light/Dark mode,
Fluent/Material/macOS style presets, in-memory accent overrides, typography
scaling, Qt properties and signals, Python subclassing, and explicit Window
child-parent tracking.

```python
from PySide6.QtGui import QColor
import fluentqt

settings_icon = fluentqt.FontIcon("ic_fluent_settings_20_regular")
settings_icon.setIconSize(20)
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
`AnnotatedScrollBarLabel` is a mutable, unhashable Python value type with
`text`, `offset`, and `detailText` fields. The category module normalizes value
equality across Shiboken versions. Static detail text, label filtering,
signals, and two-way `ScrollView` synchronization use the native implementation.
`connectToScrollView()` is borrowed: it neither reparents nor keeps the view
alive, and `connectedScrollView()` becomes `None` when that view is destroyed.
The C++ `std::function<QString(int)>` detail provider is deliberately absent
until a synchronous Python-callable adapter can preserve the same semantics on
Shiboken 6.2+; use each label's `detailText` in the current API.
The public `ScrollView` facade exposes separate, statically verifiable
ownership methods. `setContentWidget()`/`setWidget()` and
`setOwnedContentWidget()` delete content with the host;
`setBorrowedContentWidget()` detaches content when it leaves the host; and
`setReparentedContentWidget()` restores the QWidget parent present at adoption.
`takeContentWidget()`/`takeWidget()` always return a parentless child to Python.
Changing the mode of the currently hosted object requires taking and
reinstalling it first. The runtime ownership overload remains private.

Normal Python garbage collection and Qt parent destruction form the supported
host-lifecycle contract. `Shiboken.ownedByPython()` while a widget is hosted is
an implementation detail that differs across Shiboken releases, and repeated
`Shiboken.delete(host)` is not used as a compatibility requirement: PySide6
6.2.4 on Windows can fail while reclaiming even a plain `QScrollArea` wrapper
that way. Tests instead verify wrapper identity, validity, parent restoration,
and natural host collection; explicit take still verifies ownership returned
to Python.
`Expander` uses the same audited facade, except its C++-compatible
`setContentWidget()` default is Borrowed. Its explicit Owned, Borrowed, and
Reparented methods and `takeContentWidget()` follow the same lifecycle rules.
The internal header button is deliberately not part of the Python API.
`Accordion` composes those native `Expander` instances. `addItem()` and
`insertItem()` retain the C++ Borrowed default; fixed `add*Item()` and
`insert*Item()` variants publish Owned, Borrowed, and Reparented policies.
The facade retains Python subclass wrappers while items are hosted, and
`takeItem()` always returns a parentless Python-owned item. Runtime-dependent
ownership overloads remain private.
`StackView` keeps native push/pop/replace transitions, page status signals,
keyboard back navigation, and indexed stack queries. Plain `push()`,
`replace()`, and `setInitialItem()` preserve the C++ Owned default; fixed
Owned, Borrowed, and Reparented variants make every other lifetime explicit.
The facade retains Python page subclasses and Reparented restore targets until
the native transition finishes. Direct inherited `QStackedWidget`
`addWidget()`/`insertWidget()`/`removeWidget()` calls are blocked because they
bypass navigation ownership records; use the StackView navigation methods.
The generated native `setCurrentWidget(QWidget*)` wrapper is also removed
because Shiboken's name heuristic reparents its argument. The facade provides
the same operation through the safe index-only native setter.
`InfoBar` retains its action wrapper while the native widget is hosted. Replacing
or clearing the action releases the previous widget as parentless Python-owned
content; deleting the InfoBar deletes its currently hosted action.
`takeActionWidget()` makes the release explicit and preserves wrapper identity.
Normal external action destruction is validated through `deleteLater()` plus
Qt deferred-delete delivery. Direct `Shiboken.delete()` on a still-parented
Python subclass can fast-fail on PySide6 6.2.4/Windows and is not a supported
lifecycle contract.
Shiboken 6.2 silently omits `TextEdit.verticalScrollBar()` because its return
type crosses two flattened C++ namespaces. The Python category module supplies
the same method by locating TextEdit's existing Qt-owned Fluent `ScrollBar`;
the method does not create, reparent, or transfer ownership of that child.
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
