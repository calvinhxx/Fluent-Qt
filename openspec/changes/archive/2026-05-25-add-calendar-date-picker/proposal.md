## Why

The project has a `date_time` component directory but does not yet provide a Fluent/WinUI-style control for picking one date from a contextual calendar surface. WinUI Gallery and the Windows UI kit reference show CalendarDatePicker as a compact entry control that opens a calendar flyout, which fits upcoming date/time input work better than exposing raw Qt date widgets.

## What Changes

- Add custom `CalendarView` and `CalendarDatePicker` components under `src/view/date_time`.
- Make `CalendarView` the reusable month-calendar surface for standalone use and picker flyouts.
- Provide a closed entry surface with placeholder text, selected-date formatting, calendar icon, disabled state, and theme-aware Fluent styling.
- Provide an anchored CalendarDatePicker flyout that composes `CalendarView` for month navigation and single-date selection.
- Support nullable selected date, min/max date constraints, first day of week, today highlighting, and date-changed notification.
- Add unit tests and a VisualCheck sample for empty, selected, constrained, disabled, light, and dark states.
- Add the date/time test category to the test build.

## Capabilities

### New Capabilities
- `calendar-date-picker`: Covers a WinUI-style single-date picker entry, reusable CalendarView month surface, and calendar flyout implemented as reusable `date_time` components.

### Modified Capabilities
- None.

## Impact

- New component files in `src/view/date_time/`.
- New tests under `tests/views/date_time/` plus registration from `tests/views/CMakeLists.txt`.
- Reuses existing project infrastructure: `FluentElement`, `QMLPlus`, `Flyout`, `Button`, `LineEdit` style tokens where appropriate, `Typography::Icons::Calendar`, `Spacing`, and shared Qt/GTest startup.
- No new third-party dependency is expected.
