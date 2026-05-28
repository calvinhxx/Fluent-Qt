## Context

The repository currently has durable development guidance split across `.codex/skills`, `.github/skills`, README sections, and OpenSpec specs. Several skill files duplicate the same content in both `.codex` and `.github`, and the component API, logging, and testing workflow skills still use the temporary `Fluent-QT` name in file paths, frontmatter names, headings, and internal links.

This guidance is useful beyond agent execution. Humans need the same rules during review, migration, and maintenance, and future project renames should not require rewriting workflow names. The repository already has `docs/architecture/overlay-behavior.md` as a better model: docs hold the canonical contract, while agent entrypoints can reference the docs.

## Goals / Non-Goals

**Goals:**

- Make `docs/development/` the canonical location for reusable workflow guidance.
- Use project-neutral names that remain valid after the repository folder or product name changes.
- Preserve the current useful guidance from all listed skills.
- Remove docs-backed workflow skill files after their content is migrated to docs.
- Update README and agent references to point to docs instead of `Fluent-QT-*` skill files.

**Non-Goals:**

- Rename the CMake target `fluent_qt_lib`, test binaries, namespaces, or source folders.
- Change component public APIs or runtime behavior.
- Redesign the existing OpenSpec workflow skills.
- Rewrite historical audit facts that need to mention the original `audit-component-api-consistency` change.

## Decisions

### Docs Are Canonical

Create a development documentation set under `docs/development/`:

- `component-api-conventions.md`
- `component-api-audit.md`
- `logging-workflow.md`
- `testing-workflow.md`
- `qt-component-test-conventions.md`
- `visual-review.md`
- optional `README.md` index if useful for navigation

Rationale: Documentation is visible to both humans and agents, works without a specific Codex skill loader, and is the right place for durable repository conventions. Skill files are runtime entrypoints, not the canonical knowledge base.

### Docs-Backed Workflow Skills Are Removed

Remove the docs-backed workflow skills from both `.codex/skills` and `.github/skills` after the equivalent docs exist:

- `component-api-conventions`
- `component-api-audit`
- `logging-workflow`
- `testing-workflow`
- `qt-component-test-conventions`
- `visual-review`

Rationale: Once README, agent instructions, and OpenSpec workflow skills point at docs, these wrappers only add another maintenance surface. OpenSpec workflow skills remain because they implement actual workflow actions rather than merely pointing to documentation.

### Preserve History Without Branding New Guidance

The audit report can retain historical facts such as the date, originating OpenSpec change, and current build target references where they are materially useful. New document titles, file names, links, and reusable conventions must not use `Fluent-QT` branding.

Rationale: Removing useful audit context would reduce traceability. The important cleanup is removing the temporary project name from current workflow identity.

### README Links to Docs

README sections for logging, testing, component API workflow, and visual review should point to docs. They should not direct readers to `.codex/skills/Fluent-QT-*` or `.github/skills/Fluent-QT-*`.

Rationale: README should guide maintainers to stable documentation rather than agent-specific implementation files.

## Risks / Trade-offs

- Skill auto-discovery no longer exposes these workflow names -> Mitigate by linking the canonical docs from README, `.github/copilot-instructions.md`, and OpenSpec workflow instructions.
- Duplicate `.codex` and `.github` skill directories may drift again -> Mitigate by deleting docs-backed wrappers rather than preserving separate copies.
- Historical audit content may still contain `Fluent-QT` in old references -> Mitigate by allowing historical mentions only where they are clearly records, not current branding.
- Documentation-only changes can be under-validated -> Mitigate with OpenSpec validation and targeted `rg` checks for old skill names and hard-coded local checkout paths.

## Migration Plan

1. Inventory every listed skill in `.codex/skills`, `.github/skills`, and stray duplicate paths.
2. Create or update `docs/development/` files with the durable content.
3. Normalize names and links in README and agent instruction files.
4. Delete migrated docs-backed workflow skills while keeping OpenSpec workflow skills.
5. Validate OpenSpec and run text checks for stale `Fluent-QT-*` workflow references.
