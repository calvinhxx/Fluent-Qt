## ADDED Requirements

### Requirement: ListView owns selected indicator rendering
`ListView` SHALL provide a component-owned selected indicator that is rendered consistently for selected items without requiring callers to implement indicator painting in their item delegate.

#### Scenario: Selected vertical item shows component indicator
- **WHEN** a `ListView` in `TopToBottom` flow has a valid selected item
- **THEN** the selected item SHALL show an accent indicator painted by `ListView`
- **AND** the indicator SHALL be a rounded vertical pill on the leading side of the selected item

#### Scenario: Selected horizontal item shows component indicator
- **WHEN** a `ListView` in `LeftToRight` flow has a valid selected item
- **THEN** the selected item SHALL show an accent indicator painted by `ListView`
- **AND** the indicator SHALL be a rounded horizontal pill along the bottom edge of the selected item

#### Scenario: No selected item hides indicator
- **WHEN** a `ListView` has no valid selected item or the model is empty
- **THEN** the selected indicator SHALL not be rendered

### Requirement: Vertical indicator motion is direction-aware
`ListView` SHALL animate vertical-flow selected indicator transitions with different motion texture for upward and downward selection changes.

#### Scenario: Moving selection downward
- **WHEN** selection changes from a visible item to a lower visible item in `TopToBottom` flow
- **THEN** the selected indicator SHALL animate from the previous item geometry to the new item geometry
- **AND** the downward transition SHALL visually lead from the lower edge before settling into the target indicator geometry

#### Scenario: Moving selection upward
- **WHEN** selection changes from a visible item to a higher visible item in `TopToBottom` flow
- **THEN** the selected indicator SHALL animate from the previous item geometry to the new item geometry
- **AND** the upward transition SHALL visually lead from the upper edge before settling into the target indicator geometry

#### Scenario: First vertical selection snaps to target
- **WHEN** a `TopToBottom` `ListView` receives its first valid selection without a previous selected item
- **THEN** the selected indicator SHALL appear at the target item geometry without using stale direction state

### Requirement: Horizontal indicator motion is direction-aware
`ListView` SHALL animate horizontal-flow selected indicator transitions with different motion texture for left-to-right and right-to-left selection changes.

#### Scenario: Moving selection right
- **WHEN** selection changes from a visible item to an item with a larger visual x position in `LeftToRight` flow
- **THEN** the selected indicator SHALL animate along the bottom edge from the previous item geometry to the new item geometry
- **AND** the rightward transition SHALL visually lead from the right edge before settling into the target indicator geometry

#### Scenario: Moving selection left
- **WHEN** selection changes from a visible item to an item with a smaller visual x position in `LeftToRight` flow
- **THEN** the selected indicator SHALL animate along the bottom edge from the previous item geometry to the new item geometry
- **AND** the leftward transition SHALL visually lead from the left edge before settling into the target indicator geometry

#### Scenario: First horizontal selection snaps to target
- **WHEN** a `LeftToRight` `ListView` receives its first valid selection without a previous selected item
- **THEN** the selected indicator SHALL appear at the target item geometry without using stale direction state

### Requirement: Indicator follows standard selection sources
`ListView` SHALL update selected indicator motion for pointer selection, keyboard/current-index selection, programmatic `setSelectedIndex`, and direct selection-model changes.

#### Scenario: Pointer selection updates indicator
- **WHEN** the user clicks a selectable item in `ListView`
- **THEN** the selected item SHALL update through the existing selection model
- **AND** the selected indicator SHALL transition to the clicked item according to the active flow and movement direction

#### Scenario: Programmatic selection updates indicator
- **WHEN** application code calls `setSelectedIndex` with a valid row
- **THEN** the selected item SHALL update
- **AND** the selected indicator SHALL transition to the requested item according to the active flow and movement direction

#### Scenario: Keyboard selection updates indicator
- **WHEN** keyboard navigation changes the current selected item
- **THEN** the selected indicator SHALL transition to the keyboard-selected item according to the active flow and movement direction

### Requirement: Indicator remains stable across layout and model changes
`ListView` SHALL keep selected indicator geometry synchronized with item layout while preserving existing list behavior.

#### Scenario: Resize recomputes indicator target
- **WHEN** a `ListView` is resized while it has a valid selected item
- **THEN** the selected indicator target geometry SHALL be recomputed from the selected item's current `visualRect`
- **AND** the selected item, model, scroll bars, and selection mode SHALL remain unchanged

#### Scenario: Scroll recomputes visible indicator target
- **WHEN** a selected item remains visible while `ListView` scroll position changes
- **THEN** the selected indicator SHALL remain aligned with the selected item's current visible geometry

#### Scenario: Drag reorder does not corrupt indicator state
- **WHEN** drag reorder changes row positions or suppresses normal item painting
- **THEN** the selected indicator SHALL not use stale previous geometry
- **AND** after reorder completes, the indicator SHALL settle on the current selected item's geometry

### Requirement: Indicator motion is testable and theme-aware
`ListView` SHALL expose enough deterministic state for automated tests and SHALL render the selected indicator with Fluent theme tokens.

#### Scenario: Tests can query indicator geometry
- **WHEN** automated tests need to validate selected indicator placement or transition state
- **THEN** `ListView` SHALL expose a deterministic selected indicator geometry query

#### Scenario: Theme change updates indicator colors
- **WHEN** the active Fluent theme changes
- **THEN** the selected indicator SHALL repaint using the current accent color without changing selection state

#### Scenario: VisualCheck demonstrates both flows
- **WHEN** `TestListView.VisualCheck` is run interactively
- **THEN** it SHALL include controls or sample lists that demonstrate upward/downward vertical indicator motion
- **AND** it SHALL include controls or sample lists that demonstrate left/right horizontal bottom-indicator motion
