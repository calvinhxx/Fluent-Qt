## ADDED Requirements

### Requirement: Breadcrumb uses fluent namespace
The Breadcrumb capability SHALL expose its public navigation API under `fluent::navigation`.

#### Scenario: Breadcrumb public names are migrated
- **WHEN** code includes `components/navigation/Breadcrumb.h`
- **THEN** `Breadcrumb`, `BreadcrumbItem`, and related public enum types MUST be available under `fluent::navigation`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::navigation` as the canonical component namespace
