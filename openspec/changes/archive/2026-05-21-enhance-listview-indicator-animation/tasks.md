## 1. Indicator State And API

- [x] 1.1 Add minimal selected-indicator state to `ListView`, including previous/current persistent indexes, previous/target item rects, travel direction, normalized progress, and a single animation object.
- [x] 1.2 Add a deterministic selected indicator geometry query, such as `selectedIndicatorRect()`, for tests.
- [x] 1.3 Add an opt-out or animation-control property only if needed to avoid duplicate custom delegate indicators or to make tests deterministic.
- [x] 1.4 Initialize, stop, and clean up selected-indicator animation state safely in the constructor/destructor and on model/selection-model changes.

## 2. Selection Change Tracking

- [x] 2.1 Observe selection/current changes from `ListView`'s selection model so pointer, keyboard, direct selection-model updates, and `setSelectedIndex()` feed the same indicator path.
- [x] 2.2 Compute selected row/index normalization from existing `selectedIndex()` semantics and ignore invalid, empty-model, or out-of-range selections.
- [x] 2.3 Track previous and target item geometry using `visualRect(index)` and reset stale state when either geometry is invalid or hidden.
- [x] 2.4 Recompute indicator target geometry on resize, scroll, flow change, model reset, and theme refresh without mutating selection state.
- [x] 2.5 Suppress or snap selected-indicator motion during drag reorder, then settle to the current selected item after reorder completes.

## 3. Indicator Rendering And Motion

- [x] 3.1 Move selected indicator rendering into `ListView::paintEvent` as a viewport overlay after `QListView::paintEvent(event)`.
- [x] 3.2 Render `TopToBottom` selected indicators as rounded vertical accent pills on the leading side of the selected item.
- [x] 3.3 Render `LeftToRight` selected indicators as rounded horizontal accent pills along the bottom edge of the selected item.
- [x] 3.4 Implement vertical downward motion where the lower edge leads before the indicator settles into target geometry.
- [x] 3.5 Implement vertical upward motion where the upper edge leads before the indicator settles into target geometry.
- [x] 3.6 Implement horizontal rightward motion where the right edge leads before the indicator settles into target geometry.
- [x] 3.7 Implement horizontal leftward motion where the left edge leads before the indicator settles into target geometry.
- [x] 3.8 Use existing Fluent animation durations/easing and `themeColors().accentDefault`; repaint without changing selection on theme updates.
- [x] 3.9 Update `tests/views/collections/FluentListItemDelegate` so the demo/test delegate no longer paints a duplicate selected accent.

## 4. Automated Tests

- [x] 4.1 Add unit coverage for vertical selected indicator placement in `TopToBottom` flow.
- [x] 4.2 Add unit coverage for horizontal bottom selected indicator placement in `LeftToRight` flow.
- [x] 4.3 Add unit coverage for downward vs. upward vertical transition geometry at deterministic progress values.
- [x] 4.4 Add unit coverage for rightward vs. leftward horizontal transition geometry at deterministic progress values.
- [x] 4.5 Add unit coverage proving pointer, programmatic, keyboard/current-index, and direct selection-model changes update indicator state.
- [x] 4.6 Add unit coverage for first selection, clearing selection, empty model, resize, scroll, flow change, theme refresh, and drag reorder stability.

## 5. VisualCheck And Documentation

- [x] 5.1 Extend `TestListView.VisualCheck` using repository Qt UT conventions: keep the `SKIP_VISUAL_TEST` guard, use project Fluent controls, and preserve `AnchorLayout` for the main visual arrangement.
- [x] 5.2 Add a vertical navigation-style demo section with controls to force upward and downward indicator transitions.
- [x] 5.3 Add a horizontal tab-style demo section with controls to force left-to-right and right-to-left bottom-indicator transitions.
- [x] 5.4 Document any new public query/configuration helpers in code comments or README only if they become part of the public surface.

## 6. Validation

- [x] 6.1 Run `openspec validate enhance-listview-indicator-animation --strict`.
- [x] 6.2 Build the focused test target with `cmake --build build/vcpkg-osx --target test_list_view` or the active equivalent preset.
- [x] 6.3 Run `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/collections/test_list_view`.
- [x] 6.4 Run a focused adjacent collection/navigation sweep if implementation touches shared delegates, test helpers, or flow behavior.
- [x] 6.5 Manually run `TestListView.VisualCheck` and inspect both GIF-like effects: vertical up/down and horizontal left/right.
