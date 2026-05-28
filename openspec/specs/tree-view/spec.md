# TreeView Specification

## Purpose

Defines TreeView selected indicator motion, hierarchy-aware rendering support, checkbox multi-selection synchronization, and related automated and visual coverage.

## Requirements

### Requirement: TreeView selected indicator motion state
The system SHALL maintain TreeView-owned motion state for the selected-row indicator when the current selected item changes.

#### Scenario: Initial selection is neutral
- **WHEN** a TreeView receives its first valid selected item
- **THEN** the indicator motion state MUST use a neutral direction and neutral hierarchy transition
- **AND** it MUST not reference an invalid previous item

#### Scenario: Downward movement classification
- **WHEN** selection changes from a visible item to another visible item below it in the TreeView visual order
- **THEN** the indicator motion state MUST classify the vertical direction as downward

#### Scenario: Upward movement classification
- **WHEN** selection changes from a visible item to another visible item above it in the TreeView visual order
- **THEN** the indicator motion state MUST classify the vertical direction as upward

#### Scenario: Invalid movement clears transient state
- **WHEN** selection is cleared, the model is reset, or either transition endpoint becomes invalid
- **THEN** TreeView MUST clear transient indicator motion state
- **AND** it MUST avoid painting stale motion for a previous item

### Requirement: TreeView hierarchy-aware indicator motion
The system SHALL classify selected indicator motion by hierarchy depth so nested TreeView navigation can distinguish moving inward from moving outward.

#### Scenario: Moving inward
- **WHEN** selection changes from an item to a visible descendant or deeper-level item
- **THEN** the indicator motion state MUST classify the hierarchy transition as inward

#### Scenario: Moving outward
- **WHEN** selection changes from an item to a visible ancestor or shallower-level item
- **THEN** the indicator motion state MUST classify the hierarchy transition as outward

#### Scenario: Same-level movement
- **WHEN** selection changes between visible items at the same hierarchy depth
- **THEN** the indicator motion state MUST classify the hierarchy transition as same-level or neutral
- **AND** the direction-aware classification MUST still be available

### Requirement: Direction and hierarchy aware indicator rendering support
The system SHALL expose additive TreeView APIs that let Fluent delegates render selected indicators using direction-aware and hierarchy-aware motion without owning the selection comparison logic.

#### Scenario: Delegate queries active motion
- **WHEN** a delegate paints the currently selected TreeView item during an active indicator transition
- **THEN** it MUST be able to query the normalized indicator progress, vertical direction, and hierarchy transition from TreeView

#### Scenario: Direction-aware visual differs by movement direction
- **WHEN** the selected indicator is animated after downward movement
- **THEN** the visual treatment MUST be observably different from the upward movement treatment while preserving the same final selected indicator geometry

#### Scenario: Hierarchy-aware visual differs by hierarchy transition
- **WHEN** the selected indicator is animated after inward movement
- **THEN** the visual treatment MUST be observably different from outward movement while preserving the same final selected indicator geometry

#### Scenario: Final selected indicator geometry is stable
- **WHEN** an indicator animation completes
- **THEN** the selected row MUST render the normal Fluent accent indicator in its final row-local position
- **AND** the animation MUST NOT change row height, indentation, text layout, check box layout, icon layout, or scroll position

#### Scenario: Motion disablement snaps to final state
- **WHEN** indicator motion animation is disabled for deterministic tests or caller preference
- **THEN** selection changes MUST update the selected indicator to its final state synchronously
- **AND** the query APIs MUST report completed progress without running an animation

### Requirement: TreeView checkbox selection state stays synchronized
The system SHALL keep checkable TreeView item check states synchronized with multi-selection state while preserving tree tri-state semantics.

#### Scenario: Parent selection cascades to descendants
- **WHEN** a checkable parent item is selected in Multiple or Extended TreeView selection mode
- **THEN** the parent item and all checkable descendants MUST become selected
- **AND** their check states MUST become `Qt::Checked`

#### Scenario: Child partial selection updates parent
- **WHEN** only some checkable children under a parent are selected
- **THEN** the parent item MUST not remain fully selected
- **AND** the parent check state MUST become `Qt::PartiallyChecked`

#### Scenario: All children selected updates parent
- **WHEN** all checkable children under a parent become selected
- **THEN** the parent item MUST become selected
- **AND** the parent check state MUST become `Qt::Checked`

#### Scenario: CheckBox delegate defers to TreeView selection sync
- **WHEN** the Fluent TreeView test delegate handles a checkbox click in Multiple or Extended selection mode
- **THEN** it MUST update the TreeView selection model
- **AND** TreeView MUST own descendant cascade and ancestor tri-state synchronization
- **AND** selected checkable rows MUST not render the normal selected indicator in addition to the checkbox

### Requirement: TreeView indicator tests and VisualCheck
The system SHALL include automated tests and VisualCheck coverage for TreeView selected indicator motion.

#### Scenario: Automated motion classification tests
- **WHEN** `SKIP_VISUAL_TEST=1` TreeView tests run
- **THEN** tests MUST cover initial neutral selection, upward movement, downward movement, inward hierarchy movement, outward hierarchy movement, same-depth movement, invalid index clearing, and animation-disabled snapping

#### Scenario: Delegate rendering integration test
- **WHEN** the Fluent TreeView test delegate paints a selected item while motion state is active
- **THEN** tests MUST verify that the delegate consumes TreeView motion state without changing item row geometry

#### Scenario: VisualCheck sample
- **WHEN** manual TreeView VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display a nested Fluent TreeView sample with actions or controls that demonstrate upward, downward, inward, and outward selected indicator transitions
- **AND** the VisualCheck layout MUST use repository VisualCheck conventions, including `AnchorLayout` for the primary arrangement and project Fluent components where equivalents exist

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** TreeView VisualCheck MUST skip without opening an interactive window
