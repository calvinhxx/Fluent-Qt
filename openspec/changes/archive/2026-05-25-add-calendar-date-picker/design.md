## Context

`src/view/date_time` exists but currently has no concrete component. The requested control maps to WinUI `CalendarDatePicker`: a compact date entry point with placeholder text, opening an overlaid calendar surface for single-date selection. Descriptive label text is composed by the application or test page outside the control. The same calendar surface is also useful as a standalone WinUI-style `CalendarView`, so the implementation should split the picker shell from the reusable month view rather than hiding the calendar grid as a picker-only detail.

Reference inputs:
- Microsoft Learn describes CalendarDatePicker as a drop-down control optimized for picking a single date from a calendar view; the expanded calendar overlays other UI instead of pushing layout.
- WinUI Gallery uses `PlaceholderText`, `Date`, and `DateChanged` as the important first-pass picker API surface.
- The Windows UI kit screenshot shows a compact entry button with a calendar glyph and a floating month card with weekday headers, muted adjacent-month days, and an accent selected-day circle; surrounding labels belong to the application/test layout.
- Direct Figma MCP inspection timed out in this environment. The visual target for this proposal is therefore the provided screenshot plus the public WinUI references; implementation should still use VisualCheck for manual comparison.

Existing project constraints:
- Visible components should reuse project Fluent infrastructure (`FluentElement`, `QMLPlus`, typography, spacing, and theme colors).
- Popover-like UI should reuse `view::dialogs_flyouts::Flyout` instead of inventing another popup host.
- VisualCheck tests should follow `.codex/skills/qt-ut-conventions/SKILL.md`: use `AnchorLayout`, project Fluent components, and `SKIP_VISUAL_TEST`.

## Goals / Non-Goals

**Goals:**
- Add `view::date_time::CalendarDatePicker` as a reusable QWidget-based component.
- Add `view::date_time::CalendarView` as a reusable QWidget-based month calendar view.
- Support nullable single-date selection through invalid `QDate`, with placeholder text when no date is selected.
- Open a Fluent calendar flyout anchored to the entry surface and render that flyout with `CalendarView`.
- Support month navigation, first-day-of-week configuration, min/max date constraints, today indication, selected-date indication, and disabled/hover/pressed/focused states.
- Keep public API small and Qt-friendly so later DatePicker/TimePicker work can reuse the same date primitives.
- Add focused automated tests and an interactive VisualCheck.

**Non-Goals:**
- Do not implement date ranges, multiple selection, time selection, agenda content, holiday markers, or localization beyond `QLocale` day/month names.
- Do not add a new dependency.
- Do not replace existing textfield/basicinput components.

## Decisions

### Use a custom QWidget control instead of QDateEdit/QCalendarWidget

`CalendarDatePicker` should derive from `QWidget, public FluentElement, public QMLPlus` and own its entry/flyout composition. `QDateEdit` and `QCalendarWidget` provide native behavior, but they are hard to style to the Windows UI kit target and would leak native subcontrols into the component API. A custom surface gives the project control over Fluent states, typography, selected-day rendering, and flyout placement.

Alternative considered: derive from `QDateEdit`. This would provide parsing and calendar popup for free, but it would not match the requested WinUI visual model and would be difficult to integrate with project `Flyout`.

### Split picker shell and public CalendarView responsibilities

`CalendarDatePicker` should own:
- placeholder, selected date, min/max dates, display format, first day of week, and open/close methods;
- closed-entry input state such as hover, press, focus, disabled, and open;
- `CalendarDatePickerPopup`, deriving from `Flyout`, for anchored overlay behavior and light-dismiss.

`CalendarView` should own:
- visible month, selected date, min/max dates, first day of week, and date activation signals;
- the month header, weekday row, stable 6x7 day grid, selected/today/disabled states, and mouse/keyboard day activation.

This keeps `CalendarDatePicker` focused on the entry/flyout composition while making the reusable calendar view available to tests and later date/time controls.

### Represent "no date" with invalid QDate

Qt has no nullable `QDate` property type that is as convenient as WinUI's nullable `DateTimeOffset?`. The control should use invalid `QDate()` as the null state. `date()` returns invalid when empty, `setDate(QDate())` clears the value, and emitted `dateChanged(QDate())` represents clearing.

### Reuse Flyout for overlay lifecycle

The calendar card should be shown by a `Flyout` anchored to the entry surface. It should use bottom/auto placement, light-dismiss, Esc close, and no modal dimming. This matches WinUI's overlay behavior and avoids duplicating popup positioning code already validated by `Flyout` and `ComboBox`.

### Use QLocale for calendar text, not hard-coded English

Month title and weekday labels should be generated by `QLocale`, with a default `firstDayOfWeek` based on `QLocale::firstDayOfWeek()`. Tests can force Sunday/Monday starts to avoid locale-dependent assertions. This keeps the first pass globally usable without taking on full calendar-system customization.

### Keep formatting explicit

Default display text should use a stable date format such as `QLocale::ShortFormat`, but the component should expose `displayFormat` for applications/tests that need deterministic text like `yyyy-MM-dd`. Formatting only affects the closed entry text; internal calendar navigation remains based on `QDate`.

## Risks / Trade-offs

- [Custom calendar surface has more code than QCalendarWidget] -> Keep it single-month-only, tested as `CalendarView`, and composed by CalendarDatePicker instead of duplicating date-grid code.
- [Visual fidelity may drift from the Figma kit because node metadata was unavailable] -> Base geometry and colors on existing Fluent tokens, add VisualCheck cases, and compare against the provided screenshot during implementation.
- [Locale differences can make tests flaky] -> Unit tests should set deterministic display format and first day of week when asserting text/grid order.
- [Date constraints can create invalid visible months] -> Clamp displayed month to the nearest selectable month when min/max changes and make disabled cells non-activating.
- [Flyout focus/light-dismiss behavior may interact with tests] -> Reuse existing `Flyout` close policies and test with animation disabled where possible.
