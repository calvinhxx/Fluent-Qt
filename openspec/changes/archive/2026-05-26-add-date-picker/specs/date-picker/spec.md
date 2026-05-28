## ADDED Requirements

### Requirement: DatePicker exposes nullable selected date state
`DatePicker` SHALL expose a valid `date` state and a nullable `selectedDate` state. An invalid `QDate` selected date SHALL represent the unset state, and the entry surface SHALL show field placeholders while unset.

#### Scenario: Default picker is unset
- **WHEN** a DatePicker is constructed
- **THEN** `selectedDate()` SHALL be invalid
- **AND** the month, day, and year entry segments SHALL show placeholder text

#### Scenario: Setting selected date updates visible fields
- **WHEN** `setSelectedDate(QDate(2026, 5, 21))` is called
- **THEN** `selectedDate()` SHALL equal May 21, 2026
- **AND** the entry surface SHALL show formatted month, day, and year values

#### Scenario: Clearing selected date restores placeholders
- **WHEN** a picker has a selected date and `clearSelectedDate()` is called
- **THEN** `selectedDate()` SHALL become invalid
- **AND** the entry surface SHALL show placeholders without changing the configured date range

### Requirement: DatePicker enforces selectable date range
`DatePicker` SHALL expose `minimumDate` and `maximumDate` bounds. Valid selected dates SHALL be clamped to the active range, and invalid range assignments SHALL be normalized so `minimumDate <= maximumDate`.

#### Scenario: Selected date below range clamps to minimum
- **WHEN** the minimum date is May 1, 2026 and `setSelectedDate(QDate(2026, 4, 30))` is called
- **THEN** `selectedDate()` SHALL equal May 1, 2026

#### Scenario: Selected date above range clamps to maximum
- **WHEN** the maximum date is May 31, 2026 and `setSelectedDate(QDate(2026, 6, 1))` is called
- **THEN** `selectedDate()` SHALL equal May 31, 2026

#### Scenario: Month change clamps invalid day
- **WHEN** the pending date is March 31, 2026
- **AND** the month column changes to April
- **THEN** the pending date SHALL become April 30, 2026

#### Scenario: Leap day remains valid only in leap years
- **WHEN** the pending date is February 29, 2024
- **AND** the year column changes to 2025
- **THEN** the pending date SHALL become February 28, 2025

### Requirement: DatePicker supports field visibility and formatting
`DatePicker` SHALL allow month, day, and year fields to be independently shown or hidden. At least one field SHALL remain visible. Visible fields SHALL support project-defined formatting options that cover WinUI Gallery examples.

#### Scenario: Year field can be hidden
- **WHEN** `setYearVisible(false)` is called
- **THEN** the entry surface SHALL hide the year segment
- **AND** the picker flyout SHALL hide the year column

#### Scenario: Last visible field cannot be hidden
- **WHEN** only the month field is visible
- **AND** `setMonthVisible(false)` is called
- **THEN** the month field SHALL remain visible

#### Scenario: Day can include abbreviated weekday
- **WHEN** the day format is set to the weekday-abbreviated option
- **AND** the selected date is July 21, 2026
- **THEN** the day segment SHALL display a value equivalent to `21 (Tue)`

#### Scenario: Month can display full month name
- **WHEN** the month format is set to full month name
- **AND** the selected date is July 21, 2026
- **THEN** the month segment SHALL display `July`

### Requirement: DatePicker entry surface matches Fluent segmented picker behavior
`DatePicker` SHALL render a compact segmented entry surface. The surface SHALL provide hover, pressed, focused, disabled, and open states using Fluent theme tokens. Descriptive label text SHALL be composed by the application or test page outside the control.

#### Scenario: Click opens picker flyout
- **WHEN** the user clicks an enabled visible entry segment
- **THEN** the DatePicker flyout SHALL open anchored to the DatePicker
- **AND** the DatePicker SHALL enter the open visual state

#### Scenario: Disabled picker does not open
- **WHEN** the DatePicker is disabled
- **AND** the user clicks the entry surface
- **THEN** the flyout SHALL remain closed

### Requirement: DatePicker flyout provides column-based date selection
The DatePicker flyout SHALL present one column per visible field. It SHALL show selectable month, day, and year values, a shared accent selected row, column dividers, top/bottom navigation affordances, and confirm/cancel commands.

#### Scenario: Flyout opens with pending date centered
- **WHEN** a DatePicker with selected date May 21, 2026 opens its flyout
- **THEN** the month, day, and year columns SHALL center May, 21, and 2026 in the selected row

#### Scenario: Hidden year removes year column
- **WHEN** `yearVisible` is false
- **AND** the flyout opens
- **THEN** only the visible month and day columns SHALL be shown

#### Scenario: Disabled values outside range are not selectable
- **WHEN** the flyout displays values outside the configured minimum or maximum date range
- **THEN** those values SHALL render disabled
- **AND** clicking those values SHALL NOT change the pending date

#### Scenario: Confirm commits pending date
- **WHEN** the flyout pending date is May 21, 2026
- **AND** the user activates the confirm command
- **THEN** `selectedDate()` SHALL become May 21, 2026
- **AND** the flyout SHALL close

#### Scenario: Cancel preserves previous selected date
- **WHEN** a DatePicker has selected date May 1, 2026
- **AND** the flyout pending date changes to May 21, 2026
- **AND** the user activates the cancel command
- **THEN** `selectedDate()` SHALL remain May 1, 2026
- **AND** the flyout SHALL close

### Requirement: DatePicker supports keyboard, wheel, and light-dismiss interaction
DatePicker SHALL support keyboard opening, column navigation, confirmation, cancellation, and inherited Flyout light-dismiss behavior.

#### Scenario: Keyboard opens picker
- **WHEN** DatePicker has focus and the user presses Space or Enter
- **THEN** the picker flyout SHALL open

#### Scenario: Escape cancels open picker
- **WHEN** the picker flyout is open
- **AND** the user presses Escape
- **THEN** the flyout SHALL close
- **AND** pending changes SHALL NOT be committed

#### Scenario: Enter confirms open picker
- **WHEN** the picker flyout is open and has a valid pending date
- **AND** the user presses Enter
- **THEN** the pending date SHALL be committed
- **AND** the flyout SHALL close

#### Scenario: Wheel changes focused column
- **WHEN** the picker flyout is open
- **AND** the user wheels over a visible column
- **THEN** that column SHALL move to the next or previous valid value
- **AND** the pending date SHALL be normalized

#### Scenario: Outside press light-dismisses picker
- **WHEN** the picker flyout is open
- **AND** the user presses outside the flyout card
- **THEN** the flyout SHALL close without committing pending changes

### Requirement: DatePicker integrates with theme and tests
DatePicker SHALL update visuals when Fluent theme tokens change and include automated and VisualCheck coverage registered in the date_time test tree.

#### Scenario: Theme update repaints DatePicker and flyout
- **WHEN** the active Fluent theme changes while DatePicker exists
- **THEN** the entry surface, flyout card, selected row, disabled text, and command buttons SHALL repaint using the current theme tokens

#### Scenario: Test target is registered
- **WHEN** the project configures tests
- **THEN** `tests/views/date_time/CMakeLists.txt` SHALL register `test_date_picker` using `add_qt_test_module(test_date_picker TestDatePicker.cpp)`

#### Scenario: VisualCheck uses repository UI conventions
- **WHEN** `TestDatePicker.VisualCheck` is implemented
- **THEN** it SHALL guard on `SKIP_VISUAL_TEST`
- **AND** it SHALL use `AnchorLayout` for the primary visible layout
- **AND** visible command/text controls SHALL prefer project Fluent components over raw Qt widgets
