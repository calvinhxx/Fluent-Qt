## Context

`Fluent-QT` is a Qt Widgets component library with many focused GoogleTest binaries and interactive VisualCheck cases. Runtime diagnostics are currently limited to a few direct Qt logging calls such as `qWarning()` and `qDebug()`, so automated test failures and visual debugging often lack a consistent trace of component state.

The project already uses vcpkg manifest mode for GoogleTest, discovers Qt through the existing `find_package(QT ...)` flow, and centralizes Qt test process startup in `tests/support/QtGTestMain.cpp`. This makes the lowest-risk integration point a project-level logging utility plus shared test initialization, rather than adding spdlog usage directly inside each widget.

## Goals / Non-Goals

**Goals:**
- Add spdlog through vcpkg and CMake in the same style as existing manifest-managed dependencies.
- Provide a lightweight project logging facade so component code does not depend directly on scattered spdlog calls.
- Initialize logging once per process and make repeated initialization safe for tests.
- Capture Qt logging output through a message handler bridge.
- Provide predictable defaults and environment-based log level control.
- Document how future component log anchors should be added.

**Non-Goals:**
- Add detailed logging to every existing component.
- Add a UI log viewer, DebugOverlay integration, or spdlog Qt widget sink in the first pass.
- Switch tests to assert on log text except for focused initialization/bridge tests.
- Introduce asynchronous logging by default.
- Change widget public APIs or component ownership boundaries.

## Decisions

### Use vcpkg's compiled spdlog target

The change should add `spdlog` to `vcpkg.json`, call `find_package(spdlog CONFIG REQUIRED)`, and link the library target against `spdlog::spdlog`.

Alternatives considered:
- Header-only spdlog: simpler link setup, but it increases compile cost across many test binaries and exposes more dependency detail through headers.
- Vendoring spdlog source: inconsistent with the existing vcpkg manifest direction and creates extra maintenance work.

### Hide spdlog behind a project facade

Add a small utility under `src/utils`, for example `Log.h` and `Log.cpp`, with a namespace such as `utils::logging`. The facade should expose initialization, shutdown/flush helpers, level parsing, and generic logging macros or functions such as `LOG_DEBUG(...)`.

The facade should avoid leaking spdlog types in most component code. Direct spdlog includes should stay inside the logging utility implementation where practical.

Alternatives considered:
- Calling `spdlog::info(...)` directly from components: fast to start, but creates a dependency pattern that is hard to change later.
- Reusing Qt logging categories only: integrates with Qt, but does not provide the richer sink, formatting, file rotation, and AI-readable log capture model requested for the project.

### Keep default sinks minimal

The default logger should use a console sink suitable for local and test output. A file sink may be opt-in through an environment variable or initializer option, but the first pass should not force logs into a user-specific path.

Suggested environment knobs:
- `SPDLOG_LEVEL`, with values compatible with common levels such as `trace`, `debug`, `info`, `warn`, `error`, `critical`, and `off`.
- Optional `SPDLOG_FILE` for future or first-pass file capture if implementation remains small.

Alternatives considered:
- Always writing rotating logs: useful for debugging, but it introduces path, cleanup, and CI artifact questions before the project has a clear policy.
- Qt widget sink: useful for a future diagnostics panel, but not needed for a lightweight infrastructure change.

### Bridge Qt messages into the logger

The logging utility should offer an opt-in install function for `qInstallMessageHandler`. Test startup should install the bridge so existing `qDebug`, `qInfo`, `qWarning`, `qCritical`, and `qFatal` messages are routed through the same logger.

The bridge must preserve fatal behavior. It should avoid recursive logging if spdlog itself fails and should tolerate calls before full QApplication setup.

Alternatives considered:
- Bulk replacing existing Qt logging calls immediately: broader than this change and not needed to gain unified capture.
- Ignoring Qt messages: leaves existing diagnostics split between Qt output and spdlog output.

### Initialize in shared test startup

`tests/support/QtGTestMain.cpp` should initialize project logging before running tests. This gives every generated test binary consistent defaults without requiring per-test changes and preserves the existing `add_qt_test_module(...)` workflow.

The implementation should remain compatible with direct binary execution and CTest's `SKIP_VISUAL_TEST=1` flow.

### Add future anchors gradually

This change should include guidance for later log anchors but only add minimal smoke usage or existing warning migration where it proves the facade works. Good future anchors include:
- component lifecycle and style/theme refresh
- layout calculations and geometry decisions
- current index or current page transitions
- user input decisions such as wheel, click, hover, drag, and key handling
- invalid range or binding failures

Anchor messages should include stable component names and structured key/value details so they are useful to humans and AI tools.

## Risks / Trade-offs

- New dependency increases configure/build requirements -> keep it in vcpkg manifest mode and fail clearly through `find_package(spdlog CONFIG REQUIRED)`.
- Direct spdlog exposure can spread quickly -> require component code to use the project facade for new logs.
- Log volume can make tests noisy -> default to `warn` or another conservative level and allow environment overrides.
- Qt message handler can accidentally recurse or change fatal behavior -> implement a small guarded bridge and preserve Qt fatal termination semantics.
- File logging can create machine-specific paths -> make file output opt-in and document the behavior.
- AI-readable logs are only valuable when messages are structured -> document anchor conventions and avoid placeholder messages such as `here` or `xxx`.
