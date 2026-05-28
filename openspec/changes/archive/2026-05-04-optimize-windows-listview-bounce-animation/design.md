## Context

`ListView::wheelEvent` currently classifies input into PhaseBased, NoPhasePixel, and NoPhaseDiscrete paths. The Windows-like NoPhaseDiscrete path already scrolls content first and only enters a bounded boundary bounce for events that push beyond the current edge. That prevents many wheel/RDP regressions, but the visual rebound still has rough edges: the boundary offset is assigned immediately, a timer delays bounce-back, and same-direction tail events can arrive while the offset or animation is in flight.

This change focuses on the rebound animation quality for Windows fallback input. It keeps the public `ListView` API, model/delegate behavior, selection, drag reorder, and Fluent scrollbar sync unchanged.

## Goals / Non-Goals

**Goals:**

- Make NoPhaseDiscrete boundary rebound feel continuous from first edge feedback through return-to-zero.
- Prevent repeated Windows wheel/touchpad/RDP tail events from restarting, extending, or snapping the active bounce.
- Preserve reverse-direction recovery so the user can immediately scroll back into content.
- Keep existing macOS PhaseBased overscroll behavior and NoPhasePixel behavior compatible.
- Cover the smoothness contract with focused `TestListView` cases that validate state transitions rather than exact pixels per frame.

**Non-Goals:**

- Do not add OS or hardware-specific device detection.
- Do not change public `ListView` properties, signals, model/delegate behavior, selection behavior, or `ScrollBar` APIs.
- Do not redesign GridView or TreeView wheel behavior.
- Do not require frame-perfect visual assertions in automated tests.

## Decisions

### 1. Use an explicit bounce lifecycle for NoPhaseDiscrete boundary feedback

NoPhaseDiscrete boundary input should move through explicit states: idle, edge feedback pending/active, bounce-back running, and settled. The implementation can represent this with small private fields such as the active boundary direction and whether a NoPhaseDiscrete bounce is already armed/running.

This avoids using only `m_overscrollX/Y` and animation state as implicit truth. It also makes repeated same-direction tail events easy to consume without restarting the timer or animation.

Alternative considered: only tweak duration/easing. That may make a single bounce nicer, but it does not fix jitter caused by timer restarts or stale tail events.

### 2. Start bounce-back from the current overscroll value without discontinuity

When bounce-back starts, the animation start value must be the current overscroll value on the active axis. If an animation is already running in the same boundary direction, same-direction NoPhaseDiscrete tails must be consumed without calling `stop()`, resetting start/end values, or restarting the timer.

If a reverse-direction NoPhaseDiscrete event can move the scrollbar back into range, stale boundary state should be cleared and the event should use the normal-scroll-first path.

Alternative considered: clamp all tail events to zero immediately. That removes jitter but makes the rebound feel abrupt and loses the boundary affordance.

### 3. Tune rebound timing in one place

The NoPhaseDiscrete bounce should use named constants for edge feedback delay, duration, max overscroll, and easing. The default should remain restrained: enough to be visible, short enough not to block repeated wheel input. The existing `Animation` design helpers should be used where they already express Fluent timing.

Alternative considered: expose tuning as public API. This would add surface area for an internal polish problem and is unnecessary for this change.

### 4. Keep tests state-based

Tests should assert that boundary tails do not restart or destabilize bounce state, that the final offset settles, and that reverse input scrolls content again. They should avoid relying on exact per-frame interpolation because Qt timer scheduling differs across platforms and CI load.

## Risks / Trade-offs

- [Risk] Synthetic wheel tests cannot prove every Windows driver shape. -> Mitigation: test the Qt event signatures delivered to `wheelEvent`: NoScrollPhase with empty `pixelDelta()` and angle deltas.
- [Risk] Smoother animation could accidentally make wheel input feel delayed. -> Mitigation: keep normal-scroll-first behavior unchanged and only animate already-at-boundary feedback.
- [Risk] Additional private state could become stale after destruction, flow changes, or bounce completion. -> Mitigation: reset lifecycle state on bounce completion, reverse recovery, cluster reset, and destructor/timer shutdown paths.
- [Risk] Mac trackpad bounce could regress if shared code is changed too broadly. -> Mitigation: keep PhaseBased interruption and overscroll damping behavior path-specific.

## Migration Plan

1. Add private helper/state for NoPhaseDiscrete boundary bounce lifecycle if needed.
2. Refactor NoPhaseDiscrete boundary-tail handling to arm/start bounce once and consume same-direction tails without restart.
3. Make `startBounceBack()` or a new private helper start from the current overscroll value and clear lifecycle state on finish.
4. Add or update `TestListView` cases for same-direction tail stability, reverse recovery during/after bounce, and final settlement.
5. Run `test_list_view` with `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1`; manually verify on Windows when hardware is available.

## Open Questions

- The exact rebound duration/easing should be tuned during implementation against the current `Animation` constants and visual feel, then captured in tests only as lifecycle behavior, not frame timing.
