# gridview-drag-reorder Specification

## Purpose
TBD - created by archiving change fix-gridview-drag-reorder-threshold-flicker. Update Purpose after archive.
## Requirements
### Requirement: GridView drag reorder activation

GridView SHALL start custom drag reorder only when `canReorderItems` is true, the user presses a valid item with the left mouse button, and the pointer moves at least the platform drag-start distance. Drag reorder SHALL use the existing item model and SHALL NOT require Qt native drag/drop.

#### Scenario: Drag starts after platform threshold
- **WHEN** `canReorderItems` is true and the user presses a valid item
- **AND** the pointer moves at least `QApplication::startDragDistance()` with the left button held
- **THEN** GridView SHALL enter drag reorder mode
- **AND** GridView SHALL create drag feedback for the source item or selected source items

#### Scenario: Drag disabled by flag
- **WHEN** `canReorderItems` is false
- **AND** the user presses and moves over GridView items
- **THEN** GridView SHALL NOT enter drag reorder mode
- **AND** GridView SHALL NOT emit `itemReordered`

### Requirement: Stable drop target near critical thresholds

GridView SHALL stabilize drop target selection during drag reorder so small cursor jitter around a critical insertion boundary does not rapidly alternate adjacent drop targets or restart displacement animations.

#### Scenario: Boundary jitter keeps current target
- **WHEN** GridView is in drag reorder mode with an active drop target
- **AND** subsequent pointer positions jitter within a small hysteresis band around the adjacent insertion boundary
- **THEN** GridView MUST keep the current drop target
- **AND** GridView MUST NOT restart displacement animations solely because of that jitter

#### Scenario: Clear crossing changes target
- **WHEN** GridView is in drag reorder mode with an active drop target
- **AND** the pointer clearly crosses beyond the hysteresis band into a neighboring insertion slot
- **THEN** GridView SHALL update the drop target to the neighboring slot
- **AND** GridView SHALL update displacement animations for the new stable target

#### Scenario: Animated offsets do not feed target oscillation
- **WHEN** item displacement animations are running during drag reorder
- **AND** the cursor remains near a critical boundary
- **THEN** GridView MUST NOT use changing animated offsets in a way that causes the drop target to oscillate between adjacent slots

### Requirement: Drag displacement animation lifecycle

GridView SHALL animate non-source item displacement only when the stabilized drop target changes meaningfully. Existing animations SHALL NOT be stopped and recreated when their target offset is unchanged.

#### Scenario: Unchanged target does not recreate animations
- **WHEN** GridView receives multiple mouse move events that resolve to the same stabilized drop target
- **THEN** GridView SHALL keep existing displacement animations or final offsets
- **AND** GridView MUST NOT stop and recreate equivalent animations for unchanged item targets

#### Scenario: New target retargets from current visual state
- **WHEN** the stabilized drop target changes while displacement animation is running
- **THEN** GridView SHALL animate affected items from their current visual offsets toward the new target offsets
- **AND** GridView SHALL avoid visual snapping of non-source items

### Requirement: Release-time reorder semantics

GridView SHALL mutate the model only on drag release, using the final stabilized drop target. The model reorder SHALL preserve existing selection behavior and emit one `itemReordered` signal for a completed drag.

#### Scenario: Model unchanged during drag
- **WHEN** GridView is in drag reorder mode and the pointer moves across candidate slots
- **THEN** GridView SHALL NOT mutate the model before the left mouse button is released

#### Scenario: Release applies one reorder
- **WHEN** the user releases the left mouse button while drag reorder has a valid final drop target
- **THEN** GridView SHALL move the dragged item or selected dragged items to that target
- **AND** GridView SHALL emit one `itemReordered` signal for the completed drag

#### Scenario: Selection behavior remains intact
- **WHEN** drag reorder completes in Single, Multiple, Extended, or None selection mode
- **THEN** GridView SHALL preserve the existing selection and current-index behavior already covered by GridView reorder tests

