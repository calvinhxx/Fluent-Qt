## 1. Component Structure

- [x] 1.1 Add `src/view/date_time/CalendarDatePicker.h` and `.cpp` with `view::date_time::CalendarDatePicker : public QWidget, public FluentElement, public QMLPlus`.
- [x] 1.2 Define the public Qt API: placeholderText, date, minDate, maxDate, displayFormat, firstDayOfWeek, calendarOpen, open/close/clear methods, and change signals.
- [x] 1.3 Add private `CalendarDatePickerPopup` implementation and compose the public `CalendarView` so the picker API stays focused on entry/flyout behavior.
- [x] 1.4 Ensure invalid `QDate` is the documented empty state and duplicate setters do not emit duplicate signals.

## 2. Entry Surface

- [x] 2.1 Implement the closed entry rendering with placeholder/selected text, calendar glyph, focus, hover, pressed, open, disabled, and theme states.
- [x] 2.2 Keep descriptive label text in the application or test page, outside the CalendarDatePicker component.
- [x] 2.3 Use project typography, spacing, accent, border, and disabled colors instead of raw platform style defaults.
- [x] 2.4 Route mouse and keyboard activation from the entry surface to opening the calendar flyout.

## 3. Calendar Flyout And Month View

- [x] 3.1 Implement a non-modal `Flyout`-based calendar popup anchored to the CalendarDatePicker with light-dismiss and Escape close behavior.
- [x] 3.2 Implement month title, previous/next navigation, weekday headers, and a stable 6x7 day grid using `QLocale` and configurable first day of week.
- [x] 3.3 Render selected date, today, adjacent-month dates, hover/pressed date cells, and disabled dates with Fluent-compatible styling.
- [x] 3.4 Reopen the flyout on the selected month when a valid date exists, otherwise on the current month.
- [x] 3.5 Keep the flyout card within the top-level window using existing Flyout placement/clamping behavior.

## 4. Date Semantics And Interaction

- [x] 4.1 Implement date formatting for the closed entry, including deterministic `displayFormat` support for tests and applications.
- [x] 4.2 Implement min/max date constraints, including clamping programmatic out-of-range selected dates and disabling out-of-range cells.
- [x] 4.3 Implement date cell activation so selecting an enabled day updates `date`, emits `dateChanged` once, updates the entry text, and closes the flyout.
- [x] 4.4 Support selecting enabled adjacent-month cells and update the visible month consistently with the selected date.
- [x] 4.5 Implement basic keyboard navigation and Enter/Space activation while the month view has focus.
- [x] 4.6 Refresh entry and flyout visuals on Fluent theme updates.

## 5. Tests And Documentation

- [x] 5.1 Add `tests/views/date_time/CMakeLists.txt` and register it from `tests/views/CMakeLists.txt`.
- [x] 5.2 Add `tests/views/date_time/TestCalendarDatePicker.cpp` following `.codex/skills/qt-ut-conventions/SKILL.md`.
- [x] 5.3 Cover default properties, placeholder, date set/clear, duplicate signal suppression, display format, constraints, disabled state, flyout open/close, month navigation, date selection, and keyboard activation.
- [x] 5.4 Add a VisualCheck using `AnchorLayout` and project Fluent components showing empty, selected, constrained, disabled, light, and dark examples, guarded by `SKIP_VISUAL_TEST`.
- [x] 5.5 Update project documentation or component index with the new CalendarDatePicker usage snippet.
- [x] 5.6 Build the focused test target with `cmake --build --preset vcpkg-osx --target test_calendar_date_picker`.
- [x] 5.7 Run `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/date_time/test_calendar_date_picker`.
- [x] 5.8 Run a focused `ctest --preset vcpkg-osx -R "CalendarDatePicker|Flyout|ComboBox" --verbose` sweep where practical.
- [ ] 5.9 Optionally run the manual CalendarDatePicker VisualCheck without `SKIP_VISUAL_TEST=1` and compare against the provided Windows UI kit screenshot.

## 6. CalendarView Split

- [x] 6.1 Add public `src/view/date_time/CalendarView.h` and `.cpp` with `view::date_time::CalendarView : public QWidget, public FluentElement, public QMLPlus`.
- [x] 6.2 Move month title, previous/next navigation, weekday headers, 6x7 day grid, date bounds, and date activation behavior into `CalendarView`.
- [x] 6.3 Refactor `CalendarDatePicker` so it owns the compact entry and Flyout lifecycle while composing `CalendarView` for the calendar surface.
- [x] 6.4 Add `tests/views/date_time/TestCalendarView.cpp` with standalone behavior coverage and a guarded VisualCheck.
- [x] 6.5 Update documentation and OpenSpec artifacts to describe the picker/view split.
- [x] 6.6 Build `test_calendar_view` and `test_calendar_date_picker`.
- [x] 6.7 Run `SKIP_VISUAL_TEST=1` for both date-time test binaries.
- [x] 6.8 Run a focused `ctest --preset vcpkg-osx -R "CalendarView|CalendarDatePicker|Flyout|ComboBox" --verbose` sweep where practical.
