# WebAssembly Roadmap

This roadmap tracks implementation and verification of Fluent-Qt in a browser.
It is the single canonical development status source; a milestone is complete
only when its listed evidence exists on `release/1.6.x`. Production Pages
acceptance is tracked separately because deployment is intentionally triggered
only after a maintainer promotes the release branch to `main`. Desktop Qt 5.15+
and Qt 6.2+ support remains unchanged.

## Status

| Milestone | State | Deliverable | Completion evidence |
| --- | --- | --- | --- |
| M0 - Feasibility and architecture | Complete | Browser constraints, Qt/Emscripten baseline, deployment and licensing decisions | Repository-grounded design review completed on 2026-08-09 |
| M1 - Core and Hello World | Complete | Qt WebAssembly preset, exported Asyncify helper, browser-safe window chrome, Hello World target | Qt 6.9.3 / Emscripten 3.1.70 configure and build passed; Hello World canvas launched in Chromium on 2026-08-09 |
| M2 - C++ Web Gallery | Complete | Browser runtime, console logging, WebLocalStorage settings, lazy page loading, client-side title bar, Retina performance profile | DPR 2 Chromium smoke passed 88 routes plus storage and asynchronous dialog/menu checks in 11.70 s on 2026-08-09; slowest cold route was 185 ms |
| M2.1 - Browser productization | Complete | Responsive windowed shell, adaptive pixel budget, bounded route cache, Simplified Chinese font fallback, staged minimal UILib app, installed-consumer validation | At 1920x1080 / DPR 2, adaptive 1x fast smoke completed in 1.06 s versus 2.15 s at native 2x; full 88-route smoke held heap capacity at 128 MiB; an installed `FluentQt::FluentQt` consumer linked and built on 2026-08-09 |
| M2.2 - Browser parity hardening | Complete | One focusable Qt desktop surface, movable/resizable Fluent windows, maximize/restore and OpenWindow, opaque active title chrome, cached software material, idle bounce fallback, font-state isolation, opaque multi-row menus, browser keyboard input | Real-browser resize crossed the navigation breakpoint, maximize/restore and OpenWindow passed visual QA; physical browser keys reached the Fluent text controls; the latest 1.25x 88-route smoke passed in 6.89 s and the complete 1,378-test native CTest inventory had no remaining failures on 2026-08-09 |
| M3 - CI and deployment readiness | Complete | Reusable WebAssembly CI module, browser smoke, deployable `/gallery/` payload, website entry | Remote fast/full validation, installed-consumer checks, and Pages payload staging passed before integration on 2026-08-10 |
| M4 - Distribution and maintenance | Complete | Workflow/license/source-package documentation and supported-toolchain policy | Pre-integration full CI passed all 44 jobs plus `CI Gate` and `Release ready` on 2026-08-10; temporary-branch run history is pruned after integration |

All seven development milestones are complete on `release/1.6.x`. Publishing
the already-validated payload and checking its public URL are release acceptance
activities; they do not reopen the implementation roadmap.

## Supported target

The initial supported browser target is intentionally narrow:

- Qt 6.9.3 `wasm_singlethread` with Emscripten 3.1.70;
- modern browsers with WebAssembly, WebGL, and local storage enabled;
- the reusable `FluentQt` library and the C++ Gallery;
- single-threaded execution; Asyncify remains an opt-in compatibility target
  only for consumers that retain nested modal event loops;
- static deployment to `https://calvinhxx.github.io/Fluent-Qt/gallery/`.

Qt for WebAssembly is not a supported Qt 5.15 or Qt 6.2 browser target in this
project. Those versions remain the desktop compatibility baseline.

## Implementation boundaries

- `FluentQt::FluentQt` stays free of Emscripten-specific link flags.
- `FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT` adds the optional
  `platforms/webassembly/` integration, mirroring the opt-in PySide6 boundary.
- Browser applications link `FluentQt::WebAssembly`, call `configureRuntime()`
  before constructing Fluent widgets, and present windows through
  `showWindow()`. The adapter owns one Qt desktop canvas and hosts application
  windows inside it; reusable widgets remain unaware of the browser runtime.
- Browser consumers opt into nested-loop support by linking
  `FluentQt::WasmAsyncify`; native builds do not define that adapter target.
- Core `src/`, Gallery views/view-models, and shared example code contain no
  WebAssembly conditional branches. CMake selects narrow desktop/browser
  implementations behind platform capability and launcher interfaces.
- The Hello World example and Gallery do not link Asyncify. Gallery dialogs and
  menus use asynchronous `open()` / `popup()` flows; the opt-in target remains
  available for external consumers that cannot yet remove nested loops.
- The browser Gallery excludes single-instance IPC, system tray, native window
  placement, update polling, filesystem theme folders, and desktop packaging.
- Browser settings use `QSettings::WebLocalStorageFormat`; logging is written to
  the browser console; routes are built lazily rather than bulk-prewarmed. The
  browser adapter retains at most 16 recently used routes, while desktop keeps
  its unbounded resident-page behavior.
- The Web shell adaptively selects a 1x–1.25x QWidget backing store under a
  1.6-million-pixel budget. The footer exposes balanced/native choices and
  `?render-scale=native` still opts into native resolution; this policy remains
  entirely outside reusable C++ code. Explicit 1x is labeled as a soft HiDPI
  performance mode; 1.25x is the normal interaction and visual-review profile.
- On desktop-sized viewports, the WebAssembly runtime presents the same
  frameless QWidget tree as a centered, rounded child of one browser-sized Qt
  desktop surface. Compact viewports maximize it;
  `?window-mode=windowed|maximized` selects either presentation explicitly.
  The HTML shell owns the browser stage and requested mode; the optional runtime
  adapter owns widget geometry, manual move/resize, and maximize/restore without
  component-level WebAssembly conditionals.
- The Web build embeds a GB2312-subset Simplified Chinese fallback derived from
  Noto Sans SC and registers it through the optional resource contract. Native
  packages do not carry the font, and components contain no WebAssembly font
  branches.
- The supported artifact remains single-threaded. A future multithread
  experiment must first isolate measurable non-GUI work and ships as a separate
  artifact because QWidget layout/painting stays on the browser GUI thread and
  threaded deployment requires SharedArrayBuffer plus COOP/COEP headers.
- WebAssembly artifacts are a Pages channel, not desktop packages, vcpkg
  matrix entries, Python wheels, or release assets.

## CI tiers

`ci-wasm.yml` is the reusable third validation module beside the native C++
and PySide6 modules. Fast validation builds and launches Hello World plus the
Gallery shell. Full validation traverses Gallery routes at a simulated DPR 2,
exercises asynchronous dialog/menu paths, validates persistence and the render
profile, and packages the Pages payload. The
top-level `CI Gate` remains the stable branch-protection check.

## Integration record and ownership

Feature integration is complete:

1. `codex/web-sup` was based directly on `release/1.6.x`; implementation,
   focused validation, and review fixes remained isolated there.
2. PR [#27](https://github.com/calvinhxx/Fluent-Qt/pull/27) passed the fast
   `CI Gate` and the complete 44-job validation tier.
3. The work was consolidated into three responsibility-oriented commits and
   rebase-merged into `release/1.6.x` as `2b33563`, `16a5997`, and `99701c3`.
4. The local and remote `codex/web-sup` branches, its rewrite worktree, and the
   requested temporary-branch Actions history were removed after integration.

Promotion from `release/1.6.x` to `main` is maintainer-owned and must remain a
manual repository-governance action. Automation and coding assistants must not
perform that merge. The Pages workflow intentionally deploys only from `main`.

## Production acceptance after maintainer promotion

This checklist validates the published artifact without blocking or reopening
the completed development milestones:

- [ ] A maintainer manually promotes the intended `release/1.6.x` revision to
  `main`.
- [ ] The Pages workflow deploys the staged payload successfully.
- [ ] `https://calvinhxx.github.io/Fluent-Qt/gallery/` and its
  `hello-world/` consumer load over HTTPS without missing assets.
- [ ] Production smoke covers startup, route navigation, browser text input,
  menus, resize/maximize, OpenWindow, Simplified Chinese fallback, and
  WebLocalStorage persistence.
- [ ] Record the deployed revision, URL, date, and smoke result in the
  verification log or release notes.

## Verification log

| Date | Scope | Result | Evidence / blocker |
| --- | --- | --- | --- |
| 2026-08-09 | Local toolchain | Pass | Detected Qt 6.9.3 `wasm_singlethread`; installed and activated official emsdk 3.1.70 after confirming the Qt installer had not provided an Emscripten compiler. |
| 2026-08-09 | Configure and build | Pass | `cmake --preset wasm` and `cmake --build --preset wasm --parallel 6` built FluentQt, Hello World, and the C++ Gallery. |
| 2026-08-09 | Fast browser smoke | Pass | CI Playwright runner reported 4 routes, WebLocalStorage, asynchronous dialog/menu, render-profile, and Hello World canvas checks passed. |
| 2026-08-09 | Full browser smoke | Pass | At a simulated native DPR 2, CI Playwright reported all 88 routes, WebLocalStorage, asynchronous dialog/menu, render-profile, and Hello World canvas checks passed. |
| 2026-08-09 | Browser performance | Pass | Removing Gallery-wide Asyncify reduced `fluent_qt_gallery.wasm` from 33,109,235 to 23,545,148 bytes (-28.9%). At DPR 2, the final default 1.25x profile completed the full smoke in 11.70 s (slowest route 185 ms), versus 24.37 s at native 2x (slowest route 449 ms). |
| 2026-08-09 | Pages payload | Pass | `stage-wasm-pages.py` staged the whitelisted app, license files, and `build-info.json`; CI classifier and workflow-boundary tests passed. |
| 2026-08-09 | Source package | Pass | Generated `FluentQt-1.6.1-source.zip`, configured it with the WebAssembly toolchain, built Hello World, installed the development component, and verified exported `FluentQt::WasmAsyncify`. |
| 2026-08-09 | Adapter decoupling | Pass | Moved browser lifetime, settings, theme persistence, and distribution behavior behind selected platform adapters; native Gallery/Hello World builds, focused Gallery tests, and the full 88-route browser smoke passed. |
| 2026-08-09 | Windowed browser shell | Pass | At a 1280x720 browser viewport, visual QA measured a centered 922x600 app surface with Fluent client chrome, rounded clipping, shadow, and a footer switch to maximized mode. The HTML shell owns the stage; `FluentQt::WebAssembly` owns the single Qt desktop surface and hosted widget geometry. |
| 2026-08-09 | Simplified Chinese fallback | Pass | Generated a 1,990,352-byte GB2312 subset from pinned Noto Sans SC, embedded it only in WebAssembly builds, and verified `八月` plus `周一` through `周日` in CalendarView. The automated smoke also verified Han fallback registration and required glyph coverage. The final Gallery WASM is 25,524,513 bytes, +1,979,365 bytes (+8.4%) from the pre-fallback 23,545,148-byte build. |
| 2026-08-09 | Installed UILib consumer | Pass | Installed the WebAssembly Development component to an isolated prefix, configured `examples/hello_world` against the exported `FluentQt_DIR`, and built `fluentqt_hello_world.wasm`. The staged Gallery footer exposes the same minimal app at `hello-world/`. |
| 2026-08-09 | Productized full browser smoke | Pass | At simulated DPR 2 with the default 1.25x profile, all 88 routes, WebLocalStorage, async dialog/menu paths, CJK fallback, windowed shell, and staged Hello World passed in 8.22 s; the slowest route was `list-view` at 130 ms. |
| 2026-08-09 | Adaptive rendering and bounded cache | Pass | At 1920x1080 / DPR 2, fast smoke completed in 1.06 s at adaptive 1x (slowest route 171 ms), 1.25 s at balanced 1.25x (207 ms), and 2.15 s at native 2x (403 ms). Full 88-route smoke passed in 8.27 s and kept heap capacity at 128 MiB with a 34 -> 93 MiB program break; the previous unbounded cache grew capacity from 128 to 153 MiB. Native `test_gallery_shell_framework` passed 58/58 with the new LRU contract. |
| 2026-08-09 | Browser parity regression set | Pass | In a 1280x720 real browser, the 922x600 Window resized to 1095x600 and crossed the responsive navigation breakpoint; maximize/restore returned to the resized geometry and OpenWindow remained a bounded Fluent child window. Fast windowed and maximized smokes passed, then the full smoke passed all 88 routes in 9.31 s and explicitly validated a three-action menu plus the seven-action PasswordBox editing menu. The 199-test focused native typography, list bounce, menus, text fields, and windowing suite had 0 failures; 25 platform/manual visual cases were skipped as expected. |
| 2026-08-09 | Title clarity and repaint cost | Pass | Fixed two independent causes: explicit 1x is now identified as a soft HiDPI performance mode, and hosted browser chrome is forced active and painted as an opaque token surface. The static software material is cached between invalidating changes. At DPR 2 / render DPR 1.25, the full 88-route smoke passed title/cache contracts, storage, window, dialog, menus, CJK fallback, and Hello World in 7.01 s; the slowest route took 103 ms and heap capacity remained 128 MiB. The 200-test focused native suite had 0 failures with 25 expected skips. Multithreading remains an experiment because the installed Qt SDK provides only `wasm_singlethread` and a threaded kit would not move QWidget GUI work off the main thread. |
| 2026-08-09 | Browser text input | Pass | Removed the non-activating flags from the single WebAssembly desktop surface so Qt can focus its hidden HTML input bridge. Real-browser physical keys updated LineEdit, PasswordBox, TextEdit, NumberBox, and AutoSuggestBox; the latter emitted `UserInput` and opened suggestions. The automated DPR 2 / render DPR 1.25 full smoke now drives physical keys into a real Fluent LineEdit and passed all 88 routes, text editing menus, and Hello World in 6.89 s. The change remains isolated to `FluentQt::WebAssembly`; native text controls are unchanged. |
| 2026-08-09 | Pre-integration local gates | Pass | CI classifier tests (14/14), modular workflow boundaries, Gallery/UILib boundary, project metadata, all nine package scenarios, CI C++ matrix tests (6/6), reproducible pinned WebAssembly font output, and the 16-file Pages payload all passed. The complete 1,378-test macOS CTest inventory exposed one scattered Qt-version guard; after moving the Qt 6.8 font fallback API behind `QtCompat.h`, that test was rebuilt and passed, leaving no runnable native failures. A fresh WebAssembly rebuild and full browser smoke then passed all 88 routes in 6.89 s. |
| 2026-08-10 | Release branch integration | Pass | PR [#27](https://github.com/calvinhxx/Fluent-Qt/pull/27) was consolidated into three commits and rebase-merged into `release/1.6.x` at `99701c3`; local and remote `codex/web-sup` refs were removed, while `main` remained unchanged. |
| 2026-08-10 | Remote fast CI | Pass | PR #27 pre-integration fast CI passed the WebAssembly browser smoke, the three PySide6 compatibility/release paths, five native C++/integration paths, and `CI Gate`. Its temporary-branch Actions history is pruned after integration while this result summary remains in the roadmap. |
| 2026-08-10 | Remote full CI | Pass | Pre-integration full CI passed all 44 jobs, including Qt 5.15/6.2 native compatibility, x64/ARM64 packages and tests, sanitizer contracts, Python 3.10-3.13 release wheels, `CI Gate`, and `Release ready`. The WebAssembly full smoke traversed all 88 routes in 9.705 s at DPR 2 / render DPR 1.25; browser elapsed time was 10.17 s, the slowest route took 168 ms, and heap capacity stayed at 128 MiB. The temporary-branch run record is intentionally removed after integration. |
| 2026-08-10 | Pages deployment readiness | Pass | Fast and full CI staged and uploaded the `/gallery/` payload, including the minimal `hello-world/` consumer, licenses, and `build-info.json`. Public deployment awaits maintainer-owned promotion to `main` and is tracked by the production acceptance checklist. |

## Progress update rule

When a milestone changes state, update both the status table and verification
log in the same change. Record commands and concrete pass/fail evidence in the
log; do not mark browser support verified from a native build alone. This file
is the only roadmap status source. A pending production-acceptance item does not
reopen a completed development milestone unless it reveals a product defect.
