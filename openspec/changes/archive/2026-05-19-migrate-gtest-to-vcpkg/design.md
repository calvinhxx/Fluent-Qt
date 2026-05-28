## Context

The project currently vendors GoogleTest under `third_party/googletest` and adds it to the build with `add_subdirectory(third_party/googletest)`. Test targets are created through `add_qt_test_module(...)`, link `GTest::gtest_main`, and use repeated fixture-level setup to lazily create `QApplication`, load resources, load Fluent fonts, set `Fusion`, and handle offscreen automation.

This works, but it mixes a test-only external dependency into the source tree and leaves Qt test process initialization scattered across many `Test*.cpp` files. The desired direction is to make GoogleTest a declared package dependency and then centralize Qt-aware GoogleTest startup without changing component behavior.

## Goals / Non-Goals

**Goals:**

- Move GoogleTest dependency management to vcpkg manifest mode.
- Use `find_package(GTest CONFIG REQUIRED)` instead of adding the vendored GoogleTest source tree.
- Add CMake presets that encode the vcpkg toolchain/triplet contract without hard-coding one developer's absolute vcpkg path.
- Remove `third_party/googletest` once all test targets consume vcpkg GoogleTest.
- Add a shared Qt-aware GoogleTest entry point that creates and configures `QApplication` once per test process.
- Preserve existing test target names, CTest discovery, VisualCheck skip behavior, and direct binary execution.

**Non-Goals:**

- Do not migrate Qt itself to vcpkg in this change.
- Do not rewrite tests from GoogleTest to Qt Test.
- Do not redesign VisualCheck UI content or component test assertions.
- Do not introduce a new CI provider or unrelated build-system restructuring.
- Do not change production component APIs or source layout.

## Decisions

### 1. Use vcpkg manifest mode for GoogleTest only

Add `vcpkg.json` declaring the `gtest` dependency. CMake will consume the package with:

```cmake
find_package(GTest CONFIG REQUIRED)
```

The existing Qt discovery remains:

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test Network)
```

Rationale: GoogleTest is a small test-only dependency and is a good fit for vcpkg. Qt is already supplied by the user's Qt installation/Qt Creator workflow, and moving Qt at the same time would create a much larger migration.

Alternative considered: keep vendored GoogleTest. That avoids requiring vcpkg, but keeps a large third-party tree in the repository and makes dependency updates a source-copy operation.

### 2. Prefer presets over hidden local assumptions

Add `CMakePresets.json` with vcpkg-enabled configure presets. The presets should use `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` and platform triplets such as `x64-osx`, `x64-windows`, and `x64-linux` where appropriate.

Rationale: this makes the expected toolchain explicit while keeping the repository independent of a particular developer path. Developers can still configure manually by passing `-DCMAKE_TOOLCHAIN_FILE=...` if they do not use presets.

Alternative considered: require users to set the toolchain manually. That works but leaves the project with no documented canonical configure path.

### 3. Keep `add_qt_test_module(...)` as the target-level API

The test CMake macro remains the single entry point for registering Qt widget tests. Its internals change from vendored `GTest::gtest_main` to vcpkg-provided GoogleTest targets and shared test support sources.

Rationale: existing per-component `CMakeLists.txt` files stay small and do not need mass churn.

### 4. Introduce a shared Qt-aware GoogleTest main

Add test support code, for example:

```text
tests/support/
  QtGTestMain.cpp
  QtTestEnvironment.h
  QtTestEnvironment.cpp
```

`QtGTestMain.cpp` owns process-level setup:

- initialize GoogleTest;
- set `QT_QPA_PLATFORM=offscreen` before creating `QApplication` when `SKIP_VISUAL_TEST=1` and no platform is already set;
- create `QApplication`;
- set `Fusion` style;
- initialize `resources.qrc`;
- load Segoe Fluent icon and UI fonts;
- run all tests.

Fixture-specific setup remains responsible only for per-test windows, themes, models, and component-specific metatype registration.

Rationale: Qt GUI tests require a `QApplication`, and centralizing that lifecycle removes repeated setup code and prevents different test files from drifting.

Alternative considered: keep `GTest::gtest_main` and lazy-create `QApplication` in fixtures. That requires less immediate change but keeps the current duplication.

### 5. Migrate in two passes

First make GoogleTest come from vcpkg while keeping test behavior equivalent. Then add the shared Qt-aware main and incrementally remove redundant fixture-level initialization.

Rationale: dependency migration failures and test lifecycle failures have different root causes. Separating them makes regressions easier to diagnose.

## Risks / Trade-offs

- vcpkg adds a local toolchain prerequisite -> mitigate with `CMakePresets.json`, clear error messages, and documentation for `VCPKG_ROOT`.
- CI or developer machines may lack cached vcpkg packages -> mitigate by documenting package restore/cache requirements and keeping Qt outside this migration.
- Different GTest target names/configs across package versions could break CMake -> mitigate by using the config package targets exported by vcpkg and validating on configured platforms.
- Moving from `GTest::gtest_main` to a custom main can change argument handling -> mitigate by preserving GoogleTest initialization and running existing direct binaries and CTest.
- Centralizing Qt setup may reveal tests with hidden order assumptions -> mitigate by changing common setup first, then removing fixture duplication incrementally with targeted test runs.

## Migration Plan

1. Add vcpkg manifest and presets.
2. Replace vendored GoogleTest CMake integration with `find_package(GTest CONFIG REQUIRED)`.
3. Verify the existing test macro builds with vcpkg-provided GoogleTest.
4. Remove `third_party/googletest`.
5. Add `QtGTestMain.cpp` and shared Qt test support.
6. Update `add_qt_test_module(...)` to compile the shared main/support sources and link `GTest::gtest` instead of `GTest::gtest_main`.
7. Remove repeated common Qt initialization from test fixtures where it is now redundant.
8. Validate representative targeted tests and the broader CTest suite with `SKIP_VISUAL_TEST=1`.

Rollback is straightforward before deleting `third_party/googletest`: restore the previous `add_subdirectory` and `GTest::gtest_main` link. After deletion, rollback requires restoring the vendored directory from version control.

## Open Questions

- Which vcpkg baseline should be pinned for the manifest?
- Which presets should be mandatory for the project: macOS only initially, or macOS/Windows/Linux from the start?
- Should CI install vcpkg itself, rely on a checked-out vcpkg path, or use an existing cache provided by the environment?
