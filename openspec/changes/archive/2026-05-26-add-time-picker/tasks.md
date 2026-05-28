## 1. Component Structure

- [x] 1.1 Add `src/view/date_time/TimePicker.h` and `src/view/date_time/TimePicker.cpp` with `view::date_time::TimePicker` inheriting `QWidget, public FluentElement, public QMLPlus`.
- [x] 1.2 Define public Qt properties, enums, signals, and slots for `time`, `selectedTime`, `minuteIncrement`, `clockIdentifier`, and `dropDownOpen`.
- [x] 1.3 Implement invalid `QTime` selected-time handling so unset state shows placeholders while keeping a valid internal time for popup calculations.
- [x] 1.4 Implement centralized normalization helpers for minute increment clamping, minute snapping, and 12-hour/24-hour conversion around midnight/noon.

## 2. Entry Surface

- [x] 2.1 Keep descriptive label text in the application or test page, outside the TimePicker component.
- [x] 2.2 Implement the segmented hour/minute/period entry surface with Fluent rest, hover, pressed, focused, disabled, and open visuals.
- [x] 2.3 Implement twelve-hour display with AM/PM period and twenty-four-hour display without the period segment.
- [x] 2.4 Add mouse and keyboard handling so clicking the entry surface, Space, or Enter opens the picker when enabled.

## 3. Flyout Picker

- [x] 3.1 Add a private Flyout-backed picker surface anchored to TimePicker, using non-modal, non-dimming, light-dismiss behavior.
- [x] 3.2 Add private picker column widgets for hour, minute, and optional period fields with centered selected row, column dividers, and top/bottom navigation affordances.
- [x] 3.3 Implement pending-time synchronization between visible columns and the central normalization helpers.
- [x] 3.4 Add confirm and cancel commands using project `Button` components; confirm commits `selectedTime`, cancel and light-dismiss preserve the previous selection.
- [x] 3.5 Add Escape cancel, Enter confirm, arrow/wheel column navigation, and focus handling for the open flyout.

## 4. Theming and Accessibility

- [x] 4.1 Apply Fluent theme tokens to entry background, stroke, placeholder text, selected row, disabled values, dividers, focus visuals, and flyout card.
- [x] 4.2 Ensure `onThemeUpdated()` refreshes TimePicker, flyout, picker columns, and command buttons.
- [x] 4.3 Leave accessible names/descriptions to the application layer.

## 5. Tests and VisualCheck

- [x] 5.1 Register `add_qt_test_module(test_time_picker TestTimePicker.cpp)` in `tests/views/date_time/CMakeLists.txt`.
- [x] 5.2 Add unit tests for default unset state, selected-time changes, clearing, duplicate signal suppression, minute increment clamping, and minute snapping.
- [x] 5.3 Add unit tests for twelve-hour/twenty-four-hour display, midnight/noon conversion, period changes, disabled-open behavior, and popup lifecycle.
- [x] 5.4 Add interaction tests for confirm, cancel, Escape, Enter, outside light-dismiss, keyboard open, and wheel/arrow column navigation.
- [x] 5.5 Add `TestTimePicker.VisualCheck` using `AnchorLayout`, project Fluent components, and `SKIP_VISUAL_TEST` guard. Cover simple picker, arrival-time 15-minute increment, 24-hour current time, unset placeholders, disabled state, and theme switching.

## 6. Validation

- [x] 6.1 Build the focused test target with `cmake --build --preset vcpkg-osx --target test_time_picker`.
- [x] 6.2 Run the focused test binary with `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/date_time/test_time_picker`.
- [x] 6.3 Run `ctest --preset vcpkg-osx -R "TimePicker|DatePicker|CalendarDatePicker|Flyout" --output-on-failure` when practical to catch popup and date_time regressions.
- [x] 6.4 Run `openspec validate add-time-picker --strict` and confirm `openspec status --change add-time-picker` is apply-ready.
