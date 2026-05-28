## Context

`CalendarView` handles wheel input as discrete page navigation rather than continuous content scrolling. The current implementation classifies input into phase-based, pixel-delta, and NoPhaseDiscrete events, accumulates normalized wheel delta until a page threshold, then marks the cluster consumed after the first page commit.

That protects many tails, but it is still too coarse for Windows precision touchpads and RDP-forwarded gestures. Qt can deliver a touchpad burst as `NoScrollPhase` with empty `pixelDelta()` and a series of small `angleDelta()` values. A small physical wobble can therefore look like multiple discrete wheel ticks, especially when animation finishes or the cluster timer expires between tail packets. ListView fixed the related Windows problem by designing around Qt event signatures instead of attempting device detection; CalendarView should do the same while preserving CalendarView's discrete paging model.

## Goals / Non-Goals

**Goals:**

- Keep CalendarView paging deterministic across Windows mouse wheels, Windows precision touchpads, RDP-forwarded touchpad input, macOS touchpads, and pixel-delta devices.
- Ensure one continuous touchpad gesture or fallback NoPhaseDiscrete burst can commit at most one page.
- Preserve mouse wheel behavior: a full notch commits exactly one page, and sub-notch deltas accumulate to one page when the threshold is reached.
- Keep page navigation separate from Day/Month/Year content-level transitions.
- Add automated tests for event signatures rather than device names.

**Non-Goals:**

- No public `CalendarView` API changes.
- No OS-specific hardware or driver detection.
- No change to date selection, date range clamping, first-day-of-week behavior, or CalendarDatePicker composition.
- No redesign of the visual page transition animation.
- No changes to ListView, GridView, TreeView, or shared scrolling controls.

## Decisions

### 1. Keep event-signature classification, not device detection

CalendarView should continue classifying wheel input by Qt event shape:

- `PhaseBased`: `phase() != Qt::NoScrollPhase`
- `NoPhasePixel`: `phase() == Qt::NoScrollPhase` and `pixelDelta()` is non-empty
- `NoPhaseDiscrete`: `phase() == Qt::NoScrollPhase` and `pixelDelta()` is empty

This mirrors the successful ListView fix. It avoids brittle guesses about whether a packet came from a mouse, precision touchpad, RDP, or a driver fallback.

Alternative considered: add Windows-only heuristics or query device type. Qt wheel events do not reliably expose enough device identity, and device heuristics would regress remote desktop and high-resolution mouse wheels.

### 2. Split gesture state from committed-page suppression

The current `m_wheelClusterConsumed` state is a single gate for all non-phase clusters. The fix should make the state explicit:

- accumulated signed/dominant direction for thresholding;
- last wheel timestamp for NoPhaseDiscrete cluster gaps;
- input kind for deciding whether phase, pixel, or discrete rules apply;
- a committed flag that means "this gesture/cluster already navigated one page";
- enough transition awareness to prevent a stale tail from committing when animation completes.

For phase-based gestures, `ScrollBegin` and `ScrollEnd` define the gesture lifetime. For NoPhaseDiscrete fallback input, the cluster gap defines the lifetime. For NoPhasePixel, use the same conservative single-page-per-active-gesture rule unless tests prove pixel-delta trackpads need a different lifetime.

Alternative considered: only increase `kWheelClusterGapMs`. That may hide the local bug but would make mouse wheel responsiveness worse and still fails when Windows/RDP tail packets cross the timer boundary.

### 3. Commit at most one page per active wheel gesture

CalendarView is a pager, so it should not attempt ListView-style normal scrolling first. Instead, the ListView lesson becomes: do not let accumulated movement from one input cluster leak into another behavioral mode.

Once a wheel gesture commits a page:

- same-direction tail packets in the same phase gesture or NoPhaseDiscrete cluster MUST be accepted and ignored;
- wheel input while a page or content-level transition is active MUST be accepted and ignored;
- a new page may be committed only after a clear fresh gesture boundary, such as `ScrollEnd` followed by a new `ScrollBegin`, a NoPhaseDiscrete cluster gap, or a direction change that starts fresh accumulation.

This prevents Windows touchpad wobble from turning one gesture into two calendar page changes.

Alternative considered: allow repeated paging while the user keeps scrolling. That is useful in some list controls, but CalendarView page navigation is large and animated; accidental duplicate month/year jumps are worse than requiring a new intentional wheel gesture.

### 4. Keep mouse wheel notches responsive

A physical mouse wheel typically emits `NoPhaseDiscrete` with an empty `pixelDelta()` and `angleDelta()` near `±120`. CalendarView should still navigate immediately for a full notch. Sub-notch deltas such as `±30` or `±60` should accumulate within the current cluster and commit when they reach the page threshold.

The implementation should distinguish "cluster has already committed" from "cluster is accumulating." A fresh mouse notch after the cluster gap can commit another page; tail packets inside the gap cannot.

### 5. Test Qt event patterns directly

Tests should construct synthetic `QWheelEvent`s that encode:

- full mouse notch: `NoScrollPhase`, empty `pixelDelta()`, `angleDelta=±120`;
- high-frequency Windows touchpad fallback: repeated `NoScrollPhase`, empty `pixelDelta()`, small `angleDelta`;
- pixel-capable fallback: `NoScrollPhase`, non-empty `pixelDelta()`;
- macOS-like gesture: `ScrollBegin`, multiple `ScrollUpdate`, `ScrollEnd`;
- momentum: `ScrollMomentum`;
- direction changes and cluster gaps.

The expected result is based on page commits and signal counts, not hardware names.

## Risks / Trade-offs

- **[Risk] Conservative single-page gesture handling may feel slower for users who intentionally flick through many months.** -> Mitigation: allow a fresh cluster/gesture after a clear gap or phase boundary, and keep navigation buttons/keyboard paging unchanged.
- **[Risk] Synthetic wheel events may not cover every driver packet sequence.** -> Mitigation: cover the known Qt signatures from the ListView bug and keep a manual Windows verification task for mouse wheel plus precision touchpad.
- **[Risk] Touchpad tail timing can vary across machines.** -> Mitigation: make behavior depend on explicit gesture boundaries, direction changes, transition state, and cluster gap rather than a single fragile threshold.
- **[Risk] Changing wheel state may break existing sub-threshold accumulation tests.** -> Mitigation: keep existing threshold scenarios and add new regression tests before changing implementation.

## Migration Plan

1. Add/adjust CalendarView wheel helper tests for mouse, touchpad fallback, pixel-delta, phase-based, momentum, direction, and animation-tail scenarios.
2. Refactor `CalendarView::wheelEvent` around explicit wheel gesture/cluster state.
3. Keep public properties and signals unchanged; verify `visibleMonthChanged` emits once per committed page.
4. Run `test_calendar_view` and `test_calendar_date_picker` with visual tests skipped.
5. Manually verify on Windows with a physical mouse wheel and precision touchpad when hardware is available.
