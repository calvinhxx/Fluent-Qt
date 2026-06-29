# Logging Workflow

Use this workflow when adding, reviewing, or debugging project logs; choosing
between the project logging facade and `spdlog`; setting runtime logging
environment variables; validating Qt log bridge behavior; or deciding where
diagnostic log anchors belong.

## Logging Entry Points

- The project uses `spdlog` behind the lightweight facade in `src/utils/Log.h`.
- Prefer project macros such as `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`,
  and `LOG_ERROR`.
- Do not scatter direct `spdlog::info(...)` or related calls inside components
  unless the logging facade itself is being implemented or tested.
- Tests share logging setup through `tests/support/QtGTestMain.cpp`; do not
  duplicate global logging initialization in individual test files.

## Layering Policy

- `fluent_qt_lib` is meant for standalone third-party use, so the library ships
  neutral defaults: level `Warn`, console output only, no file sink, no Qt
  message handler. The library never calls `utils::logging::initialize` on its
  own; the host application decides the policy.
- Reusable components under `src/components/` stay silent in normal operation.
  Use `qWarning` only for programmer errors such as contract violations or
  invalid arguments, and never log in `paintEvent` or other per-frame paths.
- The gallery app under `app/` owns the runtime policy: `app/main.cpp`
  initializes logging at `Info` with file persistence and the Qt message
  bridge, so component-layer `qWarning` output lands in the same log file.

## App Persistence and Rotation

`app/main.cpp` enables file logging with the defaults below; third-party
consumers of `fluent_qt_lib` opt in through their own
`utils::logging::InitializationOptions`.

- File path: `utils::logging::defaultLogFilePath()` resolves to
  `QStandardPaths::AppLocalDataLocation` + `/logs/<app>.log`.
  - macOS: `~/Library/Application Support/Fluent-QT/Fluent-QT Gallery/logs/`
  - Windows: `%LOCALAPPDATA%\Fluent-QT\Fluent-QT Gallery\logs\`
- Rotation is size-based, not per-launch: a full file shifts into numbered
  backups (`<name>.1.log`, `<name>.2.log`, ...). Defaults are
  `maxFileSizeBytes` = 5 MiB and `maxRotatedFiles` = 2. Note the spdlog
  semantics: `maxRotatedFiles` counts backups and the active file is extra, so
  2 backups means at most 3 files on disk.
- The logger flushes from `info` up: lifecycle lines reach the file as they
  happen and an abnormal exit cannot lose them, while `debug`/`trace` bursts
  stay buffered.

## Diagnostic Anchors

Add logs only when they help diagnose behavior. Good locations include:

- layout recalculation
- state transitions
- navigation or selection changes
- input, wheel, drag, and gesture handling
- popup, flyout, drawer, or animation lifecycle
- test setup failures and hard-to-see visual states

Keep log messages stable and searchable:

- Include a stable component or subsystem name, such as `NavigationView` or
  `StackContentHost`.
- Include a clear event name, such as `layout`, `setCurrentIndex`,
  `wheelInput`, or `bindFailed`.
- Use key/value state, such as `old=0 new=2 width=960 reason=invalid-range`.
- Avoid vague placeholders such as `here`, `xxx`, or `failed` without context.

```cpp
LOG_DEBUG("NavigationView layout mode=Top width=960");
LOG_WARN("QMLPlus bindFailed reason=host-not-qwidget");
```

## Gallery App Anchors

For the gallery app under `app/`, keep logs at workflow boundaries where a
route or page changes hands. These anchors should make a route lifecycle
searchable without logging every paint, delegate, or layout frame.

- `GalleryNavigationViewModel` should log catalog build counts, route counts,
  and the default route.
- `GalleryNavigationState` should log selected-route transitions with
  `old=<id> new=<id>`.
- `GalleryWindow` should log `selectRoute` and `applyRoute` decisions with
  `routeId`, rejection `reason`, and `pageType` when a page is created.
- `GalleryNavigationPane` should log user item clicks, child-toggle intent, and
  selection application or clearing. A missing index in the footer pane is a
  normal trace-level condition when the selected route belongs to the main pane.
- `GalleryPageFactory` should log rejected routes with `reason` and the
  placeholder fallback for routes without content metadata.
- Page objects should log page creation with the bound `routeId` and their
  build counts (cards, samples, related links). `GalleryComponentPage` warns
  when a component route resolves zero samples — that is a sample-catalog
  coverage gap, not a normal state.
- `GalleryEntryCard` should log activation clicks with the target `routeId`,
  so a route change can be traced back to a card click versus a nav-pane
  click.
- `GalleryCodeBlock` should log one line per expander toggle (state, content
  height, animated) and per copy action; the per-frame animation path stays
  quiet.
- `GalleryToast` should log each show and the `missing-host` rejection.
- Keep delegate paint paths, row size hints, animation ticks, and scroll-thumb
  painting quiet unless diagnosing a specific rendering bug.

## Runtime Controls

- Library and test runs stay conservative by default (`Warn`, console only);
  the gallery app persists `Info` and above to the platform log file by
  default.
- `SPDLOG_LEVEL` overrides the effective level even when the application
  passed an explicit default, so a verbose repro never requires a rebuild.
  `SPDLOG_FILE` is a fallback: it applies only when no explicit
  `logFilePath` was configured (library and test runs, not the gallery app).
- Use `SPDLOG_LEVEL` for temporary debug verbosity:

```bash
SPDLOG_LEVEL=debug SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/test_project_logging
```

- Supported levels are `trace`, `debug`, `info`, `warn`, `error`, `critical`,
  and `off`.
- Invalid levels should fall back to the default behavior without breaking the
  run.
- Use `SPDLOG_FILE` when a repro needs a saved log:

```bash
SPDLOG_LEVEL=debug \
SPDLOG_FILE=build/logs/spdlog.log \
SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/components/navigation/test_navigation_view
```

## Qt Log Bridge

- The shared test entry initializes project logging.
- Qt messages from `qDebug`, `qInfo`, `qWarning`, `qCritical`, and `qFatal`
  should flow into the same logging output.
- When testing the bridge, validate output behavior through the shared test
  entry instead of adding per-test Qt message handlers.

## Scope Control

- Do not add broad logging across unrelated components in a feature change.
- Add logs near the behavior being changed or debugged, then keep only the
  anchors that remain useful after the fix.
- When a component already has clear state checks or test failures, prefer
  improving the assertion message before adding noisy logs.
