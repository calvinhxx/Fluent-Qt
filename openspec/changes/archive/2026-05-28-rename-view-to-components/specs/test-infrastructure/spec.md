## MODIFIED Requirements

### Requirement: Existing Qt test target ergonomics are preserved
The test infrastructure SHALL preserve the existing `add_qt_test_module(...)` workflow for component test registration.

#### Scenario: Existing component CMakeLists remain small
- **WHEN** a component test target is registered under `tests/components/**/CMakeLists.txt`
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

## ADDED Requirements

### Requirement: Component test path labels follow renamed tree
CTest labeling and test-output guidance SHALL reflect the `tests/components` tree while preserving existing component and category filters.

#### Scenario: Anchored component filters keep working
- **WHEN** tests are moved from `tests/views` to `tests/components`
- **THEN** existing target, component, category, `qt`, `unit`, `visual`, and `interactive` labels MUST remain available
- **AND** focused commands such as `ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure` MUST keep selecting the intended tests

#### Scenario: Source-directory labels update to components
- **WHEN** discovered tests receive source-directory labels after the move
- **THEN** they MUST use `components` instead of `views` for the component test tree label
- **AND** documentation examples MUST use the same terminology
