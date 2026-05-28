## Context

The project already has several transient surfaces:

- `Popup`: same-window overlay attached to the parent top-level widget, with optional modal/dim scrim and opacity animation.
- `Flyout`: `Popup` subclass with anchor and placement semantics.
- `ComboBox::ComboBoxPopup`: `Flyout` subclass with ComboBox-specific width, placement, ListView content, and selection behavior.
- `DrawerView`: same-window edge drawer with its own scrim, close policy, z-order, masks, and animation implementation.

These components solve similar problems independently. The recurring failure modes are visual card corners leaking through square child viewports, transparent background differences across platforms, shadow margins changing hit testing, light-dismiss event propagation, scrim z-order, raise order, top-level destruction safety, and animations diverging from synchronous test expectations.

## Goals / Non-Goals

**Goals:**

- Establish a shared overlay behavior contract for same-window overlays and any future native-window overlays.
- Centralize reusable overlay helpers where practical without forcing all components into one public base class.
- Align Popup, Flyout, DrawerView, and ComboBox dropdown behavior for transparency, rounded visual cards, shadows, light-dismiss, scrim, z-order, focus, and animation.
- Keep existing public APIs compatible unless a follow-up proposal explicitly handles a migration.
- Add automated tests and VisualCheck scenarios that catch overlay regressions across components and platforms.

**Non-Goals:**

- Replace all overlay components with a single public class.
- Redesign ComboBox selection behavior, DrawerView drag semantics, or Flyout placement semantics beyond overlay consistency.
- Introduce native OS popover APIs or new third-party dependencies.
- Solve every visual detail of component content hosted inside overlays.
- Remove existing `Popup`, `Flyout`, `DrawerView`, or `ComboBox` APIs.

## Decisions

### Shared private overlay helpers, not a new public base class

The implementation SHOULD introduce reusable internal helpers for top-level resolution, visible-card metrics, shadow margins, mask paths, scrim management, event filtering, and z-order. Those helpers can live under an internal namespace or private module. Existing public classes should keep their current inheritance unless a specific component already naturally derives from another overlay class.

Alternative considered: make `DrawerView` derive from `Popup`. That would reduce duplication but risks breaking edge-drawer geometry, drag lifecycle, and public assumptions. This change should first unify behavior; inheritance reshaping can be a later proposal if still useful.

### Same-window overlay is the default policy

Existing project components should default to same-window overlays attached to the owning top-level widget. A native top-level window model should be reserved for future cases that cannot be correctly represented inside one top-level widget, and must explicitly define platform flags and focus behavior.

Alternative considered: use native top-level windows for all overlays to avoid clipping. That creates harder cross-platform focus, transparency, stacking, and activation problems.

### Visible-card geometry is separate from widget geometry

Overlay implementation should distinguish:

- outer widget geometry that includes shadow margin and event area;
- visible card geometry used for painting rounded surfaces and content clipping;
- content geometry used for hosted widgets.

This prevents shadows from shifting logical placement or light-dismiss hit testing.

Alternative considered: make widget geometry equal the visible card. That simplifies placement but clips shadows and makes rounded visual edges harder to protect.

### Light-dismiss is centralized by policy

Outside press and Escape handling should share a policy model across Popup, Flyout, ComboBox dropdowns, and DrawerView. The policy should define whether the original outside event is swallowed or allowed to continue based on modal state and component semantics.

Alternative considered: let each component keep bespoke event filters. That is the current failure mode and makes z-order and event propagation inconsistent.

### Tests should cover behavior, not only pixels

Automated tests should verify top-level attachment, non-native-window flags, modal scrim blocking, non-modal event propagation, Escape/outside close policy, visible-card geometry, animation-disabled lifecycle, and destruction safety. VisualCheck should demonstrate real visual cases such as rounded corners, shadow, dim, nested content, and light/dark themes.

Alternative considered: rely on VisualCheck for all overlay issues. This misses regressions in event propagation and platform window flags.

## Risks / Trade-offs

- [Risk] Central helpers accidentally change working component behavior. -> Mitigation: migrate one component path at a time and keep existing focused tests passing after each migration.
- [Risk] Same-window overlays can be clipped by unusual parent/native widget setups. -> Mitigation: resolve and attach to the owning top-level widget and document native-window overlays as a future explicit policy.
- [Risk] Light-dismiss behavior differs between modal and non-modal overlays. -> Mitigation: encode event propagation expectations in tests for both modes.
- [Risk] Shadow margin changes break placement assertions. -> Mitigation: expose or test visible-card geometry separately from outer widget geometry.
- [Risk] Platform transparency behaves differently on macOS, Windows, and offscreen tests. -> Mitigation: use Qt attributes and masks consistently, and keep offscreen-friendly tests for geometry/lifecycle.

## Migration Plan

1. Add internal overlay utility types for top-level resolution, visible-card geometry, mask/shadow metrics, scrim/z-order, and close-policy handling.
2. Adapt `Popup` to use the shared rules first, since Flyout and ComboBox dropdowns depend on it.
3. Verify `Flyout` placement and clamping remain compatible while using shared visible-card metrics.
4. Align `ComboBox::ComboBoxPopup` with shared placement, rounding, light-dismiss, and z-order rules without changing selection behavior.
5. Align `DrawerView` scrim, same-window attachment, z-order, and close-policy handling while preserving edge geometry and drag semantics.
6. Add tests and VisualCheck coverage for all touched components.
7. Update docs/spec notes describing the shared overlay contract and any intentional deviations.

Rollback can be done per component by reverting that component's helper adoption while keeping the overlay behavior spec and tests as a target for a later implementation pass.

## Open Questions

- Should shared helpers be fully private implementation details, or should a small test-only API expose visible-card geometry for deterministic tests?
- Should future components be required to derive from `Popup` when they are anchored overlays, or is using the helper contract enough?
- Should there be a later change to migrate `DrawerView::ClosePolicy` to reuse `Popup::ClosePolicy` type names, or should they remain separate compatible enums?
