## ADDED Requirements

### Requirement: AnnotatedScrollBar uses fluent namespace
The AnnotatedScrollBar capability SHALL expose its public component API as `fluent::scrolling::AnnotatedScrollBar` and use `fluent::QMLPlus` for component foundation support.

#### Scenario: AnnotatedScrollBar public name is migrated
- **WHEN** code includes `components/scrolling/AnnotatedScrollBar.h`
- **THEN** the component MUST be available as `fluent::scrolling::AnnotatedScrollBar`
- **AND** active requirements, tests, and docs for this capability MUST NOT use `view::scrolling::AnnotatedScrollBar` or `view::QMLPlus` as canonical names
