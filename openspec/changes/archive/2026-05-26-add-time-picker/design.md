## Context

`src/view/date_time` already contains `DatePicker`, `CalendarDatePicker`, and `CalendarView`. `DatePicker` is the closest local pattern: it is a `QWidget, FluentElement, QMLPlus` component with a custom-painted segmented entry surface, a private Flyout, private picker columns, confirm/cancel commands, keyboard/wheel handling, and focused unit plus VisualCheck coverage.

External behavior references:

- The requested Windows UI Kit Figma node is the visual reference for the Fluent TimePicker entry and flyout composition.
- WinUI Gallery `WinUIGallery/Samples/ControlPages/TimePickerPage.xaml` demonstrates simple, minute-increment, and 24-hour TimePicker examples. In this project, surrounding labels are composed by the application/test page rather than the TimePicker component.
- WinUI TimePicker separates nullable `SelectedTime` from the displayed time, supports `MinuteIncrement`, and switches AM/PM period presentation through `ClockIdentifier`.

## Goals / Non-Goals

**Goals:**

- Add a reusable `view::date_time::TimePicker` Qt Widgets control with WinUI-style appearance and interaction.
- Support unset selected time, programmatic selected/current time, minute increments, 12-hour and 24-hour clock modes, and an observable open state.
- Use an anchored Flyout picker that opens over surrounding UI without relayout, supports commit/cancel, Escape, Enter, wheel, arrow keys, and light-dismiss.
- Add automated tests plus a VisualCheck demo that covers the WinUI Gallery examples and common edge cases.

**Non-Goals:**

- Do not implement date selection, seconds, time zone handling, or time range constraints in this change.
- Do not add locale/calendar infrastructure beyond deterministic hour/minute/AM/PM formatting needed for the first TimePicker pass.
- Do not introduce external dependencies or replace the existing Popup/Flyout infrastructure.
- Do not make TimePicker editable as free text input.

## Decisions

1. `TimePicker` will follow the DatePicker component shape.

   The public control will live in `namespace view::date_time` and inherit `QWidget, public FluentElement, public QMLPlus`. Descriptive label text should be composed by the application or test page outside the control, and TimePicker will use custom painting for the compact time entry so adjacent segments, focus, hover, pressed, disabled, and open states remain consistent with the DatePicker entry surface.

   Alternative considered: compose public `ComboBox` widgets for hour, minute, and AM/PM. That would commit per-column changes immediately and would not match the single flyout confirm/cancel model.

2. Nullable selected time will use invalid `QTime`.

   `selectedTime()` returns an invalid `QTime` when unset. `time()` remains valid and supplies the current/pending time used to seed the flyout. Setting a selected time also updates `time()`. Clearing the selected time restores placeholders while retaining a valid internal time for the next flyout open.

   Alternative considered: use `std::optional<QTime>`. Invalid `QTime` is simpler for Qt properties, signal tests, and existing DatePicker-style nullable semantics.

3. Clock mode will be a Qt enum modeled after WinUI `ClockIdentifier`.

   The component will expose `ClockIdentifier { TwelveHourClock, TwentyFourHourClock }` with `Q_ENUM`. In twelve-hour mode the entry and flyout show hour, minute, and period segments/columns. In twenty-four-hour mode the period segment/column is hidden and the hour range is 00-23.

   Alternative considered: expose raw strings such as `"24HourClock"`. An enum is safer for C++ callers and tests while still preserving the WinUI naming.

4. Minute increments will be normalized in one helper.

   `minuteIncrement` will clamp to the range 1-59. Available minute values will be `0, increment, 2*increment, ... <= 59`. Programmatic selected times and pending flyout changes will snap to the nearest available minute, clamping at the top of the hour when needed. Duplicate normalized values will not emit duplicate signals.

   Alternative considered: allow arbitrary minute values while only constraining the flyout. That would make the closed entry display values the user cannot reselect from the open picker.

5. The picker surface will be a private Flyout subclass with private column widgets.

   `TimePickerFlyout` will anchor to the TimePicker, use non-modal non-dimming light-dismiss, and own private `TimePickerColumn` widgets for hour, minute, and period. Each column will paint visible rows, disabled/hover/focus state, a shared accent selected row, and top/bottom navigation affordances. Confirm applies the pending time; cancel, Escape, and light-dismiss preserve the previous selected time.

   Alternative considered: generalize DatePicker's private picker column into a shared helper first. The first TimePicker pass should keep the blast radius low; shared extraction can follow once both implementations are stable.

6. VisualCheck will demonstrate the API states rather than hidden implementation details.

   The visible demo should use `AnchorLayout`, project `Label`/`Button` controls, and `SKIP_VISUAL_TEST` guard. It should show the simple TimePicker, the arrival-time 15-minute increment example, a 24-hour current-time example, unset placeholder state, disabled state, and theme switching.

## Risks / Trade-offs

- [Risk] Figma visual measurements can differ from DatePicker constants. -> Mitigation: reuse DatePicker geometry as the initial local baseline, keep constants local to TimePicker, and include VisualCheck states for manual comparison.
- [Risk] Minute increment snapping can surprise callers when setting non-step minutes. -> Mitigation: document snapping in the spec and add unit tests for low, midpoint, and high minute values.
- [Risk] 12-hour conversion around midnight/noon is easy to get wrong. -> Mitigation: centralize conversion helpers and test 00:xx, 12:xx, AM-to-PM, and 24-hour mode transitions.
- [Risk] A custom flyout could regress Popup/Flyout layering. -> Mitigation: reuse the existing Flyout lifecycle and add tests for open, Escape, light-dismiss, confirm, and cancel.

## Migration Plan

This is a new component with no existing public API to migrate. Existing date_time components remain unchanged.

## Open Questions

- Should a future change add min/max time constraints, or should applications compose validation externally?
- Should a future change add localized AM/PM designators and locale-default clock selection after the deterministic first pass lands?
