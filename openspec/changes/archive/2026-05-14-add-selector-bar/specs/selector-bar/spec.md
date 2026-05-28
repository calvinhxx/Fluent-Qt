## ADDED Requirements

### Requirement: SelectorBar public API
The system SHALL provide `view::navigation::SelectorBar` as a Qt Widgets Fluent control for selecting one item from an ordered horizontal selector row. The component MUST inherit `QWidget`, `FluentElement`, and `QMLPlus`, and MUST expose item, selected-index, selected-item, geometry, theme, focus, accessibility, and testing APIs.

#### Scenario: Default construction
- **WHEN** a `SelectorBar` is constructed
- **THEN** it MUST contain zero items, use selected index `-1`, be enabled and focusable, and return usable `sizeHint()` and `minimumSizeHint()` values

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/navigation/SelectorBar.h`
- **THEN** `SelectorBar` MUST provide QWidget child containment, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Selected index property
- **WHEN** callers set `selectedIndex` to a valid visible enabled item index
- **THEN** the component MUST select that item, update item selected state, update row visuals, update accessibility text, and emit selection signals once

#### Scenario: Selected item query
- **WHEN** callers query the selected item while an item is selected
- **THEN** the component MUST return the selected `SelectorBarItem` data and its index without requiring callers to inspect internal layout records

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals or mutate item order, selection state, or layout scroll position

### Requirement: SelectorBar item management
The system SHALL allow callers to add, insert, update, remove, clear, and query SelectorBar items while preserving item order, metadata, visibility, and selected state.

#### Scenario: Add item with text
- **WHEN** `addItem("Overview")` is called on an empty SelectorBar
- **THEN** the component MUST append one visible enabled item with text `Overview`, increase `itemCount()` to `1`, emit `itemsChanged()`, and select the first visible enabled item automatically

#### Scenario: Add item record
- **WHEN** callers add a `SelectorBarItem` record with text, icon glyph, enabled state, visible state, data, and accessible name
- **THEN** the component MUST preserve that metadata and expose it through `itemAt()` and `items()`

#### Scenario: Insert item
- **WHEN** an item is inserted at a valid index
- **THEN** subsequent items MUST shift right, the same logical selected item MUST remain selected where possible, layout MUST update, and `itemsChanged()` MUST be emitted

#### Scenario: Update item metadata
- **WHEN** callers update an item's text, icon glyph, enabled state, visible state, user data, accessible name, or selected flag
- **THEN** the component MUST preserve item order, refresh layout and accessibility, and emit changed signals once per effective mutation

#### Scenario: Remove selected item
- **WHEN** the selected item is removed
- **THEN** item order MUST remain synchronized, selection MUST move to a valid neighboring visible enabled item or `-1` when none remains, and selection signals MUST reflect the new selected index

#### Scenario: Invalid item index
- **WHEN** callers query, update, select, remove, or set selected state for an invalid item index
- **THEN** the component MUST fail safely without mutating unrelated items or selection state

#### Scenario: Clear items
- **WHEN** `clearItems()` is called
- **THEN** the component MUST contain zero items, set selected index to `-1`, update accessibility, clear layout records, and emit `itemsChanged()` once if items were present

### Requirement: SelectorBar selected state semantics
The system SHALL maintain exactly one selected visible enabled item, or no selection when no such item exists.

#### Scenario: Select item by index
- **WHEN** callers select a valid visible enabled index
- **THEN** the previous selected item MUST become unselected, the new item MUST become selected, and `selectedIndexChanged`, `currentChanged`, and `selectionChanged` MUST be emitted once for the effective change

#### Scenario: Select item by selected flag
- **WHEN** callers set an item's selected flag to `true`
- **THEN** the component MUST select that item using the same behavior as `setSelectedIndex(index)`

#### Scenario: Clear current item selected flag
- **WHEN** callers set the currently selected item's selected flag to `false`
- **THEN** the component MUST clear selection to `-1` unless another explicit selection is made

#### Scenario: Programmatic disabled selection
- **WHEN** callers attempt to select a disabled item
- **THEN** the component MUST ignore the request and keep the current valid selection

#### Scenario: Programmatic hidden selection
- **WHEN** callers attempt to select a hidden item
- **THEN** the component MUST ignore the request and keep the current valid selection

#### Scenario: Selection invalidated by metadata
- **WHEN** the selected item becomes hidden or disabled
- **THEN** selection MUST clamp to the nearest visible enabled item or `-1` when none remains

#### Scenario: Duplicate selection suppression
- **WHEN** callers select the already selected item
- **THEN** the component MUST NOT emit selection or activation signals again

### Requirement: Lightweight external composition
The system SHALL keep SelectorBar independent from page, route, frame, and content ownership while providing signals that let external components react to selection.

#### Scenario: External StackContentHost switching
- **WHEN** a caller connects `SelectorBar::currentChanged` to `StackContentHost::setCurrentIndex`
- **THEN** selecting a SelectorBar item MUST allow the external host to show the corresponding page without SelectorBar owning or parenting that page

#### Scenario: External data source switching
- **WHEN** a caller handles `selectionChanged` and changes an external item model or view data source
- **THEN** SelectorBar MUST leave the external data source ownership and lifetime under the caller's control

#### Scenario: External presenter visibility
- **WHEN** hidden or visible selector items are used to control which external presenter is visible
- **THEN** SelectorBar MUST emit enough selection information for the caller to update visibility without embedding presenter widgets in selector items

#### Scenario: No content host API
- **WHEN** code includes `SelectorBar.h`
- **THEN** the component MUST NOT expose page widget, content host, route, frame, or stacked-layout ownership APIs

### Requirement: SelectorBar layout and overflow
The system SHALL render SelectorBar-owned items using deterministic Fluent metrics and expose geometry helpers for tests.

#### Scenario: Row geometry
- **WHEN** SelectorBar contains visible items
- **THEN** it MUST lay out visible item headers horizontally in a selector row with stable hit rectangles and no overlap between items

#### Scenario: Hidden item layout
- **WHEN** an item is hidden
- **THEN** it MUST NOT receive a visible rectangle, pointer hit target, keyboard focus target, or visible index entry

#### Scenario: Selected indicator geometry
- **WHEN** an item is selected
- **THEN** that item MUST show an accent selected indicator without changing item geometry

#### Scenario: Text rendering constraints
- **WHEN** item text is wider than its available rectangle
- **THEN** the component MUST avoid overlapping neighboring items, overflow controls, or parent bounds

#### Scenario: Overflow required
- **WHEN** visible item headers exceed available width
- **THEN** the component MUST expose overflow controls, keep the selected item reachable whenever possible, and expose hidden item indexes through query APIs

#### Scenario: Resize recomputation
- **WHEN** the SelectorBar widget is resized
- **THEN** it MUST recompute item, overflow, and selected indicator geometry without changing item order or selected item unless clamping is required

#### Scenario: Geometry query helpers
- **WHEN** tests query selector row, item, selected indicator, overflow, visible index, or hidden index geometry
- **THEN** the component MUST return the latest rectangles/indexes and MUST return empty values for invalid indexes or hidden controls

### Requirement: SelectorBar pointer and keyboard interaction
The system SHALL allow pointer and keyboard users to move focus, select enabled visible items, and activate overflow behavior.

#### Scenario: Click enabled item
- **WHEN** the user clicks an enabled visible SelectorBar item
- **THEN** the component MUST select that item, emit item activation and selection signals, and leave item order unchanged

#### Scenario: Click disabled item
- **WHEN** the user clicks a disabled visible item
- **THEN** the component MUST NOT select the item or emit activation signals

#### Scenario: Click hidden item space
- **WHEN** the user clicks an area where a hidden item would have appeared
- **THEN** the component MUST treat the click as a non-item click and MUST NOT change selection

#### Scenario: Focus enters control
- **WHEN** SelectorBar receives keyboard focus with at least one visible enabled item
- **THEN** one visible enabled item MUST become focused for keyboard activation without painting a button border

#### Scenario: Move focus horizontally
- **WHEN** the control has focus and the user presses Left or Right
- **THEN** focus MUST move to the previous or next visible enabled item and skip disabled or hidden items

#### Scenario: Home and End keys
- **WHEN** the control has focus and the user presses Home or End
- **THEN** focus MUST move to the first or last visible enabled item respectively and bring that item into view when overflow is active

#### Scenario: Activate focused item
- **WHEN** the focused item is activated with Enter or Space
- **THEN** the component MUST select the focused item using the same behavior as pointer activation

#### Scenario: Overflow activation
- **WHEN** the user activates a SelectorBar overflow control
- **THEN** the visible item window MUST move or hidden indexes MUST be emitted without changing selection unless the user chooses an item

### Requirement: SelectorBar Fluent visual states and theming
The system SHALL paint SelectorBar using Fluent design tokens and update visuals for Light, Dark, selected, rest, hover, pressed, disabled, hidden, overflow, and theme-change states.

#### Scenario: Rest visual
- **WHEN** the component is enabled and no item is hovered, pressed, focused, or selected
- **THEN** item text, icons, and overflow controls MUST render with Fluent semantic colors matching the current theme

#### Scenario: Hover visual
- **WHEN** an enabled visible item or overflow control is hovered
- **THEN** that record MUST use a hover text/icon treatment without changing layout geometry

#### Scenario: Pressed visual
- **WHEN** an enabled visible item or overflow control is pressed
- **THEN** that record MUST use a pressed text/icon treatment without changing layout geometry

#### Scenario: Selected visual
- **WHEN** a SelectorBar item is selected
- **THEN** its text/icon treatment and selected indicator MUST identify it as selected

#### Scenario: Disabled visual
- **WHEN** the control or an item is disabled
- **THEN** disabled records MUST use disabled semantic colors and ignore pointer and keyboard activation

#### Scenario: Hidden visual
- **WHEN** an item is hidden
- **THEN** it MUST not be painted and MUST not leave a visual gap except normal spacing between remaining visible items

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** SelectorBar MUST refresh fonts/colors and repaint without changing item order, selected index, item visibility, or item metadata

#### Scenario: Token access configuration
- **WHEN** SelectorBar paints text or icons
- **THEN** font role and icon font family lookups MUST be backed by configurable member properties rather than hard-coded token strings at paint sites

### Requirement: SelectorBar accessibility
The system SHALL expose useful accessible text for the SelectorBar, items, selected item, disabled items, and hidden item changes.

#### Scenario: Control accessible text
- **WHEN** items or selection change
- **THEN** the component MUST update accessible text to identify the control as a SelectorBar and include the selected item and visible item count

#### Scenario: Item accessible name
- **WHEN** a SelectorBar item provides an explicit accessible name
- **THEN** focus and activation behavior MUST use that accessible name for assistive text

#### Scenario: Selected item announcement
- **WHEN** selection changes
- **THEN** accessible text MUST identify the newly selected item as current or selected

#### Scenario: Disabled item accessible text
- **WHEN** a SelectorBar item is disabled
- **THEN** its accessible text MUST still expose the item text while distinguishing disabled state

#### Scenario: Hidden item accessible text
- **WHEN** a SelectorBar item is hidden
- **THEN** it MUST not be reachable through keyboard focus or item hit testing

### Requirement: SelectorBar tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for SelectorBar.

#### Scenario: Test target registration
- **WHEN** tests are configured
- **THEN** `tests/views/navigation/CMakeLists.txt` MUST register `test_selector_bar` using `add_qt_test_module(test_selector_bar TestSelectorBar.cpp)`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_selector_bar` is run
- **THEN** tests MUST cover defaults, inheritance, metatypes, duplicate setter suppression, item management, selected state semantics, hidden/disabled behavior, geometry, overflow, pointer activation, keyboard navigation, external `StackContentHost` composition, theme refresh, accessibility text, and duplicate-signal suppression

#### Scenario: VisualCheck
- **WHEN** manual SelectorBar VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display WinUI Gallery-inspired selector scenarios including page switching through external `StackContentHost`, hidden sample-code-like items, disabled items, overflow, and Light/Dark theme switching

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** SelectorBar VisualCheck MUST skip without opening an interactive window

### Requirement: SelectorBar build registration
The system SHALL include SelectorBar source and tests in the normal CMake build.

#### Scenario: Source files participate in library build
- **WHEN** `fluent_qt_lib` is built
- **THEN** `src/view/navigation/SelectorBar.cpp` and `src/view/navigation/SelectorBar.h` MUST participate in compilation through existing recursive source discovery or explicit CMake wiring

#### Scenario: Test binary builds
- **WHEN** `cmake --build build --target test_selector_bar` is run after configuration
- **THEN** the test target MUST compile and link against `fluent_qt_lib`