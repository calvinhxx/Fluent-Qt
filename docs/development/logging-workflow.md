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

## Runtime Controls

- Default logging should stay conservative for normal local runs and CTest.
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
