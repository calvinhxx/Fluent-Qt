# PipsPager Specification

## Purpose

规范 scrolling 模块中的 Fluent `PipsPager` 控件：以 WinUI 风格显示页码导航指示器，支持水平/垂直布局、可视 pip 窗口滚动、Collapsed/Visible/VisibleOnPointerOver 按钮可见性策略，以及对应的主题与测试要求。

## Requirements

### Requirement: PipsPager public API
The system SHALL provide `view::scrolling::PipsPager` as a QWidget-based Fluent control for linearly paginated content, exposing page count, selected index, visible pip count, orientation, and previous/next button visibility properties.

#### Scenario: Default construction
- **WHEN** a `PipsPager` is constructed
- **THEN** `numberOfPages` MUST be `5`, `selectedPageIndex` MUST be `0`, `maxVisiblePips` MUST be `5`, `orientation` MUST be `Qt::Horizontal`, previous and next button visibility MUST be `Collapsed`, and the control MUST be enabled and focusable

#### Scenario: Property change signals
- **WHEN** a property setter changes number of pages, selected page index, max visible pips, orientation, previous button visibility, or next button visibility
- **THEN** the component MUST update geometry or repaint as needed and emit the matching changed signal exactly once

#### Scenario: Repeated property value
- **WHEN** a property setter receives the same value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals

### Requirement: Page count and selected index
The system SHALL maintain a valid selected page index for the current page range and report selection changes with old and new indexes.

#### Scenario: Select page in range
- **WHEN** `setSelectedPageIndex(3)` is called while `numberOfPages` is at least `4`
- **THEN** `selectedPageIndex` MUST become `3`, `selectedPageIndexChanged(3)` MUST be emitted, and `selectedIndexChanged(oldIndex, 3)` MUST be emitted

#### Scenario: Clamp selected index above range
- **WHEN** `numberOfPages` is `5` and `setSelectedPageIndex(99)` is called
- **THEN** `selectedPageIndex` MUST become `4`

#### Scenario: Clamp selected index below range
- **WHEN** `setSelectedPageIndex(-1)` is called
- **THEN** `selectedPageIndex` MUST become `0`

#### Scenario: Reducing page count clamps selection
- **WHEN** `selectedPageIndex` is `4` and `setNumberOfPages(3)` is called
- **THEN** `selectedPageIndex` MUST become `2` and selection change signals MUST identify the old and new index

#### Scenario: Zero pages
- **WHEN** `setNumberOfPages(0)` is called
- **THEN** the control MUST draw no pips, keep `selectedPageIndex` at `0`, and disable page navigation

### Requirement: Visible pip window
The system SHALL show at most `maxVisiblePips` pips and scroll the visible pip window when page count exceeds that limit.

#### Scenario: Page count within visible limit
- **WHEN** `numberOfPages` is `4` and `maxVisiblePips` is `5`
- **THEN** the control MUST display `4` pip hit cells and no pip window scrolling MUST occur

#### Scenario: Page count exceeds visible limit
- **WHEN** `numberOfPages` is `20`, `maxVisiblePips` is `5`, and `selectedPageIndex` is `10`
- **THEN** the control MUST display `5` pip hit cells and the selected page MUST be within the visible window, centered when start/end clamping does not prevent it

#### Scenario: Max visible pips zero
- **WHEN** `setMaxVisiblePips(0)` is called
- **THEN** the control MUST display no pips while keeping page count and selected page index state intact

#### Scenario: Selected pip remains visible near range edges
- **WHEN** selected page is the first or last page in a range larger than `maxVisiblePips`
- **THEN** the visible pip window MUST clamp to the beginning or end so the selected pip remains visible

### Requirement: Orientation and layout
The system SHALL support horizontal and vertical PipsPager layouts with stable Figma-aligned pip and caret geometry.

#### Scenario: Horizontal layout
- **WHEN** `orientation` is `Qt::Horizontal`
- **THEN** pips MUST be arranged left-to-right in 12px hit cells and previous/next caret slots MUST be left/right 24px areas when reserved

#### Scenario: Vertical layout
- **WHEN** `orientation` is `Qt::Vertical`
- **THEN** pips MUST be arranged top-to-bottom in 12px hit cells and previous/next caret slots MUST be top/bottom 24px areas when reserved

#### Scenario: Size hint without carets
- **WHEN** the control has visible pips and both button visibilities are `Collapsed`
- **THEN** `sizeHint()` MUST be based on the visible pip count and 12px pip cells

#### Scenario: Size hint with reserved carets
- **WHEN** either previous or next button visibility is `Visible` or `VisibleOnPointerOver`
- **THEN** `sizeHint()` MUST include the corresponding 24px caret slot even when the caret is hidden at an edge

### Requirement: Pip rendering states
The system SHALL render selected and inactive pips using Figma state metrics and Fluent theme colors.

#### Scenario: Inactive rest pip
- **WHEN** a pip is inactive and not hovered, pressed, or disabled
- **THEN** it MUST be drawn as a centered 4px dot inside a 12px hit cell

#### Scenario: Inactive hover and pressed pips
- **WHEN** an inactive pip is hovered
- **THEN** it MUST be drawn as a centered 5px dot
- **WHEN** an inactive pip is pressed
- **THEN** it MUST be drawn as a centered 3px dot

#### Scenario: Selected rest pip
- **WHEN** a pip is selected and not hovered, pressed, or disabled
- **THEN** it MUST be drawn as a centered 6px dot

#### Scenario: Selected hover and pressed pips
- **WHEN** a selected pip is hovered
- **THEN** it MUST be drawn as a centered 7px dot
- **WHEN** a selected pip is pressed
- **THEN** it MUST be drawn as a centered 5px dot

#### Scenario: Disabled rendering
- **WHEN** the PipsPager is disabled
- **THEN** pips and carets MUST use disabled theme colors, inactive pips MUST render at 4px, and selected pips MUST render at 6px

#### Scenario: Theme update
- **WHEN** Fluent theme changes between Light and Dark
- **THEN** PipsPager MUST repaint pips and carets using current theme colors without changing layout

### Requirement: Navigation button visibility
The system SHALL support WinUI-style previous and next button visibility policies.

#### Scenario: Collapsed buttons
- **WHEN** previous or next button visibility is `Collapsed`
- **THEN** the corresponding caret MUST NOT be painted and MUST NOT reserve layout space

#### Scenario: Visible buttons
- **WHEN** previous or next button visibility is `Visible`
- **THEN** the corresponding 24px caret slot MUST reserve layout space and the caret MUST be painted when navigation in that direction is possible

#### Scenario: Visible button at range edge
- **WHEN** the selected page is the first page and previous button visibility is `Visible`
- **THEN** the previous caret slot MUST remain reserved but previous navigation MUST be unavailable and the caret MUST not be interactively visible

#### Scenario: VisibleOnPointerOver buttons
- **WHEN** button visibility is `VisibleOnPointerOver`
- **THEN** the corresponding caret slot MUST reserve layout space and the caret MUST be painted only while the control is hovered or keyboard-focused and navigation in that direction is possible

#### Scenario: No wrapping
- **WHEN** the selected page is the first page or last page
- **THEN** clicking previous at the first page or next at the last page MUST NOT wrap to the opposite end

### Requirement: User interaction
The system SHALL allow pointer and keyboard navigation through the current page range.

#### Scenario: Click visible pip
- **WHEN** the user clicks a visible inactive pip
- **THEN** `selectedPageIndex` MUST change to that pip's page index and selection change signals MUST be emitted

#### Scenario: Click previous or next caret
- **WHEN** the user clicks an enabled previous or next caret
- **THEN** `selectedPageIndex` MUST decrement or increment by one page respectively

#### Scenario: Keyboard navigation
- **WHEN** the control has focus and the user presses a direction key matching the current orientation
- **THEN** the selected page MUST move by one page in the matching direction when possible

#### Scenario: Home and End keys
- **WHEN** the control has focus and the user presses Home or End
- **THEN** the selected page MUST move to the first or last page respectively

#### Scenario: Disabled input
- **WHEN** the control is disabled
- **THEN** pointer and keyboard input MUST NOT change `selectedPageIndex`

### Requirement: Integration and accessibility
The system SHALL support integration with paginated content and expose current page context for assistive technologies.

#### Scenario: Programmatic binding with external view
- **WHEN** external content changes page and calls `setSelectedPageIndex(newIndex)`
- **THEN** PipsPager MUST update visual selection without requiring user pointer input

#### Scenario: PipsPager drives external content
- **WHEN** user interaction changes `selectedPageIndex`
- **THEN** callers connected to `selectedIndexChanged(oldIndex, newIndex)` MUST receive the old and new indexes so they can update a FlipView, ScrollView, or carousel

#### Scenario: Accessible page announcement text
- **WHEN** selected page changes and `numberOfPages` is greater than zero
- **THEN** the control MUST update accessible text to include `Page <selectedPageIndex + 1> of <numberOfPages> selected`

### Requirement: PipsPager tests and VisualCheck
The system SHALL provide automated tests and VisualCheck coverage for PipsPager.

#### Scenario: Test target registration
- **WHEN** tests are configured
- **THEN** `tests/views/scrolling/CMakeLists.txt` MUST register `test_pips_pager`

#### Scenario: Automated tests
- **WHEN** `test_pips_pager` is built and run
- **THEN** tests MUST cover default properties, property signals, selected index clamping, max visible pip windowing, orientation layout, button visibility behavior, pointer interaction, keyboard interaction, disabled behavior, theme refresh, and accessibility text

#### Scenario: VisualCheck
- **WHEN** manual PipsPager VisualCheck is run without `SKIP_VISUAL_TEST=1`
- **THEN** it MUST display Light/Dark, horizontal/vertical, carets true/false, scrolling pips, disabled state, and a simple external-content binding example

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** PipsPager VisualCheck MUST skip without opening an interactive window
