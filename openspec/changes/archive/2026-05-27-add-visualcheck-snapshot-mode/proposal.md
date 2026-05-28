## Why

VisualCheck tests currently rely on manual inspection only, so visual regressions are hard to review consistently across repeated runs. Adding a snapshot generation mode gives developers a repeatable first step toward visual baselines without introducing brittle pixel-diff gating yet.

## What Changes

- Add a `VISUAL_SNAPSHOT=1` mode for VisualCheck tests that captures deterministic screenshots instead of requiring a purely manual review loop.
- Standardize snapshot runs around fixed window sizing, a known Fluent theme, and predictable output under `build/visual/`.
- Preserve the existing `SKIP_VISUAL_TEST=1` behavior for automated CTest runs.
- Defer pixel comparison, baseline approval, and CI failure policy to a later change.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `test-infrastructure`: Extend VisualCheck test infrastructure with a repeatable screenshot generation mode.

## Impact

- Shared Qt test utilities and/or VisualCheck helper code used by component tests.
- VisualCheck test cases that need to opt into snapshot generation.
- Build output conventions for generated images under `build/visual/`.
- README or test workflow documentation describing how to run snapshot generation.