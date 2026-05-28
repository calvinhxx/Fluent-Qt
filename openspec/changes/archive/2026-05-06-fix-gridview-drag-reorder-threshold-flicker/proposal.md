## Why

`GridView` drag reorder currently recalculates the drop target on every mouse move using the nearest insertion edge. Near critical boundary values, tiny cursor jitter can flip the target slot back and forth, repeatedly restarting displacement animations and causing visible flicker.

## What Changes

- Stabilize `GridView` drag reorder target selection around insertion thresholds so small cursor jitter does not rapidly alternate adjacent drop targets.
- Preserve responsive reorder behavior once the cursor clearly crosses into a neighboring slot.
- Prevent unnecessary displacement animation restarts when the effective target has not meaningfully changed.
- Add focused tests that simulate boundary jitter and assert stable target/animation behavior without changing model order until release.
- No public `GridView` API changes.

## Capabilities

### New Capabilities

- `gridview-drag-reorder`: Defines `GridView` drag reorder behavior, including stable drop target selection, animation lifecycle, release-time model updates, and selection preservation.

### Modified Capabilities

None.

## Impact

- Code: `src/view/collections/GridView.h` and `src/view/collections/GridView.cpp`.
- Tests: `tests/views/collections/TestGridView.cpp`.
- Specs: new `openspec/specs/gridview-drag-reorder/spec.md` capability after archive.
- No new dependencies and no public API changes.
