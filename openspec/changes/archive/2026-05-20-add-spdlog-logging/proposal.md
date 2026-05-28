## Why

The project currently has only scattered Qt debug output, which makes component behavior hard to diagnose consistently across local runs, headless tests, and future AI-assisted failure analysis.
Introduce spdlog lightly now so the project has a stable logging foundation while leaving detailed component log anchors to be added incrementally as real debugging needs appear.

## What Changes

- Add spdlog as a vcpkg-managed dependency for the component library and tests.
- Add a small project logging facade that wraps spdlog instead of exposing direct spdlog calls throughout components.
- Initialize logging from shared test process setup so automated and direct test runs get predictable logger behavior.
- Bridge Qt message output into the project logger so existing `qDebug`, `qWarning`, and related messages can be captured without a broad migration.
- Document basic logging usage, environment-based level control, and the intended pattern for adding future log anchors.
- Avoid broad component instrumentation in this change; future changes can add targeted debug/trace anchors around layout, state transitions, input handling, and test failures.

## Capabilities

### New Capabilities
- `project-logging`: Defines the lightweight spdlog-backed logging foundation, Qt message bridge, test initialization behavior, and conventions for future diagnostic anchors.

### Modified Capabilities

## Impact

- Affects `vcpkg.json`, root CMake dependency discovery/linking, `src/utils` logging support, shared Qt GoogleTest startup, and README documentation.
- Adds a third-party MIT-licensed dependency through vcpkg manifest mode.
- Does not introduce breaking public widget API changes.
- Does not require every existing component to add logging immediately.
