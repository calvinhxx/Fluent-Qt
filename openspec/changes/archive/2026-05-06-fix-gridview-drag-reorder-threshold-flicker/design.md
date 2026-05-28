## Context

`GridView` implements custom drag reorder instead of using Qt's built-in drag/drop. During a drag, `mouseMoveEvent()` calls `dropIndicatorIndex(event->pos())`; when the returned slot differs from `m_dropTargetIndex`, `updateDragDisplacement()` stops existing displacement animations and creates new ones for affected items.

The current `dropIndicatorIndex()` chooses the nearest left/right insertion edge among non-source items. That works for clear movement, but near the midpoint between adjacent insertion candidates, tiny cursor movement or animation offset changes can alternate the chosen slot. Each alternation restarts item displacement animations, producing visible flicker.

## Goals / Non-Goals

**Goals:**

- Stabilize `GridView` drag reorder target selection around critical insertion boundaries.
- Keep drag reorder responsive once the cursor clearly moves into a neighboring slot.
- Avoid restarting displacement animations when the logical target is unchanged or only jittering inside a deadband.
- Preserve multi-select drag, selection preservation, release-time model reordering, and existing public `GridView` API.
- Add tests that reproduce boundary jitter without depending on exact frame-by-frame animation output.

**Non-Goals:**

- Do not change `canReorderItems`, `itemReordered`, selection APIs, model/delegate contracts, or visual styling.
- Do not replace the custom drag implementation with Qt's native drag/drop.
- Do not redesign GridView layout, scrolling, or wheel behavior.
- Do not require pixel-perfect animation assertions in tests.

## Decisions

### 1. Add hysteresis to drop target updates

`GridView` should keep the current drop target while the cursor remains inside a small hysteresis band around the active insertion slot. The drop target should update only when the candidate slot is clearly better than the current one or the cursor crosses a stable switch threshold.

This can be implemented as a private helper/state around `dropIndicatorIndex()` or by adding a second helper that takes the previous target and drag position. The state should be reset when a drag starts, ends, is cancelled, or animations are cleared.

Alternative considered: increase `QApplication::startDragDistance()`. That delays drag start but does not fix target flicker after dragging has begun.

### 2. Base target choice on stable item geometry

Drop target calculations should avoid using actively animated displacement offsets as the only source of truth for insertion thresholds. Animated offsets are exactly what changes during `updateDragDisplacement()`, so feeding them back into target selection can make a near-boundary cursor oscillate.

The implementation should either compute candidate slots from stable `QListView::visualRect()` geometry or treat animation offsets as display-only while target selection uses the unshifted grid/item layout.

Alternative considered: continue using animated offsets and add a larger distance threshold. That can still create feedback loops because the threshold itself moves with the animation.

### 3. Reuse or retarget animations only on meaningful changes

`updateDragDisplacement()` should avoid stopping and recreating an animation when an item's target offset is unchanged. If a target changes while an animation is running, it may retarget from the current value, but this should happen only after the stabilized drop target changes.

Alternative considered: disable displacement animation during drag. That removes flicker but makes reorder feedback feel abrupt and less Fluent.

### 4. Keep tests state-based

Tests should simulate cursor jitter around a boundary and verify no reorder signal or model mutation occurs before release, that the same stable drop target is maintained through the jitter sequence, and that the eventual release performs at most one reorder. Tests can use subclass test hooks if needed, but those hooks should stay in test code or protected/private-friendly patterns and should not add public API.

## Risks / Trade-offs

- [Risk] Too much hysteresis could make drag reorder feel sluggish. -> Mitigation: keep the deadband small and require clear crossing to switch slots.
- [Risk] Stable geometry may differ from the animated visual position during a transition. -> Mitigation: use animation only for visual feedback and keep drop semantics tied to stable slots.
- [Risk] Tests may be fragile if they assert exact animation offsets. -> Mitigation: assert target/model/reorder behavior rather than every intermediate frame.
- [Risk] Multi-item drag can shift more slots than single-item drag. -> Mitigation: include multi-select drag in regression coverage or ensure the helper uses `m_dragSourceIndices.size()`.

## Migration Plan

1. Add private drag-target stabilization state/helper(s) to `GridView`.
2. Refactor `mouseMoveEvent()` to update `m_dropTargetIndex` only when the stabilized target changes.
3. Adjust `dropIndicatorIndex()` and/or target calculation to avoid animated-offset feedback near critical boundaries.
4. Ensure drag cleanup resets stabilization state and active animations.
5. Add `TestGridView` cases for boundary jitter, clear threshold crossing, multi-drag behavior, and existing release-time reorder behavior.
6. Run `test_grid_view` with `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1`.

## Open Questions

- The exact hysteresis distance should be tuned during implementation using existing cell size and spacing values; tests should assert behavior around representative boundaries rather than hard-code a single magic pixel for all layouts.
