# PySide6 bindings

The optional Qt 6-only binding target has a minimum source/build baseline of
CPython 3.10 plus PySide6, Shiboken6, and Qt 6.2.4. The three Qt for Python
packages and the C++ Qt SDK must use the same version; mixing Qt runtimes in
one process is unsupported.
This does not change the C++ library's Qt 5.15 support; PySide2/Shiboken2 are
outside the scope of this binding target.
See the compatibility roadmap
([English](ROADMAP.md) · [简体中文](ROADMAP.zh-CN.md)) for the risk-ordered
component, ownership, platform-validation, and wheel-release milestones.
The [PySide6 API compatibility policy](API_COMPATIBILITY.md) defines the
package/API version contract, SemVer boundaries, and mandatory deprecation
ledger.
The [manylinux policy](MANYLINUX.md) defines Linux build images, repair
exclusions, publish tags, and required audit evidence.

The Python deliverables are intentionally split:

- `FluentQt` / `import fluentqt` is the reusable UILib and native extension.
- `FluentQt-Gallery` / `import fluentqt_gallery` is a standalone pure-Python
  example application that depends on the exact same `FluentQt` version.

Compatibility is deliberately tiered:

- Linux/Windows x64 CI blocks regressions against the CPython 3.10 plus
  Qt/PySide/Shiboken 6.2.4 minimum. Those compatibility artifacts are never
  published.
- Official prebuilt wheel support is limited to the exact CPython, platform,
  architecture, and runtime combinations in
  [`wheel-matrix.json`](wheel-matrix.json).
- Other matching Qt 6.2.4+ toolchains may build from source, but are not an
  official binary promise until they enter that matrix.

Official `FluentQt` and `FluentQt-Gallery` wheels declare
`Requires-Python: >=3.11,<3.14`. The non-published Qt 6.2.4 compatibility
artifacts declare `>=3.10,<3.11` only so their clean-install gate remains
meaningful. PySide2, Qt 5 Python, PyPy, and 32-bit x86 are out of scope. The
standalone Gallery always follows the reusable UILib's release range.

The Gallery stays in this repository so its C++/Python parity contract can be
tested together with the library, but it is not installed into the core
package or copied into the library-only source archive. See
[gallery/README.md](gallery/README.md) for the package boundary.

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
  -DFLUENT_QT_BUILD_PYSIDE6_GALLERY=ON \
  -DFLUENT_QT_BUILD_EXAMPLES=OFF \
  -DFLUENT_QT_INSTALL=OFF \
  -DBUILD_TESTING=ON
cmake --build build/pyside6 --target fluentqt_pyside6_stubs --parallel
ctest --test-dir build/pyside6 -L '^pyside$' --output-on-failure
```

`fluentqt_pyside6_stubs` builds the native extension and generates the package
facade plus `_fluentqt.pyi` from Shiboken signature metadata. The committed
`api-manifest.json` is checked against those stubs, so a missing class, enum,
function, or required method fails before wheel creation. The generated stubs
retain Python 3.10 syntax compatibility and are installed beside `py.typed`.

For a Qt 6.9 macOS wheel, also configure with
`-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`. Qt 6.9 supports macOS 12 and newer; this
prevents the build host's newer macOS version from unnecessarily narrowing the
wheel's deployment target. The macOS CI lane enforces this value on the
generated extension.

Build the platform-specific UILib wheel and the platform-independent Gallery
wheel:

```bash
cmake --build build/pyside6 \
  --target fluentqt_pyside6_wheels \
  --parallel
```

The UILib wheel is written to `build/pyside6/wheelhouse/`; the Gallery wheel is
written to `build/pyside6/gallery-wheelhouse/`. The UILib wheel contains the
native extension and typed Python facade, but does not bundle Qt, PySide6,
Shiboken6, or the Gallery. Its metadata pins the exact matching PySide6 runtime
distribution (`PySide6` for 6.2.x, `PySide6-Essentials` for 6.3+) and
Shiboken6 version. The Gallery wheel is `py3-none-any` and pins the exact
`FluentQt` version. The native extension uses relative runtime paths so
installation is not tied to the build virtual environment.

At runtime, `fluentqt.__version__` matches the full wheel and native FluentQt
version. `fluentqt.__api_version__` identifies the compatible `MAJOR.MINOR`
Python API line. Both are typed public exports and are checked against
`api-manifest.json` during stub generation and clean-wheel smoke testing.

Validate the wheel in a fresh environment without `PYTHONPATH`:

```bash
python3.10 -m venv .venv-fluentqt-wheel
.venv-fluentqt-wheel/bin/python -m pip install \
  build/pyside6/wheelhouse/fluentqt-*.whl \
  build/pyside6/gallery-wheelhouse/fluentqt_gallery-*.whl
QT_QPA_PLATFORM=offscreen \
FLUENTQT_EXPECTED_VERSION="$(
  .venv-fluentqt-wheel/bin/python -c \
    'from importlib.metadata import version; print(version("FluentQt"))'
)" \
  .venv-fluentqt-wheel/bin/python \
  bindings/pyside6/tests/test_wheel_smoke.py
QT_QPA_PLATFORM=offscreen \
FLUENTQT_EXPECTED_VERSION="$(
  .venv-fluentqt-wheel/bin/python -c \
    'from importlib.metadata import version; print(version("FluentQt"))'
)" \
  .venv-fluentqt-wheel/bin/python \
  bindings/pyside6/gallery/tests/test_gallery_wheel_smoke.py
.venv-fluentqt-wheel/bin/python -m pip check
.venv-fluentqt-wheel/bin/python -m pip install mypy==2.3.0
env -u PYTHONPATH \
  .venv-fluentqt-wheel/bin/python -m mypy \
  --strict \
  --no-incremental \
  bindings/pyside6/tests/test_typecheck_smoke.py
```

The type-check smoke covers root and category imports, theme return types,
native widget methods, `Window.titleBar()`, and backdrop value types using the
installed wheel rather than the source tree.

## Python Gallery

The standalone `FluentQt-Gallery` wheel dogfoods only the public `fluentqt`
package. The native C++ Gallery catalogs are canonical: the build
generates a contract locking their 12 categories, 88 ordered routes, 67
component pages, and 199 SampleCards. Those routed component types plus 20
embedded support types cover all 87 classes and value/support types in
`api-manifest.json`. Every SampleCard builds its live public-API preview from
an exact executable `preview_source`, while the visible code block shows a
concise public-API teaching snippet with the same canonical operations; a
generic fallback preview or semantic drift is an acceptance failure.

This is not a catalog-only test harness. The Python app mirrors the native C++
Gallery's primary visual contracts: a 42 px custom title bar with centered
search, 260/48 px responsive side navigation, the 390 px Home hero, the same
packaged Home tiles and 74 control images, responsive component/category grids,
and the native Overview, Use, Live examples, Source code, and Category page
sections. Geometry tests reject regressions back to an expanded raw tree or
prototype button list. Native snapshots composite the transparent DWM/Mica
area over Fluent's neutral fallback canvas so saved PNG evidence stays readable
without changing the live platform backdrop.

Launch the Gallery after installing both wheels:

```bash
.venv-fluentqt-wheel/bin/python -m fluentqt_gallery
```

The Python Gallery uses its own application and single-instance identity. It
can run beside the native C++ Gallery for visual comparison, while repeated
Python launches still reactivate the existing Python window. Its persisted
settings are isolated from the native Gallery as well.

Run its deterministic headless acceptance mode from either an installed wheel
or the build-tree package:

```bash
QT_QPA_PLATFORM=offscreen \
  .venv-fluentqt-wheel/bin/python -m fluentqt_gallery \
  --verify-catalog \
  --walk-routes \
  --route home \
  --snapshot build/pyside6/pyside6-gallery.png \
  --report build/pyside6/pyside6-gallery.json
```

`--verify-catalog` constructs all 199 previews and executes every displayed
snippet, while `--walk-routes` validates all 88 native-ordered routes. The
contract generator and catalog tests reject C++/Python route or SampleCard
drift, missing or extra manifest types, and fallback previews. The
Gallery-wheel smoke repeats the route walk and verifies the artwork and exact
core-package dependency without a source-tree `PYTHONPATH`.

Run the example from the build tree:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python bindings/pyside6/examples/hello_world/main.py
```

Run the Window, TitleBar, and backdrop acceptance window on the host's native
Qt platform plugin:

```bash
PYTHONPATH=build/pyside6/python \
QT_QPA_PLATFORM=cocoa \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/window_chrome.py \
  --verify-native \
  --snapshot build/pyside6/pyside6-window-chrome.png \
  --report build/pyside6/pyside6-window-chrome.json
```

Use `QT_QPA_PLATFORM=windows` on Windows and `QT_QPA_PLATFORM=xcb` or
`wayland` on Linux. `--verify-native` rejects `offscreen` and `minimal`, then
checks the native handle, Qt-owned TitleBar, content lifetime, chrome geometry,
resize propagation, and typed Solid/Mica/Acrylic state. Add
`--require-platform-backdrop` only on a desktop where an OS/compositor material
is expected; the normal command accepts FluentQt's deterministic painted
fallback. Without `--verify-native`, the same example supports an offscreen
snapshot for layout review, but that is not platform-window evidence.

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

Run the three native date/time picker and same-window popup example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/date_time_pickers.py
```

Pass `--snapshot build/pyside6/pyside6-date-time-pickers.png` to render the
three entry surfaces together with an open `CalendarDatePicker` popup.

Run the native AutoSuggestBox and same-window suggestion Flyout example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/auto_suggest_box.py
```

Pass `--snapshot build/pyside6/pyside6-auto-suggest-box.png` to render the
search input with its keyboard-opened suggestion Flyout.

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

Run the ListView Python model and delegate example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/list_view_model.py
```

Pass `--snapshot build/pyside6/pyside6-list-view-model.png` to render the
native Fluent list consuming a Python `QAbstractListModel` and
`QStyledItemDelegate`. The interactive actions exercise insert, update,
remove, reset, and selection notifications.

Run the GridView model, delegate, selection, and reordering example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/grid_view_model.py
```

Pass `--snapshot build/pyside6/pyside6-grid-view-model.png` to render the
native Fluent grid consuming a PySide `QStandardItemModel` and Python
`QStyledItemDelegate`. The interactive view supports multiple selection and
native group drag reordering.

Run the FlowView adaptive model and delegate example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/flow_view_model.py
```

Pass `--snapshot build/pyside6/pyside6-flow-view-model.png` to render the
native wrapping view consuming per-item `QSize` values from a Python
`QAbstractListModel` and dispatching paint back to a Python delegate.

Run the FlipView page navigation and ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/flip_view_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-flip-view-ownership.png` to render the
native carousel with Owned, Borrowed, and Reparented pages without leaving an
interactive process running.

Run the SplitView pane resizing and ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/split_view_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-split-view-ownership.png` to render the
native resizable panes with Owned, Borrowed, and Reparented policies without
leaving an interactive process running.

Run the DrawerView same-window overlay and content ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/drawer_view_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-drawer-view-ownership.png` to render the
opened native drawer, dim scrim, and its Fluent content. The interactive mode
also exercises outside-press, Escape, animation, and close-policy behavior.

Run the Popup same-window overlay and QWidget dependency example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/popup_overlay.py
```

Pass `--snapshot build/pyside6/pyside6-popup-overlay.png` to render the opened
native Popup and dim scrim. Interactive mode verifies anchor-relative
placement, Escape/outside dismissal, focus return, and the toolbar passthrough.

Run the Flyout anchor placement and light-dismiss example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/flyout_overlay.py
```

Pass `--snapshot build/pyside6/pyside6-flyout-overlay.png` to render a native
Flyout using Auto placement near the bottom edge. Interactive mode exposes
Top, Bottom, Left, Right, and Auto anchors while keeping the surface inside the
owning Window.

Run the CoachMark and TeachingTip target/lifecycle example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/guidance_overlays.py
```

Pass `--snapshot build/pyside6/pyside6-guidance-overlays.png` to render both
native guidance surfaces above caller-owned Python targets. Interactive mode
demonstrates retargeting, tails, same-window placement, semantic TeachingTip
close reasons, and Qt-owned content hosts.

Run the managed Toast and attached ToolTip example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/status_overlays.py
```

Pass `--snapshot build/pyside6/pyside6-status-overlays.png` to render the
same-window Toasts and composite the separate native ToolTip surface into one
deterministic acceptance image. Interactive mode demonstrates target-owned
tooltip attachment, managed toast stacking, keyed updates, borrowed QAction
retention, and host-window lifetime.

Run the ContentDialog result and hosted-content example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/content_dialog.py
```

Pass `--snapshot build/pyside6/pyside6-content-dialog.png` to render the
same-window smoke scrim, hosted Python content, and three native command
buttons. Interactive mode reports the primary, secondary, and close results.

Run the ComboBox Python-model and same-window dropdown example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/combo_box_dropdown.py
```

Pass `--snapshot build/pyside6/pyside6-combo-box-dropdown.png` to render an
opened native dropdown backed by a Python `QAbstractListModel`, plus an
editable ComboBox using its Fluent line editor.

Run the DropDownButton, SplitButton, ToggleSplitButton, and FluentMenu example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/menu_buttons.py
```

Pass `--snapshot build/pyside6/pyside6-menu-buttons.png` to render all three
native menu-button variants together with an opened `FluentMenu`. The example
also demonstrates primary/secondary command separation, toggle state, and
caller-owned menu lifetime.

Run the FluentMenuBar, CommandBar, and CommandBarFlyout example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/command_surfaces.py
```

Pass `--snapshot build/pyside6/pyside6-command-surfaces.png` to render the
native menu bar and inline command bar together with an opened same-window
command flyout. The example shares caller-owned `QAction` objects between the
two command surfaces and demonstrates that Python wrapper retention does not
change QObject ownership.

Run the TreeView hierarchy, delegate, selection, and reordering example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/tree_view_model.py
```

Pass `--snapshot build/pyside6/pyside6-tree-view-model.png` to render the
native Fluent tree consuming a hierarchical PySide `QStandardItemModel` and
Python `QStyledItemDelegate`. The interactive view supports expansion,
selection motion, child insertion, and file-manager-style drag reordering.

Run the StackView navigation and ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/stack_view_navigation.py
```

Pass `--snapshot build/pyside6/pyside6-stack-view-navigation.png` to render
the initial native page stack without leaving an interactive process running.

Run the NavigationView responsive shell and ownership example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/navigation_view_ownership.py
```

Pass `--snapshot build/pyside6/pyside6-navigation-view-ownership.png` to
render the native side-navigation shell with Owned, Borrowed, and Reparented
pages and chrome slots. The interactive example also switches between Left
and Top display modes.

Run the TabView metadata and page-composition example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/tab_view_navigation.py
```

Pass `--snapshot build/pyside6/pyside6-tab-view-navigation.png` to render the
native tab strip connected to a regular PySide `QStackedWidget`.

Run the Breadcrumb metadata and native overflow example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/breadcrumb_navigation.py
```

Pass `--snapshot build/pyside6/pyside6-breadcrumb-navigation.png` to render
both the complete route and a narrow middle-overflow route. The example also
shows QVariant-compatible Python metadata returned by activation signals.

Run the SelectorBar/Pivot metadata and page-composition example:

```bash
PYTHONPATH=build/pyside6/python \
  .venv-pyside/bin/python \
  bindings/pyside6/examples/selector_pivot_navigation.py
```

Pass `--snapshot build/pyside6/pyside6-selector-pivot-navigation.png` to
render a native SelectorBar connected to a caller-owned PySide page stack,
Pivot filtering, and MoreButton overflow.

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

The current binding phase exports 87 required public classes, value types, and
embedded support types across every component category in the
[coverage ledger](ROADMAP.md#public-api-coverage-ledger), including `Window`,
`TitleBar`, and the backdrop value types. `api-manifest.json` is the executable
source of truth for exact names and required methods. The package also supports
Light/Dark mode,
Fluent/Material/macOS style presets, in-memory accent overrides, typography
scaling, Qt properties and signals, Python subclassing, and explicit hosted
widget ownership.

```python
from PySide6.QtGui import QColor
import fluentqt

settings_icon = fluentqt.FontIcon(fluentqt.Typography.Icons.Settings)
settings_icon.setIconSize(fluentqt.Typography.IconSize.Large)
fluentqt.set_theme(fluentqt.Theme.Dark)
fluentqt.apply_style_theme(fluentqt.StyleTheme.Material)
fluentqt.set_accent_color(QColor("#7f52ff"))
fluentqt.set_font_scale(1.1)
```

The single Python Hello World in
[`examples/hello_world`](examples/hello_world/) mirrors the C++ example: both
use the Fluent window, application font, content layout, and accent button.
Importing `fluentqt` has no application-creation or theme side effects.
`Window.nativeEvent()` follows PySide's safe two-argument override contract:
Python returns a `(handled, result)` tuple and never receives the result pointer.
`Window.titleBar()` returns the existing Qt-owned `TitleBar`; Python must not
delete or reparent it. `TitleBar.setContentWidget()` adopts its content, releases
the previous child as a parentless Python-owned widget, and destroys the current
child with the TitleBar. Component implementation-only theme hooks stay
private; Python-authored `FluentWidget` subclasses use the supported
`on_theme_updated()` hook described below.
`api-manifest.json` records the required public surface and is checked by the
binding tests so generator upgrades cannot silently remove required APIs.
The private native extension preserves the C++ namespace hierarchy to work
across Shiboken releases; the `fluentqt` package and its category modules
re-export the stable public Python API shown above.

The raw C++ `FluentElement` and `QMLPlus` multiple-inheritance mixins remain
implementation details because wrapping non-`QObject` mixins directly would
produce fragile Python MRO, object-identity, and lifetime semantics. Their UI
authoring capabilities are public through Python-shaped adapters instead:

```python
import fluentqt


class TokenCard(fluentqt.FluentWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.on_theme_updated()

    def on_theme_updated(self):
        colors = self.theme_tokens().colors
        self.setStyleSheet(
            "background: {0}; color: {1}".format(
                colors.bgLayer.name(),
                colors.textPrimary.name(),
            )
        )


fluentqt.bind(slider, "value", progress, "value")

states = fluentqt.StateGroup()
states.add("busy", {status: {"text": "Working...", "enabled": False}})
states.set("busy")

layout = fluentqt.AnchorLayout(panel)
layout.addWidget(action, fluentqt.anchors(top_right=(panel, 12)))
```

`FluentWidget` provides effective colors, typography, radius, spacing, motion,
material, elevation, breakpoint and backdrop tokens, design language, and a
Python-overridable theme hook. Its `ThemeTokens` result supports both typed
attribute access (`tokens.spacing.border.focused`) and the original mapping
syntax. Read-only `Typography.Icons`, `Typography.IconSize`, `Spacing`, and
`CornerRadius` facades mirror C++ design namespaces without publishing the
mutable registry. `bind()` uses the native `PropertyBinder`;
`StateGroup` uses the native QMLPlus state engine with default restoration;
and `AnchorLayout`/`anchors()` use the native anchor solver. This is one shared
C++ implementation, not a second Python layout or theme engine. Mutable
`ThemeRegistry` internals and overlay implementation helpers stay private.
An inherited Qt method with a similar name, such as
`QAbstractItemView.setState()`, keeps its Qt meaning and is unrelated to
`StateGroup`.

`Shimmer` exposes built-in templates plus the Python value facade
`Shimmer.Shape`/`Shimmer.Element`; `elements()`, `setElements()`, and
`clearElements()` drive the native custom-element implementation without
publishing `ShimmerPainter` internals.
`AnnotatedScrollBarLabel` is a mutable, unhashable Python value type with
`text`, `offset`, and `detailText` fields. The category module normalizes value
equality across Shiboken versions. Static detail text, label filtering,
signals, and two-way `ScrollView` synchronization use the native implementation.
`connectToScrollView()` is borrowed: it neither reparents nor keeps the view
alive, and `connectedScrollView()` becomes `None` when that view is destroyed.
The raw C++ `std::function<QString(int)>` overload stays private.
`setDetailLabelProvider(callable)` exposes the same synchronous behavior using
the native request signal and a short-lived native result; the facade validates
the callable immediately and retains it without asking Shiboken 6.2 to convert
`std::function`.
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
`ListView` consumes ordinary PySide `QAbstractItemModel`,
`QItemSelectionModel`, and `QAbstractItemDelegate` objects. They remain
caller-owned, while the facade retains their Python wrappers for as long as
they are installed and releases them when replaced or when the view is
collected. Python model notifications, delegate virtual methods, selection,
reset, insertion/removal, and persistent indexes use the native C++ view.
`SelectionMode` is published through a Qt 6.2-compatible binding adapter.
Its C++ `None` member is spelled `SelectionMode.None_` in Python to avoid the
Python keyword.
Custom header/footer QWidget hosting and the section toggle/synchronous C++
`setSectionKeyFunction(std::function<...>)` callback remain private until they
have explicit Python ownership and callable contracts; use `headerText`,
`footerText`, and external composition in the current API.
`GridView` follows the same caller-owned model, selection-model, and delegate
lifetime contract and reuses the stable `SelectionMode` adapter. Its cell size,
spacing, column limit, header/placeholder text, selection, scroll behavior, and
reorder signals remain native. The internal Fluent scroll bar is borrowed and
never transferred to Python. Native drag reordering currently operates on
`QStandardItemModel`; arbitrary `QAbstractItemModel` subclasses still support
display, selection, insertion/removal/reset notifications, and Python virtual
delegate dispatch but are not advertised as reorderable.
`TreeView` extends the same caller-owned model, selection-model, delegate, and
borrowed-scrollbar contracts to hierarchical indexes. Expansion/collapse,
hierarchy-aware selection motion, check-state propagation, persistent indexes,
and reorder signals stay in the native control. Native drag reordering is
likewise limited to `QStandardItemModel`. The implementation-oriented
`SelectionIndicatorStyle` struct remains private; Python uses the stable
indicator visibility, inset, and height methods.
`FlowView` applies that caller-owned model, selection-model, delegate, and
borrowed-scrollbar contract to variable-size wrapping geometry. Python models
may provide per-item `QSize` values through `itemSizeRole`; Python delegate
`sizeHint()` and `paint()` virtuals are dispatched by the native view. Model
insert/remove/reset notifications, persistent indexes, selection, hit testing,
keyboard navigation, and scroll behavior remain native. Drag reordering is
advertised only for `QStandardItemModel`; arbitrary Python
`QAbstractItemModel` subclasses keep the rest of the contract without a reorder
guarantee.
`FlipView` preserves its C++ host-owned default for plain `addPage()` and
`insertPage()`, while explicit Owned, Borrowed, and Reparented variants define
every other page lifetime. `removePage()` applies the recorded policy:
Owned pages are destroyed, Borrowed pages become parentless, and Reparented
pages return to their original QWidget parent. `takePage()` always transfers a
parentless Python-owned page. The facade retains Python page subclasses and
restore targets while hosted, rejects duplicate/ancestor insertion, and
removes its records when a page is destroyed externally. The legacy C++
transfer-style overloads and runtime ownership argument remain private.
`DrawerView` preserves its C++ Borrowed default for plain `setContentWidget()`
and publishes fixed Owned, Borrowed, and Reparented content methods. The Python
facade retains hosted subclasses and restore parents, rejects host/ancestor
cycles, requires an explicit take before changing the current widget's policy,
and returns parentless Python-owned content from `takeContentWidget()`. Its
native same-window overlay, dim scrim, edge geometry, animation, outside-press,
Escape, and `CloseFlag` policy remain implemented by the C++ component; the
Python binding does not create a second window or emulate the overlay.
`Popup` exposes its native open/close lifecycle, modal/dim scrim, animation,
`CloseFlag` policy, local placement, and light-dismiss behavior. Position
anchors, theme sources, and passthrough regions stay caller-owned; the Python
facade retains their wrappers without changing QWidget parentage or Shiboken
ownership, then releases them on replacement, explicit clear, external
destruction, or Popup destruction. Closing restores focus to the invocation
target only while focus remains inside the Popup, so a focus move made during
the close transition is not overwritten. C++-only automatic-placement and
focus-policy hooks remain private to overlay subclasses.
`Flyout` adds native Top, Bottom, Left, Right, Full, and Auto placement,
anchor offset, and window clamping on top of Popup behavior. Its anchor remains
caller-owned; `setAnchor()` and `showAt()` retain only the Python wrapper and
release it on replacement, clear, external destruction, or Flyout destruction.
Public `isinstance(flyout, Popup)` and `issubclass(Flyout, Popup)` checks retain
the native C++ inheritance relationship across the two Python facade classes.
Destroying the active invocation anchor closes the overlay without attempting
to restore focus to a QWidget already in destruction. C++ placement hooks stay
private, while Python subclasses can still override ordinary QWidget events.
`CoachMark` and `TeachingTip` expose their native same-window target placement,
card sizing, tail rendering, open/close lifecycle, and Qt-owned content hosts.
`TeachingTip` additionally publishes preferred placements, light-dismiss
control, and semantic close reasons. Targets remain caller-owned: each facade
retains only the Python wrapper and releases it on replacement, explicit clear,
external destruction, or overlay destruction. Target/content-host getters do
not alter QWidget parentage or Shiboken ownership. `TeachingTip` retains its
native Popup inheritance for `isinstance`/`issubclass`, while C++ theme and
placement hooks remain private to the implementation.
`Dialog` exposes the native same-window `QDialog` lifecycle, smoke scrim,
animation, modality, result, and ordinary QWidget virtual events. A local
theme source remains caller-owned; the facade retains only its wrapper and
releases it on clear, external destruction, or Dialog destruction.
`ContentDialog` preserves the native Dialog inheritance relationship and adds
title, default-command, primary/secondary/close results, signals, and hosted
content. Installed content is parented to and destroyed with the dialog;
replacement or `setContent(None)` detaches the previous widget, while
`takeContent()` explicitly returns it parentless to Python. The facade retains
Python subclass state, rejects host/ancestor cycles, and tracks external
content destruction through Qt's `deleteLater()`/deferred-delete path. As with
other still-parented Python subclasses, direct `Shiboken.delete(content)` can
fast-fail inside PySide6 6.2.4 on Windows before Qt completes its destroyed
signal chain and is not part of the supported lifecycle contract. The three
C++ `static constexpr` result fields are published as stable Python class
constants because Shiboken 6.9 otherwise initializes them on an invalid
flattened namespace during module import.
`ComboBox` preserves native item/model APIs, current-index/text signals,
editable input, keyboard interaction, and the C++ same-window Flyout dropdown.
Its caller-owned model wrapper is retained while installed and released on
replacement or host collection; the explicit derived `model()` binding avoids
the stale parent heuristic used by some Shiboken versions. A line editor passed
to `setLineEdit()` becomes Qt-owned and is destroyed when replaced, when
editable mode is disabled, or with the ComboBox. The popup's Fluent `ListView`
and row delegate remain implementation details.
Inherited `setView()`/`view()` and delegate customization fail explicitly
because they would mutate QComboBox's unused fallback popup rather than the
visible Fluent dropdown. Use item/model data and ComboBox signals instead.
`AutoSuggestBox` accepts a native `QStringList` as an ordinary Python string
list and publishes typed text-change, suggestion-choice, query-submission, and
popup-state signals. Keyboard preview, Enter/Escape handling, IME-safe focus,
painting, and its same-window Fluent Flyout remain in C++. The internal string
model, popup class, and row delegate are deliberately private, and the C++
theme-refresh hook is removed from both `LineEdit` and its Python subclasses.
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
`NavigationView` exposes native responsive Left, compact, minimal, and Top
display modes while its C++-owned `StackContentHost` provides page navigation.
Pages use fixed Owned, Borrowed, and Reparented add/insert/replace methods;
removal applies the recorded policy, `takePage()` transfers a parentless page
to Python, and the facade retains Python subclasses and original-parent restore
targets. Header, main, and footer chrome slots follow the same three explicit
policies with fixed set, release, and take methods. Runtime ownership overloads
and legacy transfer-style mutation remain outside the public native surface.
`TabView` exposes native tab metadata, selection, close/reorder signals,
keyboard accelerators, geometry queries, and RTL behavior. `TabViewItem` is a
mutable, unhashable value type whose `data` field accepts QVariant-compatible
Python values. TabView does not adopt application pages: compose it with a
PySide `QStackedWidget` (or another caller-owned page host) through
`currentChanged`, `tabMoved`, `tabCloseRequested`, and `addTabRequested`.
The internal painting/input `TabStrip` remains private.
`Breadcrumb` exposes native layout, middle overflow, activation, keyboard,
RTL, geometry, and accessibility behavior. `BreadcrumbItem` is a mutable,
unhashable value type with QVariant-compatible `data`. Python sequence dispatch
is explicit: a sequence must contain only `str` or only `BreadcrumbItem`.
The facade routes those forms through separate native adapters because older
Shiboken converters can otherwise accept value wrappers as `QStringList` and
silently replace their labels with empty strings.
`SelectorBar` and `Pivot` expose their native item mutation, selection,
activation, keyboard/RTL, geometry, accessibility, and overflow behavior.
`SelectorBarItem` and `PivotItem` are mutable, unhashable metadata values with
QVariant-compatible `data` fields. Neither control adopts application pages:
connect its selection signal to an ordinary caller-owned PySide page host.
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
The authoritative CI and first-release architecture catalog is
[`wheel-matrix.json`](wheel-matrix.json).
The reusable [PySide6 CI workflow](../../.github/workflows/ci-python.yml) owns
all binding-generation, wheel, clean-install, native-window, and manylinux
steps. The top-level [CI workflow](../../.github/workflows/ci.yml) only selects
the validation tier and invokes that module alongside the independent C++
module.
The minimum compatibility lanes remain Linux x64 and Windows x64 with
CPython 3.10 plus Qt/PySide/Shiboken 6.2.4. The first-release lanes use the
matched 6.9.3 toolchain on Linux, macOS, and Windows, each on both x64 and
ARM64 native runners. Windows, macOS, and Linux x64 cover CPython 3.11 through
3.13; Linux ARM64 covers CPython 3.12 and 3.13. Here `x64` means
x86_64/AMD64, and 32-bit x86 is not supported.

The Linux ARM64 exception is an upstream runtime constraint, not a raised Qt
minimum. Official Shiboken 6.9.3 aarch64 wheels can return borrowed `Py_None`
from wrapped void functions when loaded by pre-3.12 CPython, eventually
triggering `none_dealloc`. CPython 3.12's immortal singleton contract makes
that wheel/runtime combination safe. Configuration performs a live reference
ownership probe and rejects unsafe combinations instead of allowing a wheel
that fails only under sustained UI calls.

The normal fast CI tier keeps the two minimum lanes and the macOS ARM64
CPython 3.11 release lane. In fast CI, the Linux and Windows Qt 6.2.4 lanes
generate and compile the bindings, run the core binding contracts, and
clean-install the core wheel; the macOS release seed retains the full visible
acceptance path. Gallery wheel smoke, strict mypy, the complete example set,
and native Window/TitleBar acceptance return on the compatibility lanes in
full CI. Pip downloads are cached by the reviewed wheel-matrix policy.

Full CI expands that seed coverage to the complete 17-wheel release matrix.
Every release lane builds the extension, checks generated contracts, runs
binding tests, installs both wheels in a clean virtual environment, and
confirms that Qt, PySide6, and Shiboken6 resolve inside that environment. The
lowest supported CPython lane for each platform/architecture additionally runs
strict mypy, visible acceptance examples, and native Window/TitleBar
integration. Six final `Platform status` jobs verify all 17 release-wheel
artifacts, both compatibility-wheel artifacts, and the eight representative
acceptance pairs. They expose Linux, Windows, and macOS x64/ARM64 success as
separate checks in the Actions UI.

Full Linux release lanes additionally rebuild inside the matching PyPA
manylinux image, repair with the pinned `auditwheel`, reject bundled duplicate
Qt/PySide6/Shiboken6 libraries, and clean-install the repaired wheel. x64 uses
CPython 3.11 through 3.13 and `manylinux_2_28`; ARM64 uses CPython 3.12 through
3.13 and `manylinux_2_39`, matching the official PySide6-Essentials 6.9.3
wheel floors. Native `linux_*` artifacts remain test evidence only. Signing
and upload automation remain deliberately separate from validation; the full
17-wheel matrix has passed together, but no artifact is published until the
dedicated release workflow and its approval gate are enabled.

Passing the build-tree tests proves the declared API contract; passing the
clean-wheel smoke proves installation/runtime isolation; the interactive
showcase proves the visible controls and signal-driven behavior. The native
Window acceptance runs with XCB under Xvfb, the Windows plugin on Windows, and
Cocoa on macOS, recording a PNG and JSON state report. Those lanes prove native
plugin loading, handle/chrome integration, typed backdrop state, and valid
fallback behavior. Physical Windows 11 DWM and a non-headless Ubuntu 22.04
ARM64 GNOME Wayland session additionally cover material quality, system
drag/resize, and representative overlays. Wayland correctly uses the painted
fallback; optional KWin/X11 compositor blur remains capability-gated and is not
advertised as physically reviewed by that Wayland run.

Very old Shiboken generators embed an older Clang parser. If Shiboken 6.2
cannot parse the C++ standard-library headers from a much newer host compiler,
use the Ubuntu 22.04 baseline or a compiler/SDK contemporary with Qt 6.2. This
does not require raising FluentQt's Qt minimum version.
