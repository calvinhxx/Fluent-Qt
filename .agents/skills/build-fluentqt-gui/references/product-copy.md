# Product Copy

Write interface text that names the user's object, state, or next action. Do
not make the interface narrate what an AI intends to do. Product copy should
feel edited, not generated.

## Contents

- Establish the product register
- Rewrite by interface job
- Remove assistant narration
- Preserve useful technical precision
- Design for localization and layout
- Run the copy audit
- Product-copy acceptance gate

## Establish the product register

Use repository language, shipped UI, user feedback, and domain terms before
inventing copy. Record these in `copy_policy`:

- audience and expertise level;
- a short voice such as `direct, calm, technical`;
- locale and terminology strategy;
- technical terms the audience already understands;
- compression rules, shared state vocabulary, and forbidden patterns.

Keep one register across title bar, navigation, empty states, dialogs, status,
demo fixtures, and errors. A technical application may be precise without
sounding like a protocol inspector. A friendly application may be warm without
personifying every operation.

## Rewrite by interface job

Start from the job of each string, then remove everything that does not change
the user's decision:

| Job | Preferred form |
| --- | --- |
| Window or region title | The object or destination: `Tasks`, `History`, `Settings` |
| Primary action | Verb plus object when needed: `Run`, `Save changes`, `Retry` |
| Status | Concrete state: `Not started`, `Running`, `Complete`, `Failed` |
| Field label | The value being requested: `Workspace`, `Model`, `API key` |
| Helper text | One consequence or constraint that prevents a mistake |
| Empty state | Short title, at most one useful sentence, and one optional action |
| Error | What failed, the useful cause when known, and the recovery action |
| Tooltip | Information not already visible beside the control |

Use short visible labels and more explicit accessible names when required.
Do not make a visible button verbose merely to satisfy accessibility.

## Remove assistant narration

Reject visible interface copy that sounds like a generated response or product
pitch:

- `I will`, `I can`, `Let me`, `Here is`, `Would you like me to`, or their
  localized equivalents when a state or action label is enough;
- repeated explanations of what the product already makes obvious;
- `intelligent`, `powerful`, `seamless`, `premium`, and similar unsupported
  marketing adjectives;
- procedural headings such as `Step 1 / Step 2 / Step 3` when the workflow is
  not actually gated by those steps;
- duplicated window, pane, card, and empty-state titles;
- prose that exists only to fill sparse space.

For example, prefer `New task` + `Describe the work` over a paragraph explaining
what the assistant can do. Prefer `Checking files` over `I will now inspect the
relevant files`. Keep genuine user content and assistant results intact unless
the task explicitly asks to edit them; the audit targets product-owned chrome,
fixtures, and system messages.

## Preserve useful technical precision

Short does not mean vague. Keep a technical term when users act on it or when
removing it would hide a consequence. Explain an unfamiliar term once, nearest
to the decision. Do not expose raw RPC methods, internal enum names, UUIDs,
payload keys, or implementation paths on ordinary product surfaces.

Use a stable state vocabulary. Do not alternate between `Ready`, `Available`,
`Standing by`, and `Prepared` for the same state. Separate state from action:
`Running` is status; `Stop` is the action.

## Design for localization and layout

- Do not insert manual line breaks to force a screenshot composition.
- Give labels and helpers an explicit wrap or elide contract.
- Test long English, CJK, and mixed technical terms at normal and narrow widths.
- Avoid sentence fragments that become ambiguous when translated.
- Keep punctuation and capitalization consistent within each hierarchy.
- Treat placeholder text as a hint, not a substitute for a persistent label
  when the value needs one.

## Run the copy audit

Inventory every product-owned visible string in the changed surface, including
demo and failure states. For each string ask:

1. Does it name an object, state, action, constraint, or recovery?
2. Would removing it change the user's next decision?
3. Does nearby UI already say the same thing?
4. Is it the shortest wording that preserves domain meaning?
5. Does it still fit and read naturally in Light, Dark, normal, narrow, and
   long/localized-content captures?

Review the built application, not only the source strings. Copy that is concise
in a file may still create an awkward line, oversized panel, or repeated visual
hierarchy.

## Product-copy acceptance gate

- `copy_policy` records audience, voice, locale strategy, compression rules,
  shared states, allowed terms, and forbidden patterns.
- Product-owned visible copy names user objects, states, and actions rather than
  narrating the assistant.
- Empty, loading, failure, permission, completion, and demo states use the same
  vocabulary and level of detail.
- No decorative explanation, generic AI phrasing, protocol label, duplicated
  title, or manual screenshot line break remains.
- Long/localized copy has measured wrap, elide, and overflow behavior.
- A final native-scale review confirms the copy improves hierarchy instead of
  merely making the strings shorter.
