# Design Intelligence

Create a visual point of view before selecting controls. Use this reference for
every new GUI, major redesign, or request for a more distinctive interface.
The process is provider-neutral: use the best concept renderer available to the
current agent, then carry the accepted result into native FluentQt code.

## Contents

- Ground the design in the product's world
- Build a compact taste context
- Spend one controlled aesthetic risk
- Render resolved concepts before coding
- Critique genericity before human review
- Expose global tuning axes
- Extract an implementation design system
- Preserve native desktop quality
- Design-intelligence gate

## Ground the design in the product's world

Do not begin with a style label such as modern, premium, futuristic, or clean.
Inspect the target and record the subject's own visual vocabulary:

- **materials**: paper, glass, traces, maps, waveforms, layers, instruments,
  terminals, physical controls, or other domain surfaces;
- **artifacts**: the files, records, scenes, runs, reports, devices, or outputs
  users actually recognize;
- **instruments**: the tools and controls users repeatedly operate;
- **verbs**: the real actions that define the workflow;
- **tempo**: quiet review, live operation, rapid triage, careful authoring, or
  another dominant rhythm.

Translate those signals into structure, hierarchy, type rhythm, shape,
material, semantic color, and motion. Do not paste literal decoration from the
domain onto a generic app shell. A pipeline may justify an ordered trace; it
does not justify decorative pipes.

The primary surface is the desktop equivalent of a thesis: it should reveal
what the product is for within a few seconds. Structural devices such as
numbers, rails, grids, labels, or dividers must encode real information.

## Build a compact taste context

Collect the smallest useful set of design evidence before concepting:

1. explicit user likes, dislikes, screenshots, adjectives, and prior feedback;
2. the project's brand guide, tokens, shipped UI, assets, and copy;
3. one aligned product/UI reference and one contrast reference;
4. one non-UI subject reference when it reveals material, rhythm, typography,
   or interaction character;
5. recent unrelated outputs whose repeated shell or aesthetic must not recur.

For each source, record what authority it has, what transfers, and what is
rejected. A screenshot is evidence, not a template. Never transfer marks,
proprietary assets, exact copy, or screenshot geometry.

Keep this context task-local by default. When the user repeatedly gives the
same feedback, offer to update a repository-owned design guide or taste
profile; do not silently write personal preferences into a global store.

## Spend one controlled aesthetic risk

Choose one memorable move grounded in the subject: an unusual reading rhythm,
a distinctive but practical type contrast, a product-specific state
transition, a signature focus treatment, or an original primary-surface
composition.

Record:

- the risk and the product evidence that justifies it;
- what should remain quiet around it;
- a usability guard covering readability, focus, motion, localization, and
  native control behavior;
- a fallback if Qt, accessibility, or performance evidence rejects it.

Not taking any position produces generic UI. Taking risks everywhere produces
noise. Spend boldness once, then remove one unnecessary accessory before human
review.

## Render resolved concepts before coding

For full work, create three same-content high-fidelity desktop concepts. Use a
design-capable image model, Claude Design, Figma, SVG/raster composition, or a
code-native static mockup according to availability. Do not make one provider a
hard dependency of the Skill.

Use a two-pass concept workflow:

1. explore atmosphere, composition, hierarchy, typography character, material,
   and the signature move broadly;
2. resolve the selected candidates with readable product copy, realistic data,
   Fluent-compatible control anatomy, explicit states, and implementable
   geometry.

An image-generated concept may establish the visual world, but critical text,
control semantics, focus, and interaction must be redrawn or overlaid as
readable design information before approval. Never ship the concept image as
the UI.

Design the complete requested surface. For a dense window, also resolve the
high-risk detail or state that becomes unreadable in a full-window comp: table
anatomy, composer, inspector, popup, empty/error state, or narrow transform.
Regenerate a fresh detail at useful resolution; do not approve a blurry crop.

## Critique genericity before human review

Before presenting concepts, run a written self-critique:

- What would an untuned agent probably produce for the same request?
- Which parts of this concept could be reused unchanged for an unrelated app?
- Does the signature remain recognizable without logo and accent color?
- Do type, structure, copy, and motion express this subject or current AI
  fashion?
- Is every card, pill, badge, glow, number, gradient, and border semantically
  necessary?
- Does the concept preserve a real desktop workflow at normal and narrow size?

Record at least one detected generic choice and the concrete revision made.
Reject concepts that differ only by palette, fashionable typography, or
surface effects.

## Expose global tuning axes

Record six 1–5 axes with short semantic notes so human feedback can change the
whole system coherently instead of creating local patches:

| Axis | 1 | 5 |
| --- | --- | --- |
| density | spacious | compact |
| contrast | quiet | emphatic |
| material_depth | flat/revealed | layered/elevated |
| corner_softness | crisp | soft |
| motion_energy | still | expressive |
| visual_expressiveness | restrained | bold |

These are direction controls, not formulas. Changing an axis requires updating
the concept, tokens, affected components, and evidence together. It must not
override accessibility, Fluent component geometry, platform behavior, or the
4 px rhythm.

## Extract an implementation design system

After human selection and before production UI code, extract a compact
implementation spec from the accepted comp:

- selected concept and exact primary-surface/container model;
- semantic Light/Dark palette relationships;
- content, chrome, data, and caption typography roles;
- spacing, density, radius, border, elevation, and material rules;
- component families and explicit variants;
- icon and state grammar;
- one or two meaningful motion cues plus reduced-motion behavior;
- responsive transformations and overflow owners;
- decisions that are locked, adaptations that are allowed, and known risks;
- full-window and detail regions used for implementation comparison.

Treat this as a production design spec. During implementation, compare the
actual rebuilt window beside the accepted comp. Record any material deviation
instead of gradually reinterpreting the design.

## Preserve native desktop quality

Do not transfer Web-only freedom blindly:

- use code-native text and controls, not a screenshot surface;
- keep typography available, redistributable, and legible across supported
  platforms; use a distinctive role only where it survives localization;
- prefer a few semantic animations over constant motion and respect reduced
  motion;
- keep pointer, keyboard, focus, IME, accessibility, window resizing, and
  platform chrome first-class;
- express branding through Fluent semantic tokens and composition, never a
  second component geometry language;
- match visual ambition with feasible C++/PySide6 architecture and bounded
  rendering cost.

## Design-intelligence gate

Before concepts are ready for human review:

- subject materials, artifacts, instruments, verbs, and tempo are recorded;
- taste evidence includes user/project context, aligned and contrast
  references, and at least one rejection;
- one justified aesthetic risk has a restraint and usability guard;
- all concepts use real shared content and resolved desktop anatomy;
- genericity critique records a detected default and a revision;
- six tuning axes are explicit;
- the concepts remain practical in FluentQt and differ beyond color.

Before implementation:

- a human-approved concept exists;
- its compact implementation design system has been extracted;
- locked decisions, allowed adaptations, known risks, and comparison regions
  are recorded;
- the implementation agent can explain what makes the product recognizable
  without naming its logo, accent color, or framework.
