# split-view Specification

## Purpose
Define the Fluent `SplitView` widget for the collections module: a Qt Widgets container that manages resizable pane layouts with orientation, pane constraints, fill pane behavior, draggable split handles, theme-aware visuals, state persistence, and automated/VisualCheck coverage.
## Requirements
### Requirement: SplitView public API
The system SHALL provide `view::collections::SplitView` as a Qt Widgets Fluent container for resizable pane layouts. The component MUST inherit `QWidget`, `FluentElement`, and `view::QMLPlus`, and MUST expose pane management, orientation, handle metrics, resizing state, and state persistence APIs.

#### Scenario: Default construction
- **WHEN** a `SplitView` is constructed
- **THEN** it MUST have horizontal orientation, zero panes, resizing state false, non-empty handle hit/visual metrics, and usable `sizeHint()` / `minimumSizeHint()` values

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/collections/SplitView.h`
- **THEN** `SplitView` MUST provide QWidget child containment, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Orientation property
- **WHEN** `setOrientation(Qt::Vertical)` or `setOrientation(Qt::Horizontal)` changes orientation
- **THEN** the component MUST relayout panes, update handle geometry, repaint, and emit `orientationChanged(Qt::Orientation)` exactly once

#### Scenario: Handle metric properties
- **WHEN** callers set handle hit width or visual thickness
- **THEN** the component MUST normalize invalid values, update layout/paint geometry, and emit the matching changed signal once per effective change

#### Scenario: Default size hint properties
- **WHEN** callers set `defaultSizeHint` or `defaultMinimumSizeHint`
- **THEN** the component MUST normalize invalid dimensions, update the value returned by `sizeHint()` or `minimumSizeHint()`, and emit the matching changed signal once per effective change

### Requirement: Pane management
The system SHALL allow callers to add, insert, remove, and query child panes while preserving QWidget parenting and visibility semantics.

#### Scenario: Add pane
- **WHEN** a QWidget pane is added
- **THEN** the pane MUST become a child of the `SplitView`, be included in pane order, receive geometry during layout, and `paneCount()` MUST increase by one

#### Scenario: Insert pane
- **WHEN** a QWidget pane is inserted at a valid index
- **THEN** subsequent panes MUST shift right, handle count MUST reflect visible pane pairs, and the inserted pane MUST use the provided pane options

#### Scenario: Remove pane
- **WHEN** a pane is removed by widget pointer or index
- **THEN** it MUST no longer participate in layout, handles MUST be recalculated, and the removed widget MUST be detached or scheduled according to the documented removal API without deleting unrelated panes

#### Scenario: Query pane index
- **WHEN** `indexOf(widget)` is called for a managed pane
- **THEN** it MUST return that pane's current ordered index
- **WHEN** it is called for an unmanaged widget
- **THEN** it MUST return `-1`

#### Scenario: Hidden pane exclusion
- **WHEN** a managed pane is hidden
- **THEN** it MUST not reserve split length or handles while hidden, and it MUST rejoin layout when shown again

### Requirement: Pane constraints and fill pane
The system SHALL support per-pane minimum, preferred, maximum, and fill settings equivalent to QML SplitView attached properties.

#### Scenario: Preferred size drives layout
- **WHEN** a pane has a preferred size for the active orientation
- **THEN** SplitView MUST use that preferred size as the pane's target length before distributing leftover space

#### Scenario: Minimum and maximum constraints
- **WHEN** a pane's preferred size or drag result is below its minimum or above its maximum
- **THEN** SplitView MUST clamp the pane length within `[minimum, maximum]`

#### Scenario: Default fill pane
- **WHEN** no visible pane is explicitly marked as fill
- **THEN** the last visible pane MUST receive remaining space after fixed panes and handles are laid out

#### Scenario: Explicit fill pane
- **WHEN** a pane is marked fill
- **THEN** that pane MUST receive remaining space while still respecting its minimum and maximum constraints

#### Scenario: Multiple fill panes
- **WHEN** multiple visible panes are marked fill
- **THEN** SplitView MUST choose the first visible fill pane as the effective fill pane and treat other panes by their preferred sizes

#### Scenario: Constraint setters
- **WHEN** callers update a pane's minimum, preferred, maximum, or fill setting
- **THEN** SplitView MUST relayout, repaint handles if needed, and emit a pane configuration change signal with the pane index

### Requirement: Split handle interaction
The system SHALL render and handle draggable split handles between adjacent visible panes.

#### Scenario: Handle geometry count
- **WHEN** there are N visible panes where N is greater than one
- **THEN** SplitView MUST expose and render N - 1 handle hit rectangles between adjacent panes

#### Scenario: Hover state
- **WHEN** the pointer moves over a handle
- **THEN** SplitView MUST update hovered handle index, use the orientation-appropriate resize cursor, and repaint that handle using hover visual state

#### Scenario: Pressed and resizing state
- **WHEN** the user presses a handle with the left mouse button
- **THEN** SplitView MUST capture dragging state, set `resizing()` to true, emit `resizingChanged(true)`, and paint the handle using pressed visual state

#### Scenario: Drag updates adjacent preferred sizes
- **WHEN** the user drags a pressed handle
- **THEN** SplitView MUST update preferred sizes for the two adjacent panes in the active orientation while preserving their combined size except when constraints prevent it

#### Scenario: Release ends resizing
- **WHEN** the user releases the pressed handle
- **THEN** SplitView MUST clear pressed state, set `resizing()` to false, emit `resizingChanged(false)`, and emit a final pane size change signal if sizes changed

#### Scenario: Disabled input
- **WHEN** SplitView is disabled
- **THEN** handle hover, press, drag, and resize operations MUST NOT change pane sizes or resizing state

### Requirement: Layout and geometry
The system SHALL arrange panes and handles deterministically for horizontal and vertical orientations.

#### Scenario: Horizontal layout
- **WHEN** orientation is `Qt::Horizontal`
- **THEN** panes MUST be laid out left-to-right, handles MUST span the SplitView height, and pane lengths MUST correspond to widths

#### Scenario: Vertical layout
- **WHEN** orientation is `Qt::Vertical`
- **THEN** panes MUST be laid out top-to-bottom, handles MUST span the SplitView width, and pane lengths MUST correspond to heights

#### Scenario: Cross-axis fill
- **WHEN** panes are laid out in either orientation
- **THEN** every visible pane MUST fill the cross-axis extent of the SplitView content rect

#### Scenario: Geometry query helpers
- **WHEN** tests query `paneGeometry(index)` or `handleGeometry(index)`
- **THEN** SplitView MUST return the latest layout rectangles for valid indexes and empty rectangles for invalid indexes

#### Scenario: Resize event
- **WHEN** SplitView itself is resized
- **THEN** it MUST recompute pane and handle geometry while preserving current preferred sizes and fill-pane behavior

### Requirement: Fluent visual states and theming
The system SHALL paint SplitView handles using Fluent design tokens and update visuals for Light, Dark, hover, pressed, disabled, and theme changes.

#### Scenario: Rest handle visual
- **WHEN** SplitView is enabled and no handle is hovered or pressed
- **THEN** handles MUST render as subtle Fluent separator lines centered in their hit rectangles

#### Scenario: Hover handle visual
- **WHEN** a handle is hovered
- **THEN** that handle MUST render with a stronger subtle fill or stroke while non-hovered handles remain in rest state

#### Scenario: Pressed handle visual
- **WHEN** a handle is pressed or being dragged
- **THEN** that handle MUST render with the pressed/accent visual state

#### Scenario: Disabled visual
- **WHEN** SplitView is disabled
- **THEN** handles MUST render with disabled semantic colors and ignore input

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** SplitView MUST refresh cached colors and repaint without changing pane sizes

### Requirement: State persistence
The system SHALL allow callers to save and restore SplitView pane sizes for the current pane order.

#### Scenario: Save state
- **WHEN** `saveState()` is called
- **THEN** SplitView MUST return a non-empty `QByteArray` containing versioned orientation, fill pane index, pane count, and preferred sizes

#### Scenario: Restore matching state
- **WHEN** `restoreState(state)` is called with a valid state matching the current pane count
- **THEN** SplitView MUST restore orientation, fill pane index, and preferred sizes, relayout panes, and return true

#### Scenario: Reject mismatched state
- **WHEN** `restoreState(state)` is called with malformed data or a pane count mismatch
- **THEN** SplitView MUST return false and MUST NOT partially mutate existing pane sizes or orientation

### Requirement: Tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for SplitView.

#### Scenario: Test target registration
- **WHEN** tests are configured
- **THEN** `tests/views/collections/CMakeLists.txt` MUST register `test_split_view`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_split_view` is run
- **THEN** tests MUST cover default properties, pane add/insert/remove, orientation, min/preferred/max constraints, fill pane behavior, handle geometry, drag resizing, disabled input, theme repaint safety, and save/restore state

#### Scenario: VisualCheck QML-style splitter
- **WHEN** `test_split_view --gtest_filter="*VisualCheck*"` is run manually
- **THEN** VisualCheck MUST display a horizontal and vertical resizable multi-pane SplitView with at least three panes and visible hover/pressed handles

#### Scenario: VisualCheck WinUI-style pane/content sample
- **WHEN** `test_split_view --gtest_filter="*VisualCheck*"` is run manually
- **THEN** VisualCheck MUST display a WinUI Gallery-inspired pane/content layout with pane placement and size controls using the custom SplitView component
