# Date and time range picker

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Timeline and Stepper](sequence-controls.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:top:end -->

`fluent::date_time::DateTimeRangePicker` shows one compact range entry. Its
same-window flyout composes a range calendar, `CalendarDatePicker`, `TimePicker`,
independent clear actions, and quick presets. C++ and PySide6 expose the same
native component. Gallery's **Date & time →
DateTimeRangePicker** route shows date-only, minute-precision, and open-ended
examples.

## Compact entry and range calendar

`openPicker()` opens the flyout; `closePicker()` closes it without discarding
values. Space, Enter, F4, and Alt+Down open the entry. Escape and outside clicks
use the shared overlay close policy. `isPickerOpen()` and `pickerOpenChanged`
describe the logical open state.

The main calendar supports selecting a range in two activations. Choosing a
start date moves editing to End while preserving an existing end value.
Starting from empty bounds with reversed picks creates a chronological interval.
When the end is empty, reversing an existing start preserves that start's time
and time specification as the new end, rather than constructing a local midnight.

Start and End each open a compact, anchored **Flyout** containing a single-date
calendar. It uses smaller day cells and less padding, with the usual month/year
navigation and keyboard controls. Choosing a date closes only that endpoint
flyout, preserves the other endpoint, and updates the main calendar. Opening
the other endpoint switches flyouts. Repeating the same trigger or pressing
Escape dismisses the small flyout and leaves the range panel open.

Times are edited directly in the range panel with the configured 12/24-hour
format. Enter or leaving a time field commits valid input; Up/Down changes the
hour or minute segment at the cursor, using the configured minute increment.
Invalid text restores the current value. Selecting dates or presets keeps the
range panel open. **Done** commits pending time input and dismisses the panel
and its endpoint flyout. Escape and outside clicks retain light-dismiss behavior.

The calendar paints filled endpoint circles and a continuous accent band,
including across week and month boundaries. This decoration is private to the
range picker; ordinary `CalendarView` retains its single-date public contract.
`calendarView()` returns the picker-owned calendar for navigation and first-day
configuration. Use the range picker's endpoint getters as the selected values.

All three calendar surfaces share `CalendarView` scrolling. Pixel input and
platform momentum move the visible rows directly; discrete mouse-wheel input
uses a short, interruptible transition. Scrolling can cross month/year
boundaries within one gesture and does not change selection or close either
flyout. `visibleMonthChanged` follows the month occupying the viewport center,
so one long gesture can notify several actual month changes. Reopening either picker starts at a complete month containing its
selected date, even when the previous scroll stopped within that same month. The header arrows and Page Up/Down retain page navigation. Partially
visible rows keep their date hit targets and accessibility bounds aligned with
the painted content. Diagnostic cell maps refresh once per paint, and scrolling
accessibility resets are coalesced until the gesture ends or input becomes idle.
Reduced/Disabled motion applies to wheel interpolation;
direct input remains responsive.

`displayText()` shows the complete range, **From** or **Until** for a single
bound, or `placeholderText` for two empty bounds. The closed entry also shows
range validation. `entryButton()` returns the picker-owned entry action.

## Quick presets

The flyout includes **Today**, **Yesterday**, **Past week**, **Past month**,
**Past six months**, and **Past year**. These are compact text actions above the
endpoint controls, without a heading or an expansion step. The actions wrap
into more rows in a narrow host. Selecting one keeps the range panel open.
A short host scrolls the content while retaining the footer.
`setPresetsVisible(false)` hides these actions.

Built-in presets use whole calendar days. The past week includes the reference
day and six earlier dates. Month/year ranges begin one day after
`reference.date().addMonths(-1/-6)` or `addYears(-1)`, with Qt's ordinary
end-of-month clamping. Today/Yesterday each select one date. Time-enabled values
use 00:00 and 23:59. The end input includes 23:59 as an additional valid terminal
minute, so coarser minute increments do not shorten a whole-day preset. Ordinary
standalone `TimePicker` increments are unchanged. Child date bounds still apply;
date-only values use midnight. The reference time specification is retained.
Applications still own inclusive/exclusive query rules, including treatment of
the final minute.

`applyPreset(Preset)` uses local today. Its `QDateTime` overload accepts a
reference for deterministic or application-clock calculations. To provide
business-specific ranges, call `setAutoApplyPresetsEnabled(false)` and handle
`presetRequested(Preset)` by calling `setRange()`:

```python
picker.setAutoApplyPresetsEnabled(False)
picker.presetRequested.connect(apply_business_range)
```

## Values and validation

- Invalid `QDateTime()` values represent empty, unbounded endpoints. Either or
  both endpoints may be empty. `clearStart()`, `clearEnd()`, and `clear()` do
  not substitute the current date.
- Time inputs are enabled after their corresponding date is selected. A new
  date starts at midnight; editing an existing date preserves its time and Qt
  time specification. Clearing a time input on a selected date resets that
  endpoint to midnight.
- Values use minute precision. Child time-picker minute increments are honored,
  including the nearest-minute and upward-tie rule; the end input additionally
  accepts 23:59. Child calendar date bounds clamp their corresponding endpoint.
- `setTimeEnabled(false)` hides time inputs and normalizes both stored values
  to midnight. Re-enabling time does not restore discarded hour/minute values.
- `setRange(start, end)` updates both getters and child controls before emitting
  endpoint notifications and one `rangeChanged(start, end)` notification. An
  unchanged effective value emits no duplicate public change signals.
- Reversed endpoints remain visible. `isRangeValid()` becomes false and the end
  field displays `invalidRangeText`. Empty endpoints and equal endpoints are
  valid. Query code should check `isRangeValid()` before applying a filter.

Input time specifications and time zones are retained. Fresh values selected
through the calendar use local time. The control does not choose a database
timestamp unit, perform UTC conversion, or construct query predicates. Qt's
time-zone rules determine whether an edited wall-clock value is valid; an
invalid result is exposed as an empty endpoint.

Date-only endpoints also retain their time specification. Both store midnight;
the application decides whether to include the whole ending day. For an
inclusive final date, an application can use an exclusive bound at the next
calendar day's midnight. Advance the calendar date rather than assuming every
local day lasts exactly 24 hours.

## C++

```cpp
#include <FluentQt/FluentQt.h>

auto* picker = new fluent::date_time::DateTimeRangePicker(parent);
picker->setStartLabelText("Start");
picker->setEndLabelText("End");
picker->setRange(QDateTime(QDate(2025, 10, 1), QTime(10, 10)),
                 QDateTime(QDate(2025, 10, 3), QTime(15, 10)));
```

Connect `rangeChanged` to the application's query handler and check
`isRangeValid()` before applying either valid endpoint.

## PySide6

```python
from PySide6.QtCore import QDate, QDateTime, QTime
from fluentqt.date_time import DateTimeRangePicker

picker = DateTimeRangePicker(parent)
picker.setStartLabelText("Start")
picker.setEndLabelText("End")
picker.startDatePicker().setPlaceholderText("No lower bound")
picker.endDatePicker().setPlaceholderText("No upper bound")
picker.setStartDateTime(QDateTime(QDate(2025, 10, 1), QTime(10, 10)))
assert not picker.endDateTime().isValid()
picker.clearStart()
```

Run the [Python example](../../bindings/pyside6/examples/date_time_range_picker.py)
to inspect both endpoint controls, clear actions, mode switching, and the
selected bounds.

## Layout, ownership, and accessibility

The closed component occupies one entry row. Inside the flyout, endpoint groups
share a row when space permits and stack in a narrow container. RTL reverses
the horizontal endpoint arrangement; calendar bands follow the date-cell order. Labels are
application-owned and empty by default. The translatable validation message
and accessible action names can be customized through public APIs.

Each endpoint uses equal-width date and time controls. The clear action sits
inside the date control and disappears for an empty endpoint. A shared validation
row below both endpoints preserves their alignment when the range is reversed;
long messages wrap across the available width.
Date triggers retain their surface and text position while pressed and while
their calendars are open; these states change only the control's tone.

`startDatePicker()`, `endDatePicker()`, `startTimePicker()`, `endTimePicker()`,
`startClearButton()`, and `endClearButton()` return component-owned children.
Use them to configure display formats, placeholders, date bounds, minute
increments, clock modes, and accessible names. These getters do not transfer
ownership in C++ or Python; callers must not delete or reparent the children.

Date children use compact anchored calendars; time children host Fluent text
inputs. Their value/configuration APIs remain available. Hiding or disabling
the component closes all open pickers. The surfaces use the shared
[same-window overlay](overlay-behavior.md) placement, Escape, light-dismiss,
and focus-return behavior.

The root exposes a combo-box role, expanded/collapsed state, open actions, and
localized start/end values. The calendar exposes all visible selected date
cells and their keyboard actions. Validation is
available through the accessible description while an application-provided
description remains authoritative. The root does not misuse Qt's defunct
`invalid` accessible state as an input-validation state. Native child controls
provide keyboard actions and their own accessible roles, values, and names.

Focused contracts are in
[TestDateTimeRangePicker.cpp](../../tests/components/date_time/TestDateTimeRangePicker.cpp),
with Python value and owned-child teardown coverage in
[test_bindings.py](../../bindings/pyside6/tests/test_bindings.py).

<!-- docs-nav:bottom:start -->
---
[← Timeline and Stepper](sequence-controls.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md)
<!-- docs-nav:bottom:end -->
