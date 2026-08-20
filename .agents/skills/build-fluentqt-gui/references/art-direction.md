# Art Direction and Human Selection

Lock the visual direction before implementation so a technically correct
FluentQt shell does not become the default design. This gate applies to every
new GUI and major redesign. A focused correction using the lite profile may
keep its existing direction, but it must not use lite to disguise a new shell.

The initializer emits design-brief contract v4. Contracts v2/v3 remain readable
as legacy inputs, but they do not pass the current direct design gate for new
work.

## Contents

- Define the visual world
- Ground it in subject and taste evidence
- Define one icon system
- Use representative product content
- Produce three high-fidelity comps
- Critique genericity and expose tuning axes
- Score the visual system
- Require a human decision
- Extract and carry the implementation design system
- Art-direction acceptance gate

## Define the visual world

Start with product evidence and any taste signals supplied by the user: named
products, screenshots, brand material, adjectives, disliked patterns, or a
current build. Do not infer that “Fluent” means a generic Windows settings
screen.

Read [Design intelligence](design-intelligence.md). Record `taste_context` and
`subject_vernacular` before describing the visual world. The subject record
must name materials, artifacts, instruments, verbs, and tempo. The taste record
must separate explicit human/project evidence from inferred references and
name recent generic or repeated patterns that must not recur.

Record one `art_direction` object in the design brief:

- exactly three desired-impression words;
- a domain-specific visual world, expressed as a short scene or relationship
  rather than a style label such as “modern” or “premium”;
- one signature element recognizable without the logo or accent color;
- typography, palette, and motion voices;
- one `aesthetic_risk` with evidence, a quiet zone, usability guard, and
  fallback;
- at least three concrete anti-goals;
- one representative-content fixture shared by all concepts.

The visual world should make tradeoffs. “Precise, calm, and capable; like a
live technical notebook whose active run leaves a clear trace” is actionable.
“Clean modern Fluent UI” is not. Name what stays quiet as well as what receives
emphasis.

Use semantic palette roles that can map to Fluent Light and Dark tokens. A comp
may explore brand character, but it must not require hard-coded colors,
inaccessible contrast, a second design language, or opaque fills that erase the
window material.

## Define one icon system

Before composing concepts, define the shared icon family, provenance, source
grid, compact and standard glyph sizes, action slot, stroke character,
outline/filled behavior, semantic color policy, and icon-only accessibility
rule. Follow [Iconography](iconography.md). Concepts may use the system with
different emphasis, but may not win by switching to a more fashionable pack or
mixing unrelated families.

Record an `icon_strategy` for each visual direction. It should explain how the
shared family supports that concept's hierarchy and state model, not rename the
family. The high-fidelity comps must show representative navigation, routine
action, selected/active, status, and disabled treatment when those states
exist.

## Use representative product content

The content fixture prevents one concept from winning because it received
shorter copy, fewer rows, or a more flattering state. Derive it from repository
evidence or document why a synthetic fixture is representative. Give it a
stable id, one end-to-end scenario, at least three real strings or data values,
and at least two meaningful states.

Every full-profile comp must use that same fixture id, theme, viewport, product
moment, and content quantity. Preserve long labels, realistic status text,
errors, and mixed-height content that materially affect the composition. Do
not use lorem ipsum, control names, raw protocol labels, or empty rectangles.

## Produce three high-fidelity comps

Create three concepts before writing or rearranging production GUI code. The
bundled recipes provide different information architectures; they do not
provide the art direction. For each concept, define:

- a memorable direction name;
- composition character and reading rhythm;
- semantic palette strategy;
- typography strategy;
- icon strategy using the shared icon family;
- how subject vernacular becomes structure rather than decoration;
- one signature visual or interaction move;
- one controlled aesthetic risk and its usability guard;
- one restraint rule;
- one reason the direction may fail.

Then create one local high-fidelity comp file per concept. Use any available
design-capable workflow—Figma export, raster composition, SVG, or a code-native
static mockup—but the result must be a resolved desktop window, not a wireframe.
The comp must be at least 960 × 600 logical pixels and show:

- the complete normal-size window and its material/layer relationship;
- actual product copy and data from the shared fixture;
- resolved type scale, weight, spacing, palette, radius, icon treatment, and
  primary/secondary emphasis;
- the hero interaction and a meaningful active, completion, empty, or error
  state;
- enough content to expose density, wrapping, and hierarchy weaknesses.

A region diagram, component inventory, grayscale skeleton, or generated image
with unreadable pseudo-text does not count as a high-fidelity comp. A
design-capable image model may broaden atmosphere, composition, and material;
resolve the candidate in a second pass with readable real copy, native control
anatomy, and explicit interaction states. Do not ask it to invent product
claims or ship the concept image as UI.

When a full window makes a high-risk area unreadable, create a fresh resolved
detail or state comp at useful resolution. Do not treat a blurry crop as the
source of truth.

Full concepts already need different topologies. They must also differ pairwise
in at least three of composition character, palette strategy, typography
strategy, and signature move. Three color treatments of the same shell fail.

## Critique genericity and expose tuning axes

Before human review, record a `genericity_review` with the likely untuned
solution, concrete cliches detected in the concepts, at least one revision, and
why the revised signature remains recognizable without brand marks. Reject a
choice that could move unchanged into an unrelated product.

Record all six global `tuning_axes` from
[Design intelligence](design-intelligence.md), each with a 1–5 value and a
semantic note. These axes make feedback such as “less large,” “quieter,” or
“more tactile” update the whole direction instead of producing local patches.

## Score the visual system

Score every concept from 1–5 and attach a concrete note for each dimension:

- workflow fit;
- product signature;
- visual hierarchy;
- density and typography;
- theme and material;
- iconography;
- surface composition;
- responsive quality;
- state and interaction polish.

These scores expose tradeoffs for the human decision; they are not an automatic
winner function and do not all need to reach 4 at concept time. Judge the
actual comp at native size. `surface_composition` covers the relationship among
material, panes, cards, dividers, borders, radius, and empty space—not how many
containers were added. A score of 0, a placeholder note, or an omitted
dimension blocks `CONCEPTS READY`. The rendered board places all nine scores
under each comparable comp.

## Require a human decision

Render the shared comparison board after the comp files exist:

```bash
python3 <skill-root>/scripts/render_design_board.py \
  /path/to/design-brief.json --output /path/to/design-board.svg
python3 <skill-root>/scripts/validate_design_brief.py \
  --stage concepts /path/to/design-brief.json
```

Present the board in a neutral order with the raw comps available at full
resolution. Describe the product evidence and concept risks, but do not silently
select the first recipe or turn an agent preference into user approval. Ask the
user or named human design owner to select one concept, reject all concepts, or
request a revision.

Record the decision under `approval`:

- `status`: `pending`, `rejected`, or `approved`;
- `decision_maker_kind`: `human`;
- a non-secret `decision_maker_id` distinct from the implementation
  `author_id`;
- decision time, selected concept id, concrete selection reason, and concrete
  rejection reason for every other full-profile concept.

`pending` and `rejected` are valid review states, but they are implementation
stops. If all concepts are rejected, revise the visual world or produce new
concepts. Do not approve the least-disliked option.

After recording approval, run the default gate:

```bash
python3 <skill-root>/scripts/validate_design_brief.py \
  /path/to/design-brief.json
```

Only the default `PASS` result authorizes implementation. `CONCEPTS READY`
means the artifacts are ready for a human decision, not for C++ work.
Rerender the board after approval when the handoff artifact should visibly mark
the human-selected concept.

## Extract and carry the implementation design system

After approval and before production UI code, fill `implementation_spec` from
the selected comp. Record the source concept, container model, semantic tokens,
typography roles, component families, state grammar, motion cues, responsive
rules, locked decisions, allowed adaptations, known risks, and comparison
regions. The default validator rejects an approved v4 brief without this
handoff.

Treat the selected comp as a visual contract, while the repository and FluentQt
APIs remain the behavioral contract. Translate its relationships into semantic
tokens, shared metrics, layouts, models, and FluentQt components. Do not trace
pixels with fragile fixed geometry or preserve a comp mistake that violates
accessibility, localization, platform behavior, or component semantics.

At the first runnable vertical slice, compare the actual window directly with
the selected comp. Record every material deviation in hierarchy, density,
typography, palette role, signature move, or pane lifetime. Fix it or obtain a
new human decision; do not let implementation convenience create a fourth,
unreviewed direction.

The final independent visual review remains separate. Human concept approval
chooses the intended direction; contract v4 evidence checks whether the built
application reached it across themes, sizes, states, and interactions.

## Reject common aesthetic shortcuts

- default-selecting the first generated recipe;
- using logo and accent color as the only identity;
- placing every event, row, or setting in the same rounded card;
- adding gradients, glow, pills, or oversized headings without product meaning;
- presenting three variants that share type, hierarchy, and signature move;
- mixing icon packs, emoji, or arbitrary glyphs to make one concept feel richer;
- treating the visual scorecard as arithmetic selection instead of evidence;
- polishing a sparse shell while using placeholder or developer-facing copy;
- treating a schematic design board as pixel-quality evidence;
- allowing the implementation author to record human approval.

## Art-direction acceptance gate

Before implementation of a full-profile GUI or major redesign:

- the art direction is specific, evidence-backed, and names concrete anti-goals;
- subject vernacular and taste evidence are recorded without turning references
  into templates;
- one controlled aesthetic risk has evidence, restraint, a usability guard, and
  a fallback;
- all concepts use the same representative content, theme, and viewport;
- one coherent icon family and its provenance, sizing, state, color, and
  accessibility policies are recorded;
- three structurally and aesthetically distinct high-fidelity comps exist;
- all nine visual dimensions have a concrete 1–5 score and comp-specific note;
- genericity critique records at least one concrete revision and all six tuning
  axes are defined;
- the comparison board embeds those comps rather than only region diagrams;
- a human has approved one concept and rejected the alternatives with reasons;
- the approved concept has a complete implementation spec rather than an
  informal “make it look like the mockup” handoff;
- the default design-brief validator reports `PASS`.

Before final acceptance, the built app still expresses the approved signature
without its logo or accent, and any material deviation has a recorded human
decision rather than an agent-only rationale.
