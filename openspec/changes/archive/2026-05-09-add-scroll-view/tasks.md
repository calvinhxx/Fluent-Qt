## 1. Public Component API

- [x] 1.1 Create `src/view/scrolling/ScrollView.h` with `QScrollArea` + `FluentElement` + `view::QMLPlus` inheritance and public enums for scroll modes and scrollbar visibility.
- [x] 1.2 Expose `Q_PROPERTY` entries for horizontal/vertical scroll mode and horizontal/vertical scrollbar visibility.
- [x] 1.3 Implement content-facing convenience API for `horizontalOffset()`, `verticalOffset()`, `scrollableWidth()`, `scrollableHeight()`, `scrollTo(...)`, and `scrollBy(...)`.
- [x] 1.4 Implement `onThemeUpdated()` so viewport/background visuals respond to Fluent Light/Dark theme changes without QSS-based component styling.

## 2. ScrollBar Integration

- [x] 2.1 Create `src/view/scrolling/ScrollView.cpp` and initialize custom `ScrollBar` instances for both horizontal and vertical directions.
- [x] 2.2 Map ScrollView scroll mode and visibility enums to `QAbstractScrollArea` policies plus custom scrollbar enabled/visible state.
- [x] 2.3 Ensure hidden-scrollbar-but-scroll-enabled behavior is handled separately from fully disabled scrolling.
- [x] 2.4 Ensure replacing scrollbars does not regress standalone `ScrollBar` behavior or existing `test_scrollbar` expectations.

## 3. Programmatic Scrolling

- [x] 3.1 Implement clamped non-animated `scrollTo` and `scrollBy` using horizontal and vertical scrollbar values.
- [x] 3.2 Implement animated scroll paths using `QPropertyAnimation` and Fluent animation tokens.
- [x] 3.3 Emit or reuse suitable scroll state notifications so tests can observe offset changes after programmatic scroll calls.

## 4. Tests and VisualCheck

- [x] 4.1 Create `tests/views/scrolling/TestScrollView.cpp` with automated tests for default custom scrollbar types, content scrolling ranges, and offset query APIs.
- [x] 4.2 Add tests for horizontal/vertical scroll mode and scrollbar visibility policy mapping, including hidden-scrollbar-but-scroll-enabled behavior.
- [x] 4.3 Add tests for clamped `scrollTo` and `scrollBy`, with animated behavior either verified deterministically or scoped to VisualCheck if timing is flaky.
- [x] 4.4 Add a VisualCheck guarded by `SKIP_VISUAL_TEST` showing vertical, horizontal, and bidirectional ScrollView examples plus theme switching.
- [x] 4.5 Register `test_scroll_view` in `tests/views/scrolling/CMakeLists.txt` using `add_qt_test_module`.

## 5. Validation

- [x] 5.1 Build the affected targets with `cmake --build build --target test_scrollbar test_scroll_view`.
- [x] 5.2 Run `SKIP_VISUAL_TEST=1 ./build/tests/views/scrolling/test_scroll_view` and `SKIP_VISUAL_TEST=1 ./build/tests/views/scrolling/test_scrollbar`.
- [x] 5.3 Manually run `./build/tests/views/scrolling/test_scroll_view --gtest_filter="*VisualCheck*"` to verify Fluent scrollbar visuals, scrolling interaction, and theme switching.
- [x] 5.4 Run `openspec validate add-scroll-view` and confirm the change is valid before implementation starts.

## 6. Zoom and Visual Polish

- [x] 6.1 Replace the default bidirectional scrollbar corner artifact with a transparent non-painting corner widget.
- [x] 6.2 Add `zoomMode`, `zoomFactor`, `minZoomFactor`, and `maxZoomFactor` API plus programmatic `zoomTo`, `zoomBy`, and `resetZoom` methods.
- [x] 6.3 Handle touchpad native pinch gestures, smart zoom, high-level pinch gestures, and Ctrl+wheel zoom when zoom mode is enabled.
- [x] 6.4 Add automated tests for zoom clamping, content resize/range updates, and disabled/enabled Ctrl+wheel behavior.
- [x] 6.5 Update VisualCheck to expose zoom controls and zoom-aware demo content.
- [x] 6.6 Add a `ScrollViewZoomAware` content contract and route gesture zoom events from ScrollView, viewport, and content widgets.
- [x] 6.7 Stabilize native trackpad pinch zoom so terminal high-level pinch events do not revert the native zoom delta.
- [x] 6.8 Suppress wheel-scroll tail events and stray SmartZoom events during native trackpad pinch zoom.