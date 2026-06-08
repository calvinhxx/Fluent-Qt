## ADDED Requirements

### Requirement: Gallery main navigation mirrors component source categories
The Gallery shell SHALL build the main left navigation content from a Gallery-owned component catalog that reflects the public component categories under `src/components/`.

#### Scenario: Component categories are exposed in main navigation
- **WHEN** the Gallery window is constructed
- **THEN** the main navigation pane MUST expose categories for Basic input, Collections, Date & time, Dialogs & flyouts, Menus & toolbars, Navigation, Scrolling, Status & info, Text fields, and Windowing
- **AND** Settings MUST NOT appear in the main navigation pane

#### Scenario: Internal foundation category is excluded
- **WHEN** the Gallery navigation catalog is built from the component source taxonomy
- **THEN** foundation infrastructure MUST NOT appear as a user-facing main navigation category

### Requirement: Gallery main navigation exposes component routes
The Gallery shell SHALL expose selectable component routes under their component category routes while keeping the route model independent from right-side sample implementation.

#### Scenario: Component entries are available under source categories
- **WHEN** the Gallery navigation model is queried
- **THEN** representative component route IDs from each user-facing component category MUST be available
- **AND** each component route MUST retain its parent category relationship

#### Scenario: Selecting component route keeps placeholder content
- **WHEN** a user activates a component entry in the left navigation pane
- **THEN** the selected route MUST update to that component route
- **AND** the right content area MUST show a placeholder page for that component route

### Requirement: Gallery navigation model is separated from pane rendering
The Gallery shell SHALL separate component catalog data, navigation tree/view-model state, and navigation pane widget rendering within the Gallery app layer.

#### Scenario: Pane consumes structured navigation nodes
- **WHEN** the navigation pane is rebuilt
- **THEN** it MUST consume structured navigation nodes that distinguish section headers, category routes, component routes, and footer routes
- **AND** it MUST NOT infer hierarchy from display text formatting

#### Scenario: Reusable NavigationView remains app agnostic
- **WHEN** the component catalog navigation is implemented
- **THEN** reusable `NavigationView` APIs under `src/components/navigation/` MUST NOT gain Gallery-specific catalog, route, or component-list responsibilities

### Requirement: Footer navigation remains Settings-only
The Gallery shell SHALL keep Settings in the `NavigationView` footer chrome and keep footer navigation separate from component catalog main content.

#### Scenario: Footer contains Settings route
- **WHEN** the Gallery window is constructed
- **THEN** the footer navigation pane MUST expose the Settings route
- **AND** the Settings route MUST continue to navigate to the Settings placeholder page

#### Scenario: Footer is excluded from component catalog
- **WHEN** component category and component route IDs are enumerated
- **THEN** the Settings route MUST NOT be included as a component catalog entry