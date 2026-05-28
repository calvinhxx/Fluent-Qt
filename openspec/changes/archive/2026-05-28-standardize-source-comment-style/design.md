## Context

The repository already has partial documentation conventions under `docs/development/` and agent-facing guidance in `.github/copilot-instructions.md`. Source comments under `src/` are useful but inconsistent: some public headers use Chinese `@brief` blocks, some implementation files use targeted "注意" comments for Qt behavior, and design token headers use short module summaries.

This change should turn that useful local style into a durable convention without creating a large mechanical rewrite. The first implementation pass should establish rules and demonstrate them in representative infrastructure files; future component work can then apply the same style incrementally.

## Goals / Non-Goals

**Goals:**

- Define one canonical source comment style for `src/` that supports both English and Chinese readers.
- Prefer structured Doxygen comments for public API declarations that form reusable component or infrastructure contracts.
- Keep implementation comments sparse and focused on rationale, invariants, compatibility traps, and behavior that is not obvious from the code.
- Document module-specific comment density so `src/design/`, `src/view/foundation/`, `src/view/**`, `src/utils/`, and compatibility helpers are not treated as identical.
- Make the style discoverable from `docs/development/` and agent-facing repository guidance.

**Non-Goals:**

- Rewriting all comments under `src/` in a single change.
- Adding comments to every getter, setter, local variable, or self-explanatory branch.
- Introducing a Doxygen build pipeline or documentation publishing dependency.
- Changing runtime behavior, public APIs, include layout, tests, or build configuration.

## Decisions

### Use bilingual-friendly Doxygen for public contracts

Public component and infrastructure headers should use Doxygen block comments for non-trivial classes, structs, enums, properties, and public methods. The recommended shape is English `@brief` first, followed by a `zh_CN:` line for the Chinese explanation.

This gives AI tools and external documentation generators a stable English anchor while preserving Chinese semantics for local maintainers. Alternatives considered:

- Chinese-only comments: fits current local style but weakens external tooling and search.
- English-only comments: simpler for generated docs but loses local precision and user-facing context.
- Separate English and Chinese documentation trees: more formal, but too heavy before the project has a documentation build.

### Keep private implementation comments intent-based

Implementation comments in `.cpp` files should not be forced into bilingual Doxygen. They should explain why a branch exists, which invariant is being protected, or which Qt/platform behavior is being handled.

This preserves existing high-value comments such as Qt visibility caveats while avoiding stale duplicated translations. Alternatives considered:

- Bilingual comments everywhere: consistent visually, but creates noise and translation drift.
- No implementation comments: concise, but loses important cross-platform and rendering rationale.

### Put the durable rule in docs, not a skill

The canonical rule should live in `docs/development/comment-style.md` and be linked from `docs/development/README.md` and `.github/copilot-instructions.md`.

This matches the existing migration of reusable workflow guidance into docs and keeps the convention project-neutral if the repository name changes. Alternatives considered:

- A long-form Codex skill: useful for agent prompting, but duplicates canonical docs and becomes stale.
- Only OpenSpec requirements: durable for change tracking, but not ergonomic for day-to-day contributors.

### Apply a representative first pass plus a mechanical view-header pass

The implementation should update a small set of high-signal files, such as design token headers, foundation headers, and logging/debug utilities, to demonstrate the style. Broad component annotation should be done opportunistically when those components are edited.

After the initial representative pass, a user-requested mechanical pass may fill
missing class and `Q_PROPERTY` comments across public view headers under
`src/view/**/*.h`. This broader pass should still stay comment-only and avoid
rewriting implementation files or changing public behavior.

The bulk pass still has a quality bar: comments must describe the component
contract or property behavior. Placeholder wording such as "view component" or
"<name> property" is not acceptable, so a polish pass should replace generic
generated comments with component-specific bilingual wording.

The same quality bar applies to design token tables and compatibility shims:
English should be the tooling/search anchor, and `zh_CN:` should carry the
Chinese explanation. Design headers may keep compact inline comments for dense
token rows, but they should not leave Chinese-only row comments after being
materially edited.

This keeps the broad update reviewable as a source-comment-only change while
avoiding low-value generated comments.
Alternatives considered:

- Full `src/` rewrite: fast to generate, but hard to review and likely to create low-value comments.
- Docs only: low risk, but leaves no concrete example for future contributors.

## Risks / Trade-offs

- Risk: Bilingual comments drift over time. Mitigation: Require updates only when the documented contract changes and keep comments concise.
- Risk: Developers over-comment implementation details. Mitigation: Document that private comments must explain intent, invariants, or platform behavior rather than restating code.
- Risk: The style is ignored after the first pass. Mitigation: Link it from development docs and agent instructions, and include a small source demonstration.
- Risk: Existing comments remain mixed for a while. Mitigation: Treat this as an incremental convention; allow a scoped mechanical pass for public view headers while avoiding full-source rewrites.
- Risk: Mechanical generation creates shallow comments. Mitigation: Require a semantic polish pass and verify placeholder phrases are absent before validation.
