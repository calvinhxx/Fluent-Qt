## Context

`ListView` is a view-layer control: callers still provide models and delegates, while the component owns Fluent chrome such as container background, scroll bars, flow behavior, selection helpers, drag reorder, sections, hover state, and overscroll. The current VisualCheck uses `tests/views/collections/FluentListItemDelegate` to paint a simple selected accent that fades in on the selected row. That approach demonstrates the visual, but it keeps the actual selection indicator behavior outside `ListView` and cannot consistently support horizontal tab-like lists.

The requested behavior is about ListView selection feedback rather than item content. It should therefore live in `ListView` as component-owned indicator geometry and animation state. Delegates can keep drawing item backgrounds/text, while `ListView` paints the selected indicator overlay after item painting.

## Goals / Non-Goals

**Goals:**
- Add a component-owned selected indicator for `ListView`.
- Support vertical `TopToBottom` flow with a leading-side indicator and direction-aware upward/downward motion.
- Support horizontal `LeftToRight` flow with a bottom indicator and direction-aware left/right motion.
- Make pointer, keyboard, and programmatic selection changes use the same indicator transition path.
- Keep tests deterministic through geometry/progress helpers and keep VisualCheck useful for motion review.
- Preserve existing list model/delegate ownership boundaries.

**Non-Goals:**
- Do not make `ListView` own item text, icons, navigation semantics, or tab routing.
- Do not replace caller-provided delegates.
- Do not redesign drag reorder, overscroll bounce, scroll bars, sections, or selection modes.
- Do not require screenshot-only assertions for the animation contract.

## Decisions

1. **ListView owns indicator motion and paints the indicator as a viewport overlay.**
   - Add private selected-indicator state to `ListView`: previous selected row/index, current selected row/index, previous/target item rects, travel direction, progress, and one animation object.
   - Paint the indicator after `QListView::paintEvent(event)` and after item backgrounds/text, so it is not tied to a specific delegate implementation.
   - Update the test/demo `FluentListItemDelegate` to stop painting its own selected accent once `ListView` owns the overlay.
   - Alternative considered: keep all indicator drawing inside the delegate. That duplicates selection-motion logic per delegate and makes horizontal bottom indicators unreliable for callers that use a different delegate.

2. **Direction-aware motion uses edge latching rather than a plain linear slide.**
   - Vertical movement down: the lower edge leads first and the upper edge catches up, creating a downward stretch/compress feel.
   - Vertical movement up: the upper edge leads first and the lower edge catches up.
   - Horizontal movement right: the right edge leads first and the left edge catches up.
   - Horizontal movement left: the left edge leads first and the right edge catches up.
   - This produces the requested tactile difference without changing selection semantics.

3. **Flow determines indicator placement and axis.**
   - `TopToBottom`: indicator is a rounded vertical pill on the leading side of the selected item, using existing accent color and Fluent radii.
   - `LeftToRight`: indicator is a rounded horizontal pill on the bottom edge of the selected item, centered within the item and using existing accent color.
   - The implementation should derive geometry from `visualRect(index)` so scroll offsets, spacing, and item delegates remain respected.

4. **Selection changes are observed from the selection model.**
   - Connect to selection/current changes after model/selection-model changes and normalize the selected row using the existing `selectedIndex()` semantics.
   - Programmatic `setSelectedIndex`, pointer clicks, keyboard traversal, and direct selection-model updates all feed the same transition.
   - Initial selection, invalid selection, empty models, and selection clearing should snap or hide without stale animation.

5. **Expose only minimal additive testing/configuration surface.**
   - Add a read-only selected-indicator geometry helper, such as `selectedIndicatorRect()`, for automated tests.
   - Add animation enablement only if needed for deterministic tests and app-level motion preferences.
   - Avoid requiring callers to configure indicator behavior for the common case; `flow()` should choose the right orientation automatically.

6. **VisualCheck demonstrates both requested GIF-like effects.**
   - Extend `TestListView` VisualCheck with a vertical navigation-style list and a horizontal tab-style list.
   - Add small project `Button` controls that move selection forward/backward in each list so upward/downward and left/right indicator texture can be manually inspected.
   - Follow the repository Qt UT convention: `AnchorLayout` for the visual layout, `SKIP_VISUAL_TEST` guard, and shared Qt test startup.

## Risks / Trade-offs

- [Risk] Existing custom delegates may already paint a selected accent, causing a duplicate indicator. -> Mitigation: provide an opt-out property if needed and update repository test delegates to stop painting duplicate accents.
- [Risk] Overlay painting can be clipped or misplaced when rows are scrolled out of view. -> Mitigation: compute from `visualRect(index)`, hide when the target rect is invalid, and update on scroll/resize/model changes.
- [Risk] Drag reorder and selection animation can compete visually. -> Mitigation: suppress or snap selected-indicator motion during active drag and repaint after reorder completes.
- [Risk] Direction-aware stretch can look too elastic. -> Mitigation: keep duration short, use existing Fluent `fast`/`normal` durations and decelerate easing, and constrain maximum length to the previous/target item geometry.
- [Risk] Horizontal wrapping creates multi-row geometry that is not tab-like. -> Mitigation: support `LeftToRight` non-wrapping as the primary horizontal tab scenario and fall back to target geometry when wrapping makes the direction ambiguous.
