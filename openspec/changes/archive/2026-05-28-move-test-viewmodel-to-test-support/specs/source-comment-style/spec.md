## MODIFIED Requirements

### Requirement: Canonical Source Comment Guidance
The repository SHALL provide canonical source comment style guidance for production code under `src/`.

#### Scenario: Comment style guidance exists
- **WHEN** a maintainer looks for source comment rules
- **THEN** the repository MUST provide `docs/development/comment-style.md`

#### Scenario: Guidance covers source modules
- **WHEN** the source comment guidance is read
- **THEN** it MUST describe expectations for `src/design/`, `src/view/foundation/`, `src/view/**`, `src/utils/`, and `src/compatibility/`
- **AND** it MUST NOT describe removed or test-only helper areas such as `src/viewmodel/` as active production source modules
