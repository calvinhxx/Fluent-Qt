## 1. Regression Tests

- [x] 1.1 Review existing `CalendarView::wheelEvent` tests and identify which cases already cover threshold, momentum, cluster tail, and animation consumption.
- [x] 1.2 Extend CalendarView test helpers so synthetic `QWheelEvent`s can specify angle delta, pixel delta, scroll phase, and optional timing gaps.
- [x] 1.3 Add a failing test for a Windows-style NoPhaseDiscrete precision-touchpad fallback burst that must page only once at Day level.
- [x] 1.4 Add failing tests for NoPhaseDiscrete fallback bursts at Month and Year levels so a single gesture cannot skip multiple year pages.
- [x] 1.5 Add tests proving full mouse wheel notches remain responsive across fresh clusters.
- [x] 1.6 Add tests for sub-notch NoPhaseDiscrete deltas accumulating to one page and then suppressing same-cluster tail packets.
- [x] 1.7 Add tests for NoPhasePixel and phase-based touchpad gestures committing at most one page per active gesture.
- [x] 1.8 Add tests for direction-change behavior before commit and after a committed cluster during animation.

## 2. Wheel State Implementation

- [x] 2.1 Refactor CalendarView private wheel state into explicit gesture/cluster fields for accumulated delta, direction, input kind, last timestamp, and committed state.
- [x] 2.2 Keep `ScrollBegin`, `ScrollUpdate`, `ScrollEnd`, and `ScrollMomentum` scoped to one phase-based gesture.
- [x] 2.3 Keep NoPhaseDiscrete cluster lifetime based on `kWheelClusterGapMs` and reset on fresh clusters or intentional pre-commit direction changes.
- [x] 2.4 Treat NoPhasePixel as pixel-capable wheel input while preserving discrete CalendarView paging semantics.
- [x] 2.5 Ensure any active wheel gesture or cluster commits at most one page until a clear fresh gesture boundary occurs.
- [x] 2.6 Consume wheel input during month/page or content-level transitions without queueing another navigation.
- [x] 2.7 Preserve physical mouse wheel behavior for full notches and sub-notch accumulation.

## 3. Integration Safety

- [x] 3.1 Verify `visibleMonthChanged` emits exactly once per committed wheel page.
- [x] 3.2 Ensure stale wheel state is cleared when `setVisibleMonth()`, `setContentLevel()`, button navigation, keyboard navigation, or date range clamping changes page state.
- [x] 3.3 Confirm CalendarDatePicker still composes CalendarView and receives date activation normally.
- [x] 3.4 Keep date selection, hover, focus indicator, min/max date clamping, and first-day-of-week behavior unchanged.

## 4. Verification

- [x] 4.1 Run `test_calendar_view` with visual tests skipped.
- [x] 4.2 Run `test_calendar_date_picker` with visual tests skipped.
- [x] 4.3 Run or build affected date_time test targets to catch compile/link regressions.
- [x] 4.4 Run `openspec validate fix-calendar-view-wheel-input --strict`.
- [ ] 4.5 Manually verify CalendarView on Windows with a physical mouse wheel and precision touchpad when hardware is available.
- [ ] 4.6 Manually verify macOS or phase-based touchpad behavior when hardware is available.
