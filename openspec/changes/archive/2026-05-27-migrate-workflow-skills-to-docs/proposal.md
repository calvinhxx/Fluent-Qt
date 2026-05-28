## Why

Several repository workflow rules currently live primarily in Codex/GitHub skill files, and some of those skill names and paths still include the temporary `Fluent-QT` project name. That makes the guidance feel tied to the current checkout name, duplicates content across agent entrypoints, and leaves important conventions less visible to human maintainers.

## What Changes

- Move the durable content from these workflow skills into project-neutral documentation under `docs/development/`:
  - `Fluent-QT-component-api-audit`
  - `Fluent-QT-component-api-conventions`
  - `Fluent-QT-logging-workflow`
  - `Fluent-QT-test-workflow`
  - `qt-ut-conventions`
  - `visual-review`
- Rename the workflow documents with professional, project-neutral names, such as `component-api-conventions`, `component-api-audit`, `logging-workflow`, `testing-workflow`, `qt-component-test-conventions`, and `visual-review`.
- Update README and agent-facing references so the docs are the canonical source for workflow rules.
- Remove docs-backed workflow skill files so the migrated rules live only in `docs/development/`.
- Keep historical audit facts and current build target names where needed, but stop branding workflow guidance as `Fluent-QT`.
- This change does not rename CMake targets, namespaces, source directories, test binaries, or product-level component APIs.

## Capabilities

### New Capabilities
- `developer-workflow-docs`: Canonical project-neutral documentation for component API conventions, audit history, logging workflow, test workflow, Qt component test conventions, and VisualCheck review.

### Modified Capabilities
- None.

## Impact

- Affects `docs/development/**`, `readme.md`, `.codex/skills/**`, `.github/skills/**`, and agent instruction files that currently point to skill paths.
- Reduces duplicated guidance between `.codex` and `.github` skill copies.
- Improves migration readiness by removing `Fluent-QT` from workflow names, headings, and durable documentation links.
- Leaves OpenSpec workflow skills in place because they are executable workflow entrypoints, not docs-backed guidance wrappers.
- Does not affect compiled code or runtime behavior.
