# tab-view Specification

## Purpose

The system SHALL provide `view::navigation::TabView` as a Fluent WinUI-style tabbed workspace control for Qt Widgets. TabView owns tab item management, selected content hosting, close and add affordances, width and placement modes, overflow, optional reordering, keyboard navigation, accessibility text, and theme-aware custom painting while leaving application tab creation and close decisions under caller control.

## Requirements
### Requirement: TabView public API
The system SHALL provide `view::navigation::TabView` as a Qt Widgets Fluent control for tabbed workspaces. The component MUST inherit `QWidget`, `FluentElement`, and `view::QMLPlus`, and MUST expose tab item management, selection, width mode, close overlay mode, placement, add-button, reorder, keyboard, geometry, and theme APIs.

#### Scenario: Default construction
- **WHEN** a `TabView` is constructed
- **THEN** it MUST contain zero tabs, use selected index `-1`, use equal tab width mode, use automatic close-button overlay mode, use content-area placement, be enabled and focusable, and return usable `sizeHint()` and `minimumSizeHint()` values

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/navigation/TabView.h`
- **THEN** `TabView` MUST provide QWidget behavior, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Public enum properties
- **WHEN** callers set tab width mode, close button overlay mode, tab placement, add-button visibility, reorder enabled, or keyboard accelerator enabled properties
- **THEN** the component MUST update layout or behavior as appropriate and emit the matching changed signal exactly once per effective value change

#### Scenario: Selected index property
- **WHEN** callers set `selectedIndex` to a valid enabled tab index
- **THEN** the component MUST select that tab, show its content widget, update focus/layout state, repaint, and emit selection changed signals once

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals or mutate tab order

#### Scenario: Font and icon token roles
- **WHEN** TabView resolves typography or icon fonts for painting
- **THEN** it MUST resolve them through configurable member-backed roles and MUST NOT hard-code design token string literals at lookup sites

### Requirement: Tab item management
The system SHALL allow callers to add, insert, update, remove, clear, and query tab items while preserving tab order and item metadata.

#### Scenario: Add tab with text
- **WHEN** `addTab("Document 1")` is called
- **THEN** the component MUST append one tab with matching header text, increase `tabCount()` by one, emit `tabsChanged()`, and select the first tab automatically if no tab was previously selected

#### Scenario: Add tab with rich item
- **WHEN** callers add a `TabViewItem` with text, icon glyph, content widget, closeability, enabled state, user data, or accessible name
- **THEN** the component MUST preserve that metadata and expose it through `tabAt(index)` and activation signals

#### Scenario: Insert tab
- **WHEN** a tab is inserted at a valid index
- **THEN** subsequent tabs MUST shift right, selection indexes MUST be adjusted without losing the currently selected logical tab, layout MUST update, and `tabsChanged()` MUST be emitted

#### Scenario: Remove selected tab
- **WHEN** the selected tab is removed by index
- **THEN** the tab MUST no longer participate in layout, its content widget MUST no longer be visible, `tabsChanged()` MUST be emitted, and selection MUST move to a valid neighboring enabled tab or `-1` when none remains

#### Scenario: Invalid tab index
- **WHEN** callers query, update, select, close, move, or remove an invalid tab index
- **THEN** the component MUST fail safely without mutating unrelated tabs and without emitting misleading changed signals

#### Scenario: Clear tabs
- **WHEN** `clearTabs()` is called
- **THEN** the component MUST contain zero tabs, expose no visible tab records, hide all managed content widgets, set selected index to `-1`, and emit `tabsChanged()` once if tabs were present

#### Scenario: Update tab metadata
- **WHEN** callers update a tab's text, icon glyph, closeability, enabled state, user data, accessible name, or content widget
- **THEN** the component MUST preserve tab order, refresh layout and accessible text, and emit the matching changed signal once per effective change

### Requirement: Tab content hosting
The system SHALL host the selected tab's content widget in a stable content area while hiding non-selected tab content widgets.

#### Scenario: Select tab content
- **WHEN** a tab with a content widget becomes selected
- **THEN** that content widget MUST be parented to the TabView content host, made visible, and resized to the current content area

#### Scenario: Hide previous content
- **WHEN** selection changes from one tab with content to another tab
- **THEN** the previous content widget MUST be hidden and the newly selected content widget MUST be shown without deleting either widget

#### Scenario: Content area geometry
- **WHEN** tests query content area geometry
- **THEN** the component MUST return the latest rectangle below the tab row for content-area placement and a valid host rectangle for selected content

#### Scenario: Remove content tab
- **WHEN** a tab with a managed content widget is removed
- **THEN** the component MUST hide that widget and follow the documented ownership/take semantics without leaving it visible in the old content area

### Requirement: Tab layout, width modes, and overflow
The system SHALL arrange the tab row, tabs, close buttons, add button, overflow buttons, and content area using deterministic Fluent metrics based on the Figma TabView reference.

#### Scenario: Row and tab metrics
- **WHEN** TabView computes layout
- **THEN** the tab row MUST reserve a 40px row, tab bodies MUST use a 32px visual height, and add/overflow button affordances MUST use stable 40x32 rectangles where space allows

#### Scenario: Equal width mode
- **WHEN** tab width mode is `Equal`
- **THEN** visible tabs MUST share available tab-strip width evenly while respecting minimum and maximum tab widths

#### Scenario: Size-to-content width mode
- **WHEN** tab width mode is `SizeToContent`
- **THEN** each visible tab MUST measure its icon, text, padding, and close affordance and clamp the result without overlapping neighboring controls

#### Scenario: Compact width mode
- **WHEN** tab width mode is `Compact`
- **THEN** inactive tabs MUST compact toward the configured minimum width while the selected tab MUST retain enough width to communicate its active header when space allows

#### Scenario: Close overlay reservation
- **WHEN** close button overlay mode changes
- **THEN** tab layout MUST reserve or release close-button space according to `Auto`, `OnHover`, or `Always` without shifting unrelated row controls during hover transitions

#### Scenario: Overflow required
- **WHEN** total tab width exceeds the available tab-strip width
- **THEN** the component MUST expose a horizontal visible window over tab indexes, show overflow controls, and keep the selected tab visible whenever possible

#### Scenario: Overflow navigation
- **WHEN** the user activates previous or next overflow controls
- **THEN** the visible tab window MUST move in the requested direction, layout MUST update, and tab order MUST remain unchanged

#### Scenario: Geometry query helpers
- **WHEN** tests query tab, close button, add button, overflow button, visible-index, or content-area geometry
- **THEN** the component MUST return the latest layout rectangles and MUST return empty rectangles for invalid indexes or controls that are not currently visible

#### Scenario: Resize event
- **WHEN** the TabView widget is resized
- **THEN** it MUST recompute tab widths, overflow controls, visible indexes, and content area geometry according to the current modes

### Requirement: Tab selection, add, close, and reorder interactions
The system SHALL allow pointer users to select tabs, request new tabs, request tab closing, and optionally reorder tabs within the tab row.

#### Scenario: Click enabled tab
- **WHEN** the user clicks an enabled visible tab body
- **THEN** the component MUST select that tab and emit selection signals containing the selected index

#### Scenario: Click disabled tab
- **WHEN** the user clicks a disabled visible tab body
- **THEN** the component MUST NOT select the tab, show its content, or emit selection signals

#### Scenario: Add button click
- **WHEN** the user clicks the add-tab button while it is visible and enabled
- **THEN** the component MUST emit `addTabRequested()` and MUST NOT create an application tab by itself

#### Scenario: Close button click
- **WHEN** the user clicks a close button for a closeable tab
- **THEN** the component MUST emit `tabCloseRequested(index)` for that tab and MUST NOT treat the click as a tab body selection

#### Scenario: Non-closeable tab close attempt
- **WHEN** the user attempts to close a tab whose `closable` flag is false
- **THEN** the component MUST NOT emit `tabCloseRequested()` and MUST NOT remove the tab

#### Scenario: Programmatic close
- **WHEN** callers invoke the public close/remove API for a valid closeable tab
- **THEN** the component MUST remove that tab, update selection, hide removed content, update layout, and emit changed signals once

#### Scenario: Drag reorder enabled
- **WHEN** `tabReorderEnabled` is true and the user drags an enabled tab body beyond the drag threshold to a valid target position
- **THEN** the component MUST move the tab item and its content together, update selected index to the moved logical tab, repaint, and emit `tabMoved(from, to)` plus `tabsChanged()`

#### Scenario: Drag reorder disabled
- **WHEN** `tabReorderEnabled` is false and the user drags a tab body
- **THEN** the component MUST leave tab order unchanged and MUST NOT emit reorder signals

### Requirement: Tab keyboard and focus behavior
The system SHALL support keyboard navigation and WinUI Gallery-inspired accelerators for common TabView workflows.

#### Scenario: Focus enters control
- **WHEN** the TabView receives keyboard focus with at least one enabled tab
- **THEN** one enabled tab or row affordance MUST become the focused interactive record and the component MUST paint a Fluent focus visual around it

#### Scenario: Move focus horizontally
- **WHEN** the control has focus and the user presses Left or Right
- **THEN** focus MUST move to the previous or next enabled visible interactive record without focusing non-interactive gaps

#### Scenario: Home and End keys
- **WHEN** the control has focus and the user presses Home or End
- **THEN** focus MUST move to the first or last enabled visible tab respectively and bring that tab into view when overflow is active

#### Scenario: Activate focused tab
- **WHEN** the control has focus and the user presses Enter or Space on a focused tab
- **THEN** that tab MUST be selected using the same behavior as pointer selection

#### Scenario: New tab accelerator
- **WHEN** keyboard accelerators are enabled and the user presses Ctrl+T
- **THEN** the component MUST emit `addTabRequested()`

#### Scenario: Close selected tab accelerator
- **WHEN** keyboard accelerators are enabled and the user presses Ctrl+W or Delete while the selected tab is closeable
- **THEN** the component MUST emit `tabCloseRequested(selectedIndex)`

#### Scenario: Numbered tab accelerator
- **WHEN** keyboard accelerators are enabled and the user presses Ctrl+1 through Ctrl+8
- **THEN** the component MUST select the matching zero-based tab index if it exists and is enabled

#### Scenario: Last tab accelerator
- **WHEN** keyboard accelerators are enabled and the user presses Ctrl+9
- **THEN** the component MUST select the last enabled tab when one exists

#### Scenario: Accelerators disabled
- **WHEN** keyboard accelerators are disabled
- **THEN** Ctrl+T, Ctrl+W, Delete, and Ctrl+number shortcuts MUST NOT emit TabView accelerator actions

### Requirement: TabView Fluent visual states and theming
The system SHALL paint TabView using Fluent design tokens and update visuals for Light, Dark, active, inactive, hover, pressed, focused, disabled, close, add, overflow, and theme-change states.

#### Scenario: Active tab visual
- **WHEN** the component paints a selected tab
- **THEN** that tab MUST use active tab fill, primary text treatment, and top-level shape/stroke treatment appropriate to the current placement

#### Scenario: Inactive tab visual
- **WHEN** the component paints an enabled unselected tab
- **THEN** that tab MUST use inactive text and fill treatment and MUST remain visually distinct from the selected tab

#### Scenario: Hover visual
- **WHEN** an enabled tab, close button, add button, or overflow button is hovered
- **THEN** that record MUST render a Fluent hover treatment without changing layout geometry

#### Scenario: Pressed visual
- **WHEN** an enabled tab, close button, add button, or overflow button is pressed
- **THEN** that record MUST render a Fluent pressed treatment without changing layout geometry

#### Scenario: Focus visual
- **WHEN** an enabled interactive record has keyboard focus
- **THEN** the component MUST paint a Fluent focus rectangle around that record

#### Scenario: Disabled visual
- **WHEN** the control or an individual tab is disabled
- **THEN** disabled records MUST use disabled semantic colors and ignore pointer and keyboard activation

#### Scenario: Close button state visual
- **WHEN** a close button is visible in rest, hover, or pressed state
- **THEN** it MUST render the configured close glyph with the matching state colors and hit geometry inside the owning tab

#### Scenario: Placement visual
- **WHEN** tab placement is `InTitleBar` or `InContentArea`
- **THEN** the component MUST use the matching row background, divider, and active-tab treatment while preserving the same public interaction behavior

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** TabView MUST refresh fonts/colors and repaint without changing tab order, selected index, content ownership, or visible overflow window except as required by metric recomputation

### Requirement: TabView accessibility
The system SHALL expose useful accessible text for the control, tabs, and row affordances.

#### Scenario: Control accessible name
- **WHEN** tabs are present
- **THEN** the component MUST update accessible text to include the selected tab and tab count

#### Scenario: Tab accessible name
- **WHEN** a `TabViewItem` provides an explicit accessible name
- **THEN** focus and activation behavior MUST use that accessible name for assistive text

#### Scenario: Close and add accessible names
- **WHEN** close buttons, add button, or overflow buttons are visible
- **THEN** the component MUST expose accessible text that identifies each affordance's action and target tab where applicable

#### Scenario: Selected tab announcement
- **WHEN** selection changes
- **THEN** accessible text MUST identify the newly selected tab as selected/current

### Requirement: TabView tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for TabView.

#### Scenario: Test category registration
- **WHEN** tests are configured
- **THEN** `tests/views/navigation/CMakeLists.txt` MUST register `test_tab_view` using `add_qt_test_module(test_tab_view TestTabView.cpp)`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_tab_view` is run
- **THEN** tests MUST cover defaults, inheritance, item management, metadata preservation, content visibility, selected-index clamping, property signals, width modes, close overlay modes, placement modes, overflow visible indexes, add/close request signals, keyboard accelerators, drag reorder behavior, disabled behavior, theme refresh, accessibility text, and geometry helpers

#### Scenario: VisualCheck
- **WHEN** manual TabView VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display title-bar and content-area placements, Light/Dark examples, active and inactive tabs, close overlay modes, equal/size-to-content/compact width modes, overflow, disabled tabs, add-button interaction, and a WinUI Gallery-inspired document workspace

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** TabView VisualCheck MUST skip without opening an interactive window
