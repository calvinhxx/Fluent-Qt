## Why

The repository has a `date_time` component area but no WinUI-style DatePicker control. Apps currently cannot present the Fluent three-field month/day/year picker shown in WinUI Gallery or validate a date through a reusable project component.

This change adds a custom DatePicker that matches the Windows UI Kit / WinUI Gallery interaction model while fitting the existing Qt Widgets component architecture.

## What Changes

- Add a `DatePicker` component under `src/view/date_time` with a segmented month/day/year entry surface, nullable selected date state, min/max range constraints, field visibility, and field formatting.
- Add a Flyout-backed picker surface with three date columns, selected-row accent highlight, top/bottom column navigation affordances, confirm/cancel actions, light-dismiss, and keyboard/wheel navigation.
- Add tests and a VisualCheck demo under `tests/views/date_time`, using project Fluent components and `AnchorLayout` for the visible test layout.
- Register the new date_time test directory in the views test CMake tree.

## Capabilities

### New Capabilities
- `date-picker`: Covers the WinUI-style DatePicker component, including date state, range validation, field visibility/formatting, segmented entry rendering, anchored picker flyout behavior, commit/cancel semantics, theming, and tests.

### Modified Capabilities
- None.

## Impact

- New component files in `src/view/date_time/DatePicker.h` and `src/view/date_time/DatePicker.cpp`.
- New test files in `tests/views/date_time/TestDatePicker.cpp` and `tests/views/date_time/CMakeLists.txt`.
- Update `tests/views/CMakeLists.txt` to include the `date_time` test directory.
- Reuse existing `view::dialogs_flyouts::Flyout`, `view::basicinput::Button`, and Fluent theme tokens; no new third-party dependencies are expected.
