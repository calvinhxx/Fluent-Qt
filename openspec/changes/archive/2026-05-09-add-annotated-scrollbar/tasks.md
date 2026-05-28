## 1. Component Skeleton

- [x] 1.1 Add `src/view/scrolling/AnnotatedScrollBar.h` and `.cpp` with `QWidget + FluentElement + view::QMLPlus` inheritance, default construction, size hints, theme hook, and source registration through existing CMake glob.
- [x] 1.2 Define `AnnotatedScrollBarLabel` value type and public API for range/value/pageStep, label collection, detail label provider, and scroll/label/detail signals.
- [x] 1.3 Implement setter guards, value clamping, signal emission, updateGeometry/update scheduling, and single active `ScrollView` connection bookkeeping.

## 2. Layout And Rendering

- [x] 2.1 Implement vertical track geometry, top/bottom caret geometry, label column geometry, indicator geometry, and offset-to-y/y-to-offset mapping helpers.
- [x] 2.2 Implement label sorting, stable equal-offset ordering, visible-label cache, and collision hiding that refreshes on resize, range, font, label, and theme changes.
- [x] 2.3 Implement Light/Dark/disabled painting for labels, carets, indicator, hover/pressed states, and default Figma-sized `sizeHint()`.
- [x] 2.4 Implement ToolTip-backed detail label display with provider/fallback content, position clamping, and hide-on-leave/disabled behavior.
- [x] 2.5 Expose configurable layout metrics for preferred/minimum size, padding, caret size, label column, label spacing, indicator size, and fallback line step.

## 3. Interaction And ScrollView Integration

- [x] 3.1 Handle mouse move/leave/press/release/drag to update hover/pressed state, detail content, value, `scrollRequested`, and `labelActivated`.
- [x] 3.2 Handle wheel and keyboard input for line/page/end-point scrolling with focus policy and disabled-state guards.
- [x] 3.3 Implement `connectToScrollView(ScrollView*)` and `disconnectScrollView()` to sync vertical range/value/pageStep and drive `ScrollView::scrollTo()` while preserving horizontal offset.
- [x] 3.4 Add a small example helper or internal utility for rebuilding labels from content offsets, if needed by tests/VisualCheck.

## 4. Tests And VisualCheck

- [x] 4.1 Add `tests/views/scrolling/TestAnnotatedScrollBar.cpp` covering default API, range/value clamp, duplicate setter guards, label collection, label sorting, and collision hiding.
- [x] 4.2 Add interaction tests for detail provider/fallback, hover leave, click label/track, drag indicator, wheel/key input, disabled state, and emitted signals.
- [x] 4.3 Add `ScrollView` integration tests covering initial sync, external ScrollView scroll sync, user input driving ScrollView, disconnect behavior, and preserving horizontal offset.
- [x] 4.4 Add render/theme tests for Light/Dark/disabled key pixels where practical without making tests brittle.
- [x] 4.5 Register `test_annotated_scrollbar` in `tests/views/scrolling/CMakeLists.txt` and add a VisualCheck with Figma-style 2015-2023 labels plus WinUI Gallery-style `ScrollView` linked example.
- [x] 4.6 Rebuild VisualCheck with AnchorLayout and a height slider so label density changes can be reviewed interactively without clipping the linked bar.

## 5. Verification

- [x] 5.1 Run the new `test_annotated_scrollbar` with `SKIP_VISUAL_TEST=1`.
- [x] 5.2 Run existing scrolling tests (`test_scrollbar`, `test_scroll_view`) to catch regressions.
- [ ] 5.3 Manually run `test_annotated_scrollbar --gtest_filter="*VisualCheck*"` and verify hover detail, label hiding, theme switching, and linked ScrollView scrolling.
