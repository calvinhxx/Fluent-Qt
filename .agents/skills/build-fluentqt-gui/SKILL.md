---
name: build-fluentqt-gui
description: Analyze existing or greenfield projects and design, build, performance-test, and visually refine validated FluentQt C++ or PySide6 desktop GUIs. Use when adding a GUI to a library, CLI, TUI, service, plugin, data tool, or project with no current interface; when creating a new FluentQt application; when choosing FluentQt components and model/view architecture; when defining brand-aware Light/Dark themes; when installing Mica/Acrylic window material and revealed layer hierarchy; when finishing a conversation, run timeline, document canvas, or composer so it is not a labeled log or opaque sticker on material; or when fixing density, alignment, wrapping, dynamic height, scrolling, transient lifetime, responsiveness, and visual detail to Gallery-equivalent quality.
---

# Build a Polished FluentQt GUI

Build from target-project evidence and public FluentQt contracts. Deliver a
working vertical slice and an intentional visual system; do not stop when the
first window compiles.

Treat visual quality and engineering quality as co-equal, independent release
gates. Component semantics, hierarchy, density, alignment, and polish must pass
with model/view architecture, responsiveness, bounded memory, asynchronous
behavior, and object lifetime. A lighter workflow never means a lower bar.

Use the shipped FluentQt Gallery as the finish benchmark, not as an
information-architecture template. Match its component fidelity, semantic
tokens, typography, 4 px spacing rhythm, radius, window material, icons,
interaction states, theme behavior, and resize resilience.

When the slice owns a top-level window, install the
[Premium shell](references/premium-shell.md) before composing product content:
`Window` + Mica, theme before construct, and pane gaps that reveal material.
When the GUI is embedded in a host-owned window, preserve that host's chrome,
material, lifecycle, and unload contract; do not create a second `Window`.
Finish the applicable [Signature surface](references/signature-surface.md): a
designed primary object, quiet chrome on its owning surface, intentional
sparse/empty states, and an integrated input only when the workflow has a
primary input. A flat opaque shell, labeled log, or white composer slab on
Mica is a failed first render, not a neutral starting point.

## Choose a proportional profile

Classify the requested slice before loading detailed references.

Use **lite** only when the work is either a focused correction with a bounded
blast radius or a small single-surface utility, and all of these are true:

- no new application shell or integration boundary;
- no long, growing, hierarchical, streamed, or server-owned collection;
- no new background process, network operation, cache, or cancellation path;
- no new dialog, flyout, drawer, popup, or other transient lifetime;
- no custom brand palette, raw-widget theme bridge, or cross-product redesign;
- normal, narrow, Light, Dark, focus, long-text, and close behavior can be
  validated directly.

Use **full** when any lite condition is false, when building a new GUI or major
surface, or when uncertainty remains. Record the selected profile and the
evidence for it. Reclassify to full as soon as implementation introduces a full
trigger.

Profiles scale discovery artifacts and evidence breadth only. Both require an
actual built application, one implementation-review-fix loop, visual and
engineering acceptance, no clipping or overlap, responsive layouts, and safe
teardown.

## Load only the applicable contract

1. Read the target repository's agent instructions, build/package metadata,
   tests, supported-platform documentation, and relevant entry points.
2. Use this routing table. Read every selected file completely; do not load
   unrelated references merely because they exist.

`<skill-root>` is the directory that contains this `SKILL.md`.

| Trigger | Required reading |
| --- | --- |
| New GUI, greenfield app, or changed integration boundary | The integration-boundary and vertical-slice sections in this Skill |
| New top-level GUI, application-owned shell, or window chrome | [Premium shell](references/premium-shell.md) |
| Conversation, run timeline, document/object canvas, composer, sparse state, or pane chrome | [Signature surface](references/signature-surface.md) |
| Any visible layout, density, typography, or interaction change | [Visual refinement](references/visual-refinement.md) |
| Adding/replacing controls, collections, navigation, or overlays | [Component selection](references/component-selection.md) |
| Custom theme, brand mapping, or visible raw Qt widget | [Theme system](references/theme-system.md) |
| Repeated/growing data, async work, caches, streams, or transient surfaces | [Performance and lifecycle](references/performance-lifecycle.md) |
| Full profile, product reference, new shell, or relevant prior GUIs | [Experience differentiation](references/experience-differentiation.md) and [Product reference patterns](references/product-reference-patterns.md) |
| Full evidence matrix, dynamic layout, or transient/scroll acceptance | [Visual evidence contract](references/visual-evidence-contract.md) |

Query only catalog slices needed for the current decision:

```bash
python3 <skill-root>/scripts/query_catalog.py --pattern greenfield
python3 <skill-root>/scripts/query_catalog.py --search "user intent"
python3 <skill-root>/scripts/query_catalog.py --component component-id
python3 <skill-root>/scripts/query_catalog.py --guide navigation
```

Run from any working directory. The script reads
`assets/fluentqt-ai-catalog.json`, the snapshot bundled with this Skill. Use
`--project-root /path/to/Fluent-QT` or
`--catalog /path/to/catalog.json` only when an explicit checkout or catalog
should override that snapshot.

## Analyze the integration boundary

For a new GUI or changed boundary, identify languages, build systems, entry
points, domain modules, long-running work, persistence, external I/O, platform
requirements, and existing interfaces. Define user outcomes before widgets.

Choose one primary pattern with file-level evidence:

- `direct-library` for stable in-process application APIs;
- `service-api` for an authoritative service boundary;
- `structured-process` for a mature executable with structured I/O;
- `plugin-extension` for an intentional host extension point;
- `extract-core` when behavior is trapped in an interface layer;
- `greenfield` when no reusable application layer exists.

Apply the catalog's `window_ownership` before application-pattern component
candidates. `host-owned` is a hard prohibition on creating a second
application `Window` or event loop; `caller-decides` requires target evidence.

Record why materially different alternatives are unsafe or disproportionate.
If evidence is insufficient, test a narrow adapter spike before composing the
GUI. Preserve existing CLI, TUI, service, plugin, library, or GUI entry points
unless replacement is explicit.

## Define the experience

Keep the design brief task-local unless it is useful project documentation.

### Lite brief

Record:

1. the user outcome, affected surface, and why lite is valid;
2. the primary object, hero interaction, and one selected composition;
3. one rejected alternative and one project/Gallery reference;
4. major component decisions plus exact density metrics for the touched region;
5. applicable normal, narrow, Light, Dark, focus, long-text, disabled, and
   close states.

State explicitly that data and child count are finite. If repeated data,
asynchronous work, or a transient surface appears, switch to full.

### Full brief

Record:

1. primary workflow and required states;
2. product-signature identity card: primary object, dominant time model, core
   outcome, hero interaction, signature surface, and supporting surfaces;
3. aligned and contrastive reference synthesis with transferred and rejected
   traits and excluded brand/screenshot copying;
4. at least three structurally distinct information-architecture concepts,
   comparison criteria, selected concept, and why the alternatives lose;
5. named project/Gallery/`VisualCheck` evidence for every major surface;
6. surface hierarchy and Light/Dark theme strategy;
7. semantic component-opportunity scan and decision table;
8. normal, narrow, and minimum layouts;
9. applicable loading, empty, error, permission, cancellation, and teardown;
10. exact density metrics: chrome slot, insets, gaps, control/row/footer
    heights, icon size, typography, wrap/elide, and overflow owner;
11. state-by-region evidence matrix from
    [Visual evidence contract](references/visual-evidence-contract.md);
12. data/lifetime table for repeated and transient surfaces from
    [Performance and lifecycle](references/performance-lifecycle.md).

Do not default a CLI, TUI, service, or coding tool to the same persistent
navigation/session/chat/inspector skeleton. If the identity card names a run
or conversation as the primary object, finish that transcript with
[Signature surface](references/signature-surface.md) instead of demoting it
to a labeled log. When the user asks to resemble another product, transfer
hierarchy, density, spacing, panel lifetime, and semantic color roles—not
marks, assets, exact colors, copy, or screenshot geometry.

## Implement one complete vertical slice

1. Keep domain/application behavior, integration adapter, view state, and
   FluentQt view as distinct responsibilities.
2. Build one end-to-end workflow before broad navigation. A generic shell
   around placeholder content, or a Mica window around labeled log rows, is
   not a vertical slice.
3. Keep blocking work off the GUI thread. Define ownership, progress,
   cancellation, retry, teardown, and stale-result handling.
4. Select components by behavior, lifetime, data shape, interaction, and
   density. Verify the public header/Python import, Gallery construction, and
   focused test rather than trusting a short catalog result alone.
5. Prefer FluentQt components and semantic tokens. Keep visible raw Qt widgets
   behind small theme-aware adapters when no public component fits.
6. Install the theme before constructing an application-owned top-level
   window. Follow [Premium shell](references/premium-shell.md) only when the
   slice owns that shell; otherwise inherit the host surface. Follow
   [Signature surface](references/signature-surface.md) for the primary object,
   any primary input, and pane chrome. Centralize repeated shell metrics. Do
   not stamp `bgCanvas` / `bgLayer` onto every `QWidget`, wrap a composer in a
   `Card`, or use a filled `ComboBox` as a pane title.
7. Put mixed-size chrome items and peer pane footers in shared-height hosts;
   layout flags alone are not optical-alignment evidence.
8. Give dynamic text and collections an explicit wrap/elide, growth,
   min/max-size, overflow, and scroll-follow contract.
9. Use item models and delegates for long/growing collections. Avoid per-row
   widgets and persistent editors, update only affected rows, preserve a reader
   scroll anchor, and bound model, transport, and cache retention separately.
10. Construct one-shot surfaces on demand with guarded ownership and
    finish/close cleanup. Cache a repeated inspector only when state or measured
    recreation cost justifies it.

## Run the visual refinement loop

Both profiles must:

1. launch the actual rebuilt application with deterministic representative
   content and safe live/persisted content when relevant;
2. compare the touched controls or surfaces with named Gallery or
   `VisualCheck` evidence on the same platform, theme, scale, and font setup;
3. inspect Light and Dark plus normal and narrow layouts;
4. inspect long/localized text, focus, disabled state, keyboard order, and
   window close/teardown that apply;
5. inspect the full window and native-resolution perimeter crops for touched
   title-bar, pane, selected-row, footer, and input regions;
6. measure painted gaps, baselines, optical centers, text fit, and terminal
   content rather than trusting layout rectangles;
7. record concrete defects, fix them, rebuild, and recapture the same state.

Full additionally exercises realistic sparse/dense cardinality, minimum width,
loading, empty, error, cancellation, permission, streaming/settled layout,
scroll end, maximum multiline input, IME, overlays, and lifetime stress that
apply.

When Computer Use is available, drive the live accessibility tree and combine a
full-window view with native-resolution detail crops or a compact
picture-in-picture board. A board supplements live control; it never replaces
interaction or pixel inspection. If automation cannot cover a state, report it
as unverified rather than passing it.

Any clipping, overlap, unreadable terminal row, unstable dynamic height, stale
overlay, wrong density, or repeated alignment defect blocks visual acceptance
even when the build and functional tests pass.

## Validate visual evidence

Keep a task-local v2 manifest with `"contract_version": 2` and
`"profile": "lite"` or `"profile": "full"`:

```bash
python3 <skill-root>/scripts/validate_visual_evidence.py \
  /path/to/visual-evidence.json
```

Lite validates a compact invariant set. Full validates the complete state,
region, and dynamic-convergence matrix. The script also requires the reviewed
build and evidence files to exist; it checks bookkeeping and the declared
material/signature fields, not pixel aesthetics. Inspect pixels and
interactions yourself. Declaring `wireframe`, `filled-stickers`,
`dead-space`, or `developer-labeled` fails. A missing, failed, or unverified
mandatory entry blocks acceptance. A legacy manifest without
`contract_version` remains readable as v1 with a migration warning; do not
create new v1 evidence.

## Validate engineering and report

Run target tests, adapter/view-state tests, focused FluentQt tests when the
library changes, the supported build/package flow, and profile-appropriate
stress tests. For growing collections, prove targeted model signals,
viewport-bounded materialization, scroll-anchor behavior, and a retention or
pagination contract. For one-shot surfaces, prove repeated close returns live
instances to baseline after deferred deletion.

Record visual and engineering results separately and require both to pass. Do
not label feasibility as verified support.

Before finishing, require:

- semantic components and tokens with no unjustified raw-widget substitute;
- Mica or Acrylic with revealed pane gaps for an application-owned top-level
  shell; otherwise a recorded host-owned surface contract;
- a finished signature surface: product copy, quiet chrome on material,
  composed sparse/empty canvas, and an integrated input when the workflow has
  one (or a recorded `none` when it does not);
- one clear primary action per region and restrained typography/density;
- no layout dependent on one demo string or one window size;
- no unbounded collection implemented as a child-widget stack;
- no full-model rebuild for an append or streaming token;
- bounded model/transport/cache retention where growth is possible;
- guarded, lazy transient ownership with tested cleanup;
- deliberate wrap/elide, dynamic height, and scroll-end behavior;
- consistent 4 px rhythm, panel insets, row cadence, shared chrome/footer
  centers, and separate indicator/icon/text slots;
- no stale palette, clipped popup, hidden focus cue, or unreadable final row;
- evidence from the final rebuilt binary rather than stale captures;
- a product signature that is not a relabeled
  navigation/session/chat/inspector template.

When modifying FluentQt's catalog, guidance, docs, or this Skill inside a full
FluentQt checkout, run from that checkout:

```bash
python3 tools/ai/evaluate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
```

Report the selected profile, integration pattern and evidence when applicable,
preserved interfaces, design/component/theme decisions, implemented slice,
exact validation results, visual coverage, and unverified platform or packaging
boundaries.
