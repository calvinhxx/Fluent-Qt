## Context

The reusable component library has already moved from `src/view/` to `src/components/`, and project-owned includes now use the `components/...` prefix. The remaining public API still uses `view::...`, which keeps the old layer name in the C++ type system and in OpenSpec component contracts.

The current namespace surface is broad: component declarations, cross-component forward declarations, fully qualified type references, tests, VisualCheck helpers, docs, live specs, `Q_DECLARE_METATYPE`, `Q_DECLARE_OPERATORS_FOR_FLAGS`, and `qRegisterMetaType(...)` string names all reference `view::...`. Foundation is split as well: `QMLPlus`, `AnchorLayout`, and overlay helpers live in `view`, while `FluentElement` is a global public class.

Because this project is still early and the user explicitly prefers a strong architecture over compatibility, this design treats the migration as a breaking API cleanup rather than a staged compatibility bridge.

## Goals / Non-Goals

**Goals:**

- Make `fluent` the only canonical public namespace for reusable component APIs.
- Move component family namespaces from `view::<category>` to `fluent::<category>`.
- Move component foundation public types into `fluent`, including `FluentElement`, `QMLPlus`, `AnchorLayout`, property binding/state helpers, and `overlay` helpers.
- Remove active `view::...` component API references from project-owned source, tests, docs, agent guidance, and live OpenSpec specs.
- Preserve component behavior, source layout, include paths, class names, properties, signals, enum names, CTest labels, and VisualCheck contracts.
- Make Qt reflection names and metatype declarations match the new canonical namespace.

**Non-Goals:**

- Do not move files or change canonical include paths away from `components/...`.
- Do not redesign component ownership, rendering, layout, interaction, overlay, or open-state semantics.
- Do not add compatibility namespace aliases such as `namespace view = fluent` or `namespace view::basicinput { using Button = ...; }`.
- Do not migrate non-component support namespaces such as `compatibility` or `utils::logging`.
- Do not consolidate design token namespaces such as `Typography`, `ThemeColors`, `Spacing`, or `Animation`; that can be proposed separately if desired.

## Decisions

### Decision 1: Use `fluent` as the canonical component API namespace

All reusable component categories move from `view::<category>` to `fluent::<category>`:

```cpp
fluent::basicinput::Button
fluent::collections::ListView
fluent::dialogs_flyouts::Popup
fluent::navigation::NavigationView
fluent::textfields::Label
```

The name matches the library domain better than `view`, and it avoids colliding conceptually with root-level `app/view` Gallery code. Keeping `view` after the source tree migration would leave the architecture half-renamed.

Alternative considered: keep `view::...` and rely on `components/...` include paths for clarity. This was rejected because public C++ names are part of the API and remain visible to every caller.

### Decision 2: Move foundation public types into `fluent`

`QMLPlus`, `AnchorLayout`, `PropertyBinder`, `PropertyLink`, `QMLState`, `PropertyChange`, and `overlay` helpers currently live under `view`, while `FluentElement` is global. The migration should make these foundation types explicit component infrastructure:

```cpp
fluent::FluentElement
fluent::QMLPlus
fluent::AnchorLayout
fluent::overlay::visibleCardRect(...)
```

This makes component inheritance and helper ownership coherent. Inside `namespace fluent::<category>`, component declarations may spell base classes as `public FluentElement, public QMLPlus` because parent-namespace lookup resolves them to `fluent::FluentElement` and `fluent::QMLPlus`.

Alternative considered: only rename existing `view::...` symbols and leave `FluentElement` global. This would reduce churn but keep one of the most important foundation types outside the component namespace.

### Decision 3: Do not keep `view` compatibility aliases

The implementation should not add namespace aliases, type aliases, or forwarding declarations for `view::...`. Searches for active `view::` component API references should be expected to go to zero outside archived history and explicit migration notes.

Alternative considered: introduce `namespace view = fluent` or per-type aliases. That would soften downstream migration but preserve the ambiguous public surface and make Qt metatype naming harder to reason about.

### Decision 4: Keep include paths and build topology unchanged

Headers remain available through `components/...`, and `fluent_qt_lib` remains the reusable component library target. This keeps the change focused on public C++ spelling and avoids mixing namespace churn with source layout churn.

The recursive `src/` source discovery should continue to include components, design tokens, compatibility helpers, utilities, and resources, while excluding root-level `app/` code.

### Decision 5: Treat Qt reflection names as part of the migration

Qt metatype declarations and runtime registrations should use the new canonical types and names:

```cpp
Q_DECLARE_METATYPE(fluent::navigation::NavigationView::DisplayMode)
qRegisterMetaType<fluent::navigation::NavigationView::DisplayMode>(
    "fluent::navigation::NavigationView::DisplayMode");
```

Tests that assert inheritance, meta-enum availability, signal delivery, or registered type names should be updated to `fluent::...`. Leaving string names as `view::...` would make the compiled C++ API and Qt reflection surface disagree.

### Decision 6: Synchronize live specs and docs, not archived history

Live specs under `openspec/specs/`, development docs, README, and agent guidance should describe `fluent::...`. Archived OpenSpec changes remain historical records and can keep `view::...` references unless copied into active guidance.

## Risks / Trade-offs

- [Risk] Broad namespace churn can miss a declaration, forward declaration, metatype macro, or string registration. -> Mitigation: use repeated focused searches for `namespace view`, `view::`, `::view::`, `using namespace view`, `using view::`, and `Q_DECLARE_.*view::`, excluding archived history.
- [Risk] Moving `FluentElement` into `fluent` can expose unqualified-name assumptions in tests or support code. -> Mitigation: compile `fluent_qt_lib` first, then build focused foundation and component tests before broadening validation.
- [Risk] Qt moc or metatype names may drift if declarations and registration strings are not migrated together. -> Mitigation: treat `Q_DECLARE_METATYPE`, `Q_DECLARE_OPERATORS_FOR_FLAGS`, `qRegisterMetaType`, and meta-enum tests as an explicit task group.
- [Risk] Removing `view::...` compatibility breaks downstream code immediately. -> Mitigation: document the change as breaking and provide a clear mechanical migration rule: `view::` to `fluent::`, plus `FluentElement` to `fluent::FluentElement` when qualified.
- [Risk] Docs and specs can become inconsistent with code after a mechanical rename. -> Mitigation: validate OpenSpec strictly and search live docs/specs for stale namespace references before completion.

## Migration Plan

1. Inventory active namespace references in source, tests, docs, and live specs, excluding archived OpenSpec history.
2. Move foundation declarations and definitions from `view`/global into `fluent`.
3. Move each component category namespace from `view::<category>` to `fluent::<category>`.
4. Update cross-component references, forward declarations, using declarations, inheritance checks, and helper calls.
5. Update Qt reflection declarations and registration strings to `fluent::...`.
6. Update tests and VisualCheck code without changing test target names, labels, skip behavior, or direct binary execution paths.
7. Update live docs, agent guidance, and active OpenSpec specs to `fluent::...`.
8. Validate with namespace searches, OpenSpec strict validation, configure/build, representative focused test targets, and focused CTest runs.

Rollback is a repository revert of this change. Because no compatibility layer is introduced, there is no runtime rollout path; correctness is established through compile-time and test validation before merge.

## Open Questions

- Should design token namespaces such as `Typography`, `Spacing`, and `ThemeColors` eventually move under `fluent::design`? This design intentionally leaves that for a separate proposal so component namespace migration stays reviewable.