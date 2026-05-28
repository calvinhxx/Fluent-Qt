## ADDED Requirements

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