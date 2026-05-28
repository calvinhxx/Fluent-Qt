# developer-workflow-docs Specification

## Purpose

Define the project-neutral documentation contract for reusable development workflows, including component API guidance, audit history, logging, testing, Qt component test conventions, VisualCheck review, and the removal of docs-backed workflow skill wrappers.

## Requirements

### Requirement: Canonical Development Workflow Documentation
The repository SHALL publish reusable development workflow guidance as project-neutral documentation under `docs/development/`.

#### Scenario: Workflow docs are present
- **WHEN** a maintainer looks for component API, audit, logging, testing, Qt component test, or visual review guidance
- **THEN** the repository MUST provide corresponding docs under `docs/development/` with project-neutral file names

#### Scenario: README points to docs
- **WHEN** README references logging, testing, component API, or visual review workflow guidance
- **THEN** it MUST link to `docs/development/` documents rather than long-form skill files

### Requirement: Source Comment Style Documentation
The repository SHALL expose source comment style guidance through the canonical development workflow documentation.

#### Scenario: Development docs link source comment guidance
- **WHEN** a maintainer reads `docs/development/README.md`
- **THEN** it MUST link to the source comment style guidance document

#### Scenario: Agent guidance references source comment rules
- **WHEN** agent-facing repository guidance is inspected
- **THEN** it MUST point contributors and agents to the source comment style guidance for new or materially edited source comments

### Requirement: Project-Neutral Workflow Naming
Workflow document file names, headings, and current guidance text SHALL avoid temporary project branding such as `Fluent-QT`.

#### Scenario: Current workflow names are neutral
- **WHEN** workflow docs and live repository references are inspected
- **THEN** their names MUST use neutral terms such as `component-api-conventions`, `component-api-audit`, `logging-workflow`, `testing-workflow`, `qt-component-test-conventions`, and `visual-review`

#### Scenario: Historical facts remain traceable
- **WHEN** the component API audit document records the original audit change or existing build target names
- **THEN** it MAY retain those historical references while keeping the document title, reusable guidance, and current links project-neutral

### Requirement: Docs-Backed Workflow Skills Are Removed
The repository SHALL remove docs-backed workflow skill files for the migrated development workflows after their canonical docs exist.

#### Scenario: Migrated workflow skills are absent
- **WHEN** `.codex/skills` and `.github/skills` are inspected
- **THEN** they MUST NOT contain `component-api-audit`, `component-api-conventions`, `logging-workflow`, `testing-workflow`, `qt-component-test-conventions`, or `visual-review` skill files

#### Scenario: OpenSpec workflow skills remain
- **WHEN** `.codex/skills` and `.github/skills` are inspected
- **THEN** OpenSpec workflow skills such as `openspec-propose`, `openspec-apply-change`, `openspec-archive-change`, and `openspec-explore` MAY remain because they are executable workflow entrypoints

### Requirement: Existing Development Contracts Are Preserved
The migration SHALL preserve the existing effective guidance for component APIs, logging, testing, Qt component test structure, and VisualCheck review.

#### Scenario: Component API guidance is preserved
- **WHEN** a maintainer reads the migrated component API docs
- **THEN** the docs MUST retain guidance for inheritance, ownership, properties, setter no-op behavior, nullable date/time values, open state, selection naming, fix-or-defer decisions, and focused tests

#### Scenario: Logging guidance is preserved
- **WHEN** a maintainer reads the migrated logging workflow docs
- **THEN** the docs MUST retain guidance for `src/utils/Log.h`, `LOG_*` macros, `SPDLOG_LEVEL`, `SPDLOG_FILE`, Qt log bridge behavior, and scoped diagnostic anchors

#### Scenario: Testing guidance is preserved
- **WHEN** a maintainer reads the migrated testing docs
- **THEN** the docs MUST retain guidance for `add_qt_test_module`, CTest labels, focused builds, `SKIP_VISUAL_TEST`, VisualCheck execution, and component directory synchronization

### Requirement: Migration Validation
The migration SHALL include validation that stale workflow-skill references and hard-coded checkout paths were removed from live guidance.

#### Scenario: Old skill links are gone
- **WHEN** live README, docs, `.codex/skills`, `.github/skills`, and agent instruction files are searched
- **THEN** references to `Fluent-QT-*` workflow skill names MUST NOT remain except in archived OpenSpec history or explicitly historical audit notes

#### Scenario: Local checkout paths are gone
- **WHEN** live workflow docs and agent instruction files are searched
- **THEN** they MUST NOT require the current `/Users/calvinhxx/Ws/qttest/Fluent-QT` checkout path
