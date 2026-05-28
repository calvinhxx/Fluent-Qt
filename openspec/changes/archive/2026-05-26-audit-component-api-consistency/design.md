## Context

The project now contains a large Qt Widgets Fluent component surface under `src/view/**` with matching tests under `tests/views/**` and archived OpenSpec specs for many components. Recent work also clarified several architectural conventions:

- component layers should preserve ownership boundaries and keep application-owned content in tests or caller code;
- visible controls should prefer existing Fluent components where they fit;
- controls that are button-like should derive from the existing `view::basicinput::Button` rather than duplicating button state handling;
- test and VisualCheck windows should use shared Qt test setup and project Fluent components.

This change is cross-cutting because it audits many public headers and specs. It should improve consistency without turning into a broad redesign.

## Goals / Non-Goals

**Goals:**

- Establish a durable component API convention checklist for public Qt component APIs.
- Audit existing components against that checklist and record findings with severity and recommended action.
- Apply low-risk consistency fixes that do not remove public APIs or change ownership boundaries.
- Add or tighten tests for repeated setter no-op behavior, inheritance expectations, nullable value semantics, and open/close state behavior where gaps are found.
- Document follow-up items for any breaking or broad migration work.

**Non-Goals:**

- Rename large public API surfaces without compatibility aliases.
- Redesign component architecture or move application-owned composition back into component classes.
- Rewrite VisualCheck windows purely for style normalization unless they are directly touched by an API consistency fix.
- Introduce new runtime dependencies or code generators.
- Solve every style or visual issue in the component library.

## Decisions

### Audit first, then patch

The implementation SHALL start with an audit report rather than editing components immediately. The report should classify findings as:

- `Blocker`: breaks build, tests, or documented public contract.
- `High`: public API inconsistency likely to mislead users or AI-assisted maintenance.
- `Medium`: missing test coverage or naming inconsistency that can be fixed compatibly.
- `Low`: documentation, comments, or future cleanup.

Alternative considered: directly normalize public headers in one pass. That is too risky because public API changes can ripple through tests, specs, and user code.

### Compatibility over cosmetic purity

Low-risk fixes MAY add aliases, no-op setter tests, docs, and missing signal checks. Public method removal or semantic renames MUST be deferred unless compatibility is preserved.

Alternative considered: enforce one ideal naming scheme immediately. That would create churn in mature components and distract from the audit value.

### Use existing specs as the source of behavior truth

The audit should compare code, tests, and OpenSpec specs. If a component intentionally differs because its spec defines a different ownership or inheritance model, the audit should record that as intentional rather than force uniformity.

Alternative considered: infer conventions only from headers. That misses previous architecture decisions such as lightweight shell ownership in `NavigationView`.

### Keep test changes focused

When a finding requires test coverage, update the component's existing focused test file. Follow `.codex/skills/qt-ut-conventions/SKILL.md` for VisualCheck and shared Qt setup. Avoid creating a massive cross-component test binary unless repeated patterns prove it is useful.

Alternative considered: build one reflection-style API audit test that introspects every QObject. This can be useful later, but it would be brittle before conventions are agreed.

## Risks / Trade-offs

- [Risk] The audit becomes a subjective style cleanup list. -> Mitigation: tie each finding to a convention category, a component, and an explicit severity.
- [Risk] A consistency fix accidentally changes public behavior. -> Mitigation: prefer additive compatibility, keep focused tests close to the component, and run targeted ctest filters.
- [Risk] Existing specs and code disagree. -> Mitigation: document the mismatch and choose either a code/test fix or a follow-up spec change explicitly.
- [Risk] Scope grows across every component. -> Mitigation: finish with a bounded report and low-risk fixes; defer broad migrations into named follow-up changes.
- [Risk] Full validation is noisy due existing unrelated spec failures. -> Mitigation: validate this change strictly and identify unrelated failures separately.

## Migration Plan

1. Generate the initial audit report from headers, tests, and specs.
2. Review findings and mark intentional deviations.
3. Apply compatible fixes in small groups by component category.
4. Update focused tests and docs.
5. Run targeted builds/tests for touched components plus `openspec validate audit-component-api-consistency --strict`.
6. Record deferred breaking migrations as follow-up proposals instead of landing them silently.

Rollback is straightforward for low-risk patches: revert the affected component/test/doc files. The audit report can remain as documentation if code fixes are reverted.

## Open Questions

- Should the final audit report live under `docs/` or in `readme.md` as a shorter checklist plus a detailed linked document?
- Should compatibility aliases be kept indefinitely, or should future breaking migrations include a deprecation window?
- Should a later change add a small static checker for common API conventions once the checklist stabilizes?
