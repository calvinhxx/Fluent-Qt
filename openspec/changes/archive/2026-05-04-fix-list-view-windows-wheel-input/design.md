## Context

`ListView::wheelEvent` already classifies wheel input into `PhaseBased`, `NoPhasePixel`, and `NoPhaseDiscrete`. That was enough to prevent the first round of RDP and Qt5 failures, but Windows still has an edge case: physical mouse wheels, Windows precision touchpads, and RDP-forwarded touchpad gestures can all arrive as `NoScrollPhase` with empty `pixelDelta()`. The current NoPhaseDiscrete branch accumulates deltas before it knows whether the event performed normal content scrolling or represents extra boundary pressure, so a normal Windows wheel/touchpad sequence can carry a large cluster accumulator into the edge and trigger bounce or consume follow-up input incorrectly.

The existing active capability is `listview-wheel-input`. This change tightens that contract without changing public `ListView` API.

## Goals / Non-Goals

**Goals:**

- Make Windows mouse wheel scrolling remain predictable for single notches and rapid wheel sequences.
- Make Windows precision touchpad and RDP-forwarded touchpad sequences scroll normally without boundary bounce flapping or direction reversal.
- Keep keyboard navigation, selection, drag reorder, and Fluent scrollbar synchronization unchanged.
- Preserve macOS phase-based touchpad behavior and existing NoPhasePixel behavior where pixel deltas are available.
- Add tests that reproduce Windows `NoScrollPhase` mouse/touchpad/RDP patterns using synthetic `QWheelEvent`s.

**Non-Goals:**

- Do not add hardware or OS-specific device detection; Qt wheel events do not reliably distinguish physical mouse wheel from RDP touchpad fallback.
- Do not change public properties, signals, selection APIs, model/delegate behavior, or `ScrollBar` implementation.
- Do not redesign the overscroll animation curve for phase-based trackpad input.
- Do not solve TreeView/GridView wheel behavior in this change.

## Decisions

### 1. Treat NoPhaseDiscrete as normal scrolling first

For `NoPhaseDiscrete`, the implementation should move the target scrollbar through a pixel-based normal-scroll path before using the event for overscroll. If the target scrollbar value changes, the event was real content scrolling; the ListView should sync Fluent scrollbars, reset boundary-only cluster state, accept the event, and return.

This prevents accumulated wheel deltas from normal Windows scrolling from becoming overscroll pressure when the list reaches the top or bottom.

The pixel-based path also avoids a Windows/Qt default-handler edge case where smaller high-resolution wheel deltas can be ignored or feel inert when the view remains in item-scroll units.

Alternative considered: keep the current cluster accumulator and only raise thresholds. That is fragile because the same threshold must cover a mouse notch, a fast touchpad cluster, and RDP jitter.

### 2. Make NoPhaseDiscrete boundary handling conservative

When a `NoPhaseDiscrete` event arrives while the scrollbar is already at the relevant boundary and requests more movement beyond that boundary, the event may start one bounded overscroll bounce for visual feedback. Repeated same-direction tail events should be consumed without extending or repeatedly restarting the bounce. This preserves the Fluent boundary response while avoiding RDP/touchpad fallback flap when Qt did not provide phase or pixel-delta intent.

Phase-based touchpad and `NoPhasePixel` events can still use the existing overscroll path because they carry stronger continuous-gesture intent.

Alternative considered: infer touchpad-like behavior from event frequency. That cannot reliably separate a high-resolution mouse wheel from a touchpad, and it still lets RDP jitter trigger UI movement at the edge.

### 3. Reset NoPhaseDiscrete cluster state on direction and scroll boundary transitions

The cluster state should be explicit and boundary-scoped:

- reset after a gap greater than `kClusterGapMs`;
- reset when the sign of the dominant delta changes;
- reset when native scrolling moves content;
- reset when bounce starts or completes;
- never accumulate unbounded deltas while the scrollbar is inside its range.

This makes recovery gestures reliable: after hitting the bottom with a touchpad, a reverse-direction wheel/touchpad gesture should immediately scroll back into content instead of being swallowed by stale boundary state.

### 4. Keep bounce interruption rules path-specific

When bounce is running, `NoPhaseDiscrete` tail events should still be consumed to protect the animation. A reverse-direction `NoPhaseDiscrete` event that can move content back inside the range should cancel or finish the boundary state and allow normal scrolling rather than being swallowed. `PhaseBased` behavior remains allowed to interrupt bounce as it does today.

### 5. Test the event patterns, not the device names

Tests should encode the Qt event signatures that Windows exposes:

- single `NoScrollPhase` + empty `pixelDelta` + `angleDelta=±120` mouse wheel tick;
- high-frequency `NoScrollPhase` + empty `pixelDelta` touchpad/RDP cluster;
- same-direction events at an edge;
- opposite-direction recovery after edge/tail events;
- `NoScrollPhase` + non-empty `pixelDelta` precision-touchpad path;
- horizontal `LeftToRight` dominant-axis behavior.

## Risks / Trade-offs

- NoPhaseDiscrete boundary bounce is intentionally bounded on Windows fallback paths -> mitigated by allowing one short visual bounce while preventing repeated tail-event extension.
- Synthetic tests cannot prove every Windows driver variant -> mitigated by testing the event signatures Qt actually delivers to `wheelEvent`.
- Running native Windows/Qt5 locally may not be available on every development machine -> keep automated offscreen tests platform-neutral and document manual Windows verification.

## Migration Plan

1. Refactor `ListView::wheelEvent` NoPhaseDiscrete handling around normal-scroll-first behavior.
2. Add small private state if needed for boundary cluster direction/gap tracking.
3. Add or update `TestListView` wheel tests for Windows mouse wheel, touchpad/RDP clusters, reverse-direction recovery, NoPhasePixel, and horizontal flow.
4. Run `test_list_view` with `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1`.
5. When available, manually verify on Windows with a physical mouse wheel and precision touchpad.
