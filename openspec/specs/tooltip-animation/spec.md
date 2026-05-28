# tooltip-animation Specification

## Purpose
Defines Fluent opacity motion behavior for `ToolTip` show and hide transitions.

## Requirements

### Requirement: ToolTip animates showing with Fluent opacity motion
`ToolTip` SHALL animate its visible entry when animation is enabled. The entry transition MUST animate the tooltip window opacity from the current opacity toward `1.0` using Fluent animation timing from `themeAnimation()`.

#### Scenario: Show starts a fade-in transition
- **WHEN** a hidden `ToolTip` has animation enabled and `show()` is called
- **THEN** the tooltip becomes visible without activating the application
- **AND** its opacity animates toward `1.0`

#### Scenario: Repeated show keeps the final visible state stable
- **WHEN** `show()` is called while `ToolTip` is already visible or entering
- **THEN** the tooltip remains visible
- **AND** the transition ends with opacity equal to `1.0`

### Requirement: ToolTip animates hiding before becoming hidden
`ToolTip` SHALL animate its hidden exit when animation is enabled. The exit transition MUST animate the tooltip window opacity from the current opacity toward `0.0`, and the widget MUST remain visible until the transition completes.

#### Scenario: Hide starts a fade-out transition
- **WHEN** a visible `ToolTip` has animation enabled and `hide()` is called
- **THEN** the tooltip remains visible while opacity animates toward `0.0`
- **AND** the tooltip becomes hidden after the transition completes

#### Scenario: Hide during entry reverses cleanly
- **WHEN** `hide()` is called while the tooltip entry animation is still running
- **THEN** the entry animation stops
- **AND** the exit animation starts from the current opacity instead of jumping to a new visual state

### Requirement: ToolTip preserves existing behavior and styling
`ToolTip` animation SHALL NOT change the tooltip text, font, margins, theme colors, window flags, or caller-managed position. Existing callers that use `show()` and `hide()` MUST receive animated transitions without changing call sites.

#### Scenario: Existing call sites need no API migration
- **WHEN** a caller positions a `ToolTip` with `move()` and then calls `show()`
- **THEN** the tooltip uses that caller-provided position
- **AND** no animated resize, scale, or placement adjustment is applied

#### Scenario: Disabling animation keeps synchronous visibility behavior
- **WHEN** animation is disabled and `show()` or `hide()` is called
- **THEN** the tooltip visibility changes synchronously
- **AND** the steady-state opacity is `1.0` when shown and `0.0` when hidden
