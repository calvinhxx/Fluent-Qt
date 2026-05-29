## ADDED Requirements

### Requirement: Flyout uses fluent namespace
The Flyout capability SHALL expose Popup and Flyout public APIs under `fluent::dialogs_flyouts`.

#### Scenario: Flyout public names are migrated
- **WHEN** code includes `components/dialogs_flyouts/Flyout.h` or `components/dialogs_flyouts/Popup.h`
- **THEN** `Flyout`, `Popup`, and related public types MUST be available under `fluent::dialogs_flyouts`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::dialogs_flyouts` as the canonical component namespace
