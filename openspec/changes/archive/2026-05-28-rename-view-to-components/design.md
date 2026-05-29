## Context

The project currently uses `src/view/` for reusable Fluent Qt widgets and infrastructure, while the application shell area already has `app/model`, `app/view`, and `app/viewmodel`. As the Gallery application grows, keeping reusable components under a directory named `view` makes the architecture read as if library components and app views occupy the same layer.

The current component library is built by recursively collecting source files under `src/`, and component tests live under `tests/views/`. OpenSpec specs, development docs, AGENTS.md, include paths, and many tests also reference `src/view`, `tests/views`, and `view/...` headers. This is therefore a repository-wide source layout migration, not a local move.

## Goals / Non-Goals

**Goals:**

- Make `src/components/` the canonical home for reusable Fluent Qt components and component foundation infrastructure.
- Make `tests/components/` the canonical home for tests that mirror reusable component categories.
- Update project-owned includes to use `components/...` header paths.
- Preserve runtime behavior, public class names, Qt inheritance, test semantics, CTest labels, and VisualCheck contracts.
- Keep `app/` as the application layer for Gallery model/view/viewmodel work.
- Synchronize live docs and OpenSpec specs so future work does not reintroduce `src/view` or `tests/views`.

**Non-Goals:**

- Do not rename the C++ namespace from `view::...` to `fluent::...` in this change.
- Do not redesign component APIs, ownership semantics, rendering, or interaction behavior.
- Do not implement the Gallery app.
- Do not introduce new dependencies or change supported Qt/CMake/vcpkg versions.

## Decisions

### Decision 1: Rename directories before namespace

The migration will move the source and test directories first:

```text
src/view/        -> src/components/
tests/views/     -> tests/components/
```

The C++ namespace remains `view::...` for this change. This keeps the diff focused on filesystem, include, build, documentation, and spec references. Namespace migration can follow as a separate explicit API change once the component source layout is stable.

Alternative considered: rename both directories and namespace in one change. That would reduce the number of migration passes but would mix path churn with public API churn, making failures harder to isolate and review.

### Decision 2: Make `components/...` the canonical include root

Project-owned source and tests will include component headers through `components/...`, for example:

```cpp
#include "components/basicinput/Button.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
```

The implementation should remove project-owned `view/...` includes from live source, docs, and specs. Compatibility forwarding headers under `src/view/` are not part of the target architecture because they keep the ambiguous directory alive. If temporary forwarding is needed during implementation, it should be removed before the task is complete.

Alternative considered: keep permanent forwarding headers from `view/...` to `components/...`. This would reduce immediate include churn for downstream users but would leave two canonical surfaces and undermine the reason for the migration.

### Decision 3: Keep app code outside the component library

Gallery business code belongs under the existing root-level `app/` structure:

```text
app/model/
app/view/
app/viewmodel/
```

The reusable library target should compile `src/components`, `src/design`, `src/compatibility`, and `src/utils`. It should not absorb `app/` code. Gallery build wiring can be added later through an app executable target that links `fluent_qt_lib`.

### Decision 4: Update CMake source discovery deliberately

The current top-level source discovery collects `src/*.h` and `src/*.cpp` recursively. After the rename, this still excludes root-level `app/`, but the implementation should verify source grouping, target include directories, and test output directories use the new naming. If explicit component source lists are introduced, they must continue including design, compatibility, utilities, resources, and all component categories.

### Decision 5: Treat docs and OpenSpec as part of the migration

The change should update live guidance and specs that define component ownership and paths. Archived OpenSpec changes can retain historical `src/view` and `tests/views` references. Live docs, live specs, AGENTS.md, README, and tests should not keep stale active-path guidance after implementation.

## Risks / Trade-offs

- [Risk] A broad path rename can miss includes, docs, or generated test paths. -> Mitigation: use repository-wide searches for `src/view`, `tests/views`, `view/`, and `tests/views/` after the move, excluding archived history where appropriate.
- [Risk] CTest label filters or VisualCheck instructions may drift when `tests/views` becomes `tests/components`. -> Mitigation: preserve component/category labels and update examples to the new physical paths.
- [Risk] Removing `view/...` include compatibility can break external callers. -> Mitigation: mark the change as breaking and document migration to `components/...`; do not mix this with namespace migration.
- [Risk] OpenSpec specs with component-specific path references may remain stale. -> Mitigation: include a live spec synchronization task and validate both the change and the full spec tree.

## Migration Plan

1. Move `src/view` to `src/components` and `tests/views` to `tests/components`.
2. Update includes, CMake paths, test output paths, docs, and live OpenSpec specs from view paths to component paths.
3. Preserve the `view::...` namespace and all public component behavior.
4. Run focused source searches to confirm no active references to old paths remain outside archived history or explicit migration notes.
5. Configure/build and run focused component test validation, then run strict OpenSpec validation.

Rollback is a filesystem rename back to `src/view` and `tests/views` plus reverting the mechanical reference updates.
