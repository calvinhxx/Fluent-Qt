## ADDED Requirements

### Requirement: CalendarDatePicker exposes a Fluent date entry surface
The `CalendarDatePicker` component SHALL provide a reusable WinUI-style date entry control under `view::date_time`.

#### Scenario: Empty picker shows placeholder
- **WHEN** a CalendarDatePicker is created with placeholder text but no valid date
- **THEN** the control SHALL render the placeholder in the entry surface

#### Scenario: Entry surface contains calendar affordance
- **WHEN** the CalendarDatePicker is visible and enabled
- **THEN** the entry surface SHALL include a calendar glyph affordance and SHALL use project Fluent typography, spacing, border, background, and state colors

#### Scenario: Disabled picker blocks interaction
- **WHEN** the CalendarDatePicker is disabled
- **THEN** the entry surface SHALL render disabled styling and SHALL NOT open the calendar flyout from mouse or keyboard activation

### Requirement: CalendarDatePicker manages nullable selected date state
The CalendarDatePicker SHALL expose the selected date as a `QDate`, using an invalid `QDate` to represent no selected date.

#### Scenario: Setting a valid date updates display text
- **WHEN** `setDate()` is called with a valid date
- **THEN** `date()` SHALL return that date and the entry surface SHALL display formatted date text

#### Scenario: Clearing date restores placeholder
- **WHEN** `setDate(QDate())` or the clear API is used
- **THEN** `date()` SHALL return an invalid date and the entry surface SHALL display placeholder text

#### Scenario: Duplicate date does not emit duplicate notification
- **WHEN** `setDate()` is called with the same date value already held by the control
- **THEN** `dateChanged` SHALL NOT be emitted again

#### Scenario: Display format controls closed text
- **WHEN** a display format such as `yyyy-MM-dd` is configured and a date is selected
- **THEN** the entry surface SHALL format the selected date using that display format

### Requirement: CalendarDatePicker opens an anchored calendar flyout
The CalendarDatePicker SHALL open a calendar flyout anchored to the entry surface and compose a reusable CalendarView inside that flyout.

#### Scenario: Entry activation opens flyout
- **WHEN** the user activates the enabled entry surface
- **THEN** the control SHALL open a non-modal Flyout anchored to the CalendarDatePicker

#### Scenario: Open state is observable
- **WHEN** the calendar flyout opens or closes
- **THEN** `isCalendarOpen()` SHALL reflect the current state and the control SHALL emit the corresponding open-state notification once per transition

#### Scenario: Light dismiss closes flyout
- **WHEN** the calendar flyout is open and the user presses Escape or clicks outside according to Flyout close policy
- **THEN** the flyout SHALL close without changing the selected date

#### Scenario: Reopen starts from selected or current month
- **WHEN** the calendar flyout is opened
- **THEN** it SHALL show the selected date's month when a valid date exists, otherwise it SHALL show the current month

#### Scenario: Picker exposes composed calendar view
- **WHEN** the CalendarDatePicker flyout has been created
- **THEN** applications and tests SHALL be able to access the composed CalendarView instance without relying on private implementation classes

### Requirement: CalendarView exposes a standalone navigable month calendar
The CalendarView SHALL render a single-month grid suitable for standalone date selection and picker flyout composition.

#### Scenario: Month grid includes weekday headers and six weeks
- **WHEN** the flyout is open for a visible month
- **THEN** it SHALL show a month title, weekday headers ordered by `firstDayOfWeek`, and a stable 6 row by 7 column day grid

#### Scenario: Adjacent month days are visible but muted
- **WHEN** the visible month does not start on the first grid column or end on the last grid column
- **THEN** leading and trailing adjacent-month days SHALL be visible with muted styling

#### Scenario: Month navigation changes visible month
- **WHEN** the user activates previous-month or next-month navigation on CalendarView
- **THEN** the visible month SHALL move by one month and the day grid SHALL update without changing the selected date unless the user activates a day

#### Scenario: Today is indicated
- **WHEN** today's date is visible in the grid and is not selected
- **THEN** the grid SHALL render a distinct today indication that does not conflict with selection styling

#### Scenario: Standalone date activation is observable
- **WHEN** the user activates an enabled date cell directly on CalendarView
- **THEN** CalendarView SHALL update its selected date and emit the activated date

### Requirement: CalendarDatePicker enforces selectable date bounds
The CalendarDatePicker SHALL support minimum and maximum selectable dates.

#### Scenario: Date before minimum is clamped
- **WHEN** `setDate()` is called with a date before `minDate`
- **THEN** the control SHALL clamp the selected date to `minDate` and SHALL NOT expose an out-of-range selected date

#### Scenario: Date after maximum is clamped
- **WHEN** `setDate()` is called with a date after `maxDate`
- **THEN** the control SHALL clamp the selected date to `maxDate` and SHALL NOT expose an out-of-range selected date

#### Scenario: Out-of-range grid cells are disabled
- **WHEN** the calendar flyout shows dates outside the configured min/max range
- **THEN** those dates SHALL render disabled styling and SHALL NOT become selected when activated

#### Scenario: Constraint changes preserve valid state
- **WHEN** `minDate` or `maxDate` changes
- **THEN** any valid selected date outside the new range SHALL be clamped to the nearest range endpoint, and an invalid selected date SHALL remain invalid

### Requirement: CalendarDatePicker selection updates from calendar activation
The CalendarDatePicker SHALL update its selected date when the user activates an enabled date cell in the flyout.

#### Scenario: Selecting day updates date and closes flyout
- **WHEN** the user activates an enabled date cell
- **THEN** the control SHALL set `date()` to that cell's date, emit `dateChanged` once, update entry text, and close the flyout

#### Scenario: Selecting adjacent-month day is supported
- **WHEN** the user activates an enabled adjacent-month day cell
- **THEN** the control SHALL select that date and SHALL update the visible month consistently with the selected date

#### Scenario: Keyboard day activation works while flyout is open
- **WHEN** the flyout has keyboard focus and the user navigates to an enabled date cell then presses Enter or Space
- **THEN** the control SHALL select that focused date with the same result as mouse activation

### Requirement: CalendarDatePicker supports theme updates and visual validation
The CalendarDatePicker SHALL integrate with existing Fluent theme and test infrastructure.

#### Scenario: Theme update refreshes entry and flyout
- **WHEN** the active Fluent theme changes while the control or flyout is visible
- **THEN** the entry surface, calendar card, day cells, selected state, today state, and disabled state SHALL repaint using the new theme colors

#### Scenario: Automated tests cover public behavior
- **WHEN** the CalendarDatePicker test target runs with `SKIP_VISUAL_TEST=1`
- **THEN** it SHALL validate default properties, date changes, formatting, constraints, flyout open/close, month navigation, selection, disabled state, and duplicate signal suppression without opening an interactive window

#### Scenario: VisualCheck demonstrates reference states
- **WHEN** the CalendarDatePicker VisualCheck runs without `SKIP_VISUAL_TEST=1`
- **THEN** it SHALL show empty, selected, constrained, disabled, and dark/light examples using project Fluent components and `AnchorLayout`
