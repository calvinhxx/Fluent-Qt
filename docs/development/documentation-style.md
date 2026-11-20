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

Keep documentation prose in English, including existing guides with filenames
that have no language suffix. The project maintains localized entry points in
`README.md`, `README.zh-CN.md`, `README.zh-TW.md`, `README.ja.md`,
`README.ko.md`, `README.de.md`, `README.ru.md`, `README.es.md`, and
`README.fr.md`.

Create Chinese versions of other documents only when explicitly requested.
Use the `.zh-CN.md` suffix for those translations and keep them aligned with
the English source. Do not put Chinese prose in an unsuffixed document.

The documentation check reports CJK text in English Markdown with its file
and line number. It checks headings, paragraphs, lists, tables, and visible
link text. Exceptions are the root README files (`README.md` and the localized
root `README.<lang>.md` translations), `.zh-CN.md` translations, fenced
and inline code, HTML comments, link destinations, generated navigation, and
the Chinese column of the [comment glossary](comment-style.md#glossary).
Keep Chinese example text in code spans or fenced code blocks.

## Prefer direct prose

- Lead with the outcome, constraint, or command the reader needs.
- Keep one idea per paragraph. Remove restatements of the heading.
- Prefer a short table for repeated fields and a checklist for ordered work.
- Use concrete component, file, command, and test names.
- Avoid promotional adjectives, invented maturity labels, defensive non-change
  boilerplate (`without changing ...`, `while preserving ...`), and long chains
  of abstract nouns. For website version history and migration copy, follow the
  [release and migration writing rules](release-governance.md#release-and-migration-writing-rules).
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

## Verify website claims

Website prose comes from `tools/site/documentation_guides.py`,
`tools/site/component-docs.json`, `tools/site/component-translations.json`,
and the Gallery catalog and sample descriptions. API descriptions come from
installed public headers. Correct the owning source and regenerate; never
patch a generated HTML page or catalog by hand.

Check each capability against its public declaration and implementation.
Name the object that owns an API: `ChartModel::setPoints()` supplies data,
while a chart's `setModel()` attaches the model. Distinguish a component's
built-in behavior from signal connections, file access, validation or page
switching implemented by the example. State relevant defaults, motion policy,
thread affinity, model ownership and platform fallback conditions. Keep the
English and Chinese text aligned.

### Component changes require a website review

Every component source change must include a website documentation alignment
review before it is considered complete. This applies to public headers,
implementation files and private helpers, even when no public signature changes.
Changes to shared foundations, design tokens, compatibility adapters or bindings
must also cover the components whose documented behavior they affect.

1. Identify the affected component pages, shared guides, inherited APIs and
   Gallery examples. Include both English and Chinese text and the C++ and
   PySide6 examples that describe the changed surface.
2. Compare their claims with the updated declarations, implementation and owning
   tests. Check API availability, defaults, parameters, signal behavior,
   ownership, motion settings, thread requirements and platform limitations as
   applicable. A passing build or an unchanged API signature does not establish
   documentation alignment.
3. Fix mismatches in their owning sources in the same change as the component.
   Update sample code and previews when their demonstrated behavior changes.
   Do not describe an application-provided behavior as a built-in capability,
   or promise an interface that the component does not expose.
4. Regenerate affected catalogs and pages, run the checks below, and inspect the
   affected generated pages. Refresh visual evidence through the
   [preview workflow](gallery-preview-workflow.md) when the documented appearance
   or interaction changes. Reused draft images are not current visual evidence.
5. In the PR description or task handoff, name the pages reviewed, summarize the
   corrections and checks, and state whether the result is local or published.
   If the review finds no documentation impact, explain why the existing text
   and examples remain accurate; do not make unrelated wording changes just to
   produce a documentation diff.

Do not leave known source/documentation mismatches for a later task. A local
update is not a live-site update: keep deployment status explicit and follow the
[CI publication workflow](ci-workflow.md) for the authorized release or deployment.

When component, sample or public-header inputs change, regenerate in this order:

```bash
python3 tools/ai/generate_ai_catalog.py --project-root .
python3 tools/site/generate_api_reference.py
```

Run the callable-reference check and generation regressions:

```bash
python3 tools/site/validate_documentation_claims.py --report build/site-documentation-audit.json
python3 tools/site/test_generate_api_reference.py
```

Website generation also runs the callable-reference check. It rejects unknown
owners and known methods attributed to the wrong component, including inherited
members. Its small, type-scoped Qt member list is reviewed against Qt headers;
it is not a complete Qt inventory. An otherwise unknown callable on a Qt-derived
type is reported as an unverified reference for source review, not as proof of a
nonexistent API or an automatic site-generation failure. These warnings remain
in the audit report and generation output. Guide Python examples are checked for
syntax. Passing these checks does not establish argument correctness, runtime
behavior, platform support, or visual approval; those still need source review
and the owning tests. Local regeneration does not update the published site.

## Keep agent instructions task-specific

Use `AGENTS.md` for the repository map, non-obvious constraints, and links to
owning guides. A Skill description should identify when the Skill applies;
its entry file should route tasks to the relevant references. Keep detailed
procedures with their owner rather than repeating them in every entry point.
Supporting references may link to scripts and assets; the entry file need not
list every resource directly. Keep descriptive fields proportional to the
decision. Use fixed counts only when they protect a specific comparison or
correctness requirement, and keep templates and validators aligned with that
requirement.

Distinguish application design, focused repairs, and library maintenance.
State the completion evidence and actual approval boundaries for each. Do not
turn a routine fix into a new design selection, or substitute a generic
instruction to "test thoroughly" for the project's named gates. Preserve
existing session authorization and task scope.

Keep repository guidance portable across agents and model versions. Treat
prompting advice as something to evaluate, not a reason to weaken API,
security, lifecycle, or visual acceptance contracts.

## Review checklist

- [ ] The title and status describe what the document is now.
- [ ] The first screen answers who the document is for and where to start.
- [ ] Current guidance is separate from historical evidence.
- [ ] Links point to canonical files rather than duplicate explanations.
- [ ] Commands are runnable from the stated directory.
- [ ] A diagram or table is present only when it improves comprehension.
- [ ] Generated catalogs and local links still validate.
- [ ] Component source changes include a website alignment review, with either
      matching documentation updates or a recorded no-impact explanation.

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
