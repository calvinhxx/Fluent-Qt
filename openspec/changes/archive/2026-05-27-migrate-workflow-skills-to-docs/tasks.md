## 1. Inventory

- [x] 1.1 List all source skill files in `.codex/skills`, `.github/skills`, and stray duplicate paths that correspond to component API audit, component API conventions, logging workflow, test workflow, Qt component test conventions, and visual review.
- [x] 1.2 Record the current README and agent-instruction links that point at `Fluent-QT-*` workflow skills or long-form skill bodies.
- [x] 1.3 Identify historical references that should be preserved in the migrated component API audit document.

## 2. Documentation Migration

- [x] 2.1 Create `docs/development/` with a short index document for repository development workflows.
- [x] 2.2 Migrate component API conventions into `docs/development/component-api-conventions.md`.
- [x] 2.3 Migrate the component API audit report into `docs/development/component-api-audit.md`, preserving historical audit facts while using a neutral title and current links.
- [x] 2.4 Migrate logging guidance into `docs/development/logging-workflow.md`.
- [x] 2.5 Migrate testing and CTest guidance into `docs/development/testing-workflow.md`.
- [x] 2.6 Migrate Qt component and VisualCheck test authoring conventions into `docs/development/qt-component-test-conventions.md`.
- [x] 2.7 Migrate visual review instructions into `docs/development/visual-review.md`.

## 3. Skill and Reference Cleanup

- [x] 3.1 Remove `.codex/skills/Fluent-QT-component-api-audit` and `.github/skills/Fluent-QT-component-api-audit` after migrating their content to docs.
- [x] 3.2 Remove `.codex/skills/Fluent-QT-component-api-conventions` and `.github/skills/Fluent-QT-component-api-conventions` after migrating their content to docs.
- [x] 3.3 Remove `.codex/skills/Fluent-QT-logging-workflow` and `.github/skills/Fluent-QT-logging-workflow` after migrating their content to docs.
- [x] 3.4 Remove `.codex/skills/Fluent-QT-test-workflow` and `.github/skills/Fluent-QT-test-workflow` after migrating their content to docs.
- [x] 3.5 Remove `qt-ut-conventions` skill copies after migrating their content to `docs/development/qt-component-test-conventions.md`.
- [x] 3.6 Remove `visual-review` skill copies after migrating their content to `docs/development/visual-review.md`.
- [x] 3.7 Remove stray duplicated skill paths that are not under `.codex/skills` or `.github/skills`.

## 4. Repository Documentation Links

- [x] 4.1 Update `readme.md` logging, testing, component API, and visual review sections to link to `docs/development/` documents.
- [x] 4.2 Update `.github/copilot-instructions.md` and OpenSpec workflow skill references that currently point at migrated skill rule files.
- [x] 4.3 Ensure new docs avoid hard-coded local checkout paths and use relative repository paths.

## 5. Validation

- [x] 5.1 Run `openspec validate migrate-workflow-skills-to-docs --strict`.
- [x] 5.2 Search live README/docs/skill/agent-instruction files for stale `Fluent-QT-*` workflow skill names and verify only allowed historical audit references remain.
- [x] 5.3 Search migrated docs and agent instruction files for `/Users/calvinhxx/Ws/qttest/Fluent-QT` and remove live checkout-path dependencies.
- [x] 5.4 Review `git diff` to confirm the change is documentation and workflow-entrypoint only, with no production code or CMake target rename.
