## ADDED Requirements

### Requirement: Canonical Source Comment Guidance
The repository SHALL provide canonical source comment style guidance for code under `src/`.

#### Scenario: Comment style guidance exists
- **WHEN** a maintainer looks for source comment rules
- **THEN** the repository MUST provide `docs/development/comment-style.md`

#### Scenario: Guidance covers source modules
- **WHEN** the source comment guidance is read
- **THEN** it MUST describe expectations for `src/design/`, `src/view/foundation/`, `src/view/**`, `src/utils/`, `src/compatibility/`, and `src/viewmodel/`

### Requirement: Bilingual Public API Comments
Public source comments SHALL support both English and Chinese readers without requiring every source comment to be duplicated mechanically.

#### Scenario: Public API comment format is documented
- **WHEN** the source comment guidance describes comments for public headers
- **THEN** it MUST define a Doxygen block format with an English summary and a Chinese `zh_CN:` explanation for non-trivial public contracts

#### Scenario: Public API examples are present
- **WHEN** the source comment guidance is read
- **THEN** it MUST include examples for class-level and method-level public API comments

### Requirement: Intent-Based Implementation Comments
Implementation comments SHALL explain rationale, invariants, compatibility behavior, or non-obvious rendering/layout decisions rather than restating code.

#### Scenario: Implementation comment rule is documented
- **WHEN** the source comment guidance describes `.cpp` comments
- **THEN** it MUST say that implementation comments are not required to be bilingual unless the behavior is part of a reusable public contract

#### Scenario: Low-value comments are discouraged
- **WHEN** the source comment guidance is read
- **THEN** it MUST discourage comments that merely repeat getter, setter, local variable, or branch behavior already obvious from the code

### Requirement: Design And Compatibility Comment Coverage
Design token and compatibility headers SHALL use bilingual-friendly comments for reusable public contracts and non-obvious platform/version rules.

#### Scenario: Design token comments are bilingual-friendly
- **WHEN** design token headers under `src/design/` are updated
- **THEN** module, section, and non-obvious inline comments SHOULD provide an English anchor plus `zh_CN:` explanation

#### Scenario: Compatibility comments document version and platform rationale
- **WHEN** compatibility headers under `src/compatibility/` are updated
- **THEN** public comments MUST describe the normalized API surface and include Chinese `zh_CN:` explanations for Qt-version or platform-specific behavior

### Requirement: Shared Terminology
The repository SHALL define stable terminology for common component, Fluent UI, and platform concepts used in source comments.

#### Scenario: Glossary is available
- **WHEN** the source comment guidance is read
- **THEN** it MUST include a glossary for terms such as component, control, overlay, same-window overlay, light-dismiss, design token, selection, current item, host, and content host

#### Scenario: Terminology is project-neutral
- **WHEN** glossary terms and examples are inspected
- **THEN** they MUST avoid temporary project branding and local checkout names

### Requirement: Representative First Pass
The change SHALL demonstrate the comment style in representative source files without rewriting the full `src/` tree.

#### Scenario: Representative files are updated
- **WHEN** the implementation is complete
- **THEN** at least one high-signal file from design tokens, foundation infrastructure, or utilities MUST demonstrate the canonical comment style

#### Scenario: Full source rewrite is avoided
- **WHEN** the implementation diff is reviewed
- **THEN** it MUST NOT mechanically rewrite comments across every file under `src/`

### Requirement: Mechanical View Header Coverage
The change SHALL allow a mechanical comment fill pass for public view headers under `src/view/**/*.h` only when the result remains semantically useful.

#### Scenario: Missing view header comments are filled
- **WHEN** a public view header class or property has no nearby source comment
- **THEN** the implementation MAY add a concise bilingual Doxygen comment without changing declarations or behavior

#### Scenario: Placeholder comments are avoided
- **WHEN** generated or bulk-added view header comments are reviewed
- **THEN** they MUST describe the component contract or property behavior rather than using generic phrases such as "view component" or "`name` property"

#### Scenario: Implementation behavior is unchanged
- **WHEN** the mechanical view-header pass is reviewed
- **THEN** it MUST remain comment-only except for whitespace or indentation cleanup needed around touched declarations
