## Why

Current visual review loops rely too heavily on launching the gallery, taking screenshots, and asking a human or AI to judge alignment from pixels. This is slow and imprecise for geometry problems such as title bar alignment, icon sizing, and input-button centering, where the expected result can be expressed as deterministic layout contracts.

## What Changes

- Add a structured visual geometry verification capability for app/gallery UI surfaces that need precise layout validation.
- Introduce stable object names for visually important widgets so tests and diagnostic tools can locate them without relying on implementation-local pointers.
- Add geometry contract checks for key app UI regions, starting with gallery title bar controls.
- Add an opt-in geometry dump path for manual and AI-assisted debugging, producing text output with widget names, rectangles, sizes, centers, and selected style metadata.
- Keep screenshots and VisualCheck flows as final review aids rather than the primary mechanism for layout correctness.

## Capabilities

### New Capabilities
- `visual-geometry-verification`: Defines structured widget identification, geometry dumps, and deterministic layout assertions for app/gallery visual UI verification.

### Modified Capabilities
- None.

## Impact

- Affected code: gallery shell tests, app-owned gallery widgets, and app-focused test support.
- Affected workflow: app visual review should prefer geometry assertions and dump output before screenshot inspection.
- Affected docs: testing and visual review guidance should document when to use geometry tests, geometry dumps, screenshots, and manual VisualCheck.
- Dependencies: no new third-party dependency is expected.
