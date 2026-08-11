---
name: build-fluentqt-gui
description: Analyze existing or greenfield projects and design, build, and visually refine validated FluentQt C++ or PySide6 desktop GUIs. Use when adding a GUI to a library, CLI, TUI, service, plugin, data tool, or project with no current interface; when creating a new FluentQt application; when choosing FluentQt components; when defining brand-aware Light/Dark themes; or when polishing layout, hierarchy, interaction states, responsiveness, and visual detail to Gallery-equivalent quality.
---

# Build a Polished FluentQt GUI

Build from target-project evidence and public FluentQt contracts. Deliver both a
working vertical slice and an intentional visual system. Do not assume the
project already has a TUI or CLI, and do not stop when the first functional
window compiles.

Treat the shipped FluentQt Gallery as the quality benchmark. Match its level of
finish rather than copying its information architecture: component fidelity,
semantic tokens, typography, 4 px spacing rhythm, radius, material, elevation,
icons, interaction states, theme behavior, and resize resilience must feel like
the same UI library.

## Load the contract

1. Locate the FluentQt root and target-project root. They may be the same
   checkout, siblings, or a consumer and installed dependency.
2. Read `../../../docs/ai/README.md` and
   `../../../docs/ai/add-gui-to-project.md` completely.
3. Read the target repository's agent instructions, build metadata, package
   metadata, tests, and supported-platform documentation.
4. Before composing visible UI, read these references completely:
   - [Theme system](references/theme-system.md)
   - [Component selection](references/component-selection.md)
   - [Visual refinement](references/visual-refinement.md)
5. Query only the catalog slices needed for the task:

```bash
python3 tools/ai/query_ai_catalog.py --pattern greenfield
python3 tools/ai/query_ai_catalog.py --search "user intent"
python3 tools/ai/query_ai_catalog.py --component component-id
python3 tools/ai/query_ai_catalog.py --guide navigation
```

Run these commands from the FluentQt root, or pass `--project-root` and
`--catalog` explicitly when using a packaged catalog.

## Analyze the integration boundary

Identify languages, build systems, entry points, domain modules, long-running
work, persistence, external I/O, platform requirements, and existing
interfaces. Define user outcomes before widgets.

Choose one primary integration pattern with file-level evidence:

- `direct-library` for stable in-process application APIs;
- `service-api` for an authoritative service boundary;
- `structured-process` for a mature executable with structured I/O;
- `plugin-extension` for an intentional host extension point;
- `extract-core` when behavior is trapped in an interface layer;
- `greenfield` when no reusable application layer exists.

Record why rejected alternatives are unsafe or disproportionate when they
change isolation, dependencies, packaging, or supported platforms. If evidence
is insufficient, build and test a narrow adapter spike before composing the
GUI.

## Define the experience before implementation

Create a concise task-local design brief. Do not commit it unless useful as
project documentation. Include:

1. the primary user workflow and required states;
2. visual evidence: existing product, project assets, official reference, or a
   neutral Fluent baseline, plus a named Gallery route, sample, or component
   `VisualCheck` for each major surface or control family;
3. surface hierarchy: canvas, layers, panels, overlays, and primary action;
4. Light/Dark theme strategy and any brand-token mapping;
5. a component decision table with intent, chosen component, rejected
   alternative, and catalog/API evidence;
6. normal, narrow, and minimum supported layouts;
7. loading, empty, error, permission, cancellation, and teardown behavior that
   applies;
8. one density and metric sheet naming the title-bar/toolbar slot, panel
   insets, group and section gaps, control and row heights, footer height, and
   icon size. Use exact Gallery metrics where available and the defaults in
   [Visual refinement](references/visual-refinement.md) otherwise.

When the user asks to resemble another product, derive hierarchy, density,
spacing, and semantic color roles. Do not copy protected marks or hard-code a
screenshot into the application.

## Implement one complete vertical slice

1. Preserve existing CLI, TUI, service, plugin, library, or GUI entry points
   unless replacement is explicit.
2. Keep domain/application behavior, integration adapter, view state, and
   FluentQt view as distinct responsibilities.
3. Build one end-to-end workflow before broad navigation.
4. Keep blocking work off the GUI thread. Define ownership, progress,
   cancellation, retries, teardown, and queued result delivery.
5. Select components by behavior and state semantics, not by superficial
   appearance. Verify the public C++ header or Python import and inspect the
   embedded sample and its actual construction in the linked Gallery source.
   For model/view controls, verify the model roles, delegate, row metrics,
   indicator ownership, and icon size used by the rendered reference; a short
   catalog snippet may omit surrounding sample setup.
6. Prefer FluentQt components and semantic tokens. Use raw Qt widgets only when
   FluentQt has no suitable public component, and make every visible raw widget
   respond to theme changes.
7. Install the selected theme before constructing the main window. Keep one
   accent hierarchy per region and avoid widget-local brand-color literals.
8. Centralize repeated shell metrics instead of scattering unrelated literals.
   Put mixed-size chrome items and peer pane footers in explicit shared-height
   hosts; layout flags alone are not visual-alignment evidence.

## Run the visual refinement loop

After the slice works, perform at least one implementation-review-fix cycle:

1. Launch the actual built application with deterministic representative data.
2. Open the selected Gallery or component reference on the same platform,
   theme, scale, and font setup, then compare the real application beside it.
3. Inspect the normal layout in Light and Dark themes.
4. Inspect a narrow layout near the responsive breakpoint and the minimum
   supported width.
5. Exercise primary actions, focus, hover/pressed where observable, disabled,
   selected, loading, empty, error, text composition/IME, and overlay states
   that apply.
6. Inspect the full window and 100% crops of the perimeter: title bar, pane
   headers, selected navigation rows, pane footers, and composer/input edge.
7. Run the geometry and layer gate in
   [Visual refinement](references/visual-refinement.md): measure the marked
   gaps and alignments, verify indicator/text separation, and confirm transient
   content paints above its owner without stale content beneath it.
8. Record a concise Gallery-parity review covering hierarchy, spacing,
   typography, color, radius, material, elevation, alignment, text fit, icon
   meaning, control density, and interaction states.
9. Fix the issues, rebuild, and capture the same views again.

Use available desktop automation or screenshot tooling; if interactive UI
control is unavailable, use deterministic snapshots and report the missing
interaction coverage. A single screenshot of the default state is not visual
validation.

## Validate and report

Run existing target tests, adapter/view-state tests, the supported build/package
flow, and visual review. Cover applicable success, empty, validation, failure,
cancellation, permission, and teardown states. Verify keyboard focus,
Light/Dark, resize, scaling, text fit, long content, and responsiveness.

Before finishing, ensure:

- every major visible surface or control family has a named Gallery, sample, or
  `VisualCheck` reference;
- theme changes update FluentQt and visible raw Qt controls;
- component choices have public API evidence and no unnecessary raw-widget
  substitute;
- primary, secondary, subtle, selected, and destructive actions are visually
  distinct;
- no layout depends on one demo string or one window size;
- compact desktop regions do not use oversized controls, headings, cards, or
  empty action containers without a semantic reason;
- peer chrome groups and same-edge pane footers share intentional heights,
  centers, and insets;
- no text, indicator, placeholder, focus cue, popup, or transient surface
  overlaps another element unintentionally;
- the full-window composition and 100% perimeter crops both pass review;
- required geometry measurements and important interaction states pass the
  visual acceptance gate rather than relying on an unmarked screenshot;
- the final side-by-side review shows no unexplained drop below Gallery quality
  in design-token fidelity, state completeness, or visual finish;
- visual review includes before/after evidence or a concise issue/fix record.

When modifying FluentQt's catalog, guidance, docs, or this Skill in a full
checkout, run:

```bash
python3 tools/ai/evaluate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
```

Report the integration pattern and evidence, preserved interfaces, design
brief, component decisions, theme strategy, implemented slice, exact validation
results, visual coverage, and unverified platform or packaging boundaries.
Distinguish feasibility from verified support.
