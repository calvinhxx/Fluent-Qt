## Context

`TreeView` is currently a view-layer control: it owns Fluent theming, scroll chrome, selection helpers, overscroll, chevron rotation, and expand/collapse reveal, but callers still provide the model and row delegate. The visible selected-row indicator used by the current tests is painted by `tests/views/collections/FluentTreeItemDelegate`, which starts a basic accent appear animation for newly selected indexes.

That split is the right boundary for this change. Selection movement and hierarchy comparison belong in `TreeView`, because they depend on current/previous indexes, visual row order, expansion state, and scroll geometry. Painting belongs in the delegate, because row composition, icon/check box/text metrics, and item templates remain caller/delegate responsibilities.

## Goals / Non-Goals

**Goals:**
- Add TreeView-owned selected indicator motion state for direction-aware and hierarchy-aware transitions.
- Let delegates query normalized motion state and render richer indicator effects without duplicating selection comparison logic.
- Keep default behavior compatible: callers that do not use the new motion state still get normal TreeView selection.
- Keep automated tests deterministic and VisualCheck useful for inspecting the richer motion.

**Non-Goals:**
- Do not move model creation, item content, routing, or navigation semantics into `TreeView`.
- Do not require `TreeView` to inject a concrete delegate.
- Do not redesign drag reorder, check box behavior, chevron animation, scroll bounce, or row typography.
- Do not make the indicator animation depend on screenshot-only assertions.

## Decisions

1. **TreeView owns motion classification; delegates own drawing.**
   - Add additive public/read-only query APIs for the selected indicator motion state, similar in spirit to the existing `chevronRotation(index)` helper.
   - Track the previous and current selected/current indexes using `QPersistentModelIndex` where safe, compute visual movement from current row geometry or model visual order, and compute depth by walking parent indexes.
   - Alternative considered: keep all logic in `FluentTreeItemDelegate`. That would make each delegate reimplement direction/depth comparison and would break when the selection model changes after delegate construction.

2. **Represent motion as small enums plus normalized progress.**
   - Use enum values for vertical direction (`None`, `Up`, `Down`) and hierarchy transition (`None`/`SameLevel`, `Inward`, `Outward`) with `Q_ENUM` so tests and Qt users can inspect them.
   - Use a `0.0..1.0` progress value driven by a single indicator animation. If animations are disabled, progress snaps to `1.0`.
   - Alternative considered: expose raw rectangles only. Rectangles are useful for tests, but they do not communicate why a motion is happening and are less useful for custom delegates.

3. **Direction and hierarchy effects combine rather than compete.**
   - Vertical direction determines the leading edge and stretch/compression feel: moving down should have a different visual treatment than moving up.
   - Hierarchy transition modulates the indicator treatment: inward movement can feel like the indicator settles deeper into the row, while outward movement can feel like it returns toward the parent level.
   - Same-level movement uses the direction-aware effect only.

4. **Initial and invalid transitions are neutral.**
   - The first selection after model install/show, clearing selection, hidden/collapsed targets, and invalid indexes should not produce stale direction or hierarchy values.
   - When the current item changes because a drag reorder moves an item, TreeView should recompute from the resulting valid indexes rather than holding old geometry.

5. **VisualCheck follows repository test conventions.**
   - Extend `TestTreeView` VisualCheck with project Fluent components and `AnchorLayout`.
   - Include controls or sample actions that force upward, downward, inward, and outward transitions so the two requested effects can be inspected manually.

## Risks / Trade-offs

- [Risk] Model resets or selection model replacement can invalidate persistent indexes. -> Mitigation: clear motion state on model/selection reset and recompute only from valid indexes.
- [Risk] Delegate and TreeView geometry can diverge if multiple custom delegates use different indicator lanes. -> Mitigation: TreeView exposes motion classification/progress, while delegates keep their own row-local indicator rectangle.
- [Risk] Combining direction and hierarchy effects can look over-animated. -> Mitigation: keep durations short, use Fluent easing tokens, and make animation disablement/snap behavior available for deterministic tests.
- [Risk] Animating every selection update can repaint too broadly. -> Mitigation: update only the viewport/affected rows where practical and reuse one animation object for the active selected indicator transition.
