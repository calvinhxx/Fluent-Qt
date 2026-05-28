## ADDED Requirements

### Requirement: Pivot public API
The system SHALL provide `view::navigation::Pivot` as a Qt Widgets Fluent control for switching between related pages. The component MUST inherit `QWidget`, `FluentElement`, and `QMLPlus`, and MUST expose item, selected-index, content host, header geometry, theme, focus, accessibility, and testing APIs.

#### Scenario: Default construction
- **WHEN** a `Pivot` is constructed
- **THEN** it MUST contain zero items, use selected index `-1`, be enabled and focusable, and return usable `sizeHint()` and `minimumSizeHint()` values

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/navigation/Pivot.h`
- **THEN** `Pivot` MUST provide QWidget child containment, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Selected index property
- **WHEN** callers set `selectedIndex` to a valid enabled item index
- **THEN** the component MUST select that item, show its content page, update header visuals, update accessibility text, and emit selection signals once

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals or mutate item/page order

### Requirement: Pivot item and page management
The system SHALL allow callers to add, insert, update, remove, clear, and query Pivot items while preserving item order, metadata, and associated page widgets.

#### Scenario: Add item with header
- **WHEN** `addItem("All")` is called on an empty Pivot
- **THEN** the component MUST append one item with header text `All`, increase `itemCount()` to `1`, emit `itemsChanged()`, and select the first enabled item automatically

#### Scenario: Add item with content widget
- **WHEN** callers add an item with a QWidget page
- **THEN** the page MUST be parented to the Pivot content host, associated with the item index, and shown only when that item is selected

#### Scenario: Insert item
- **WHEN** an item is inserted at a valid index
- **THEN** subsequent items and page slots MUST shift right, the selected logical page MUST remain selected where possible, layout MUST update, and `itemsChanged()` MUST be emitted

#### Scenario: Update item metadata
- **WHEN** callers update an item's header text, icon glyph, enabled state, user data, accessible name, or page widget
- **THEN** the component MUST preserve item order, refresh layout and accessibility, and emit changed signals once per effective mutation

#### Scenario: Remove selected item
- **WHEN** the selected item is removed
- **THEN** its page MUST no longer be visible, item/page order MUST remain synchronized, and selection MUST move to a valid neighboring enabled item or `-1` when none remains

#### Scenario: Invalid item index
- **WHEN** callers query, update, select, remove, or take a page for an invalid item index
- **THEN** the component MUST fail safely without mutating unrelated items, pages, or selection state

#### Scenario: Clear items
- **WHEN** `clearItems()` is called
- **THEN** the component MUST contain zero items, show no page, set selected index to `-1`, update accessibility, and emit `itemsChanged()` once if items were present

### Requirement: Pivot stacked content host
The system SHALL manage Pivot page content through a QStackedLayout-style content host synchronized with Pivot item order.

#### Scenario: Selected page visibility
- **WHEN** selection changes from one item with a page to another item with a page
- **THEN** the previous page MUST be hidden and the newly selected page MUST become the current stacked page

#### Scenario: Animated page transition
- **WHEN** selection changes between visible Pivot pages
- **THEN** the content host MUST animate the newly selected page with a short left-to-right slide while keeping the previous page hidden so page text and controls do not overlap

#### Scenario: Content area geometry
- **WHEN** tests query the content area geometry
- **THEN** the component MUST return the latest rectangle below the Pivot header region and MUST size the current page to that area

#### Scenario: Replace page widget
- **WHEN** callers replace an item's page widget
- **THEN** the old page MUST be detached or returned according to the documented API and the new page MUST occupy the same item slot

#### Scenario: Take page widget
- **WHEN** callers take a page widget from an item
- **THEN** the component MUST detach and return that QWidget without deleting it, leave an empty page slot, and keep item metadata intact

#### Scenario: Theme update for content host
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** the content host MUST refresh background-related visuals without changing item order, selected index, or page ownership

### Requirement: Pivot header layout and overflow
The system SHALL render Pivot-owned item headers using deterministic Fluent metrics and expose geometry helpers for tests.

#### Scenario: Header row geometry
- **WHEN** Pivot contains visible items
- **THEN** it MUST lay out item headers horizontally in a header row above the content area, with stable hit rectangles and no overlap between headers

#### Scenario: Selected indicator geometry
- **WHEN** an item is selected
- **THEN** that header MUST show an accent selected indicator without changing header geometry

#### Scenario: Header text rendering
- **WHEN** a header is wider than its available rectangle
- **THEN** the component MUST NOT render ellipsis text and MUST avoid overlapping neighboring headers, overflow controls, or content

#### Scenario: Overflow required
- **WHEN** item headers exceed available width
- **THEN** the component MUST expose overflow controls, keep the selected item reachable whenever possible, and expose hidden item indexes through query APIs

#### Scenario: Resize recomputation
- **WHEN** the Pivot widget is resized
- **THEN** it MUST recompute header, overflow, selected indicator, and content geometry without changing item order or selected item unless clamping is required

#### Scenario: Geometry query helpers
- **WHEN** tests query header row, item header, selected indicator, overflow, visible index, hidden index, or content geometry
- **THEN** the component MUST return the latest rectangles/indexes and MUST return empty values for invalid indexes or hidden controls

### Requirement: Pivot selection and interaction
The system SHALL allow pointer and keyboard users to move focus, select enabled Pivot items, and switch pages.

#### Scenario: Click enabled header
- **WHEN** the user clicks an enabled visible Pivot item header
- **THEN** the component MUST select that item, show its page, emit item activation and selection signals, and leave item order unchanged

#### Scenario: Click disabled header
- **WHEN** the user clicks a disabled Pivot item header
- **THEN** the component MUST NOT select the item, show its page, or emit activation signals

#### Scenario: Programmatic disabled selection
- **WHEN** callers attempt to set selected index to a disabled item
- **THEN** the component MUST ignore the request and keep the current valid selection

#### Scenario: Focus enters control
- **WHEN** Pivot receives keyboard focus with at least one enabled visible header
- **THEN** one enabled header MUST become focused for keyboard activation without painting a header border

#### Scenario: Move focus horizontally
- **WHEN** the control has focus and the user presses Left or Right
- **THEN** focus MUST move to the previous or next enabled visible header and skip disabled or hidden headers

#### Scenario: Home and End keys
- **WHEN** the control has focus and the user presses Home or End
- **THEN** focus MUST move to the first or last enabled Pivot item respectively and bring that header into view when overflow is active

#### Scenario: Activate focused header
- **WHEN** the focused header is activated with Enter or Space
- **THEN** the component MUST select the focused item using the same behavior as pointer activation

#### Scenario: Overflow navigation
- **WHEN** the user activates a Pivot overflow control
- **THEN** the visible header window MUST move or hidden indexes MUST be emitted without changing selection unless the user chooses an item

### Requirement: Pivot Fluent visual states and theming
The system SHALL paint Pivot using Fluent design tokens and update visuals for Light, Dark, selected, rest, hover, pressed, disabled, overflow, and theme-change states.

#### Scenario: Rest visual
- **WHEN** the component is enabled and no header is hovered, pressed, or selected
- **THEN** headers, icons, content boundary, and overflow controls MUST render with Fluent semantic colors matching the current theme

#### Scenario: Hover visual
- **WHEN** an enabled header or overflow control is hovered
- **THEN** that record MUST use a text/icon color treatment without painting a hover background or changing layout geometry

#### Scenario: Pressed visual
- **WHEN** an enabled header or overflow control is pressed
- **THEN** that record MUST use a pressed text/icon color treatment without painting a pressed background or changing layout geometry

#### Scenario: Selected visual
- **WHEN** a Pivot item is selected
- **THEN** its header MUST use selected text treatment and an accent indicator, and the matching page MUST be visible

#### Scenario: Disabled visual
- **WHEN** the control or an item is disabled
- **THEN** disabled records MUST use disabled semantic colors and ignore pointer and keyboard activation

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** Pivot MUST refresh fonts/colors and repaint without changing item order, selected index, visible page, or page ownership

### Requirement: Pivot accessibility
The system SHALL expose useful accessible text for the Pivot, headers, selected item, and pages.

#### Scenario: Control accessible text
- **WHEN** items or selection change
- **THEN** the component MUST update accessible text to identify the control as a Pivot and include the selected header and item count

#### Scenario: Item accessible name
- **WHEN** a Pivot item provides an explicit accessible name
- **THEN** focus and activation behavior MUST use that accessible name for assistive text

#### Scenario: Selected item announcement
- **WHEN** selection changes
- **THEN** accessible text MUST identify the newly selected item as current or selected

#### Scenario: Disabled item accessible text
- **WHEN** a Pivot item is disabled
- **THEN** its accessible text MUST still expose the header while distinguishing disabled state

### Requirement: Pivot tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for Pivot.

#### Scenario: Test target registration
- **WHEN** tests are configured
- **THEN** `tests/views/navigation/CMakeLists.txt` MUST register `test_pivot` using `add_qt_test_module(test_pivot TestPivot.cpp)`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_pivot` is run
- **THEN** tests MUST cover defaults, inheritance, metatypes, property duplicate suppression, item/page management, content host synchronization, selected-index clamping, page take/replace semantics, header geometry, overflow, pointer activation, keyboard navigation, disabled behavior, theme refresh, accessibility text, and duplicate-signal suppression

#### Scenario: VisualCheck
- **WHEN** manual Pivot VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display a WinUI Gallery email-style Pivot with an externally anchored label, disabled items, many-header overflow, page switching, and Light/Dark theme switching

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** Pivot VisualCheck MUST skip without opening an interactive window

### Requirement: Build registration
The system SHALL include Pivot source and tests in the normal CMake build.

#### Scenario: Source files participate in library build
- **WHEN** `fluent_qt_lib` is built
- **THEN** `src/view/navigation/Pivot.cpp` and `src/view/navigation/Pivot.h` MUST participate in compilation through existing recursive source discovery or explicit CMake wiring

#### Scenario: Test binary builds
- **WHEN** `cmake --build build --target test_pivot` is run after configuration
- **THEN** the test target MUST compile and link against `fluent_qt_lib`
