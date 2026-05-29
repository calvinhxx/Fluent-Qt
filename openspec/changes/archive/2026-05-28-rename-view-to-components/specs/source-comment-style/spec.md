## MODIFIED Requirements

### Requirement: Canonical Source Comment Guidance
The repository SHALL provide canonical source comment style guidance for code under `src/`.

#### Scenario: Comment style guidance exists
- **WHEN** a maintainer looks for source comment rules
- **THEN** the repository MUST provide `docs/development/comment-style.md`

#### Scenario: Guidance covers source modules
- **WHEN** the source comment guidance is read
- **THEN** it MUST describe expectations for `src/design/`, `src/components/foundation/`, `src/components/**`, `src/utils/`, and `src/compatibility/`
- **AND** it MUST NOT describe removed or test-only helper areas as active production source modules

#### Scenario: Test helper model comments are test-owned
- **WHEN** test-only QObject helper models need comments
- **THEN** the guidance MUST direct them to `tests/support/` or local test fixtures rather than production source module guidance

### Requirement: Mechanical View Header Coverage
The change SHALL allow a mechanical comment fill pass for public component headers under `src/components/**/*.h` only when the result remains semantically useful.

#### Scenario: Missing view header comments are filled
- **WHEN** a public component header class or property has no nearby source comment
- **THEN** the implementation MAY add a concise bilingual Doxygen comment without changing declarations or behavior

#### Scenario: Placeholder comments are avoided
- **WHEN** generated or bulk-added component header comments are reviewed
- **THEN** they MUST describe the component contract or property behavior rather than using generic phrases such as "view component" or "`name` property"

#### Scenario: Implementation behavior is unchanged
- **WHEN** the mechanical component-header pass is reviewed
- **THEN** it MUST remain comment-only except for whitespace or indentation cleanup needed around touched declarations
