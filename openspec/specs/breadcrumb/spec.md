# Breadcrumb Specification

## Purpose

规范 navigation 模块中的 Fluent `Breadcrumb` 控件：以 WinUI 风格显示层次路径导航，支持标准/大号尺寸、None/Beginning/Middle 溢出模式、项目激活与自动截断、键盘焦点导航，以及对应的主题与测试要求。

## Requirements

### Requirement: Breadcrumb public API
The system SHALL provide `view::navigation::Breadcrumb` as a Qt Widgets Fluent control for hierarchical path navigation. The component MUST inherit `QWidget`, `FluentElement`, and `view::QMLPlus`, and MUST expose item management, size, overflow, activation, truncation, focus, and testing APIs.

#### Scenario: Default construction
- **WHEN** a `Breadcrumb` is constructed
- **THEN** it MUST contain zero items, use standard size, use no-overflow mode, be enabled and focusable, and return usable `sizeHint()` and `minimumSizeHint()` values

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/navigation/Breadcrumb.h`
- **THEN** `Breadcrumb` MUST provide QWidget behavior, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Size property
- **WHEN** `setBreadcrumbSize(Standard)` or `setBreadcrumbSize(Large)` changes the size
- **THEN** the component MUST update typography, layout metrics, size hints, and repaint while emitting the matching changed signal exactly once

#### Scenario: Overflow mode property
- **WHEN** callers set overflow mode to `None`, `Beginning`, or `Middle`
- **THEN** the component MUST recompute visible path records, repaint, and emit the matching changed signal once per effective change

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals

### Requirement: Breadcrumb item management
The system SHALL allow callers to set, add, insert, remove, clear, and query breadcrumb path items while preserving item order and item metadata.

#### Scenario: Set string items
- **WHEN** `setItems(QStringList{"Home", "Documents", "Images"})` is called
- **THEN** the component MUST create three ordered items with matching display text and make `itemCount()` return `3`

#### Scenario: Set rich items
- **WHEN** callers set a collection of `BreadcrumbItem` records with text, user data, enabled state, or accessible name
- **THEN** the component MUST preserve that metadata and expose it through `itemAt(index)` and activation signals

#### Scenario: Insert item
- **WHEN** an item is inserted at a valid index
- **THEN** subsequent items MUST shift right, layout MUST update, and `itemsChanged()` MUST be emitted

#### Scenario: Remove item
- **WHEN** an item is removed by index
- **THEN** it MUST no longer participate in layout or activation, subsequent indexes MUST update, and `itemsChanged()` MUST be emitted

#### Scenario: Invalid item index
- **WHEN** callers query, remove, or activate an invalid item index
- **THEN** the component MUST fail safely without mutating unrelated items

#### Scenario: Clear items
- **WHEN** `clearItems()` is called
- **THEN** the component MUST contain zero items, expose no visible records, and emit `itemsChanged()` once if items were present

### Requirement: Breadcrumb layout and sizing
The system SHALL arrange breadcrumb text crumbs, separator chevrons, and optional overflow affordance using Figma-aligned metrics for standard and large sizes.

#### Scenario: Standard metrics
- **WHEN** breadcrumb size is `Standard`
- **THEN** text crumbs MUST use 14px Body typography in a 20px row, separators MUST reserve 16px slots, and separator icons MUST use Segoe Fluent Icons glyph `E974`

#### Scenario: Large metrics
- **WHEN** breadcrumb size is `Large`
- **THEN** text crumbs MUST use 28px semibold Title typography in a 40px row, separators MUST reserve 24px slots, and separator icons MUST use Segoe Fluent Icons glyph `E974`

#### Scenario: Active item emphasis
- **WHEN** the component paints a non-empty path
- **THEN** the final visible path item MUST use primary text color and ancestor path items MUST use secondary text color

#### Scenario: Text elision
- **WHEN** an item's text is wider than the available crumb rectangle
- **THEN** the component MUST elide text without overlapping separators, ellipsis, neighboring crumbs, or widget bounds

#### Scenario: Geometry query helpers
- **WHEN** tests query item, separator, or overflow geometry for valid visible records
- **THEN** the component MUST return the latest layout rectangles, and MUST return empty rectangles for invalid indexes

#### Scenario: Resize event
- **WHEN** the Breadcrumb widget is resized
- **THEN** it MUST recompute visible records and hidden indexes according to the current overflow mode

### Requirement: Breadcrumb overflow behavior
The system SHALL support no overflow, beginning overflow, and middle overflow display strategies with deterministic hidden item indexes.

#### Scenario: No overflow mode
- **WHEN** overflow mode is `None`
- **THEN** the component MUST attempt to display all items in order and use text elision when available width is insufficient

#### Scenario: Beginning overflow mode
- **WHEN** overflow mode is `Beginning` and the full path does not fit
- **THEN** the component MUST preserve trailing path context, replace one or more leading hidden items with an ellipsis affordance, and expose the hidden item indexes

#### Scenario: Middle overflow mode
- **WHEN** overflow mode is `Middle` and the full path does not fit
- **THEN** the component MUST preserve the first item and trailing active path context, replace middle hidden items with an ellipsis affordance, and expose the hidden item indexes

#### Scenario: Overflow affordance visual
- **WHEN** hidden items are represented by overflow
- **THEN** the component MUST display a clickable ellipsis affordance using Segoe Fluent Icons glyph `E712` with the current size's icon metrics

#### Scenario: Overflow activation
- **WHEN** the user activates the ellipsis affordance
- **THEN** the component MUST emit `overflowActivated` with the hidden item indexes and MUST NOT activate a normal breadcrumb item

#### Scenario: Active item preservation
- **WHEN** overflow is required and at least one item can fit
- **THEN** the component MUST preserve the final active item whenever possible

### Requirement: Breadcrumb interaction and truncation
The system SHALL allow pointer and keyboard users to activate visible breadcrumb items and optionally truncate trailing path items after activation.

#### Scenario: Click visible item
- **WHEN** the user clicks an enabled visible breadcrumb item
- **THEN** the component MUST emit item activation signals containing the original item index and item data

#### Scenario: Automatic trailing truncation
- **WHEN** auto-truncation is enabled and the user activates item index `i`
- **THEN** the component MUST remove every item after index `i`, emit `itemsChanged()`, and keep items `0..i` in order

#### Scenario: Automatic truncation disabled
- **WHEN** auto-truncation is disabled and the user activates an item
- **THEN** the component MUST emit activation signals without mutating the item list

#### Scenario: Separator click
- **WHEN** the user clicks a separator chevron
- **THEN** the component MUST NOT emit item activation signals and MUST NOT mutate items

#### Scenario: Disabled item activation
- **WHEN** the user attempts to activate a disabled breadcrumb item
- **THEN** the component MUST NOT emit item activation signals and MUST NOT truncate the path

#### Scenario: Disabled control input
- **WHEN** the Breadcrumb widget is disabled
- **THEN** pointer and keyboard input MUST NOT activate items, activate overflow, or mutate the path

### Requirement: Breadcrumb keyboard and focus behavior
The system SHALL support keyboard navigation across visible interactive breadcrumb records.

#### Scenario: Focus enters control
- **WHEN** the Breadcrumb receives keyboard focus with at least one enabled visible item
- **THEN** one visible enabled item MUST become the focused interactive record and the component MUST paint a Fluent focus visual for it

#### Scenario: Move focus horizontally
- **WHEN** the control has focus and the user presses Left or Right
- **THEN** focus MUST move to the previous or next enabled visible interactive record without focusing separators

#### Scenario: Home and End keys
- **WHEN** the control has focus and the user presses Home or End
- **THEN** focus MUST move to the first or last enabled visible breadcrumb item respectively

#### Scenario: Activate focused record
- **WHEN** the control has focus and the user presses Enter or Space
- **THEN** the focused breadcrumb item or overflow affordance MUST be activated using the same behavior as pointer activation

#### Scenario: Focus after item mutation
- **WHEN** items are inserted, removed, cleared, or truncated
- **THEN** the focused record MUST be clamped to a valid enabled visible record or cleared when none exists

### Requirement: Breadcrumb Fluent visual states and theming
The system SHALL paint Breadcrumb using Fluent design tokens and update visuals for Light, Dark, hover, pressed, focused, disabled, and theme changes.

#### Scenario: Rest visual
- **WHEN** the component is enabled and no visible record is hovered, pressed, or focused
- **THEN** text, chevrons, and overflow affordance MUST render with Fluent primary/secondary text colors matching active and ancestor roles

#### Scenario: Hover visual
- **WHEN** an enabled item or overflow affordance is hovered
- **THEN** that record MUST render a subtle hover background or text treatment without changing layout geometry

#### Scenario: Pressed visual
- **WHEN** an enabled item or overflow affordance is pressed
- **THEN** that record MUST render a pressed visual state without changing layout geometry

#### Scenario: Focus visual
- **WHEN** an enabled item or overflow affordance has keyboard focus
- **THEN** the component MUST paint a Fluent focus rectangle around that interactive record

#### Scenario: Disabled visual
- **WHEN** the control or an item is disabled
- **THEN** disabled records MUST use disabled semantic colors and ignore input

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** Breadcrumb MUST refresh fonts/colors and repaint without changing item order or activation state

### Requirement: Breadcrumb accessibility
The system SHALL expose useful accessible text for the control and individual breadcrumb items.

#### Scenario: Control accessible name
- **WHEN** items are present
- **THEN** the component MUST update its accessible name or description to include the current breadcrumb path in display order

#### Scenario: Item accessible name
- **WHEN** a `BreadcrumbItem` provides an explicit accessible name
- **THEN** activation and focus behavior MUST use that accessible name for assistive text

#### Scenario: Current item announcement
- **WHEN** the active final item changes because items are set, removed, or truncated
- **THEN** accessible text MUST identify the final item as the current page or current location

### Requirement: Breadcrumb tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for Breadcrumb.

#### Scenario: Test category registration
- **WHEN** tests are configured
- **THEN** `tests/views/CMakeLists.txt` MUST add `add_subdirectory(navigation)` and `tests/views/navigation/CMakeLists.txt` MUST register `test_breadcrumb`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_breadcrumb` is run
- **THEN** tests MUST cover defaults, item management, metadata preservation, property signals, standard and large metrics, overflow hidden-index calculation, pointer activation, keyboard activation, optional truncation, disabled behavior, theme refresh, and accessibility text

#### Scenario: VisualCheck
- **WHEN** manual Breadcrumb VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display standard and large Breadcrumbs, Light/Dark examples, no/beginning/middle overflow, disabled items, focus states, and a WinUI Gallery-inspired folder path that can truncate after ancestor activation

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** Breadcrumb VisualCheck MUST skip without opening an interactive window
