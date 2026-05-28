## Context

The current `src/view` root contains public foundation files (`FluentElement`, `QMLPlus`), a private implementation header (`FluentElement_p.h`), component category folders, and `internal/overlay` helpers. Component code is already organized by category, but the foundation layer is not visually separated from component APIs.

The project also has a same-window overlay contract documented in `docs/architecture/overlay-behavior.md`; those helpers are shared infrastructure for `Popup`, `Flyout`, `ComboBox`, `DrawerView`, and future overlay surfaces. They fit better as view foundation infrastructure than as a generic `internal` bucket.

## Goals / Non-Goals

**Goals:**
- Make `src/view/foundation/` the home for reusable view-layer infrastructure.
- Keep component category directories directly under `src/view/`.
- Move private implementation details out of the public root.
- Move overlay helpers into a foundation overlay area while preserving their existing `view::overlay` namespace.
- Keep existing public include paths working during the migration.
- Preserve runtime behavior, component inheritance, and test-facing APIs.

**Non-Goals:**
- Do not add a `src/view/components/` wrapper in this change.
- Do not rename `FluentElement`, `QMLPlus`, overlay helper APIs, or component namespaces.
- Do not change overlay behavior, animation semantics, dismissal semantics, or stacking rules.
- Do not move `viewmodel`, `design`, or non-view infrastructure.
- Do not remove compatibility headers in this change.

## Decisions

### Decision: Use `src/view/foundation/` for shared view infrastructure

`foundation` describes both public mixin infrastructure and private support code without implying that everything under it is a user-facing component. It is more accurate than keeping mixed root files and less vague than a broad `internal` directory.

Alternative considered: keep `FluentElement`, `QMLPlus`, and overlay helpers in the root or `internal`. That keeps churn low, but it leaves public, private, and shared helper concepts mixed together.

### Decision: Keep component category directories flat under `src/view/`

Existing categories such as `basicinput`, `collections`, `date_time`, `dialogs_flyouts`, and `navigation` already map to component families. Adding `src/view/components/` would create large include churn without clarifying ownership more than the current category names.

Alternative considered: move every component directory under `src/view/components/`. That would be a bigger migration and would not solve the immediate confusion around foundation files.

### Decision: Preserve namespaces and public include compatibility

The move should be structural. `FluentElement`, `QMLPlus`, and `view::overlay` helper APIs keep their existing names and namespaces. Existing includes like `view/FluentElement.h`, `view/QMLPlus.h`, and `view/internal/overlay/OverlayGeometry.h` remain available through forwarding headers while project-owned code moves toward canonical foundation paths.

Alternative considered: rename namespaces to match the new folder layout. That would make the migration noisier and risk breaking component contracts unrelated to source organization.

### Decision: Make canonical includes point to foundation

New or touched project-owned code should include foundation headers through canonical paths such as `view/foundation/FluentElement.h`, `view/foundation/QMLPlus.h`, and `view/foundation/overlay/OverlayGeometry.h`, except when a forwarding header or compatibility test intentionally exercises the old include path.

Alternative considered: keep all call sites on legacy includes. That preserves compatibility but makes the new layout harder to discover and weakens the purpose of the migration.

## Risks / Trade-offs

- Header move accidentally changes include ordering or generated MOC behavior -> keep forwarding headers simple and run focused foundation and overlay-consuming tests.
- Canonical include migration creates noisy diffs -> update only project-owned files that are directly affected by the move, leaving unrelated component cleanup for later.
- Forwarding headers hide stale conventions for too long -> document canonical paths and add review guidance so new code uses `foundation`.
- Overlay helper move is mistaken for behavior refactor -> keep `view::overlay` APIs and same-window overlay behavior unchanged, and validate Popup/Flyout/ComboBox/DrawerView tests.

## Migration Plan

1. Create `src/view/foundation/`, `src/view/foundation/private/`, and `src/view/foundation/overlay/`.
2. Move foundation implementation files and overlay helpers into the new directories.
3. Add forwarding headers at the old public and overlay helper paths.
4. Update implementation includes, tests, and docs to prefer canonical foundation paths where touched.
5. Build and run focused validation for foundation tests and overlay-consuming component tests.

Rollback is straightforward: move files back to their original paths and restore direct includes, because this change does not alter runtime state, data formats, or public behavior.

## Open Questions

None for the first migration pass.
