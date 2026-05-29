## ADDED Requirements

### Requirement: NavigationView uses fluent namespace
The NavigationView capability SHALL expose its public navigation API under `fluent::navigation`.

#### Scenario: NavigationView public names are migrated
- **WHEN** code includes `components/navigation/NavigationView.h` or `components/navigation/StackContentHost.h`
- **THEN** `NavigationView`, `StackContentHost`, and related public names MUST be available under `fluent::navigation`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::navigation` or `view::QMLPlus` as canonical component namespaces
