# test-infrastructure Specification

## Purpose
Define repeatable Qt widget test infrastructure, including third-party test dependencies, CMake presets, CTest discovery behavior, and shared Qt-aware GoogleTest process setup.
## Requirements
### Requirement: GoogleTest dependency is managed by vcpkg
The test infrastructure SHALL consume GoogleTest through vcpkg manifest mode rather than vendoring the GoogleTest source tree in the repository.

#### Scenario: Configure with vcpkg preset
- **WHEN** a developer configures the project using a provided vcpkg CMake preset
- **THEN** CMake MUST use the vcpkg toolchain file from `VCPKG_ROOT`
- **AND** CMake MUST resolve GoogleTest through `find_package(GTest CONFIG REQUIRED)`
- **AND** CMake MUST continue resolving Qt through the existing Qt package discovery flow

#### Scenario: Configure without vendored GoogleTest
- **WHEN** `third_party/googletest` is absent
- **THEN** configuring and building tests MUST NOT require `add_subdirectory(third_party/googletest)`
- **AND** test targets MUST link against GoogleTest targets exported by the vcpkg package

#### Scenario: Missing vcpkg setup
- **WHEN** tests are enabled and the vcpkg toolchain or GoogleTest package is unavailable
- **THEN** CMake MUST fail during configuration with a clear package/toolchain error
- **AND** the failure MUST occur before compiling project sources

### Requirement: CMake presets define repeatable test configurations
The project SHALL provide CMake presets for vcpkg-enabled test builds so developers and automation can use a documented configure path.

#### Scenario: Preset uses environment-based vcpkg root
- **WHEN** a vcpkg configure preset is inspected
- **THEN** it MUST reference the vcpkg toolchain through an environment-derived path such as `$env{VCPKG_ROOT}`
- **AND** it MUST NOT hard-code one developer's absolute filesystem path

#### Scenario: Preset keeps tests enabled
- **WHEN** a vcpkg configure preset is used
- **THEN** `BUILD_TESTING` MUST be enabled unless the preset explicitly documents a non-test build
- **AND** generated test targets MUST remain discoverable by CTest

#### Scenario: Platform triplet is explicit
- **WHEN** a vcpkg configure preset is used
- **THEN** the preset MUST declare or inherit an explicit `VCPKG_TARGET_TRIPLET`
- **AND** the triplet MUST match the intended host platform for that preset

### Requirement: Existing Qt test target ergonomics are preserved
The test infrastructure SHALL preserve the existing `add_qt_test_module(...)` workflow for component test registration.

#### Scenario: Existing component CMakeLists remain small
- **WHEN** a component test target is registered under `tests/views/**/CMakeLists.txt`
- **THEN** it MUST continue to use `add_qt_test_module(<target> <source> ...)`
- **AND** component directories MUST NOT need to repeat GoogleTest package linking or Qt test support source lists

#### Scenario: CTest discovery still skips VisualCheck automatically
- **WHEN** tests are discovered through `gtest_discover_tests`
- **THEN** discovered tests MUST retain the `SKIP_VISUAL_TEST=1` environment setting
- **AND** interactive VisualCheck cases MUST be skipped during automated CTest runs

#### Scenario: Direct binary execution remains possible
- **WHEN** a developer runs a generated test binary directly
- **THEN** the binary MUST still execute GoogleTest normally
- **AND** VisualCheck cases MUST remain available when `SKIP_VISUAL_TEST` is not set

### Requirement: VisualCheck snapshot mode
The test infrastructure SHALL support a `VISUAL_SNAPSHOT=1` mode for VisualCheck tests that generates repeatable screenshot files without requiring manual window review.

#### Scenario: CTest automation still skips VisualCheck
- **WHEN** a VisualCheck test is run with `SKIP_VISUAL_TEST=1`
- **THEN** the VisualCheck test MUST skip without opening an interactive window
- **AND** it MUST NOT generate snapshot files even if `VISUAL_SNAPSHOT=1` is also present

#### Scenario: Direct snapshot run captures an image
- **WHEN** a developer runs a test binary directly with `VISUAL_SNAPSHOT=1` and a VisualCheck filter
- **THEN** the VisualCheck test MUST build its sample window, capture a PNG screenshot, and return without entering the manual `qApp->exec()` review loop
- **AND** the generated PNG MUST be written under the active CMake binary directory's `visual/` output folder
- **AND** the generated PNG MUST be non-empty

#### Scenario: Snapshot capture uses deterministic visual settings
- **WHEN** `VISUAL_SNAPSHOT=1` is active
- **THEN** the snapshot path MUST use a fixed window size chosen by the VisualCheck test or shared helper
- **AND** the Fluent theme MUST be set to a known value before capture
- **AND** Qt device scaling MUST be configured or verified so the captured device pixel ratio is repeatable
- **AND** pending Qt layout, polish, paint, and deferred-delete events MUST be processed before grabbing the window image

#### Scenario: Manual VisualCheck remains available
- **WHEN** a developer runs a VisualCheck test without `SKIP_VISUAL_TEST=1` and without `VISUAL_SNAPSHOT=1`
- **THEN** the VisualCheck test MUST preserve the existing manual review behavior using `qApp->exec()` until the window is closed

### Requirement: Snapshot output conventions
The test infrastructure SHALL use deterministic snapshot file naming and avoid baseline comparison in the first snapshot generation phase.

#### Scenario: Snapshot filenames are stable
- **WHEN** a VisualCheck snapshot is generated
- **THEN** the output path MUST include enough test identity, such as target, suite, test name, and optional variant, to avoid collisions between component VisualCheck cases
- **AND** repeated runs of the same snapshot MUST overwrite or replace the same output file path rather than creating timestamp-only filenames

#### Scenario: No pixel diff is performed
- **WHEN** snapshot generation completes successfully
- **THEN** the test MUST NOT compare the image against a committed baseline
- **AND** the test MUST NOT fail because of pixel differences from a previous run

### Requirement: Discovered Qt tests expose semantic CTest labels
The test infrastructure SHALL add semantic CTest labels to individual discovered GoogleTest cases while preserving the existing `add_qt_test_module(...)` registration workflow and base labels.

#### Scenario: Base labels remain available on every discovered test
- **WHEN** a test target is registered through `add_qt_test_module(<target> <source> ...)`
- **THEN** every discovered test case MUST retain the existing `qt`, `unit`, target, component, and source-directory labels
- **AND** component subdirectory `CMakeLists.txt` files MUST NOT need to repeat GoogleTest, Qt support, or label boilerplate

#### Scenario: VisualCheck tests receive visual and interactive labels
- **WHEN** a discovered GoogleTest case name contains the established `VisualCheck` token
- **THEN** its CTest test MUST include the `visual` label
- **AND** its CTest test MUST include the `interactive` label
- **AND** it MUST retain the `SKIP_VISUAL_TEST=1` environment setting for automated CTest runs

#### Scenario: Semantic labels classify slow animation and platform-specific tests
- **WHEN** discovered test case names use project-recognized tokens for animation, slow, Windows-specific, or macOS-specific behavior
- **THEN** CTest MUST add the matching `animation`, `slow`, `platform_windows`, or `platform_macos` label to those individual tests
- **AND** tests without those recognized tokens MUST NOT receive the corresponding semantic labels accidentally

#### Scenario: Anchored label filters select semantic slices
- **WHEN** a developer runs CTest with anchored label filters such as `-L '^visual$'`, `-L '^interactive$'`, `-L '^animation$'`, `-L '^platform_macos$'`, or `-L '^platform_windows$'`
- **THEN** CTest MUST be able to select the corresponding semantic test slice independently of category and component labels
- **AND** existing anchored filters for target, component, and category labels MUST continue to work

#### Scenario: Direct VisualCheck execution remains available
- **WHEN** a developer runs a test binary directly with a VisualCheck GoogleTest filter and without `SKIP_VISUAL_TEST=1`
- **THEN** VisualCheck tests MUST remain interactive and manually reviewable
- **AND** the new CTest labels MUST NOT change direct binary execution behavior

### Requirement: Qt-aware GoogleTest main centralizes process setup
The test infrastructure SHALL provide a shared Qt-aware GoogleTest entry point for Qt widget tests.

#### Scenario: QApplication is initialized once
- **WHEN** a Qt widget test binary starts
- **THEN** the shared test main MUST create exactly one `QApplication` before running tests
- **AND** individual test fixtures MUST NOT need to lazily create `QApplication` for common setup

#### Scenario: Common visual test environment is initialized
- **WHEN** a Qt widget test binary starts
- **THEN** the shared test main MUST set the `Fusion` style
- **AND** it MUST initialize repository Qt resources
- **AND** it MUST load the Segoe Fluent icon font and Segoe UI font used by component tests

#### Scenario: Offscreen automation remains automatic
- **WHEN** `SKIP_VISUAL_TEST=1` is set and `QT_QPA_PLATFORM` is unset
- **THEN** the shared test main MUST set `QT_QPA_PLATFORM=offscreen` before creating `QApplication`
- **AND** it MUST NOT override an explicitly configured `QT_QPA_PLATFORM`

#### Scenario: Component-specific setup remains local
- **WHEN** a test file needs component-specific metatypes, helper models, or fixture windows
- **THEN** that setup MUST remain in the test file or fixture
- **AND** the shared test main MUST NOT introduce component-specific dependencies into unrelated tests

### Requirement: Test-only helper models live in test support
The test infrastructure SHALL keep reusable test-only QObject models and fixture helpers under the test tree rather than compiling them into the production component library.

#### Scenario: Shared binding model is test-owned
- **WHEN** a reusable QObject model exists only to support property binding, fixture setup, or visual/unit tests
- **THEN** it MUST live under `tests/support/` or a specific `tests/views/**` test file
- **AND** it MUST NOT live under `src/` or be compiled into `fluent_qt_lib`

#### Scenario: Test helper naming is explicit
- **WHEN** a shared test-only model is introduced under `tests/support/`
- **THEN** its name MUST make the test-support role clear, such as `BindingTestModel` or `PropertyBindingTestModel`
- **AND** it MUST NOT use broad production-layer names such as `ViewModel` unless the class is promoted to a documented runtime API

#### Scenario: Specialized local mocks stay local
- **WHEN** a test needs component-specific property names, signals, or setup behavior
- **THEN** the helper MAY remain local to that test file or fixture
- **AND** the shared test support layer MUST NOT force unrelated tests to use a generic model

#### Scenario: Test support wiring stays out of runtime library
- **WHEN** a shared test helper uses `QObject` or `Q_OBJECT`
- **THEN** the test CMake wiring MUST make MOC/build behavior valid for test targets
- **AND** the helper MUST be linked only into relevant test binaries through test support sources or explicit test target sources

### Requirement: Vendored GoogleTest source is removed after migration
The repository SHALL remove the vendored GoogleTest source tree once all test targets build against the vcpkg package.

#### Scenario: Repository no longer contains vendored GoogleTest
- **WHEN** the migration is complete
- **THEN** `third_party/googletest` MUST be absent from the repository
- **AND** no CMake file MUST reference `third_party/googletest`

#### Scenario: Build remains reproducible after removal
- **WHEN** `third_party/googletest` has been removed and the project is configured with a vcpkg preset
- **THEN** all existing test targets MUST configure and build using the vcpkg-provided GoogleTest package
