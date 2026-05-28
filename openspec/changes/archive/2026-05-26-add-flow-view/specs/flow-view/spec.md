## ADDED Requirements

### Requirement: FlowView Component Contract

`FlowView` SHALL provide a Fluent collection view for model/delegate driven variable-size item layouts without changing `GridView` fixed-cell behavior.

#### Scenario: FlowView accepts an existing item model and delegate

- WHEN a caller assigns a `QAbstractItemModel` and `QAbstractItemDelegate` to `FlowView`
- THEN the view MUST render model rows through the assigned delegate
- AND the view MUST not create or own business item data itself

#### Scenario: GridView remains the fixed-size grid owner

- WHEN `FlowView` is added to the collection components
- THEN existing `GridView` cell-size, grid-size, wrapping, and reorder behavior MUST remain unchanged

### Requirement: Variable-Size Row-Wrap Layout

`FlowView` SHALL lay out items left-to-right in model order and wrap to a new row when the next item cannot fit in the available viewport width.

#### Scenario: Items wrap according to their own widths

- GIVEN a `FlowView` with mixed item sizes
- WHEN the first two items fit on one row and the third item would exceed the available width
- THEN the first two items MUST remain on the first row
- AND the third item MUST start the next row

#### Scenario: Row height follows the tallest item in the row

- GIVEN a row containing items with different heights
- WHEN `FlowView` computes the next row position
- THEN the next row MUST start below the tallest item in the previous row plus vertical spacing

#### Scenario: Resize recomputes wrapping

- GIVEN a `FlowView` with multiple variable-size items
- WHEN the viewport width changes
- THEN item rectangles MUST be recomputed for the new width
- AND model order MUST remain unchanged

### Requirement: Item Size Resolution

`FlowView` SHALL resolve each item size from a configurable model role, delegate `sizeHint()`, or a default item size, and MUST clamp resolved sizes to configured minimum and maximum bounds when present.

#### Scenario: Model role supplies item size

- GIVEN `itemSizeRole` is configured
- AND the model returns a valid `QSize` for an item using that role
- WHEN `FlowView` lays out the item
- THEN that `QSize` MUST be used before delegate and default fallback sizes

#### Scenario: Delegate size hint supplies item size

- GIVEN `itemSizeRole` is not configured or returns no valid size
- AND the assigned delegate returns a valid `sizeHint()` for an item
- WHEN `FlowView` lays out the item
- THEN the delegate size hint MUST be used before the default item size

#### Scenario: Invalid or excessive sizes are bounded

- GIVEN an item resolves to an invalid, too-small, or too-large size
- WHEN `FlowView` lays out the item
- THEN the view MUST use a valid size within the configured minimum and maximum item size bounds

### Requirement: Geometry and Hit Testing

`FlowView` SHALL expose geometry APIs that match the actual variable-size layout.

#### Scenario: visualRect returns the real item rectangle

- GIVEN a valid model index in `FlowView`
- WHEN a caller requests `visualRect(index)`
- THEN the returned rectangle MUST match that item's computed viewport rectangle

#### Scenario: indexAt uses actual item rectangles

- GIVEN a point inside a visible item rectangle
- WHEN a caller requests `indexAt(point)`
- THEN the view MUST return that item's model index

#### Scenario: Empty space does not hit an item

- GIVEN a point inside the viewport but outside every item rectangle
- WHEN a caller requests `indexAt(point)`
- THEN the view MUST return an invalid model index

### Requirement: Scrolling and Viewport Chrome

`FlowView` SHALL support vertical scrolling for content taller than the viewport and SHALL use Fluent collection chrome consistent with existing collection views.

#### Scenario: Content height drives vertical scroll range

- GIVEN flow content taller than the viewport
- WHEN layout is computed
- THEN the vertical scroll range MUST allow reaching the final item
- AND a Fluent vertical scrollbar MUST be synchronized with the native scroll range

#### Scenario: Header and placeholder are displayed independently of item layout

- GIVEN `headerText` is set
- WHEN the view is shown
- THEN the header MUST appear above the flow viewport
- AND the header MUST not consume a row in the flow layout

#### Scenario: Placeholder appears for an empty model

- GIVEN the model is absent or contains zero rows
- AND `placeholderText` is set
- WHEN the view paints
- THEN the placeholder text MUST be visible in the viewport

### Requirement: Selection, Hover, and Keyboard Navigation

`FlowView` SHALL support click item selection, modifier-based multi-selection, hover feedback, and keyboard navigation over variable-size item geometry.

#### Scenario: Selection modes map to Qt item-view selection behavior

- WHEN a caller sets selection mode to none, single, multiple, or extended
- THEN `FlowView` MUST apply the corresponding Qt item-view selection behavior

#### Scenario: Multi-selection requires a modifier click

- GIVEN a `FlowView` in multiple or extended selection mode
- WHEN the user clicks an item without Control or Meta pressed
- THEN `FlowView` MUST clear the prior selection and select only that item
- WHEN the user clicks another item with Control or Meta pressed
- THEN `FlowView` MUST toggle that item without clearing the existing selected items

#### Scenario: Pointer drag does not rubber-band select items

- GIVEN a `FlowView` with multiple visible items
- WHEN the user drags the pointer across several item rectangles without starting a reorder operation
- THEN `FlowView` MUST NOT select every item intersecting the dragged rectangle

#### Scenario: Pointer selection uses hit-tested item rectangles

- GIVEN a visible item in `FlowView`
- WHEN the user clicks inside that item's rectangle
- THEN the item MUST become selected according to the active selection mode
- AND the view MUST emit an item-clicked signal for that model row

#### Scenario: Arrow navigation follows item geometry

- GIVEN a current item and nearby items above, below, left, or right
- WHEN the user presses an arrow key
- THEN the current item MUST move to the nearest appropriate item according to actual item rectangle positions

### Requirement: Optional Drag Reorder

`FlowView` SHALL optionally support drag reorder for compatible item models using actual flow rectangles rather than fixed grid slots.

#### Scenario: Drag reorder is disabled by default

- WHEN a `FlowView` is constructed
- THEN item dragging for reorder MUST be disabled by default

#### Scenario: Dragging computes a drop slot from variable item rectangles

- GIVEN `canReorderItems` is enabled
- AND the user drags an item across mixed-size items
- WHEN the drag position moves near a target item's leading or trailing edge
- THEN the drop indicator MUST represent the final insertion slot computed from actual item geometry and row-wrap layout
- AND the dragged source area MUST NOT be covered with a viewport-colored patch over displaced items

#### Scenario: Dragging an unselected item without modifiers selects only that item

- GIVEN `canReorderItems` is enabled
- AND another item is currently selected
- WHEN the user starts dragging an unselected item without Control or Meta pressed
- THEN `FlowView` MUST clear the previous selection and select only the dragged item

#### Scenario: Compatible model rows are reordered on drop

- GIVEN `canReorderItems` is enabled
- AND the model supports row mutation through `QStandardItemModel`
- WHEN the user drops an item at a valid slot
- THEN the model rows MUST be reordered
- AND `FlowView` MUST emit an item-reordered signal containing the source and destination rows

### Requirement: Theme and Accessibility

`FlowView` SHALL update its visual chrome when the Fluent theme changes and SHALL expose useful accessibility metadata.

#### Scenario: Theme update refreshes view chrome

- WHEN the Fluent theme changes
- THEN `FlowView` MUST refresh viewport background, border, header, placeholder, scrollbar, and drag feedback colors from theme tokens

#### Scenario: Accessible name falls back to header text

- GIVEN no explicit accessible name is set
- AND `headerText` is set
- WHEN accessibility metadata is queried
- THEN `FlowView` SHOULD expose the header text as its accessible name