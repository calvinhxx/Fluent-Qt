# Logging Workflow

Use this workflow when adding, reviewing, or debugging logs in the reusable
library, Gallery, or tests.

## Layering Contract

- `FluentQt::FluentQt` depends only on Qt Widgets. It uses Qt logging categories
  and must not link spdlog, install a global Qt message handler, create files, or
  choose an application's logging policy.
- Reusable code under `src/` uses `qCDebug`, `qCInfo`, `qCWarning`, or
  `qCCritical` with a category named `fluentqt.<subsystem>`.
- Categories are declared and defined in private library files. They are an
  implementation detail, not installed public API.
- Gallery and tests link the non-exported `FluentQtLoggingSupport` target from
  `support/logging/`. That application-side target uses spdlog for console/file
  sinks and bridges Qt messages into the same output in every build type.
- Only the executable owns bridge initialization and shutdown. The library
  never replaces the host application's Qt message handler.

Current library categories include:

```text
fluentqt.theme
fluentqt.windowing
```

Add a category only when a stable subsystem boundary exists. Keep normal
operation quiet and never log from paint, animation-tick, or other per-frame
paths.

## Choosing an Entry Point

In reusable library code:

```cpp
qCDebug(fluent::logging::windowingCategory)
    << "restore" << "state=" << state;
qCWarning(fluent::logging::themeCategory)
    << "unknown theme" << themeId;
```

In Gallery or test code:

```cpp
#include "support/logging/Log.h"

LOG_DEBUG("GalleryWindow routeChanged routeId=settings");
LOG_WARN("GalleryPageFactory createPage reason=missing-route");
```

Do not call spdlog directly outside the logging support implementation.
Tests share initialization through `tests/support/QtGTestMain.cpp`; individual
tests must not install their own global handler.

## Diagnostic Anchors

Useful anchors include layout recalculation, state transitions, navigation or
selection changes, input handling, popup/animation lifecycle boundaries, test
setup failures, and hard-to-see fallback decisions.

Keep messages stable and searchable:

- name the subsystem or component;
- name the event;
- express state as `key=value`;
- include a rejection or fallback reason;
- avoid vague messages such as `here` or `failed` without context.

## Runtime Controls

Qt categories are controlled by the host application or environment. Library
debug/info categories are disabled by default; warnings remain visible.

```bash
QT_LOGGING_RULES="fluentqt.*=true" ./my_application
QT_LOGGING_RULES="fluentqt.windowing.debug=true" ./my_application
```

Gallery and tests additionally support:

```bash
SPDLOG_LEVEL=debug SKIP_VISUAL_TEST=1 \
  ./build/vcpkg-osx/tests/test_project_logging

SPDLOG_LEVEL=debug \
SPDLOG_FILE=build/logs/gallery.log \
  ./build/vcpkg-osx/app/fluent_qt_gallery
```

Supported spdlog levels are `trace`, `debug`, `info`, `warn`, `error`,
`critical`, and `off`. `SPDLOG_FILE` is used when the executable did not provide
an explicit file path.

## Gallery Persistence and Rotation

`app/main.cpp` enables Gallery file logging. The default path is
`QStandardPaths::AppLocalDataLocation/logs/<app>.log`:

- macOS: `~/Library/Application Support/Fluent-Qt/Fluent-Qt Gallery/logs/`
- Windows: `%LOCALAPPDATA%\Fluent-Qt\Fluent-Qt Gallery\logs\`

Rotation is size-based. Defaults are a 5 MiB active file and two numbered
backups, for at most three files. Info and higher levels flush immediately;
debug and trace output may remain buffered until normal shutdown.

## Qt Message Bridge

The Gallery and shared test entry initialize the application logging support's Qt
message bridge. Messages from Qt and all `fluentqt.*` categories then use the
same sinks and retain their category name. Validate bridge behavior through
`test_project_logging`; do not duplicate the bridge in component tests.

## Scope Control

- Add logs near behavior being changed and retain only diagnostically useful
  anchors.
- Prefer a clearer assertion when a failing test already exposes the state.
- Do not broaden logging across unrelated components in a feature change.
- Do not expose `support/logging/` through install rules or exported targets.
