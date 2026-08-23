---
name: build-fluentqt-gui
description: Analyze existing or greenfield projects and architect, art-direct, build, performance-test, and independently review distinctive FluentQt C++ or PySide6 desktop GUIs. Use when adding a GUI to a library, CLI, TUI, service, plugin, data tool, or project with no current interface; when creating or redesigning a FluentQt application; when deriving a subject-grounded visual identity, editing concise product copy, generating or refining application icons, expanding brand assets into semantic Light/Dark themes, exploring three high-fidelity concepts, and preserving an approved concept in production code; when establishing a maintainable application structure, choosing FluentQt components and model/view architecture, installing Mica/Acrylic window material and revealed layer hierarchy, finishing a conversation, run timeline, document canvas, or composer, or correcting density, alignment, wrapping, dynamic height, scrolling, transient lifetime, responsiveness, and visual detail to Gallery-equivalent quality.
---

# Build a Polished FluentQt GUI

Build from target-project evidence and public FluentQt contracts. Deliver a
working vertical slice and an intentional visual system; do not stop when the
first window compiles.

For a new GUI or major redesign, do not start production UI code from a recipe
or wireframe. First define a visual world, produce three comparable
high-fidelity full-window concepts using the same real content, and obtain a
recorded human selection. A pending or rejected direction is an implementation
stop.

Follow [Design intelligence](references/design-intelligence.md) to ground the
direction in the product's materials, artifacts, instruments, verbs, and tempo;
spend one justified aesthetic risk; critique generic defaults; expose global
tuning axes; and extract a compact implementation design system from the human
pick. The result should be recognizable without its logo or accent color.

Treat visual quality and engineering quality as co-equal, independent release
gates. Component semantics, hierarchy, density, alignment, and polish must pass
with model/view architecture, responsiveness, bounded memory, asynchronous
behavior, and object lifetime. A lighter workflow never means a lower bar.
For full-profile work, a validated project architecture manifest is the
engineering counterpart to the validated design brief; neither can substitute
for the other.

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
| New application, full profile, new standalone GUI root, or an oversized shell | [Project architecture](references/project-architecture.md) |
| New top-level GUI, application-owned shell, or window chrome | [Premium shell](references/premium-shell.md) |
| Conversation, run timeline, document/object canvas, composer, sparse state, or pane chrome | [Signature surface](references/signature-surface.md) |
| Any visible layout, density, typography, or interaction change | [Visual refinement](references/visual-refinement.md) |
| Any user-visible label, status, empty/error text, demo fixture, localization work, or “AI-sounding” copy feedback | [Product copy](references/product-copy.md) |
| New/replaced icons, icon-only actions, mixed icon sources, or optical-alignment review | [Iconography](references/iconography.md) |
| Adding/replacing controls, collections, navigation, or overlays | [Component selection](references/component-selection.md) |
| Custom theme, brand mapping, or visible raw Qt widget | [Theme system](references/theme-system.md) |
| Repeated/growing data, async work, caches, streams, or transient surfaces | [Performance and lifecycle](references/performance-lifecycle.md) |
| Full profile, product reference, new shell, or relevant prior GUIs | [Experience differentiation](references/experience-differentiation.md) and [Product reference patterns](references/product-reference-patterns.md) |
| New GUI, major redesign, distinctive visual direction, taste feedback, concept rendering, or implementation fidelity | [Design intelligence](references/design-intelligence.md) |
| New GUI, major redesign, visual concept generation, or user taste decision | [Art direction and human selection](references/art-direction.md) |
| Full evidence matrix, dynamic layout, or transient/scroll acceptance | [Visual evidence contract](references/visual-evidence-contract.md) |
| Evaluating this Skill across Codex, Claude Code, and Cursor | [Cross-agent benchmark](references/cross-agent-benchmark.md) |

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

Resolve `<onboarding>` before editing the target project. Use
`<skill-root>/tools/onboarding/fluentqt` from the installable Skill, or
`<FluentQt-root>/tools/onboarding/fluentqt` from a checkout, source package, or
development package. Then run its read-only consumer preflight:

```bash
python3 <onboarding> doctor --profile cpp --format json
```

Resolve blocking findings before scaffolding. A missing optional tool or a
doctor warning is evidence to record, not permission to rewrite the target's
build system.

For a greenfield project, or when the target has no stable application
structure yet, start from a maintained starter instead of creating a flat
shell by hand:

```bash
python3 <onboarding> create /path/to/new-app \
  --language cpp --starter workbench --format json
```

Use `existing-qt` for a bounded integration slice and `pyside6` only when the
chosen delivery path is Python. Preserve the generated architecture manifest;
product-specific composition and art direction still require the workflow
below.

For a new GUI or major redesign, initialize one of the bundled composition
recipes instead of inventing the first shell from scratch:

```bash
python3 <skill-root>/scripts/init_design_brief.py \
  --application "Product name" --recipe agent-run --profile full \
  --author-id "implementation-agent-id" --output /path/to/design-brief.json
```

Available recipe ids live in `assets/composition-recipes.json`: `agent-run`,
`data-console`, `document-workbench`, and `focused-utility`. They provide three
different region topologies, density defaults, and failure risks. Treat them as
starting compositions, then replace identity, reference, art direction,
scoring, iconography, copy, theme, and component decisions with target-project
evidence.
Create the declared comp files before rendering the board. Do not ship the
generated placeholders or select the first concept without a human decision.
Use any available design-capable renderer for exploration. If image generation
is available, use it to broaden atmosphere and composition, then resolve real
copy, native control anatomy, and interaction semantics in a readable comp.
Do not make one vendor tool a requirement and never ship a screenshot as UI.

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

For a new application or full-profile GUI, follow
[Project architecture](references/project-architecture.md) and initialize a
language-appropriate structure from
[`assets/project-structure-templates.json`](assets/project-structure-templates.json)
before production UI code. For an existing GUI, map and migrate the real files
rather than generating parallel placeholders:

```bash
python3 <skill-root>/scripts/init_project_structure.py \
  --project-root /path/to/app \
  --application "Product name" \
  --language cpp \
  --profile full \
  --source-root src \
  --tests-root tests
```

Fill `.fluentqt/architecture.json` with the real shell files and any narrowly
justified compatibility exception. A full-profile implementation cannot pass
while its production files remain flat under the source root or while one
window owns unrelated UI, workflow, infrastructure, settings, and demo state.

## Define and validate the experience

Keep the design brief task-local unless it is useful project documentation.
For a new GUI or major redesign, follow
[Art direction and human selection](references/art-direction.md). Fill the
brief from repository and user-taste evidence, create three same-content
high-fidelity comps, render the comparison board, and validate concept
readiness:

```bash
python3 <skill-root>/scripts/render_design_board.py \
  /path/to/design-brief.json --output /path/to/design-board.svg
python3 <skill-root>/scripts/validate_design_brief.py \
  --stage concepts /path/to/design-brief.json
```

`CONCEPTS READY` authorizes only human review. Present the board and raw comps;
record the human's approval or rejection in the brief. Then run the validator
without `--stage`. Only its default `PASS` authorizes implementation. It rejects
placeholders, generic primary objects, duplicate full-profile topologies,
near-duplicate visual directions, missing or incomparable high-fidelity comps,
unscored concepts, an incomplete or mixed icon system, incomplete brand/copy
policies, non-4-px density, missing board evidence, and agent self-selection.
Passing records an explicit chosen direction; final pixels still require
built-app review.

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
4. taste context plus subject materials, artifacts, instruments, verbs, tempo,
   and recent unrelated patterns that must not recur;
5. a specific visual world, signature element, typography/palette/motion voice,
   one justified aesthetic risk with a usability guard, anti-goals, and one
   representative-content fixture;
6. one licensed/provenanced icon family with an explicit reuse-or-generation
   decision, application-tile and in-product-mark strategies, small-size
   evidence, and size/state/color/accessibility policies from
   [Iconography](references/iconography.md);
7. exactly three structurally and aesthetically distinct concepts, each with a
   same-content high-fidelity full-window comp and a nine-dimension visual
   scorecard;
8. a genericity critique, one concrete revision, and six global tuning axes;
9. human-selected concept plus concrete reasons the alternatives lose;
10. an implementation spec extracted from the selected comp: locked decisions,
    allowed adaptations, state grammar, responsive rules, and comparison regions;
11. named project/Gallery/`VisualCheck` evidence for every major surface;
12. surface hierarchy and Light/Dark theme strategy, including the approved
    identity asset or other evidence used to derive the semantic palette;
13. an action inventory assigning each user-visible operation one owning region,
    one primary entry, and any mutually exclusive responsive fallback;
14. semantic component-opportunity scan and decision table;
15. normal, narrow, and minimum layouts plus applicable loading, empty, error,
    permission, cancellation, and teardown;
16. exact density metrics: chrome slot, insets, gaps, control/row/footer
    heights, icon size, typography, wrap/elide, and overflow owner;
17. state-by-region evidence matrix from
    [Visual evidence contract](references/visual-evidence-contract.md);
18. data/lifetime table for repeated and transient surfaces from
    [Performance and lifecycle](references/performance-lifecycle.md), including
    separate organization, retention, and execution-admission contracts for
    runnable collections;
19. a product-copy policy and final string inventory following
    [Product copy](references/product-copy.md), including shared state terms,
    compression rules, and localization behavior.

Do not default a CLI, TUI, service, or coding tool to the same persistent
navigation/session/chat/inspector skeleton. If the identity card names a run
or conversation as the primary object, finish that transcript with
[Signature surface](references/signature-surface.md) instead of demoting it
to a labeled log. When the user asks to resemble another product, transfer
hierarchy, density, spacing, panel lifetime, and semantic color roles—not
marks, assets, exact colors, copy, or screenshot geometry.

## Implement one complete vertical slice

1. For full new or redesigned GUI work, rerun the default design-brief validator
   and stop unless human approval is `PASS`. Treat the selected comp as the
   intended hierarchy and its extracted implementation spec as the design
   system; record rather than silently inventing material changes.
2. Keep domain/application behavior, integration adapter, view state, and
   FluentQt view as distinct responsibilities. Apply
   [Project architecture](references/project-architecture.md); a shell split
   across several implementation files still fails if it remains one God
   object.
3. Build one end-to-end workflow before broad navigation. A generic shell
   around placeholder content, or a Mica window around labeled log rows, is
   not a vertical slice.
4. Keep blocking work off the GUI thread. Define ownership, progress,
   cancellation, retry, teardown, and stale-result handling. For runnable
   collections, keep task/session organization and retention separate from
   active execution capacity; overflow queues visibly instead of blocking
   creation or grouping.
5. Inventory user-visible operations before placing controls. Give each action
   one owning region and one primary entry in a visible layout. Responsive
   fallbacks must be mutually exclusive; a simultaneously visible secondary
   entry requires a distinct context or consequence, not merely the same slot
   or handler in another region.
6. Select components by behavior, lifetime, data shape, interaction, and
   density. Verify the public header/Python import, Gallery construction, and
   focused test rather than trusting a short catalog result alone.
7. Prefer FluentQt components and semantic tokens. Keep visible raw Qt widgets
   behind small theme-aware adapters when no public component fits.
8. Use one licensed/provenanced icon family and one semantic action-to-icon
   mapping. Follow [Iconography](references/iconography.md); reuse an official
   mark when valid, otherwise generate and human-select distinct candidates,
   then optimize separate platform application tiles and transparent in-app
   marks. Do not scatter emoji, Unicode glyphs, raster paths, or mixed packs
   through widgets.
9. Audit all product-owned visible strings with
   [Product copy](references/product-copy.md). Prefer user objects, states, and
   actions over assistant narration, promotional adjectives, repeated help,
   and raw protocol terms. Keep visible labels short while accessible names may
   remain explicit.
10. Install the theme before constructing an application-owned top-level
   window. Follow [Premium shell](references/premium-shell.md) only when the
   slice owns that shell; otherwise inherit the host surface. Follow
   [Signature surface](references/signature-surface.md) for the primary object,
   any primary input, and pane chrome. Centralize repeated shell metrics. Do
   not stamp `bgCanvas` / `bgLayer` onto every `QWidget`, wrap a composer in a
   `Card`, or use a filled `ComboBox` as a pane title.
11. Put mixed-size chrome items and peer pane footers in shared-height hosts;
   layout flags alone are not optical-alignment evidence.
12. Give dynamic text and collections an explicit wrap/elide, growth,
   min/max-size, overflow, and scroll-follow contract.
13. Use item models and delegates for long/growing collections. Avoid per-row
   widgets and persistent editors, update only affected rows, preserve a reader
   scroll anchor, and bound model, transport, and cache retention separately.
14. Construct one-shot surfaces on demand with guarded ownership and
    finish/close cleanup. Cache a repeated inspector only when state or measured
    recreation cost justifies it.

## Run the visual refinement loop

Both profiles must:

1. launch the actual rebuilt application with deterministic representative
   content and safe live/persisted content when relevant;
2. compare the touched controls or surfaces with named Gallery or
   `VisualCheck` evidence on the same platform, theme, scale, and font setup;
3. compare the full window and named high-risk detail directly with the
   accepted concept rather than reviewing the implementation in isolation;
4. inspect Light and Dark plus normal and narrow layouts;
5. inspect long/localized text, focus, disabled state, keyboard order, and
   window close/teardown that apply;
6. inspect the full window and native-resolution perimeter crops for touched
   title-bar, pane, selected-row, footer, and input regions;
7. measure painted gaps, baselines, optical centers, text fit, and terminal
   content rather than trusting layout rectangles;
8. inspect the shared icon family at native scale for optical alignment,
   semantic color, state variants, icon-only clarity, and separate packaged
   application-icon versus in-product-mark behavior;
9. inspect product-owned visible copy for AI narration, duplicate explanation,
   unstable state vocabulary, awkward wrapping, and unnecessary text;
10. record concrete defects, fix them, rebuild, and recapture the same state.

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

After the implementation agent finishes the inspect-fix-recapture loop, render
one local review board and give the raw brief, board, and final build
to a human or a fresh independent agent. Do not pass the implementation
agent's diagnosis or desired verdict. The reviewer must score workflow fit,
product signature, hierarchy, density/typography, theme/material,
iconography, surface composition, responsiveness, and state polish. The
reviewer identity must differ from the implementation `author_id`; every score
must cite final-build evidence, and an open blocker or major finding fails
acceptance.

## Validate visual evidence

New work uses contract v4. Initialize it only after the design brief passes:

```bash
python3 <skill-root>/scripts/init_visual_evidence.py \
  --brief /path/to/design-brief.json \
  --reviewed-build /path/to/final/executable \
  --platform "platform, scale, Qt, build type" \
  --output /path/to/visual-evidence.json
```

Replace every generated failure/default with measured final-build evidence.
Add one or more local reference images for a full review, then render the board:

```bash
python3 <skill-root>/scripts/render_visual_review.py \
  /path/to/visual-evidence.json --output /path/to/visual-review.html
```

The default HTML links the local full-resolution captures and stays small. Add
`--embed-images` only when the board must be a single portable file; large
evidence sets can otherwise produce very large HTML.

After an independent reviewer records scores, notes, evidence, findings, and a
passing verdict, run:

```bash
python3 <skill-root>/scripts/validate_visual_evidence.py \
  --require-current \
  /path/to/visual-evidence.json
```

Lite validates a compact invariant set. Full validates the complete state,
region, dynamic-convergence, local-reference, and review matrix. Contract v4
also requires the validated design brief and a different reviewer with nine
evidence-backed scores of at least 4/5, including iconography and surface
composition. The script still cannot infer taste
from pixels; the independent board review supplies that judgment. Declaring
`wireframe`, `filled-stickers`, `dead-space`, or `developer-labeled` fails. A
missing, failed, or unverified mandatory entry blocks acceptance. Legacy
v1/v2/v3 manifests remain readable with a warning, but `--require-current`
rejects them for new work.

## Validate engineering and report

Run target tests, adapter/view-state tests, focused FluentQt tests when the
library changes, the supported build/package flow, and profile-appropriate
stress tests. For growing collections, prove targeted model signals,
viewport-bounded materialization, scroll-anchor behavior, and a retention or
pagination contract. For one-shot surfaces, prove repeated close returns live
instances to baseline after deferred deletion.

For a new application, full-profile GUI, or architecture migration, validate
the actual source tree before declaring engineering acceptance:

```bash
python3 <skill-root>/scripts/validate_project_structure.py \
  --project-root /path/to/app --strict
```

Fix structural failures by extracting coherent ownership boundaries. Do not
raise budgets, add broad root-file exceptions, or mechanically create partial
source files to make the validator green.

For applications built with a FluentQt version that provides Inspector, run it
after the real window has completed layout. Generated C++ Workbench projects
provide a machine-readable entry point:

```bash
./build/<app-target> --quality-report > /path/to/quality-report.json
```

Other C++ applications include `<FluentQt/Diagnostics.h>` explicitly and can
serialize `fluent::diagnostics::Inspector::report(rootWidget)`. PySide6
applications use `fluentqt.inspect_widget(root_widget)`. Both routes call the
same native rules and return a versioned JSON-compatible report. Keep the report
with the final binary evidence. Treat every finding as a review prompt: resolve
it or record a scene-specific reason, but never raise a global budget to
manufacture a pass.
If the consuming FluentQt version has no Inspector, record that boundary rather
than copying its private heuristics into the application.

Inspector does not judge composition, hierarchy, brand fit, or taste. A clean
report therefore never replaces Light, Dark, narrow, long-copy, interaction,
and independent visual review.

Record visual and engineering results separately and require both to pass. Do
not label feasibility as verified support.

Before finishing, require:

- a project structure appropriate to the selected profile, with dependency
  direction and ownership recorded in `.fluentqt/architecture.json`;
- a top-level shell limited to composition, window/platform events, and
  responsive placement rather than process, protocol, settings, demo, and
  domain ownership;
- for full new-GUI or redesign work, a built surface traceable to the
  human-approved comp, with any material deviation explicitly re-decided;
- semantic components and tokens with no unjustified raw-widget substitute;
- Mica or Acrylic with revealed pane gaps for an application-owned top-level
  shell; otherwise a recorded host-owned surface contract;
- a finished signature surface: product copy, quiet chrome on material,
  composed sparse/empty canvas, and an integrated input when the workflow has
  one (or a recorded `none` when it does not);
- one clear primary action per region, no unexplained simultaneously visible
  duplicate entry for the same operation, and mutually exclusive responsive
  fallbacks;
- concise product-owned copy that uses one state vocabulary, exposes no raw
  protocol labels, and does not narrate what the assistant intends to do;
- no layout dependent on one demo string or one window size;
- no unbounded collection implemented as a child-widget stack;
- no full-model rebuild for an append or streaming token;
- bounded model/transport/cache retention where growth is possible;
- task/session grouping, retention, and execution capacity are separate; a
  runtime slot limit never masquerades as a navigation or creation limit;
- guarded, lazy transient ownership with tested cleanup;
- deliberate wrap/elide, dynamic height, and scroll-end behavior;
- consistent 4 px rhythm, panel insets, row cadence, shared chrome/footer
  centers, and separate indicator/icon/text slots;
- one coherent, licensed icon family with stable optical size, semantic color,
  accessible icon-only actions, verified Light/Dark states, a complete
  platform application tile, and a separate crisp in-product mark;
- an evidence-derived semantic palette that relates to the approved identity
  asset without painting every control or status in brand color;
- deliberate surface composition: material, panes, cards, borders, radius, and
  empty space express hierarchy without opaque-card proliferation;
- no stale palette, clipped popup, hidden focus cue, or unreadable final row;
- evidence from the final rebuilt binary rather than stale captures;
- a versioned Inspector report from the settled final surface, or a recorded
  compatibility reason when the consuming FluentQt version cannot provide it;
- a product signature that is not a relabeled
  navigation/session/chat/inspector template.
- one controlled aesthetic risk grounded in the subject, with everything else
  edited and restrained; no generic fallback silently replacing it.

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

When changing this Skill's composition or review behavior, use
`assets/benchmarks/agent-run-workspace.json` as the first blind forward test.
Run the same packaged Skill in each target agent without leaking the intended
layout or prior diagnosis. Follow
[Cross-agent benchmark](references/cross-agent-benchmark.md), initialize and
validate each `assets/benchmarks/agent-run.schema.json` record with
`scripts/benchmark_run.py`, and compare judged artifacts rather than prose
similarity.
