## ADDED Requirements

### Requirement: Label supports configurable single-line elision
`Label` SHALL expose configurable text elision using Qt text elide modes. The default elide mode MUST be `Qt::ElideNone`, preserving existing Label behavior unless a caller opts into elision.

#### Scenario: Default labels are not elided
- **WHEN** a `Label` is constructed without changing its elide mode
- **THEN** its elide mode MUST be `Qt::ElideNone`
- **AND** its displayed text MUST remain equal to the full text

#### Scenario: Elide mode truncates overflowing text
- **WHEN** a `Label` has long plain text, a constrained width, and elide mode `Qt::ElideRight`
- **THEN** the displayed text MUST be the font-metric elided form
- **AND** the full text MUST remain available through `Label::text()`

### Requirement: Elided Label shows full text in Fluent ToolTip on hover
`Label` SHALL show a `view::status_info::ToolTip` containing the full unelided text when hovered if and only if the current text is actually elided in the rendered label.

#### Scenario: Hovering an actually elided label shows full text
- **WHEN** a `Label` with elide mode enabled renders truncated text and receives hover enter
- **THEN** a Fluent `ToolTip` MUST be shown
- **AND** the tooltip text MUST equal the full `Label::text()`

#### Scenario: Hovering a non-truncated label does not show tooltip
- **WHEN** a `Label` has enough width to render the full text
- **AND** the label receives hover enter
- **THEN** no elide tooltip MUST be shown

#### Scenario: Leaving the label hides the tooltip
- **WHEN** an elide tooltip is visible for a `Label`
- **AND** the label receives hover leave
- **THEN** the tooltip MUST hide

### Requirement: Label keeps elide tooltip state current
`Label` SHALL recompute rendered elision and tooltip eligibility whenever text, font, typography, geometry, elide mode, or theme refresh can affect text width.

#### Scenario: Resize removes tooltip eligibility
- **WHEN** a previously elided `Label` is resized wide enough to fit its full text
- **THEN** the displayed text MUST return to the full text
- **AND** hover MUST NOT show an elide tooltip

#### Scenario: Text changes update tooltip content
- **WHEN** a Label with an active elide tooltip changes its full text
- **THEN** the displayed elided text MUST be recomputed
- **AND** the tooltip content MUST use the updated full text when shown again

#### Scenario: Typography changes recompute elision
- **WHEN** `setFluentTypography()` or `onThemeUpdated()` changes the effective Label font
- **THEN** the elision state MUST be recomputed using the updated font metrics
