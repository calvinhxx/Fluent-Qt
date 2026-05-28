## ADDED Requirements

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