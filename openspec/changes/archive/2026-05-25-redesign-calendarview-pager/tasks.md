## 1. Baseline And Test Coverage

- [x] 1.1 Review current `CalendarView` wheel, page transition, level transition, and public property tests to identify assertions that encode obsolete visible preview/rebound behavior.
- [x] 1.2 Add or update `TestCalendarView` cases for sub-threshold wheel input remaining visually idle and not committing a page.
- [x] 1.3 Add or update `TestCalendarView` cases for threshold/discrete wheel input committing exactly one Day, Month, and Year page.
- [x] 1.4 Add or update `TestCalendarView` cases for NoScrollPhase cluster tail suppression, momentum suppression, direction-change reset, and animation-time wheel consumption.
- [x] 1.5 Add or update `TestCalendarView` cases for Day/Month/Year drill-up and drill-down transitions staying separate from page navigation state.

## 2. Pager Input Model

- [x] 2.1 Introduce private CalendarView wheel input classification for phase-based, no-phase pixel, and no-phase discrete events.
- [x] 2.2 Replace visible wheel preview state with private invisible accumulation state and cluster timing state.
- [x] 2.3 Implement one-page-per-cluster protection for NoScrollPhase wheel input while navigation animation is running.
- [x] 2.4 Handle ScrollBegin, ScrollUpdate, ScrollEnd, ScrollMomentum, and zero-delta wheel events without leaking stale gesture state.
- [x] 2.5 Ensure wheel events outside the CalendarView content area continue to follow existing QWidget/base behavior.

## 3. Page Key Decoupling

- [x] 3.1 Add private page key helpers for Day month pages, Month year pages, and Year 12-year range pages.
- [x] 3.2 Move page title, next/previous page math, and range clamping into level-specific helper paths.
- [x] 3.3 Move Day, Month, and Year hit testing and activation mapping behind level-specific helper paths.
- [x] 3.4 Preserve the existing `visibleMonth()` and `setVisibleMonth()` public contract while mapping it onto the decoupled internal page state.
- [x] 3.5 Ensure min/max date constraints clamp all three page levels and prevent activation of out-of-range cells.

## 4. Transition Separation

- [x] 4.1 Split page navigation transition state from Day/Month/Year level zoom transition state.
- [x] 4.2 Make right-side previous/next buttons use the same committed page navigation path as wheel paging, without wheel gesture state.
- [x] 4.3 Make title button drill-up use only level zoom-out state and clear incompatible page transition state explicitly.
- [x] 4.4 Make year/month cell drill-down use only level zoom-in state and clear incompatible page transition state explicitly.
- [x] 4.5 Remove obsolete wheel preview/rebound properties and test expectations once the replacement state is covered.

## 5. CalendarDatePicker Integration

- [x] 5.1 Verify `CalendarDatePickerPopup` still composes `CalendarView`, applies date range and first day of week, and receives `dateActivated` without API changes.
- [x] 5.2 Update `TestCalendarDatePicker` only where expected CalendarView page behavior changes, preserving popup open/close and date selection behavior.
- [x] 5.3 Review DatePicker column flyout wheel behavior for naming or shared helper opportunities, but do not migrate DatePicker to CalendarView in this change.

## 6. VisualCheck And Documentation

- [x] 6.1 Update CalendarView VisualCheck to make wheel navigation, right-side button navigation, and title/cell level transitions easy to inspect.
- [x] 6.2 Update CalendarDatePicker VisualCheck if needed to expose CalendarView wheel behavior inside the popup.
- [x] 6.3 Update component documentation or notes to describe CalendarView discrete pager behavior and the absence of wheel rebound.

## 7. Validation

- [x] 7.1 Build the focused CalendarView test target with `cmake --build build/vcpkg-osx --target test_calendar_view`.
- [x] 7.2 Run `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/date_time/test_calendar_view`.
- [x] 7.3 Build the focused CalendarDatePicker test target with `cmake --build build/vcpkg-osx --target test_calendar_date_picker`.
- [x] 7.4 Run `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/date_time/test_calendar_date_picker`.
- [x] 7.5 Run focused DatePicker tests if DatePicker-adjacent code or shared wheel helpers are touched.
- [x] 7.6 Run `openspec validate redesign-calendarview-pager --strict` or the repository's equivalent OpenSpec validation command.
