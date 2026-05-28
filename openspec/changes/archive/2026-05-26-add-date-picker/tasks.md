## 1. Component Structure

- [x] 1.1 Add `src/view/date_time/DatePicker.h` and `src/view/date_time/DatePicker.cpp` with `view::date_time::DatePicker` inheriting the project `Button` control.
- [x] 1.2 Define public Qt properties and signals for `date`, `selectedDate`, `minimumDate`, `maximumDate`, field visibility, and field format enums.
- [x] 1.3 Implement invalid `QDate` selected-date handling so unset state shows month/day/year placeholders while keeping a valid internal date for popup calculations.
- [x] 1.4 Implement range normalization helpers for min/max bounds, month-end clamping, leap-year transitions, and selected-date clamping.

## 2. Entry Surface

- [x] 2.1 Keep descriptive label text in the application or test page, outside the DatePicker component.
- [x] 2.2 Implement the segmented month/day/year entry surface with Fluent rest, hover, pressed, focused, disabled, and open visuals.
- [x] 2.3 Implement field visibility and supported formatting options, including full month name and day with abbreviated weekday.
- [x] 2.4 Add mouse and keyboard handling so clicking a visible segment, Space, or Enter opens the picker when enabled.

## 3. Flyout Picker

- [x] 3.1 Add a private Flyout-backed picker surface anchored to DatePicker, using non-modal, non-dimming, light-dismiss behavior.
- [x] 3.2 Add private picker column widgets for visible month/day/year fields with centered selected row, column dividers, disabled values, and top/bottom navigation affordances.
- [x] 3.3 Implement pending-date synchronization between visible columns and central normalization logic.
- [x] 3.4 Add confirm and cancel commands using project `Button` components; confirm commits `selectedDate`, cancel and light-dismiss preserve the previous selection.
- [x] 3.5 Add Escape cancel, Enter confirm, arrow/wheel column navigation, and focus handling for the open flyout.

## 4. Theming and Accessibility

- [x] 4.1 Apply Fluent theme tokens to entry background, stroke, placeholder text, selected row, disabled values, dividers, focus visuals, and flyout card.
- [x] 4.2 Ensure `onThemeUpdated()` refreshes DatePicker, flyout, picker columns, and command buttons.
- [x] 4.3 Leave accessible names/descriptions to the application layer.

## 5. Tests and VisualCheck

- [x] 5.1 Create `tests/views/date_time/CMakeLists.txt` with `add_qt_test_module(test_date_picker TestDatePicker.cpp)` and register the directory from `tests/views/CMakeLists.txt`.
- [x] 5.2 Add unit tests for default unset state, selected-date changes, clearing, min/max clamps, invalid range normalization, month-end clamping, and leap-year transitions.
- [x] 5.3 Add unit tests for field visibility, last-visible-field protection, field formatting, disabled-open behavior, and popup lifecycle.
- [x] 5.4 Add interaction tests for confirm, cancel, Escape, Enter, outside light-dismiss, keyboard open, and wheel/arrow column navigation.
- [x] 5.5 Add `TestDatePicker.VisualCheck` using `AnchorLayout`, project Fluent components, and `SKIP_VISUAL_TEST` guard. Cover external labels, unset placeholders, formatted day with hidden year, constrained range, disabled state, and dark theme.

## 6. Validation

- [x] 6.1 Build the focused test target with `cmake --build --preset vcpkg-osx --target test_date_picker`.
- [x] 6.2 Run the focused test binary with `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/date_time/test_date_picker`.
- [x] 6.3 Run `ctest --preset vcpkg-osx -R "DatePicker|Flyout|ComboBox" --verbose` when practical to catch popup and dropdown regressions.
- [x] 6.4 Run `openspec validate add-date-picker --strict` and confirm `openspec status --change add-date-picker` is apply-ready.
