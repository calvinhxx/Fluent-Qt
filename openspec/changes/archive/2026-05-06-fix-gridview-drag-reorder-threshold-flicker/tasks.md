## 1. Reproduce Threshold Flicker

- [x] 1.1 Review current `GridView::mouseMoveEvent`, `dropIndicatorIndex`, and `updateDragDisplacement` paths to identify where adjacent targets oscillate.
- [x] 1.2 Add test helpers or a small test subclass that can simulate drag movement around a critical insertion boundary without adding public API.
- [x] 1.3 Add a failing boundary-jitter test showing adjacent target oscillation does not emit reorder or mutate the model before release.

## 2. Stabilize Drop Target Selection

- [x] 2.1 Add minimal private state/helper(s) for stabilized drop target selection and reset them when drag starts, ends, or is cancelled.
- [x] 2.2 Refactor target calculation so active displacement animation offsets do not feed back into drop target oscillation.
- [x] 2.3 Add a small hysteresis/deadband so jitter around a critical boundary keeps the existing drop target.
- [x] 2.4 Ensure clear cursor movement beyond the hysteresis band still switches to the neighboring drop target promptly.

## 3. Animation Lifecycle

- [x] 3.1 Avoid stopping and recreating displacement animations when the stabilized drop target and per-item target offsets are unchanged.
- [x] 3.2 When a stabilized target genuinely changes during an active animation, retarget affected items from their current offsets without visual snapping.
- [x] 3.3 Preserve drag pixmap, source item masking, drop indicator painting, and viewport updates.

## 4. Reorder Semantics and Regression Coverage

- [x] 4.1 Keep model mutation limited to mouse release using the final stabilized drop target.
- [x] 4.2 Keep existing Single, Multiple, Extended, and None selection-mode drag reorder behavior intact.
- [x] 4.3 Add or update tests for clear threshold crossing, multi-item drag reorder, and disabled reorder behavior.

## 5. Verification

- [x] 5.1 Build `test_grid_view`.
- [x] 5.2 Run `test_grid_view` with `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1`.
- [x] 5.3 Run `openspec validate fix-gridview-drag-reorder-threshold-flicker`.
- [ ] 5.4 Manually verify GridView drag reorder near item boundaries when an interactive Windows desktop is available.
