## Why

Popup-like components currently solve rounded clipping, transparent backgrounds, shadow margins, light-dismiss, z-order, animation, and platform window behavior in several places. Recent work across Popup, Flyout, DrawerView, ComboBox dropdowns, and other transient surfaces keeps revisiting the same edge cases, so the project needs one durable overlay behavior contract instead of component-by-component fixes.

## What Changes

- Define a shared overlay behavior contract for same-window overlays, independent top-level windows, transparent backgrounds, clipping/masks, shadows, light-dismiss, z-order, focus, and animation.
- Establish when components SHALL use the same-window overlay model versus an independent native window model.
- Standardize the semantics of:
  - transparent overlay backgrounds and rounded visual cards,
  - shadow margin and visible-card geometry,
  - top-level attachment and cross-platform window flags,
  - modal/dim scrim behavior,
  - light-dismiss through outside press and Escape,
  - z-order/raise order among overlay, scrim, hosted content, and background widgets,
  - animation enablement and synchronous open/close behavior,
  - parent/top-level destruction safety.
- Apply the contract to existing overlay-like components: `Popup`, `Flyout`, `DrawerView`, and `ComboBox` dropdowns.
- Add focused automated tests and VisualCheck coverage for shared overlay rules and cross-component compatibility.
- Preserve existing public APIs where possible; any breaking API consolidation must be deferred to follow-up proposals.

## Capabilities

### New Capabilities

- `overlay-behavior`: Cross-component rules for overlay lifecycle, windowing model, transparent rendering, light-dismiss, z-order, animation, and validation.

### Modified Capabilities

- None.

## Impact

- Affected source areas:
  - `src/view/dialogs_flyouts/Popup.*`
  - `src/view/dialogs_flyouts/Flyout.*`
  - `src/view/collections/DrawerView.*`
  - `src/view/basicinput/ComboBox.*`
  - nearby tests under `tests/views/dialogs_flyouts/`, `tests/views/collections/`, and `tests/views/basicinput/`
- The implementation may introduce shared private helpers for overlay geometry, scrim/light-dismiss handling, z-order, and visual-card metrics.
- No new third-party runtime dependency is expected.
- Existing component specs remain valid; this change adds a cross-cutting contract that those components must satisfy.
