## ADDED Requirements

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

### Requirement: Vendored GoogleTest source is removed after migration
The repository SHALL remove the vendored GoogleTest source tree once all test targets build against the vcpkg package.

#### Scenario: Repository no longer contains vendored GoogleTest
- **WHEN** the migration is complete
- **THEN** `third_party/googletest` MUST be absent from the repository
- **AND** no CMake file MUST reference `third_party/googletest`

#### Scenario: Build remains reproducible after removal
- **WHEN** `third_party/googletest` has been removed and the project is configured with a vcpkg preset
- **THEN** all existing test targets MUST configure and build using the vcpkg-provided GoogleTest package
