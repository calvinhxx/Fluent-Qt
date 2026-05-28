## Context

`src/view/date_time` exists but currently has no DatePicker implementation or test directory. The repository already has reusable Fluent infrastructure that should shape this component:

- `Popup` and `Flyout` provide top-level anchored transient surfaces with light-dismiss and Escape closure.
- `ComboBox` already demonstrates a Flyout-backed dropdown pattern, theme-aware item painting, and selected item synchronization.
- VisualCheck tests should use project Fluent components and `AnchorLayout`.

External behavior references:

- Windows UI Kit / Figma reference shows the compact entry surface as segmented month/day/year fields and the open picker as three vertical columns with a single accent selected row and confirm/cancel commands.
- WinUI Gallery `DatePickerPage.xaml` demonstrates a simple DatePicker sample and a `DatePicker` with `DayFormat="{}{day.integer} ({dayofweek.abbreviated})"` and `YearVisible="False"`. In this project, surrounding labels are composed by the application/test page rather than the DatePicker component.
- Microsoft DatePicker guidance distinguishes nullable `SelectedDate` from non-null `Date`, supports hiding month/day/year fields, supports per-field formatting, and uses min/max year constraints.

## Goals / Non-Goals

**Goals:**
- Add a reusable `view::date_time::DatePicker` Qt Widgets control with WinUI-style appearance and interaction.
- Support unset selected date, current date display, min/max date range, month/day/year visibility, and practical field formatting.
- Use an anchored Flyout picker that opens over surrounding UI without relayout, supports commit/cancel, and stays theme-aware.
- Add automated tests plus a VisualCheck demo that covers the WinUI Gallery examples and common edge cases.

**Non-Goals:**
- Do not implement `CalendarDatePicker` or a calendar grid. This change is the wheel/column-style `DatePicker`.
- Do not add locale/calendar-system infrastructure beyond English month names and documented format enum options in the first pass.
- Do not introduce external dependencies or replace the existing Popup/Flyout infrastructure.
- Do not make DatePicker editable as free text input.

## Decisions

1. `DatePicker` will be a `QWidget` component using composition plus focused custom painting.

   The public control will live in `namespace view::date_time` and inherit the project `view::basicinput::Button` so the closed entry behaves as a button-like control. Descriptive label text should be composed by the application or test page outside the control, command buttons inside the flyout should use `view::basicinput::Button`, and the component-specific segmented field should be painted by DatePicker itself so adjacent segment borders, focus, hover, pressed, and placeholder states stay pixel-consistent.

   Alternative considered: compose three public `ComboBox` instances. That would duplicate a lot of popup lifecycle and make a single shared selected-row flyout difficult. It would also commit per-field values immediately, which does not match the WinUI picker surface.

2. The selected date API will use invalid `QDate` as the nullable state.

   `selectedDate()` returns an invalid `QDate` when the picker is unset. `date()` remains valid and mirrors `selectedDate()` when selected; when unset it supplies an internal pending/default date for the flyout and range calculations. The entry surface shows placeholders when `selectedDate()` is invalid.

   Alternative considered: use `std::optional<QDate>` only. That is clean in C++ but awkward for Qt properties and tests. Invalid `QDate` is idiomatic enough for Qt property/state APIs.

3. Range will be expressed as full dates, not only years.

   Public `minimumDate` and `maximumDate` properties will define selectable bounds. WinUI exposes `MinYear` and `MaxYear`, but full-date bounds are more natural in Qt and still cover the WinUI Gallery example. Convenience behavior can initialize the range to current date minus/plus 100 years, matching the documented WinUI default range shape.

   Alternative considered: expose only minimum/maximum year integers. That would not handle scenarios like "arrival date starts today" without extra API.

4. Formatting will use small enums first, not arbitrary WinUI format strings.

   The first pass will expose format enums that cover the gallery visuals: full month name, abbreviated month name, numeric month, integer day, day with abbreviated weekday, and four-digit year. This avoids shipping a partial parser for WinUI `DateTimeFormatter` format strings while still making common DatePicker samples testable.

   Alternative considered: accept raw format strings. That creates ambiguous parser behavior and localization expectations that the current codebase does not already support.

5. The picker surface will be a private Flyout subclass with private column widgets.

   A `DatePickerFlyout` will anchor to the DatePicker, use non-modal non-dimming light-dismiss, and own private `PickerColumn` widgets for month/day/year. Each column will paint visible rows, disabled rows outside range, hover/focus, and the shared accent selected row. Confirm applies the pending date and closes; cancel or light-dismiss restores the previous selected date.

   Alternative considered: use `ListView` for each column. `ListView` is useful for normal lists, but the DatePicker needs wheel-like centered selection, a shared highlight band, constrained day counts, and compact column dividers. A private purpose-built column keeps that logic clearer.

6. Date validity is centralized in a small normalization helper.

   Date changes from API setters, flyout column navigation, month/year transitions, and range updates will pass through a single helper that clamps day-of-month, respects leap years, and clamps to the active range. This prevents subtle mismatch between entry display and flyout state.

## Risks / Trade-offs

- [Risk] Enum-based formatting is less flexible than WinUI format strings. -> Mitigation: document the supported formats and keep the implementation extensible so a later change can add formatter strings without changing the main state model.
- [Risk] Wheel-style column painting can drift visually from Figma. -> Mitigation: keep geometry constants local, add VisualCheck states for unset, selected, hidden-year, constrained range, dark theme, and compare manually before applying.
- [Risk] Range and leap-year transitions can produce invalid dates. -> Mitigation: centralize normalization and add tests for month-end, February 29, min/max clamps, and visible column changes.
- [Risk] A custom flyout could regress Popup/Flyout layering. -> Mitigation: reuse the existing Flyout lifecycle and add tests for open, Escape, light-dismiss, confirm, and cancel.
