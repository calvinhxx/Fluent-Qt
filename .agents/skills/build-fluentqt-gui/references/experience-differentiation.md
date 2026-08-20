# Experience Differentiation

Make the interface recognizable from its product workflow, not from a reusable
shell. FluentQt supplies consistent controls and tokens; it does not imply that
every tool needs a navigation rail, conversation column, and permanent
inspector.

## Contents

- Product signature
- Reference synthesis
- Structurally distinct concepts
- Semantic component opportunities
- Cross-product similarity review
- Differentiation acceptance gate

## Define the product signature

Before choosing an application pattern or shell, record one short identity card:

| Field | Question |
| --- | --- |
| Primary object | What object does the user continuously inspect or change? |
| Dominant time model | Is the work a live stream, ordered run, replay, document revision, queue, or snapshot? |
| Core outcome | What durable result makes the session successful? |
| Hero interaction | Which interaction should feel unique and receive the clearest space? |
| Signature surface | Which surface directly visualizes that interaction and time model? |
| Supporting surfaces | Which controls are necessary but should remain secondary or transient? |

Use repository evidence for every answer. A generic noun such as “workspace,”
“chat,” or “dashboard” is insufficient unless the domain itself makes that the
primary object. If the primary object is a run or conversation, name it that
way and finish the transcript; do not relabel a log as a chat or hide a real
run behind an unrelated artifact workbench.

## Synthesize references instead of copying them

After the product signature is stable, use
[Product reference patterns](product-reference-patterns.md) to choose one
evidence-aligned reference and one deliberately contrastive reference. Extract
relationships such as canvas dominance, persistent versus transient panels,
reading direction, density, and narrow transformation. Do not extract a full
screen shell or component inventory.

Record both the transferable rules and the traits rejected by the target's
workflow. This makes the reference useful even when the final design should not
look like it. Brand marks, proprietary assets, product copy, exact colors, and
screenshot geometry are outside the synthesis.

## Generate structurally distinct concepts

Produce at least three credible concepts before implementation. The concepts
must differ in information architecture, not only color, corner radius, or pane
width. Useful lenses include:

- object or artifact first;
- ordered run or pipeline first;
- event timeline and replay first;
- command palette or action first;
- spatial board or multi-actor orchestration first;
- conversation first;
- guided task or wizard first;
- monitoring and exception first.

For each concept, name the primary surface, hero interaction, persistent panes,
temporary surfaces, narrow-layout behavior, and one reason it may fail. Score
the concepts against workflow fit, state visibility, responsiveness, component
semantics, implementation risk, and product distinctiveness. For full new-GUI
or redesign work, continue through
[Art direction and human selection](art-direction.md): resolve each concept as
a same-content high-fidelity comp and obtain a human selection. Scores inform
the review; they do not authorize the implementation agent to select its own
concept.

At most one concept may retain the aligned reference's complete region
topology. Start the other concepts from the target's object, time model, or
contrast reference so that reference selection does not quietly become template
selection.

Do not treat the catalog's application patterns as complete screen templates.
They are starting hypotheses. Two projects with the same integration boundary
may need different time models and therefore different compositions.

## Scan semantic component opportunities

After selecting the concept, scan the relevant catalog selection guides and
classify each plausible component family:

- **must use**: directly owns a required behavior or state;
- **conditional**: useful only when the corresponding state appears;
- **not applicable**: would be decorative, redundant, or misleading.

Record the reasoning for major surfaces. This is an opportunity scan, not a
component quota. Never add a carousel, badge, drawer, card, or visualization
merely to increase variety. Reusing a small set of controls is correct when the
workflow is genuinely simple; repeating the same shell without product evidence
is not.

Examples of meaningful compositions:

| Product signal | Candidate signature composition |
| --- | --- |
| Ordered build with stages and logs | pipeline or stage tree + progress + expandable step output |
| Replayable event protocol | event timeline + replay cursor + transient event details |
| Artifact inspection and revision | document tabs + hierarchy + annotated navigation + review overlay |
| Several autonomous workers | spatial/collection view + identity/status + coordinated task disclosure |
| Prompt-driven agent with editable artifacts | artifact workbench + compact command surface + temporary run details |
| Prompt-driven agent whose primary object is the run | designed transcript (user / assistant / tool / permission) + integrated composer |

These examples are not mandatory layouts. Use them to challenge a generic
conversation-first default unless the identity card names the run or
conversation as the primary object. Then follow
[Signature surface](signature-surface.md).

## Review cross-product similarity

When relevant prior GUIs, mockups, or screenshots are available, compare the
selected concept with them before implementation and again after the first
render. Review these dimensions:

1. outer shell and number of persistent panes;
2. primary object and dominant reading direction;
3. hero action and location of the main input;
4. navigation model and selected-state treatment;
5. permanent versus transient detail surfaces;
6. repeated component families and card rhythm;
7. narrow-layout transformation.

If four or more dimensions match a recent unrelated product, either cite domain
evidence proving the similarity is necessary or revise the concept. Do not
manufacture novelty by moving controls randomly; change the composition around
the product signature.

## Differentiation acceptance gate

Before the vertical slice:

- the identity card is specific and evidence-backed;
- the aligned/contrast synthesis names both transferred and rejected traits;
- at least three structurally distinct concepts were considered;
- full new-GUI or redesign concepts have comparable high-fidelity comps and a
  recorded human selection;
- the human-selected concept gives the signature surface more visual priority
  than supporting controls;
- component opportunities were classified without a usage quota;
- any resemblance to prior unrelated GUIs is justified or redesigned.

Before final acceptance:

- the application remains recognizable in grayscale and without its logo;
- the hero interaction is visible within a few seconds;
- temporary details do not consume permanent space unless continuous
  comparison is a core task;
- the narrow layout preserves the product signature rather than collapsing into
  an undifferentiated list;
- the result is not merely the same navigation/session/chat/inspector skeleton
  with new labels and colors, unless the identity card names that skeleton's
  primary object and the signature surface is finished rather than wireframed.
