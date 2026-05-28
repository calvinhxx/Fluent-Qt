## Why

`ListView` already has cross-platform wheel handling, but Windows still reports abnormal scroll behavior when mouse wheels, precision touchpads, and RDP-forwarded touchpad gestures all arrive as `NoScrollPhase` events. The current shared NoPhaseDiscrete path can still let discrete mouse ticks and high-frequency touchpad clusters interfere with normal scrolling, overscroll entry, and bounce recovery.

## What Changes

- Refine `ListView::wheelEvent` so Windows `NoScrollPhase` input distinguishes practical sub-cases inside the existing NoPhaseDiscrete path:
  - single physical mouse wheel ticks,
  - high-frequency precision touchpad clusters,
  - RDP-forwarded touchpad clusters with jitter or momentum tail events.
- Preserve normal scrolling for mouse wheels and touchpads while preventing edge bounce flapping, direction reversal, and swallowed wheel events on Windows.
- Keep `PhaseBased` and `NoPhasePixel` handling compatible with existing macOS and pixel-delta touchpad behavior.
- Add targeted automated tests that simulate Windows mouse wheel ticks, Windows touchpad high-frequency clusters, RDP jitter/tail clusters, opposite-direction recovery, and horizontal `LeftToRight` flow.
- No public `ListView` API changes.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `listview-wheel-input`: Tighten the existing ListView wheel-input contract for Windows mouse wheel, precision touchpad, and RDP-forwarded touchpad handling.

## Impact

- Code: `src/view/collections/ListView.h` and `src/view/collections/ListView.cpp`.
- Tests: `tests/views/collections/TestListView.cpp`.
- Specs: existing `openspec/specs/listview-wheel-input/spec.md` receives delta requirements.
- No new dependencies and no public API changes.
