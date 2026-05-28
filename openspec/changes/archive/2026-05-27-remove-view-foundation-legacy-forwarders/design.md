## Context

`organize-view-foundation-structure` moved reusable view infrastructure to `src/view/foundation/` and made `view/foundation/...` the canonical include family. It intentionally kept legacy forwarding headers at `src/view/FluentElement.h`, `src/view/QMLPlus.h`, and `src/view/internal/overlay/*.h` during the migration.

The project now wants a stricter source layout: `src/view` should show component category directories plus `foundation/`, without root-level foundation forwarding headers or an `internal/overlay` compatibility bucket. Existing project-owned code still has many legacy include references, so the cleanup is broad but mechanical.

## Goals / Non-Goals

**Goals:**
- Make `view/foundation/FluentElement.h`, `view/foundation/QMLPlus.h`, and `view/foundation/overlay/...` the only supported project include paths for these foundation APIs.
- Remove root-level forwarding headers from `src/view`.
- Remove `src/view/internal/overlay/` forwarding headers and directory.
- Update project-owned source, tests, docs, and guidance to use canonical include paths.
- Preserve runtime behavior, namespaces, component inheritance, and overlay behavior.

**Non-Goals:**
- Do not move component category directories under a new `components/` wrapper.
- Do not rename `FluentElement`, `QMLPlus`, `view::overlay`, or component namespaces.
- Do not change Fluent theme behavior, AnchorLayout behavior, Popup/Flyout/ComboBox/DrawerView behavior, or public component APIs beyond removed include compatibility.
- Do not archive previous changes as part of this implementation.

## Decisions

### Decision: Remove compatibility headers instead of keeping deprecation shims

The cleanup should delete `src/view/FluentElement.h`, `src/view/QMLPlus.h`, and `src/view/internal/overlay/*.h` after all project-owned references have moved to canonical includes. This makes the directory structure unambiguous and prevents new code from copying stale include paths.

Alternative considered: keep forwarding headers with comments marking them deprecated. That would avoid source-level breakage for external consumers, but it keeps the root clutter and does not satisfy the goal of fully normalizing project structure.

### Decision: Use mechanical include migration for project-owned code

All project-owned source and tests should switch directly from legacy includes to canonical foundation includes. The migration is intentionally broad and mechanical; behavioral edits should be avoided in the same change.

Alternative considered: update only source headers and leave tests on legacy paths. That would preserve compile coverage for removed paths, which is the opposite of this change's intent.

### Decision: Validate broadly because public include paths are removed

Removing compatibility headers can break any component header that still includes legacy paths. The implementation should run at least a full build and a focused set of foundation plus overlay-consuming tests; running full CTest is appropriate because the touched headers are widespread.

Alternative considered: only build a few overlay targets. That is too narrow for a repository-wide include migration.

## Risks / Trade-offs

- Source-level breakage for downstream users that still include legacy paths -> document the breaking change and canonical migration path.
- Large mechanical diff across many component/test files -> keep changes limited to include replacements and header deletions.
- Missed legacy include path in ignored or generated files -> search source, tests, docs, OpenSpec artifacts, and customization guidance before final validation.
- Qt/MOC include ordering surprises -> build affected targets and the full library after migration.

## Migration Plan

1. Replace project-owned `#include "view/FluentElement.h"` with `#include "view/foundation/FluentElement.h"`.
2. Replace project-owned `#include "view/QMLPlus.h"` with `#include "view/foundation/QMLPlus.h"`.
3. Replace any project-owned `view/internal/overlay/...` include with `view/foundation/overlay/...`.
4. Delete `src/view/FluentElement.h`, `src/view/QMLPlus.h`, and `src/view/internal/overlay/`.
5. Update docs and repository guidance to remove legacy compatibility language.
6. Validate with build, focused tests, broad CTest, include-path searches, and OpenSpec strict validation.

Rollback is straightforward: restore forwarding headers and update guidance back to compatibility wording. Runtime state and data formats are unaffected.

## Open Questions

None.