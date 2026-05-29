## MODIFIED Requirements

### Requirement: Component inheritance and ownership conventions
The project SHALL document and test component inheritance and ownership conventions without forcing unrelated components into one base class.

#### Scenario: Widget-host components preserve Fluent mixins
- **WHEN** a public component is implemented as a QWidget-hosted Fluent control
- **THEN** it SHALL provide QWidget child containment, `fluent::FluentElement` theme access, and `fluent::QMLPlus` binding support either directly or through an established project base component

#### Scenario: Button-like controls use Button when appropriate
- **WHEN** a public component behaves as a clickable button-like entry surface and does not need a more specialized Qt item-view base
- **THEN** the audit SHALL evaluate whether it should derive from `fluent::basicinput::Button` or document why it intentionally does not

#### Scenario: Caller-owned content remains caller-owned
- **WHEN** a component is documented as a shell, container, view, or host whose content is supplied by callers
- **THEN** the component SHALL NOT absorb application-owned item composition, selection routing, or page choice as part of this consistency pass

## ADDED Requirements

### Requirement: Component API namespace conventions
The component API consistency contract SHALL treat `fluent::...` as the canonical public namespace for reusable component and component foundation APIs.

#### Scenario: API audits reject legacy view namespace
- **WHEN** a component API consistency audit is performed after the namespace migration
- **THEN** active component declarations, tests, docs, and live specs MUST use `fluent::...` for component APIs
- **AND** findings MUST be recorded for any active `view::...` component API spelling outside archived history or explicit migration notes
