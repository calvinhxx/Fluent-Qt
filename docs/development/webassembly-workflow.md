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
```

Confirm that `em++ --version` reports 3.1.70 before configuring. Qt minor
versions and Emscripten versions are coupled; do not silently substitute a
different SDK version.

## Configure and build

The `wasm` preset builds the reusable library, Hello World, and the C++ Gallery
without vcpkg, desktop packaging, tests, or PySide6. It explicitly enables the
optional `FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` integration layer; ordinary
native builds leave that layer disabled:

```bash
cmake --preset wasm
cmake --build --preset wasm --parallel
```

The main browser entry points are:

- `build/wasm/app/index.html` for the C++ Web Gallery;
- `build/wasm/examples/hello_world/fluentqt_hello_world.html` for the minimal
  example build output;
- `build/wasm/app/hello-world/index.html` for the minimal UILib app exactly as
  it is staged beside the Gallery for Pages.

Serve the build directory over HTTP. Browsers must not load the generated files
directly through `file://`:

```bash
python3 -m http.server 4173 --bind 127.0.0.1 --directory build/wasm
```

Then open `http://127.0.0.1:4173/app/index.html`.

### High-density rendering

Qt WebAssembly sizes the QWidget backing store from the browser device pixel
ratio. Painting a 2x Retina backing store requires four times as many pixels as
1x, which is costly on the single browser thread. The Gallery Web shell selects
an adaptive 1x–1.25x scale in 0.05 steps, targeting at most 1.6 million viewport
pixels while preserving the same logical layout. Large desktop viewports
therefore default to 1x; smaller surfaces retain the sharper 1.25x profile.

On a Retina display, an explicit 1x profile is intentionally a maximum-speed
mode and its text will look softer because the browser scales a lower-resolution
backing store. Use 1.25x for normal interaction and visual review; reserve 1x
for large viewports or performance comparisons.

- use the footer links to cycle from adaptive to balanced 1.25x and then native
  resolution;
- append `?render-scale=native` for native-resolution screenshots and visual
  review;
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
- browser smoke records WebAssembly heap capacity and program-break watermarks;
- both tiers verify the Simplified Chinese fallback and launch the independently
  staged minimal UILib app.

The runner publishes `data-fluent-qt-smoke="pass|fail"` and a detail field on
the document root. CI drives that contract through Playwright:

```bash
python -m pip install "playwright==1.58.0"
python -m playwright install chromium
python .github/scripts/run-wasm-browser-smoke.py \
  --root build/wasm \
  --mode fast
```

The smoke runner defaults to a simulated DPR 2 so CI covers the Retina profile.
Use `--render-scale native` for a controlled quality/performance comparison.
Use `--viewport-width 1920 --viewport-height 1080` to exercise the large-screen
adaptive profile.

Use `--mode full` before changing route construction, settings persistence,
dialogs, menus, the Web shell, or Pages packaging.

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
`app/platform/wasm/`, and `examples/hello_world/platform/wasm/`. Build-system
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
cmake --build build/wasm-installed-consumer --parallel
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
validation. On `main`, that same run passes the staged payload to the reusable
Pages deployment, so the Gallery is built once. A manual Pages run rebuilds the
full tier before deploying and remains the recovery path. The site is published
below `/Fluent-Qt/gallery/` together with `build-info.json` and license material.

The open-source WebAssembly binary statically links Qt and is distributed under
GPLv3. FluentQt's own source remains MIT licensed. The deployed payload must
retain the FluentQt, Qt, Emscripten, and bundled-asset notices plus the exact Qt
corresponding-source link; see `THIRD_PARTY_NOTICES.md`.

<!-- docs-nav:bottom:start -->
---
[← Linux Workflow](linux-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
