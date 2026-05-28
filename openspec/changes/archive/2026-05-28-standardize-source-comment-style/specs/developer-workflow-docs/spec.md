## ADDED Requirements

### Requirement: Source Comment Style Documentation
The repository SHALL expose source comment style guidance through the canonical development workflow documentation.

#### Scenario: Development docs link source comment guidance
- **WHEN** a maintainer reads `docs/development/README.md`
- **THEN** it MUST link to the source comment style guidance document

#### Scenario: Agent guidance references source comment rules
- **WHEN** agent-facing repository guidance is inspected
- **THEN** it MUST point contributors and agents to the source comment style guidance for new or materially edited source comments
