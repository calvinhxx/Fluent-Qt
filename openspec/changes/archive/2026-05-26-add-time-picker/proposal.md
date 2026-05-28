## Why

The `date_time` area now has DatePicker and CalendarDatePicker coverage, but it still lacks a WinUI-style TimePicker. Applications cannot offer a reusable Fluent control for selecting an hour/minute value, using minute increments, or switching between 12-hour and 24-hour presentations without composing ad hoc widgets.

This change adds a custom Qt Widgets TimePicker that follows the Windows UI Kit / WinUI Gallery interaction model while reusing the repository's existing Fluent theme, Flyout, and date_time component patterns.

## What Changes

- Add a `TimePicker` component under `src/view/date_time` with nullable selected time state, a valid current time state, minute increments, 12-hour/24-hour clock mode, observable open state, and formatted closed display.
- Add a Flyout-backed picker surface with hour, minute, and optional period columns, selected-row accent highlight, top/bottom column navigation affordances, confirm/cancel actions, light-dismiss, and keyboard/wheel navigation.
- Add automated tests and a VisualCheck demo under `tests/views/date_time`, covering the WinUI Gallery examples with application-level labels, `MinuteIncrement=15`, and 24-hour clock initialized to current time.
- Register the new focused TimePicker test target in the date_time CMake test tree.

## Capabilities

### New Capabilities

- `time-picker`: Covers the WinUI-style TimePicker component, including nullable time state, 12-hour/24-hour display, minute increment normalization, segmented entry rendering, anchored flyout behavior, commit/cancel semantics, theming, and tests.

### Modified Capabilities

- None.

## Impact

- New component files in `src/view/date_time/TimePicker.h` and `src/view/date_time/TimePicker.cpp`.
- New test file in `tests/views/date_time/TestTimePicker.cpp` and update to `tests/views/date_time/CMakeLists.txt`.
- Reuse existing `view::dialogs_flyouts::Flyout`, `view::basicinput::Button`, Fluent design tokens, and date_time test conventions; no new third-party dependencies are expected.
