---
name: build-fluentqt-gui
description: Create, integrate, redesign, or fix C++ and PySide6 GUIs using FluentQt, including component and Gallery work. Use for native FluentQt interfaces, not unrelated Qt/QML or documentation-only tasks.
---

# Build a GUI with FluentQt

Deliver the requested interface through implementation, build, launch, review,
and fixes. Keep an audit or advice request within that scope. Read the target's
instructions, owning code, and reference sections needed for the current
decision. `<skill-root>` is the directory containing this file.

## Choose the task

| Mode | Task | Start with |
|---|---|---|
| `create` | New standalone C++ or PySide6 application | New or redesigned interfaces below |
| `integrate` | Add a GUI to an existing project | Integration boundary below, then the new-interface workflow |
| `improve` | Repair or redesign an existing FluentQt application | Existing interfaces below; use the design workflow for a major redesign |
| `maintain-library` | Change FluentQt components, Gallery, bindings, or GUI verification | [Maintainer gates](references/fluentqt-maintainer-gates.md) and the owning repository contract |

Library fixes use repository tests and GUI evidence. They do not require
consumer design briefs, architecture manifests, or lite/full profiles.
Documentation, catalog, and CI maintenance can use the maintainer dispatcher
directly without entering a GUI workflow.

## Existing interfaces

Reproduce the issue, preserve the accepted design, implement the fix, and
verify the affected behavior. Reuse valid project fixtures and evidence.
Create planning files only for decisions the project needs to retain.

Async work, growing collections, and transient lifetime add engineering
checks; they do not by themselves require new concepts or human design
selection. Use the applicable references below.

## New or redesigned interfaces

For a new GUI or major redesign, follow [Art direction](references/art-direction.md):
produce three high-fidelity full-window concepts with the same representative
content, obtain a human selection, and pass the design-brief gate before
production UI implementation. Honor an existing recorded selection; ask
again only for a material departure. [Design intelligence](references/design-intelligence.md)
supports product-specific visual decisions.

Build one complete user workflow before expanding navigation. Use
[Project architecture](references/project-architecture.md) for a new application
or architecture migration and [Premium shell](references/premium-shell.md) for
an application-owned window. Compare component finish with the Gallery and
product composition with the selected concept.

## Integration boundary

Use the [integration patterns](references/project-architecture.md#choose-the-integration-boundary)
to select a reusable API, service, process, or host extension from code evidence.
Preserve existing entry points unless replacement is requested. A host-owned
integration returns an embedded surface and preserves its host's window,
material, event loop, and unload contract.

## Find components and bootstrap

Query small slices of the bundled [catalog](assets/fluentqt-ai-catalog.json)
with [query_catalog.py](scripts/query_catalog.py), from any directory:

```bash
python3 <skill-root>/scripts/query_catalog.py --search "user intent"
```

Use `--component component-id` or `--pattern pattern-id` for a known candidate.
Confirm the public header or Python import, Gallery example, and focused test;
private source classes are not consumer APIs. For a new setup, use the
[starter preflight and creation tools](references/polished-starter.md#prepare-a-new-environment).
Reuse a working environment for ordinary edits.

## Read details when needed

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
| Formal consumer acceptance and lite/full evidence coverage | [Visual evidence contract](references/visual-evidence-contract.md) |

## Validate and finish

Build and run the changed surface. Run focused behavior tests and applicable
runtime checks; expand only for changed dependencies, failures, or required
project gates. For visible changes, inspect Light/Dark, normal/narrow layouts,
relevant text and input states, and teardown. Fix findings and review the
rebuilt result. Skipped tests and offscreen captures are not native approval.

New and redesigned applications require the architecture checks and
[final-build visual evidence](references/visual-evidence-contract.md), including
Inspector when supported and a human or fresh independent reviewer distinct
from the implementation author. The linked contracts own the tools and fields.

Report the runnable path, implemented behavior, validation and visual evidence,
and unverified platform or packaging boundaries. Complete authorized local
work without repeated permission requests. Use existing session authorization
for commits, publication, or other external actions; otherwise leave them for
an explicit request.

## Evaluating this Skill

Use the [cross-agent benchmark](references/cross-agent-benchmark.md) when
evaluating composition or review quality. Package validation alone does not
establish that a model produces better interfaces.
