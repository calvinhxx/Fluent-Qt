# gallery-shell-framework Specification

## Purpose

Define the optional Fluent Gallery shell application framework, including its launchable window structure, intentionally shallow placeholder routing, navigation content, and focused automated shell coverage.

## Requirements

### Requirement: Gallery shell launches as an optional application
The Gallery shell framework SHALL provide an optional Qt Widgets application executable that opens a Fluent-style Gallery window without requiring changes to reusable component APIs.

#### Scenario: Gallery executable starts
- **WHEN** the Gallery application target is built and launched
- **THEN** the application MUST create a top-level Gallery window
- **AND** the window MUST contain a title/header area, left navigation area, and right content area

#### Scenario: Library APIs remain unchanged
- **WHEN** the Gallery shell framework is implemented
- **THEN** public component APIs under `src/components/` MUST NOT be changed for Gallery-specific catalog or navigation behavior

### Requirement: Gallery shell matches the reference layout structure
The Gallery shell framework SHALL follow the structural layout shown in the provided WinUI Gallery reference image: a persistent left navigation pane, a top header/search affordance, and a main content surface on the right.

#### Scenario: Header search affordance is visible
- **WHEN** the Gallery window is shown
- **THEN** the header area MUST include a search input affordance labelled for searching controls and samples
- **AND** the search input does not need to perform filtering in this phase

#### Scenario: Left navigation is visible
- **WHEN** the Gallery window is shown
- **THEN** the left navigation area MUST show entries for Home, Fundamentals, Design, Accessibility, Controls, and Settings
- **AND** the Controls area MUST expose first-pass component groups such as Basic input, Collections, Date & time, Dialogs & flyouts, Navigation, Scrolling, Status & info, Text fields, and Windowing

### Requirement: Navigation switches colored placeholder content
The Gallery shell framework SHALL provide route switching from the left navigation pane to visually distinct placeholder content pages in the right content area.

#### Scenario: Selecting a navigation entry updates content
- **WHEN** a user activates a navigation entry in the left pane
- **THEN** the right content area MUST switch to a placeholder page for that entry
- **AND** the placeholder page MUST include a `Label` naming the selected route

#### Scenario: Placeholder pages are visually distinguishable
- **WHEN** different navigation entries are selected
- **THEN** their placeholder pages MUST use distinct background or accent colors sufficient for manual visual review of route changes

### Requirement: Gallery shell scope stays intentionally shallow
The Gallery shell framework SHALL avoid implementing final component sample behavior during this phase.

#### Scenario: Real samples are deferred
- **WHEN** the shell framework is delivered
- **THEN** component pages MUST NOT contain real live component sample cards, source code snippets, favorites, recent items, or search result lists
- **AND** those behaviors MUST remain available for later Gallery changes

### Requirement: Gallery shell is covered by focused tests
The Gallery shell framework SHALL include focused automated tests for the shell contract.

#### Scenario: Shell construction is tested
- **WHEN** Gallery tests run in the existing Qt/GTest infrastructure
- **THEN** they MUST verify that the shell can be constructed without showing interactive visual UI
- **AND** they MUST verify expected navigation entries are available

#### Scenario: Placeholder routing is tested
- **WHEN** Gallery tests activate representative navigation entries programmatically
- **THEN** they MUST verify the selected route changes
- **AND** they MUST verify the right content placeholder reflects the selected route

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
