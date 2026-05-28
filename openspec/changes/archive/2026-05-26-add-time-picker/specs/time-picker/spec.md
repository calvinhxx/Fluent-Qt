## ADDED Requirements

### Requirement: TimePicker exposes nullable selected time state
The `TimePicker` component SHALL provide a reusable WinUI-style time picker under `view::date_time`. It SHALL expose a valid `time` state and a nullable `selectedTime` state, using an invalid `QTime` selected time to represent the unset state.

#### Scenario: Default picker is unset
- **WHEN** a TimePicker is constructed
- **THEN** `selectedTime()` SHALL be invalid
- **AND** `time()` SHALL be valid
- **AND** the entry surface SHALL show hour and minute placeholders

#### Scenario: Setting selected time updates display
- **WHEN** `setSelectedTime(QTime(9, 30))` is called
- **THEN** `selectedTime()` SHALL equal 09:30
- **AND** `time()` SHALL equal 09:30
- **AND** the entry surface SHALL show the formatted time fields

#### Scenario: Clearing selected time restores placeholders
- **WHEN** a picker has a selected time and `clearSelectedTime()` is called
- **THEN** `selectedTime()` SHALL become invalid
- **AND** the entry surface SHALL show placeholders without invalidating `time()`

#### Scenario: Duplicate normalized time does not emit duplicate notification
- **WHEN** a TimePicker already has selected time 09:30
- **AND** `setSelectedTime(QTime(9, 30))` is called again
- **THEN** `selectedTimeChanged` SHALL NOT be emitted again

### Requirement: TimePicker supports twelve-hour and twenty-four-hour clock modes
TimePicker SHALL expose a `ClockIdentifier` setting with twelve-hour and twenty-four-hour modes. Twelve-hour mode SHALL show an AM/PM period field, while twenty-four-hour mode SHALL hide the period field and use 00-23 hour display.

#### Scenario: Twelve-hour mode displays period
- **WHEN** the clock identifier is `TwelveHourClock`
- **AND** the selected time is 13:05
- **THEN** the entry surface SHALL display an hour value equivalent to `1`
- **AND** it SHALL display minute `05`
- **AND** it SHALL display period `PM`

#### Scenario: Midnight maps to twelve AM
- **WHEN** the clock identifier is `TwelveHourClock`
- **AND** the selected time is 00:15
- **THEN** the entry surface SHALL display an hour value equivalent to `12`
- **AND** it SHALL display period `AM`

#### Scenario: Twenty-four-hour mode hides period
- **WHEN** the clock identifier is `TwentyFourHourClock`
- **AND** the selected time is 13:05
- **THEN** the entry surface SHALL display hour `13`
- **AND** it SHALL NOT show an AM/PM period segment

#### Scenario: Clock mode change preserves absolute time
- **WHEN** a TimePicker has selected time 23:45 in twenty-four-hour mode
- **AND** the clock identifier changes to `TwelveHourClock`
- **THEN** `selectedTime()` SHALL remain 23:45
- **AND** the entry surface SHALL display an equivalent 11:45 PM value

### Requirement: TimePicker enforces minute increments
TimePicker SHALL expose `minuteIncrement`. The effective minute increment SHALL be clamped to 1 through 59, and selected or pending minutes SHALL snap to the nearest selectable minute generated from that increment.

#### Scenario: Minute increment defines selectable minutes
- **WHEN** `setMinuteIncrement(15)` is called
- **THEN** the minute column SHALL offer 00, 15, 30, and 45 as selectable values

#### Scenario: Programmatic time snaps to nearest increment
- **WHEN** `minuteIncrement` is 15
- **AND** `setSelectedTime(QTime(9, 10))` is called
- **THEN** `selectedTime()` SHALL equal 09:15

#### Scenario: High minute snaps without rolling hour
- **WHEN** `minuteIncrement` is 15
- **AND** `setSelectedTime(QTime(9, 58))` is called
- **THEN** `selectedTime()` SHALL equal 09:45

#### Scenario: Invalid increment is clamped
- **WHEN** `setMinuteIncrement(0)` is called
- **THEN** `minuteIncrement()` SHALL return 1
- **WHEN** `setMinuteIncrement(90)` is called
- **THEN** `minuteIncrement()` SHALL return 59

### Requirement: TimePicker entry surface matches Fluent segmented picker behavior
TimePicker SHALL render a compact segmented entry surface. The surface SHALL provide hover, pressed, focused, disabled, and open states using Fluent theme tokens. Descriptive label text SHALL be composed by the application or test page outside the control.

#### Scenario: Click opens picker flyout
- **WHEN** the user clicks an enabled entry segment
- **THEN** the TimePicker flyout SHALL open anchored to the TimePicker
- **AND** the TimePicker SHALL enter the open visual state

#### Scenario: Disabled picker does not open
- **WHEN** the TimePicker is disabled
- **AND** the user clicks the entry surface
- **THEN** the flyout SHALL remain closed

### Requirement: TimePicker flyout provides column-based time selection
The TimePicker flyout SHALL present hour and minute columns plus a period column in twelve-hour mode. It SHALL show a shared accent selected row, column dividers, top/bottom navigation affordances, and confirm/cancel commands.

#### Scenario: Flyout opens with pending time centered
- **WHEN** a TimePicker with selected time 09:30 opens its flyout
- **THEN** the hour and minute columns SHALL center 9 and 30 in the selected row

#### Scenario: Twenty-four-hour flyout hides period column
- **WHEN** `clockIdentifier` is `TwentyFourHourClock`
- **AND** the flyout opens
- **THEN** only the hour and minute columns SHALL be shown

#### Scenario: Period column changes AM and PM
- **WHEN** the flyout is open in twelve-hour mode with pending time 09:30
- **AND** the period column changes from AM to PM
- **THEN** the pending time SHALL become 21:30

#### Scenario: Confirm commits pending time
- **WHEN** the flyout pending time is 14:45
- **AND** the user activates the confirm command
- **THEN** `selectedTime()` SHALL become 14:45
- **AND** the flyout SHALL close

#### Scenario: Cancel preserves previous selected time
- **WHEN** a TimePicker has selected time 09:00
- **AND** the flyout pending time changes to 10:30
- **AND** the user activates the cancel command
- **THEN** `selectedTime()` SHALL remain 09:00
- **AND** the flyout SHALL close

### Requirement: TimePicker supports keyboard, wheel, and light-dismiss interaction
TimePicker SHALL support keyboard opening, column navigation, confirmation, cancellation, and inherited Flyout light-dismiss behavior.

#### Scenario: Keyboard opens picker
- **WHEN** TimePicker has focus and the user presses Space or Enter
- **THEN** the picker flyout SHALL open

#### Scenario: Escape cancels open picker
- **WHEN** the picker flyout is open
- **AND** the user presses Escape
- **THEN** the flyout SHALL close
- **AND** pending changes SHALL NOT be committed

#### Scenario: Enter confirms open picker
- **WHEN** the picker flyout is open and has a valid pending time
- **AND** the user presses Enter
- **THEN** the pending time SHALL be committed
- **AND** the flyout SHALL close

#### Scenario: Wheel changes focused column
- **WHEN** the picker flyout is open
- **AND** the user wheels over a visible column
- **THEN** that column SHALL move to the next or previous valid value
- **AND** the pending time SHALL be normalized through the minute increment helper

#### Scenario: Outside press light-dismisses picker
- **WHEN** the picker flyout is open
- **AND** the user presses outside the flyout card
- **THEN** the flyout SHALL close without committing pending changes

### Requirement: TimePicker integrates with theme and tests
TimePicker SHALL update visuals when Fluent theme tokens change and include automated and VisualCheck coverage registered in the date_time test tree.

#### Scenario: Theme update repaints TimePicker and flyout
- **WHEN** the active Fluent theme changes while TimePicker exists
- **THEN** the entry surface, flyout card, selected row, disabled text, dividers, and command buttons SHALL repaint using the current theme tokens

#### Scenario: Test target is registered
- **WHEN** the project configures tests
- **THEN** `tests/views/date_time/CMakeLists.txt` SHALL register `test_time_picker` using `add_qt_test_module(test_time_picker TestTimePicker.cpp)`

#### Scenario: VisualCheck uses repository UI conventions
- **WHEN** `TestTimePicker.VisualCheck` is implemented
- **THEN** it SHALL guard on `SKIP_VISUAL_TEST`
- **AND** it SHALL use `AnchorLayout` for the primary visible layout
- **AND** visible command/text controls SHALL prefer project Fluent components over raw Qt widgets
