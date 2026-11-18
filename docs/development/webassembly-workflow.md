# WebAssembly Workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[← Linux Workflow](linux-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

Use this workflow to build FluentQt or the C++ Gallery for a browser, run the
local Chromium smoke, or change the Pages deployment. The supported browser
baseline is Qt 6.9.3 `wasm_singlethread` with Emscripten 3.1.70. Qt 5.15 and
Qt 6.2 remain supported desktop baselines; they are not browser targets.

Progress and verification evidence are tracked in the
[WebAssembly Roadmap](webassembly-roadmap.md).

## Toolchain

Install both parts of the browser toolchain:

- the Qt 6.9.3 desktop kit for the current host;
- Qt 6.9.3 `wasm_singlethread`;
- Emscripten 3.1.70 from the official `emsdk` repository;
- Ninja and CMake 3.25 or newer for the public preset.

Activate Emscripten and point the preset at the Qt target and host kits. These
example paths match a default macOS Qt installation; adjust them for the host:

```bash
source "$HOME/Qt/Tools/emsdk/emsdk_env.sh"
export QT_WASM_ROOT="$HOME/Qt/6.9.3/wasm_singlethread"
export QT_HOST_ROOT="$HOME/Qt/6.9.3/macos"
export FLUENT_QT_WASM_SOURCE_CACHE="$PWD/build/qt-wasm-source-cache"
```

Confirm that `em++ --version` reports 3.1.70 before configuring. Qt minor
versions and Emscripten versions are coupled; do not silently substitute a
different SDK version.

The Qt 6.9.3 browser adapter uses two small, pinned Qt accessibility patches.
Fetch their exact upstream sources explicitly before configuration:

```bash
python3 tools/wasm/qt_accessibility_overlay.py fetch \
  --cache "$FLUENT_QT_WASM_SOURCE_CACHE"
```

The manifest under `src/compatibility/private/wasm/qt-6.9.3/` owns source
revision, hashes, licenses and SDK identity. `fetch --offline` verifies an
existing cache and fails on missing or mismatched files. CMake reads only that
cache, builds the two replacement objects in a separate adapter build, and
keeps the Qt SDK unchanged. Native builds do not load this helper or require
Python or Qt private headers. Keep the same cache available for the installed
WebAssembly consumer below.

## Configure and build

The `wasm` preset builds the reusable library, Hello World, and the C++ Gallery
without vcpkg, desktop packaging, tests, or PySide6. It explicitly enables the
optional `FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` integration layer; ordinary
native builds leave that layer disabled:

```bash
cmake --preset wasm
python3 tools/dev/fluent_qt_build.py --preset wasm
```

The main browser entry points are:

- `build/wasm/app/index.html` for the C++ Web Gallery;
- `build/wasm/examples/hello_world/hello-world/index.html` for the standalone
  minimal example with the shared browser shell;
- `build/wasm/app/hello-world/index.html` for the minimal UILib app exactly as
  it is staged beside the Gallery for Pages;
- `build/wasm/app/hello-world-spatial/index.html` for the minimal Spatial app,
  built from the same C++ window as the native Spatial executable; its
  standalone entry is
  `build/wasm/examples/hello_world/hello-world-spatial/index.html`;
- `build/wasm/app/plugin-extension/index.html` for the optional plugin lifecycle
  app; its standalone build entry is
  `build/wasm/examples/plugin_extension/index.html`.

The [Hello World guide](../../examples/hello_world/README.md) maps the native,
Python, and browser Spatial examples. The
[plugin example guide](../architecture/plugins.md#minimal-extension-example)
maps the corresponding Plugins entries.

Serve the build directory over HTTP. Browsers must not load the generated files
directly through `file://`:

```bash
python3 -m http.server 4173 --bind 127.0.0.1 --directory build/wasm
```

Then open `http://127.0.0.1:4173/app/index.html`.

### High-density rendering

Qt WebAssembly sizes the QWidget backing store from the browser device pixel
ratio. The Gallery uses native resolution by default in both 2D and 3D, including
2x Retina displays. Enabling 3D does not lower the canvas resolution.

Lower-resolution rendering remains an explicit performance option. It reduces
pixel work but makes text softer on high-density displays. The adaptive option
selects 1x–1.25x in 0.05 steps, targeting at most 1.6 million viewport pixels;
it preserves the logical layout.

- use the footer link to opt into lower resolution and return to native;
- append `?render-scale=native` to explicitly select the default quality;
- append `?render-scale=adaptive` for the bounded performance profile;
- append `?render-scale=1` or another positive value for an explicit profile;
- the selected/native values, profile, and pixel budget are exposed as
  `data-fluent-qt-render-dpr`, `data-fluent-qt-native-dpr`,
  `data-fluent-qt-render-profile`, and `data-fluent-qt-pixel-budget` for browser
  automation.

This is a browser-shell policy and does not add WebAssembly branches or scale
assumptions to `FluentQt` C++ code.

### Threading policy

The supported build remains `wasm_singlethread`. Selecting Qt's
`wasm_multithread` kit does not automatically parallelize QWidget layout,
painting, input delivery, or canvas presentation; those GUI operations remain
on the browser main thread. A threaded build is useful only after a consumer
extracts measurable non-GUI work into worker-backed threads.

Treat multithreading as a separate future experiment rather than a replacement
for the supported artifact. It requires a matching Qt multithread kit,
`SharedArrayBuffer`, a secure context, and COOP/COEP response headers, and it
must be published as a separate binary with runtime selection because the same
WebAssembly binary cannot fall back to the single-thread configuration. Profile
an extracted worker workload before adding this deployment and compatibility
cost to M3.

### Window presentation

Desktop-sized browser viewports default to a centered Fluent app window with
rounded clipping, shadow, caption buttons, and client-side move/resize.
Compact viewports maximize the same application window inside one browser-sized
Qt desktop surface.
Use the footer switch or append one of these query values:

- `?window-mode=windowed` forces the centered app-window presentation;
- `?window-mode=maximized` fills the available browser stage.

The requested mode is exposed as `data-fluent-qt-window-mode`. The effective
application-window geometry is exposed as
`data-fluent-qt-window-x`, `data-fluent-qt-window-y`,
`data-fluent-qt-window-width`, `data-fluent-qt-window-height`, and
`data-fluent-qt-window-maximized` for browser automation. The HTML shell owns
only the browser stage and presentation request. `FluentQt::WebAssembly` owns
the single Qt desktop surface plus hosted QWidget geometry, move/resize, and
maximize/restore behavior; `GalleryWindow` remains the actual visible window.

### Embedded theme protocol

The website and an embedded Gallery share the same three-value theme wire
format: `light`, `dark`, and `high-contrast`. The initial value is passed as
`?embed=site&host-theme=<value>`. Later changes use a same-origin message:

```js
galleryFrame.contentWindow.postMessage({
  source: "fluent-qt-site",
  type: "theme",
  theme: "high-contrast"
}, window.location.origin);
```

Existing `light` and `dark` hosts remain compatible. Browser
`(forced-colors: active)` always takes precedence without overwriting the
stored host preference; when forced colors ends, the most recent host or system
preference is restored. The website theme button also cycles through all three
values, so an embedded Gallery can enter high contrast without relying on an OS
setting.

### Component property editor

Core component pages with a descriptor have a **Playground**
button beside the page introduction. Activate it to open an independent centered interactive playground
and property dialog. `/api/button/?playground=1` opens the playground directly.
Button retains its styles and interaction-state examples; the other core component
pages open independent fixtures with reproducible sample data.

Button picking reads the existing Qt widget's public properties. Other core components
use the published fixture, including their models and initial content. Opening the
editor creates an isolated Qt WebAssembly iframe with its own component, model,
focus and overlays. Browser pointer, keyboard, wheel and text input go directly
to Qt. Native read-only and enabled properties determine which actions work.
A bounded observer reads back changed values and updates the property fields and
C++/PySide6 exports. Transient hover, focus and animation-frame progress are not exported as configuration.
Automatic image fit owns its geometry-dependent zoom; exports only set an explicit
zoom factor in Manual mode.
Closing the dialog removes this runtime. Pointer re-entry restores the browser's
focus to the canvas; Tab boundaries hand it back to the property dialog without
replacing the component's input policy. Editing, resetting and closing the
preview do not replace the original sample, change its values or rewrite its source.
Optional extension component pages retain their existing examples without interactive
playgrounds.

`app/assets/playgrounds/button.json` defines the original Button editor.
`app/assets/playgrounds/components.json` owns the other 94 core descriptors and
their C++/PySide6 setup recipes. `tools/site/generate_playgrounds.py` derives
individual descriptors, the resource list, and the compiled factory from these
recipes; do not edit generated files. The generator uses the pinned C++ formatter.
`GalleryComponentPlayground` applies an allowlisted patch to a real control, then reads back its values and produces matching C++ and
PySide6 code. Unknown properties or invalid types reject the entire patch.
Numeric values are clamped to the documented editor range. The editor does not
change library defaults or expose arbitrary Qt method calls. ComboBox's selection
editor reads labels, enabled flags and indices from the preview's actual model.
The No selection entry maps to -1; code still uses numeric `setCurrentIndex()`.
TreeSelect's selected-key editor follows the component's actual selection mode:
Single uses radio choices with a clear option, while Multiple uses checkboxes.
Switching modes reads back the component's retained selection before updating
the editor, so a new single choice replaces the old choice in one click.
Navigation, tab, step and page selection fields also derive their choices from
the current control, and display labels or page numbers rather than a numeric
slider. Enum options are deduplicated by their actual Qt value; slider tick
labels follow the current orientation, so aliases are never presented as
different actions. Related numeric bounds refresh after each accepted change,
including the other RangeSlider endpoint, its minimum separation, and maximum
rating. A fixed range disables the editor with an explanation. LineEdit's text
editor also follows its current maximum length, so typing cannot offer text
that the component would silently truncate. Special values such as automatic
tick spacing have explicit labels while exports keep the
numeric API value. RangeSlider exports both endpoints with `setValues()` so
constructor defaults cannot clamp the first endpoint before the second is set.
The Gallery language gate checks defaults, preset values, API names, and English
copy. Explicit `zh` strings in descriptor descriptions, category labels, property
labels, property descriptions, and preset labels are treated as website translations.

Chart and collection playgrounds use a wider dialog and descriptor-owned data,
layout and interaction categories. Their independent fixtures expose bounded
row editors (32 rows), with meaningful labels for selection and tree paths.
`GalleryPlaygroundData` owns the preview-only model adapters; native models
remain owned by those fixtures, never borrowed from the original example.
Chart data edits preserve ordered finite X values and accept a blank Y as a
gap. Numeric cells remove redundant leading zeros during input and normalize
complete numbers on blur; partial decimals, empty gaps and text labels keep
their editing semantics. Tree data editors rename existing nodes without changing their hierarchy;
Timeline edits retain timestamp and description roles. Native cell edits,
column sorting and chart cursor movement read back into the panel and both
exports. Data reset recreates and validates against a fresh fixture, including
when all rows were removed. Invalid cell drafts remain visible until corrected
or reset and are not exported.

Fields follow actual component semantics: FlowView exposes minimum item size,
so model/delegate hints can grow items above the chosen lower bound; GridView's maximum
column count is a cap, subject to cell size and available width. Chart-only
fields appear only for supported chart types. DrawerView uses a file-details
scene composed from Fluent Card, Label, Button and ScrollView. Its open and
close buttons update the same component state as the property panel. Size,
corner radius, scrim strength and background blur use sliders. The initial
background is clear; the detail body uses a matching Fluent surface and a floating
scrollbar when needed. Its canvas is clipped to the
fixed application surface, so all four animated edges keep the preview layout
stable; it counts as an internal overlay for Escape handling.

The browser bridge checks the parent window, origin, session, route, selection
generation, and request identifier. Late replies cannot replace newer edits.
Code copying waits for the accepted Qt state; a failed runtime disables editing
and offers retry. All property inputs are disabled after a
timeout; retry creates a fresh preview from the original example and restores
each field's dependency rules. Unacknowledged edits are not exported.
The dialog isolates background content and keeps Tab/Shift+Tab focus inside;
its dropdown options are part of the same dialog and support arrow-key navigation.
The live preview uses real pointer and keyboard input; it has no simulated state
selector. Editing a property does not move focus, hover the component, or
activate it. Property, code and dropdown scrollbars use a slim thumb at the
scrollable surface's edge. The dialog keeps its height during ordinary input
and popup opening/closing. Tall controls leave room for the independently
scrollable property pane, and native popups paint above that pane without
resizing it. The browser layout contract samples geometry on animation frames
during menu selection and action feedback, and checks short/narrow viewports
and access to the final overflowing field. The native canvas width is independent
of popup bounds; display scaling uses the main surface width. A read-only
geometry handoff updates the same-origin host before Qt commits a paint, while
versioned geometry prevents a later property reply from restoring an old clip.
Closing overlays retain their paint space until they are actually hidden.
The browser contract checks canvas position/scale and pre-paint clip coverage,
not just the outer dialog rectangle.

Descriptors own property categories and semantic `group` membership. Button and
its derivatives share the same renderer; main icons, dropdown indicators and
query icons are separate groups. `visibleWhen` progressively reveals dependent
editors without clearing their configured values. Icon transforms require an
icon-bearing layout. All eight generic button families expose the six inherited
layouts and the same rotation, scale and two-axis offset controls.
`enabledWhen` only restricts
invalid edits; `effectWhen` and localized `effectHint` describe when an editable
value becomes visible. Behavior-only options explain their effect on input or
later actions. `effectFixture` supplies reproducible prerequisites for the native
and browser property audits. Value readback alone does not establish visual
correctness: focus and hover checks also compare cropped browser pixels before
and after real input. The native property audit records pixel changes separately;
a recorded change is not automatically a visual approval.
Placeholder fields remain editable while content is selected. ComboBox's editable
fixture configures its real line editor too, and exports that setup. MultiSelectComboBox
labels its row limit as popup height, independently of the full model's selections.
Nullable date fields accept an empty value without a theme toggle.
Popup fixtures marked `deferOpen` apply their configuration before opening, and
reopen after placement changes. Current-state properties marked `afterOpen`
(such as CommandBarFlyout expansion) are applied after opening in both the
preview and exported code. Fixed-width text fixtures retain their viewport
for elision; popup fixtures crop to the settled content while reserving enough
host space to demonstrate their row limits.
Closing restores the background's prior inert state and returns focus when appropriate.
A null corner radius means **Follow theme**, and reset restores
the selected component's starting values, including its theme-following radius.
Closing, changing examples, and unloading the page release the preview copy.
Removing an icon restores text layout; replacing an icon preserves an explicit
TextOnly choice. Icon transforms remain available when an icon is selected again.
Checkable changes toggle behavior; Checked changes the selected appearance.
The independent Qt canvas is interactive. `focusVisual` permits a ring only
while the enabled button has actual focus; editing that flag does not focus it.
The field explains how to focus the preview with a click or Tab. Static native
capture fixtures may simulate focus for image comparison; this is separate from
the browser's live interaction contract.
The embedded runtime clears its Qt focus when the browser moves focus to the
property editor, and restores an eligible preview control on browser re-entry.
This policy belongs to the isolated WebAssembly preview host, not Button.

The TimePicker editor writes `selectedTime` through `setSelectedTime()`.
`setTime()` retains its library contract: it updates the fallback/pending time
without committing or replacing the selected time.

Run the editor's real-browser regression after building the Gallery and
generating the website:

```bash
python3 tools/site/test_component_playground_browser.py \
  --wasm-root build/wasm --screenshots
python3 tools/site/test_input_playgrounds_browser.py \
  --wasm-root build/wasm --screenshots
```

It serves the website and existing WASM payload locally. The full browser smoke
also runs this contract and retains its failure evidence under
`build/wasm/component-playground-validation/`.
The Button contract audits its 15 live descriptor fields, enum choices, numeric endpoints,
icon/layout dependencies, selection, disabled input and exact reset behavior.
It checks native canvas input, read-only/disabled behavior and both generated languages, and writes
`cases.json` with the configurations and source used by the test.
Both browser scripts disable Chromium's default headless scrollbar hiding, so
captures include the real scrollbars and their position can be reviewed.
The shared `playground_interaction_audit.py` contract visits all 95 registered descriptors and checks each visible
editor, dependent fields, value readback, both generated languages, source
isolation, and exact reset. Tag lists and selected items use bounded lists,
and TextEdit has explicit adapters for its method-based content APIs.
Collection routes also run `playground_scroll_audit.py`: large boundary wheel
input must stay bounded, reverse input must move content during a return,
and dragging the thumb must discard stale elastic displacement. These checks
preserve selection and generated source, keep the preview's geometry fixed,
and verify that the native collection and HTML properties scroll independently.
The fixed tree fixture uses its overflowing horizontal axis; the native shared
contract also tests a tree with vertical overflow.
Narrow screens switch between Properties and Code and retain both language tabs
and copying. Preview geometry respects each control's minimum size, then fits
the live canvas into the browser stage. Open menus and popups expand its visible bounds. Full reset rebuilds the independent
fixture to restore internal scroll, cursor and disclosure state.
Live preview transitions run normally. Raster-only native comparison tests can
settle finite animations before capture; application animation settings and the
original Gallery examples remain application-owned.

To compile and execute those exports against a matching Qt 6 static build and
its PySide6 runtime, run the following with the build's Python interpreter:

```bash
PYTHONPATH=build/vcpkg-osx/python .venv-pyside69/bin/python \
  tools/site/verify_playground_exports.py \
  --audit build/component-playground/browser/cases.json \
  --build-dir build/vcpkg-osx --qt-prefix /path/to/Qt/6.9.3/macos \
  --output build/component-playground/exports
```

The verifier executes trusted local test exports, compares their actual
properties, and compares C++/PySide6 rendered pixels. It does not infer native
appearance from the browser result or execute untrusted downloaded snippets.

### Embedded Spatial sample

`?showcase=spatial` starts the existing `spatial-view-scene` sample without the
Gallery navigation, startup pages, or saved preferences. Its C++ host lives in
`app/platform/wasm/GallerySpatialShowcase.cpp`; the sample and component code are
unchanged. It includes scene controls and is separate from the minimal
`app/hello-world-spatial/index.html` application. `embed=site` retains the host
theme contract. The website loads this
view on demand at native render density and can send same-origin
`showcase-visibility` (boolean `active`) and `showcase-motion` (boolean `reduced`)
messages. Hiding the sample lets SpatialView release its GPU viewport; unloading
the iframe releases the browser runtime. An optional `host-session` query is
echoed in lifecycle messages so discarded loads cannot overwrite a new session.

The browser smoke operates the real camera Slider and 2D/3D switch, checks theme
and reduced-motion transitions, and saves Light/Dark evidence. The host publishes
`data-fluent-qt-showcase-*` values from component signals for verification; those
attributes do not drive the UI. Software-rendered CI evidence is not hardware
GPU performance approval.

### Simplified Chinese font fallback

WebAssembly builds embed
`res/fonts/FluentQtUISimplifiedChinese-Regular.otf`, a GB2312 subset generated
from the pinned Noto Sans SC source. CMake includes it only when
`FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` is enabled under Emscripten, so native
binaries do not inherit its approximately 1.9 MiB payload.

Regenerate or verify that asset with pinned fontTools 4.59.1:

```bash
python tools/fonts/generate_typography_assets.py --web-fallback
python tools/fonts/generate_typography_assets.py --check-web-fallback
```

Resource initialization registers the optional family as a Han fallback. The
CalendarView smoke checks both registration and the `月` / `周` glyphs; reusable
components contain no WebAssembly-specific font branches.

## Browser smoke

The Gallery owns condition-driven browser checks selected by a query parameter:

- `?wasm-smoke=fast` visits four representative routes;
- `?wasm-smoke=full` traverses the complete route catalog;
- both tiers verify WebLocalStorage, asynchronous dialog/menu completion, a
  bounded secondary Fluent window, a three-action menu, the seven-action
  PasswordBox editing menu, physical browser-key delivery into a real Fluent
  LineEdit, opaque active client chrome, the painted-surface cache, and the
  selected render profile;
- both tiers verify the embedded `light` / `dark` / `high-contrast` query and
  message contract, forced-colors precedence and restoration;
- browser smoke records WebAssembly heap capacity and program-break watermarks;
- both tiers verify the Simplified Chinese fallback, launch the staged base and
  Spatial minimal apps, and click the Spatial Button at its projected position;
- both tiers operate the plugin example through activation, creation,
  deactivation, and restoration, checking retained widgets and native
  dependency/failure probes.

The default local command runs all checks for its tier. In full CI,
`--group interaction` runs command menus, the command palette, forms and
DataGrid on a separate runner. The `gallery` group runs the remaining checks
and the 1920×1080 `--mode viewport` pass. The wide pass exercises only the
size-dependent Gallery and base Hello World pages.

Website theme cycling and persistence are covered by the
[static theme contract](../../tools/site/test_theme_browser.py) in
[website CI](ci-workflow.md#keep-workflow-ownership-explicit), without loading
WebAssembly.

The runner publishes `data-fluent-qt-smoke="pass|fail"` and a detail field on
the document root. CI drives that contract through Playwright:

```bash
python -m pip install "playwright==1.58.0" "Pillow==12.1.1"
python -m playwright install chromium
python .github/scripts/run-wasm-browser-smoke.py \
  --root build/wasm \
  --mode fast
```

The smoke runner defaults to a simulated DPR 2 so CI covers the Retina profile.
Use `--render-scale adaptive` for a controlled quality/performance comparison.
Use `--viewport-width 1920 --viewport-height 1080` to exercise the large-screen
adaptive profile.

Use `--mode full` before changing route construction, settings persistence,
dialogs, menus, the Web shell, or Pages packaging. The existing full command
palette/overlay contract also recreates the Drawer five times. Each opening
requires one correctly typed proxy for Modal, DimEnabled, CloseOutside, Blur,
Dim and Close; closure must retire every Drawer proxy, including hidden and
construction-time nodes, before the next opening. It waits for that lifecycle
condition without a fixed delay or selecting the first duplicate. Results and
snapshots are under `command-palette-validation/drawer-lifecycle/`.

The full browser smoke also exercises Gallery title-bar search pagination with
real pointer input. It verifies that the Load more accessibility proxy follows
its footer after results change the popup layout, then checks query replacement
and Enter navigation. Results and screenshots are under
`gallery-search-validation/`. The pinned Qt adapter enables accessibility
notifications and coalesces geometry refreshes for already published visible
proxies; it does not create additional model-cell interfaces.

### Spatial WebGL validation

With `FLUENT_QT_BUILD_SPATIAL=ON` (included in the `wasm` preset), the Gallery's
**Settings → 3D Gallery** switch uses the same native compositor through WebGL.
The browser canvas owns the context. Initialization checks the real viewport;
reparenting into the browser desktop can replace that context and is revalidated.
Initialization failure selects the usable 2D layout and updates the support badge.

Run the optional rendering and performance probe separately from the route smoke:

```bash
python3 .github/scripts/run-wasm-spatial-smoke.py \
  --root build/wasm --headed --channel chrome \
  --output build/spatial-validation/web
```

`?wasm-smoke=spatial` measures three seconds of pointer following and an idle
interval, then clicks the projected switch and returns through 2D → 3D. It also
opens a Spatial example and changes its camera-distance control. It writes
frame-swap counts and timings to
`data-fluent-qt-spatial-metrics`; the runner also records the browser's GPU device
and saves a screenshot. Normal visits create no measurement timer.
Frame swaps count renderer submissions, not monitor scanout. Keep the browser
foreground and distinguish hardware WebGL from SwiftShader when reporting
performance; a headless pass alone does not establish GPU performance.

Add `--quality` to capture the same Popup page in 2D, at the flat GPU endpoint,
in 3D, and after returning to 2D. This checks projected text edge contrast against
the 2D reference and saves full-page and control-detail images for visual review.
Use `--render-scale adaptive` to repeat that check for the optional lower-resolution
profile. Interaction smoke and frame counts alone do not establish text clarity.

To check the 2D fallback with Chromium's software renderer:

```bash
python3 .github/scripts/run-wasm-spatial-smoke.py \
  --root build/wasm --software-gl \
  --output build/spatial-validation/web-fallback
```

This check requires the Gallery to reject SwiftShader and remain usable in 2D.

## Adapter boundary

WebAssembly follows the same opt-in shape as the PySide6 bindings. The root
option adds `platforms/webassembly/`, which owns the exported browser runtime
and Asyncify helper; the Emscripten toolchain is still the actual platform
selection boundary.

Reusable C++ code stays platform-neutral:

- `src/` has no WebAssembly platform enum, Emscripten include, or `Q_OS_WASM`
  branch;
- Gallery views and view-models depend on `app/platform/GalleryPlatform.h`
  capabilities rather than browser macros;
- CMake selects exactly one desktop or browser implementation for application
  lifetime, settings storage, theme persistence, and distribution UI;
- Hello World uses the same selected-launcher pattern, so its shared window
  construction is identical on native and browser targets.

Browser-only C++ is confined to `platforms/webassembly/`,
`app/platform/wasm/`, and `examples/hello_world/platform/wasm/`. Pinned Qt
accessibility source manifests and patches live in the private compatibility
owner `src/compatibility/private/wasm/`; fetched sources remain in the external
cache, rather than components. Build-system
and deployment policy stay in the WebAssembly CMake/CI modules.

## Consumer contract

`FluentQt::FluentQt` does not propagate Emscripten-specific flags. A windowed
browser consumer enables the adapter and links the runtime explicitly:

```cmake
set(FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT ON CACHE BOOL "" FORCE)
add_subdirectory(path/to/FluentQt)
target_link_libraries(my_web_app PRIVATE
    FluentQt::FluentQt
    FluentQt::WebAssembly)
```

Call `fluent::webassembly::configureRuntime()` before constructing Fluent
widgets, then present each application window with
`fluent::webassembly::showWindow()`. This keeps browser mechanics outside the
shared component and application code.

`FluentQt::WasmAsyncify` exists separately when the optional adapter is enabled
with an Emscripten toolchain. Link it only when an application retains nested
modal event loops; the Gallery and Hello World do not. Native builds define
neither browser target. Asyncify increases output size and runtime overhead.

Use `qt_add_executable()` for a WebAssembly executable. Objects that must live
for the browser session cannot be stack-owned by a `main()` that returns to the
JavaScript event loop. The selected browser launch adapters keep `QApplication`
and the top-level window alive on the heap without leaking platform branches
into the shared examples or Gallery code.

The `wasm` preset also enables installation so CI can verify a consumer against
the exported package rather than the source tree:

```bash
cmake --install build/wasm \
  --prefix /tmp/fluentqt-wasm-install \
  --component Development
"$QT_WASM_ROOT/bin/qt-cmake" \
  -S examples/hello_world \
  -B build/wasm-installed-consumer \
  -G Ninja \
  -DFluentQt_DIR=/tmp/fluentqt-wasm-install/lib/cmake/FluentQt
python3 tools/dev/fluent_qt_build.py build/wasm-installed-consumer
```

This is the independent UILib proof: the Gallery is one consumer, not the only
WebAssembly output.

## Gallery platform boundaries

The browser Gallery intentionally excludes desktop-only behavior:

- single-instance `QLocalServer` IPC and second-launch activation;
- system tray integration and close-to-tray prompts;
- native window placement persistence and desktop packaging;
- update polling and filesystem theme import/export;
- OS-native caption buttons, native system move/resize, and compositor-specific
  backdrops.

Browser settings use `QSettings::WebLocalStorageFormat`, logs go to the browser
console, and pages are created lazily. The hosted Fluent title bar uses an
opaque active token surface, while the otherwise static software window
material is cached until size, theme, effect, activation, or display scale
changes. The hosted Fluent window still provides caption buttons and manual
move/resize inside the browser desktop. Gallery dialogs and menus are
asynchronous, so the Gallery itself does not link whole-program Asyncify.

## CI and Pages

`.github/workflows/ci-wasm.yml` is the reusable browser validation module. The
top-level CI selects it for browser-affecting pull requests and for full `main`
validation. Browser jobs download the staged files from the producing build
attempt and check its commit and tier. Once all groups pass, CI copies those
same files to `fluentqt-wasm-pages`. On `main` the reusable Pages deployment
consumes that artifact. A manual Pages run rebuilds the
full tier before deploying and remains the recovery path. The site is published
below `/Fluent-Qt/gallery/` together with `build-info.json` and license material.

The open-source WebAssembly binary statically links Qt and is distributed under
GPLv3. FluentQt's own source remains MIT licensed. The deployed payload must
retain the FluentQt, Qt, Emscripten, and bundled-asset notices plus the exact Qt
corresponding-source link; see `THIRD_PARTY_NOTICES.md`.


The interactive fixtures also export committed tri-state values, editable ComboBox
text and items, collection selection and expansion, form values, navigation,
transfer selections, splitter preferences and nullable date/time or number values.
Collection selection options come from the actual model and honor its selection
mode. The menu-backed DropDownButton fixture omits inherited toggle fields:
its primary activation opens the attached menu. Action-only controls report their
signals without inventing a new component value. Application-owned callbacks implement tab add/close, step navigation and
StackView forward/back examples; generated snippets contain those callbacks.
Recipe or component changes must exercise pointer and keyboard behavior in
addition to setting properties. A successful setter/readback check alone does not
prove that user interaction, focus, reset and exported state remain aligned.
Native GUI checks and browser canvas checks are separate from platform and
screen-reader certification. Record the tested Qt/browser/OS with the report.

<!-- docs-nav:bottom:start -->
---
[← Linux Workflow](linux-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
