---
name: build-fluentqt-gui
description: Create, integrate, redesign, or fix C++ and PySide6 GUIs using FluentQt, including FluentQt component and Gallery work. Use for native FluentQt interfaces, not unrelated Qt/QML or documentation-only tasks.
---

# Build a GUI with FluentQt

Deliver the requested interface through implementation, build, launch, review,
and fixes. For an audit or advice request, keep that requested scope. Start
with the target's instructions and the files needed for the current decision;
follow dependencies as evidence requires. Do not preload this entire Skill or
the target repository.

## Choose the task

| Mode | Task | Start with |
|---|---|---|
| `create` | New standalone C++ or PySide6 application | New or redesigned interfaces below |
| `integrate` | Add a GUI to a CLI, library, service, plugin, or data tool | Integration boundary below, then the new-interface workflow |
| `improve` | Repair or redesign an existing FluentQt application | Existing interfaces below; use the new-interface workflow for a major redesign |
| `maintain-library` | Change FluentQt components, Gallery, bindings, or GUI verification | [FluentQt maintainer gates](references/fluentqt-maintainer-gates.md) and the owning repository contract |

Maintainer mode uses repository tests and GUI evidence. Consumer design briefs,
application architecture manifests, and Skill visual contract v4 are not
prerequisites for a library fix. Documentation, catalog, and CI maintenance
can use the maintainer dispatcher directly without entering a GUI workflow.

## Existing interfaces

For a focused change, reproduce the issue, identify the owning component or
adapter and its tests, preserve the accepted design, implement the fix, and
verify the affected behavior. Inspect the rebuilt UI when appearance or
interaction changes. Reuse valid project evidence and fixtures; create new
planning files only when they carry a decision the project needs to retain.

Async work, growing collections, and transient lifetime require deeper
engineering checks. They do not by themselves require new visual concepts or
another human design selection. Read the applicable performance or visual
reference below. Escalate to the design workflow when the requested change
actually introduces a new interface or major redesign.

## New or redesigned interfaces

For a new GUI or major redesign, define the product's visual direction,
produce three high-fidelity full-window concepts with the same representative
content, and obtain a human selection before production UI implementation.
Follow [Art direction](references/art-direction.md) and
[Design intelligence](references/design-intelligence.md). Honor an existing
recorded selection; ask again only for a material departure from it.

Design artifacts use `full` for a new GUI or major redesign. `lite` is available
for a bounded single-surface design review without a new shell, integration
boundary, growing collection, async operation, or transient lifetime. These
profiles describe artifact coverage, not a mandatory classification for every
repair. Neither reduces the correctness or visual quality bar.

Use [Project architecture](references/project-architecture.md) for a new
application or architecture migration. Its
[structure templates](assets/project-structure-templates.json) and
[initializer](scripts/init_project_structure.py) record real responsibilities
and ownership in `.fluentqt/architecture.json`; do not create placeholder
layers just to match a directory tree.

Build one complete user workflow before expanding navigation. Keep domain
behavior, integration, view state, and widgets separate. Use the Gallery as
the component finish benchmark and the selected concept as the product design
reference. An application-owned shell uses the
[Premium shell](references/premium-shell.md); embedded surfaces preserve the
host's window, material, event loop, and unload contract.

## Integration boundary

Inspect the entry points, reusable application APIs, runtime, persistence,
and delivery constraints that the new GUI will touch. Select a pattern using
file-level evidence:

| Pattern | Reusable boundary |
|---|---|
| `direct-library` | Stable in-process application API |
| `service-api` | Existing authoritative service |
| `structured-process` | Executable with structured input, output, and errors |
| `plugin-extension` | Supported host extension point and lifecycle |
| `extract-core` | Small UI-independent service extracted from an existing interface |
| `greenfield` | New application use cases and state |

Preserve existing CLI, TUI, service, library, and host entry points unless their
replacement is requested. Apply the catalog's `window_ownership`: `host-owned`
returns an embedded surface, never a second application window or event loop.
If the boundary is uncertain, test a small adapter before composing the GUI.

## Find components and bootstrap

`<skill-root>` is the directory containing this file. Query small slices of
the bundled [catalog](assets/fluentqt-ai-catalog.json):

```bash
python3 <skill-root>/scripts/query_catalog.py --pattern direct-library
python3 <skill-root>/scripts/query_catalog.py --search "user intent"
python3 <skill-root>/scripts/query_catalog.py --component component-id
```

The query works from any directory. Use `--project-root /path/to/Fluent-QT` or
`--catalog /path/to/catalog.json` only to select a different source. Confirm
the chosen public header or Python import, Gallery example, and focused test;
private source classes are not consumer APIs.

For a new consumer setup, resolve `<onboarding>` to
`<skill-root>/tools/onboarding/fluentqt` in an installed Skill or
`<FluentQt-root>/tools/onboarding/fluentqt` in a checkout. Run its read-only
preflight before scaffolding:

```bash
python3 <onboarding> doctor --profile cpp --format json
python3 <onboarding> create /path/to/new-app --language cpp --starter workbench
```

Use `python` / `pyside6` for the Python profile/language and `existing-qt` for a
host-owned panel. Follow the [maintained starter](references/polished-starter.md).
Resolve blocking findings; a warning alone does not justify replacing the
target's build system. Reuse a working setup for ordinary edits.

## Read details when needed

Read the sections relevant to the task; these references own the detailed
contracts.

| Change or decision | Reference |
|---|---|
| Components, models, navigation, or overlays | [Component selection](references/component-selection.md) |
| Growing data, streams, async work, caches, cancellation, or transient ownership | [Performance and lifecycle](references/performance-lifecycle.md) |
| Layout, density, typography, resize, or interaction | [Visual refinement](references/visual-refinement.md) |
| Primary object, input, sparse canvas, or pane chrome | [Signature surface](references/signature-surface.md) |
| Visible labels, status, empty/error copy, or localization | [Product copy](references/product-copy.md) |
| Icons, identity assets, provenance, or icon-only actions | [Iconography](references/iconography.md) |
| Branding, palette, or raw Qt widget bridge | [Theme system](references/theme-system.md) |
| New product composition or major redesign | [Experience differentiation](references/experience-differentiation.md), [reference patterns](references/product-reference-patterns.md), [composition recipes](assets/composition-recipes.json) |
| Formal consumer visual acceptance | [Visual evidence contract](references/visual-evidence-contract.md) |

## Validate and finish

Build and run the changed surface. Run focused behavior tests and applicable
runtime checks; once they pass, expand coverage only for dependencies, failures,
or required project gates. For visible changes, inspect Light/Dark,
normal/narrow layouts, relevant text and input states, and teardown. Fix
findings, rebuild, and recapture the same state. Skipped tests and offscreen
captures are not native visual approval.

New and redesigned applications use the following tools at their stated
stages. The linked references define their fields and acceptance rules:

| Stage | Tools |
|---|---|
| Prepare concepts | [init_design_brief.py](scripts/init_design_brief.py), [render_design_board.py](scripts/render_design_board.py), [validate_design_brief.py](scripts/validate_design_brief.py) with `--stage concepts` |
| Selected design | Run `validate_design_brief.py` without `--stage`; require `PASS` before implementation |
| Actual application structure | [validate_project_structure.py](scripts/validate_project_structure.py) with `--project-root /path/to/app --strict` |
| Final rebuilt UI | [init_visual_evidence.py](scripts/init_visual_evidence.py), [render_visual_review.py](scripts/render_visual_review.py), [validate_visual_evidence.py](scripts/validate_visual_evidence.py) with `--require-current` |

`CONCEPTS READY` is not design approval. Contract v4 needs final-build captures
and a human or fresh independent reviewer, distinct from the implementation
author. Do not invent approval identities, fill default failures with assumed
passes, or relax tolerances to clear a gate. Keep engineering and visual
acceptance separate.

When the consuming FluentQt version provides Inspector, inspect the settled
window: generated Workbench apps expose `--quality-report`; other C++ apps use
`<FluentQt/Diagnostics.h>` and `fluent::diagnostics::Inspector::report(rootWidget)`;
Python uses `fluentqt.inspect_widget(root_widget)`. Resolve findings or explain
their scene-specific cause. Record unsupported versions; do not copy private
Inspector heuristics. A clean report does not judge visual composition.

Report the runnable path, implemented behavior, validation results, relevant
visual evidence, and unverified platform or packaging boundaries. Complete
authorized local work without repeatedly requesting permission. Use existing
session authorization for commits, publication, or other external actions;
otherwise leave those actions for an explicit request.

## Evaluating this Skill

Use the [cross-agent benchmark](references/cross-agent-benchmark.md) when
evaluating composition or review quality. Its fixed
[workspace prompt](assets/benchmarks/agent-run-workspace.json),
[run schema](assets/benchmarks/agent-run.schema.json), and
[benchmark_run.py](scripts/benchmark_run.py) keep inputs and final evidence
comparable. Package validation alone is not evidence that a model produces
better interfaces.
