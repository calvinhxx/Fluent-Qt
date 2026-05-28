## 1. Dependency and Build Wiring

- [x] 1.1 Add `spdlog` to `vcpkg.json` without changing the existing vcpkg baseline or Qt discovery behavior.
- [x] 1.2 Add `find_package(spdlog CONFIG REQUIRED)` to the root CMake configuration before creating project targets.
- [x] 1.3 Link `fluent_qt_lib` against the compiled spdlog target, using public link visibility only if the logging facade header requires spdlog headers for callers.
- [x] 1.4 Confirm the existing `BUILD_TESTING=OFF` path still builds the component library without requiring GoogleTest.

## 2. Logging Facade

- [x] 2.1 Add a small logging utility under `src/utils`, such as `Log.h` and `Log.cpp`, with project-owned initialization, flush, shutdown, level parsing, and Qt message bridge entry points.
- [x] 2.2 Provide generic logging helpers or macros such as `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, and `LOG_ERROR`.
- [x] 2.3 Make logging initialization idempotent so repeated calls do not duplicate loggers, sinks, or Qt message handler setup.
- [x] 2.4 Configure conservative default logging and support environment-based level overrides through a documented project environment variable.
- [x] 2.5 Keep default sinks minimal; use console output by default and make file logging opt-in if implemented in this pass.
- [x] 2.6 Preserve Qt fatal message behavior when routing Qt messages through the project logger.

## 3. Test Integration and Coverage

- [x] 3.1 Initialize project logging from `tests/support/QtGTestMain.cpp` without disrupting offscreen setup, `QApplication` creation, or GoogleTest execution.
- [x] 3.2 Add a focused logging test target that uses the shared Qt GoogleTest main and verifies repeated initialization, supported/unsupported level configuration, and Qt warning bridge behavior.
- [x] 3.3 Register the focused test target through the existing test CMake workflow so component test directories do not need to repeat dependency wiring.
- [x] 3.4 Keep VisualCheck behavior unchanged: CTest-discovered tests still receive `SKIP_VISUAL_TEST=1`, while direct binaries can still run interactive VisualCheck cases.

## 4. Documentation and Anchor Guidelines

- [x] 4.1 Add README documentation for enabling logs, changing log level, optional file output if present, and expected local/CTest behavior.
- [x] 4.2 Document future log anchor conventions: stable component names, event names, and key/value fields for state, geometry, indices, ranges, and failure reasons.
- [x] 4.3 Document that this change intentionally does not add broad component instrumentation; future component changes should add targeted anchors only where they aid diagnosis.

## 5. Validation

- [x] 5.1 Run `openspec validate add-spdlog-logging --strict`.
- [x] 5.2 Configure with a vcpkg preset if needed, then build `fluent_qt_lib` and the focused logging test target.
- [x] 5.3 Run the focused logging test binary with `SKIP_VISUAL_TEST=1`.
- [x] 5.4 Run a small adjacent CTest sweep that includes the focused logging tests and at least one existing Qt widget test target to catch startup regressions.
