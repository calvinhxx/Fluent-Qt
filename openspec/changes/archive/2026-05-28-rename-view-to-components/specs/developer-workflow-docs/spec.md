## ADDED Requirements

### Requirement: Development docs reference component paths
Reusable development workflow documentation SHALL describe reusable widgets as components under the component source tree.

#### Scenario: Component guidance uses canonical paths
- **WHEN** README, docs under `docs/development/`, docs under `docs/architecture/`, and agent guidance mention reusable component source or tests
- **THEN** they MUST reference `src/components/` and `tests/components/`
- **AND** they MUST NOT present `src/view/` or `tests/views/` as active canonical locations

#### Scenario: App view guidance remains distinct
- **WHEN** Gallery application structure is described
- **THEN** root-level `app/view/` MUST be described as application UI/view code
- **AND** it MUST NOT be conflated with reusable component implementation

### Requirement: Workflow examples use renamed component test binaries
Reusable workflow documentation SHALL update focused build, CTest, VisualCheck, and snapshot examples to the renamed test tree.

#### Scenario: VisualCheck direct-run examples use tests/components
- **WHEN** a maintainer reads VisualCheck or testing workflow examples
- **THEN** direct binary paths MUST use `build/<preset>/tests/components/<category>/test_<name>` style locations
- **AND** the examples MUST preserve `SKIP_VISUAL_TEST=1`, `VISUAL_SNAPSHOT=1`, and `--gtest_filter="*VisualCheck*"` semantics
