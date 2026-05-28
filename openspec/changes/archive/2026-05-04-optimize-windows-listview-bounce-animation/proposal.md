## Why

Windows `ListView` overscroll already avoids the worst NoScrollPhase input issues, but the rebound can still feel uneven when discrete wheel/touchpad tail events arrive around the boundary. The current bounce path can snap from a manually assigned overscroll offset into a delayed animation, which makes the recovery feel less fluid than the expected Fluent/WinUI-style response.

## What Changes

- Refine the Windows/NoPhaseDiscrete boundary rebound so it starts from a stable overscroll value and transitions into bounce-back without visible snap, jitter, or repeated restart.
- Preserve normal mouse-wheel, precision-touchpad, and RDP-forwarded touchpad scrolling behavior while making boundary feedback visually smoother.
- Tune rebound duration/easing and tail-event handling only where needed for smoothness; keep macOS phase-based behavior and NoPhasePixel compatibility intact.
- Add focused tests or instrumentation hooks that verify repeated Windows boundary-tail events do not restart, extend, or destabilize an active bounce.
- No public `ListView` API changes.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `listview-wheel-input`: Tighten the existing ListView wheel-input contract for smooth Windows boundary rebound animation under NoPhaseDiscrete input.

## Impact

- Code: `src/view/collections/ListView.h` and `src/view/collections/ListView.cpp`.
- Tests: `tests/views/collections/TestListView.cpp`.
- Specs: existing `openspec/specs/listview-wheel-input/spec.md` receives delta requirements.
- No new dependencies and no public API changes.
