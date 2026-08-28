# Documentation style

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › API, policy, and writing

[← Source Comment Style](comment-style.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Accessibility Contract →](accessibility-contract.md)
<!-- docs-nav:top:end -->

Write for a reader who is trying to make a decision or complete a task. A
document should make its scope, current status, and next action clear without
requiring the reader to reconstruct project history.

## Start with the document contract

State one of these near the title:

- **Current guide** — commands and steps for the active branch.
- **Living reference** — an inventory maintained with the implementation.
- **Accepted contract** — a behavior or API decision that remains in force.
- **Historical record** — dated evidence that must not be read as current state.

If a document mixes historical and current material, split it or label the
boundary explicitly.

## Prefer direct prose

- Lead with the outcome, constraint, or command the reader needs.
- Keep one idea per paragraph. Remove restatements of the heading.
- Prefer a short table for repeated fields and a checklist for ordered work.
- Use concrete component, file, command, and test names.
- Avoid promotional adjectives, invented maturity labels, and long chains of
  abstract nouns.
- Do not describe a completed implementation as a future plan.
- Keep Chinese prose and Chinese list items on one source line per logical
  paragraph. Do not hard-wrap CJK text to an English column width; let the
  Markdown renderer wrap it for the available viewport.

## Use visuals when they explain structure

Use the smallest visual that makes a relationship easier to understand:

| Relationship | Preferred format |
|---|---|
| Reader choice or execution path | Mermaid flowchart |
| State and event order | Mermaid state or sequence diagram |
| Ownership or hierarchy | Mermaid graph or text tree |
| Exact comparison | Table |
| Appearance or layout | Real Gallery or VisualCheck screenshot |

Every screenshot needs useful alt text. Diagrams must still be understandable
from nearby prose. Do not add decorative generated images to technical guides.

## Keep one source of truth

Do not copy changing totals, platform matrices, or API inventories into prose
unless the value is deliberately preserved as a dated snapshot. Link to the
generated source instead:

- component and sample facts: `docs/ai/generated/fluentqt-ai-catalog.json`;
- installed API facts: `site/api/catalog.json`;
- accessibility state: `docs/development/accessibility-inventory.md`;
- PySide6 exports: `bindings/pyside6/api-manifest.json`;
- binary support: `bindings/pyside6/wheel-matrix.json`.

## Review checklist

- [ ] The title and status describe what the document is now.
- [ ] The first screen answers who the document is for and where to start.
- [ ] Current guidance is separate from historical evidence.
- [ ] Links point to canonical files rather than duplicate explanations.
- [ ] Commands are runnable from the stated directory.
- [ ] A diagram or table is present only when it improves comprehension.
- [ ] Generated catalogs and local links still validate.

Run the repository check before handing off a documentation change:

```bash
python3 tools/docs/generate_navigation.py --project-root .
python3 tools/docs/validate_documentation.py --project-root . --self-test
```

The validator requires every reader-facing Markdown file in
`docs/navigation.json` to declare a `Status`; this includes linked guides
outside `docs/`, such as contribution and PySide6 pages.

`docs/navigation.json` is the single source for the complete contents page,
breadcrumbs, section links, and previous/next navigation. Add a reader-facing
Markdown file there instead of hand-writing navigation blocks.

<!-- docs-nav:bottom:start -->
---
[← Source Comment Style](comment-style.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Accessibility Contract →](accessibility-contract.md)
<!-- docs-nav:bottom:end -->
