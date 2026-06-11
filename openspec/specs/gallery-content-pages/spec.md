# gallery-content-pages Specification

## Purpose

Define the Gallery app content pages layer: the static content catalog, page factory, category overview pages, component detail pages, sample cards, seeded component samples, and theme refresh behavior for the right-side content area of the Gallery shell.

## Requirements

### Requirement: Gallery content catalog maps routes to page metadata
The Gallery app SHALL provide a static content catalog that maps Gallery route IDs to right-side page metadata without requiring runtime source-tree scanning or reusable component API changes.

#### Scenario: Seeded content routes are available
- **WHEN** the Gallery content catalog is queried
- **THEN** it MUST expose page metadata for Home, the Basic input category, the Collections category, the Navigation category, `button`, `tree-view`, and `tab-view`
- **AND** each exposed route ID MUST correspond to a route known by the Gallery navigation view model

#### Scenario: Content catalog stays app-owned
- **WHEN** Gallery content metadata is implemented
- **THEN** the metadata MUST live under the Gallery app layer
- **AND** reusable components under `src/components/` MUST NOT gain Gallery-specific catalog, page, sample, or documentation APIs

### Requirement: Gallery page factory creates route-specific pages
The Gallery app SHALL create right-side page widgets through an app-layer page factory that maps selected routes to Home, category, component, Settings, or fallback page types.

#### Scenario: Category route creates category page
- **WHEN** a user activates a seeded component category route
- **THEN** the page factory MUST create a category overview page for that route
- **AND** the page MUST expose the selected route ID for tests and diagnostics

#### Scenario: Component route creates component page
- **WHEN** a user activates `button`, `tree-view`, or `tab-view`
- **THEN** the page factory MUST create a component detail page for the selected route
- **AND** the page MUST expose the selected route ID, title, overview text, and sample count

#### Scenario: Unknown content route uses fallback page
- **WHEN** a known navigation route has no content catalog entry in this phase
- **THEN** the page factory MAY create a fallback placeholder page
- **AND** the fallback MUST still show the selected route title so route changes remain visually reviewable

### Requirement: Category overview pages summarize components
Category overview pages SHALL present category-level content and component cards for the components that belong to that category.

#### Scenario: Category overview lists component cards
- **WHEN** the Basic input category page is shown
- **THEN** it MUST show a category title and overview text
- **AND** it MUST include a component card for the `button` route

#### Scenario: Category cards retain route identity
- **WHEN** a category overview creates component cards
- **THEN** each card MUST expose the target component route ID
- **AND** each card MUST display the component title from the content or navigation catalog

### Requirement: Component detail pages contain structured sections
Component detail pages SHALL use a common page structure with title, overview, examples, and implementation notes sections.

#### Scenario: Component page shows standard sections
- **WHEN** the `button` component page is shown
- **THEN** it MUST show the component title
- **AND** it MUST include Overview and Examples sections
- **AND** it MUST include at least one live sample card

#### Scenario: Component page uses common layout
- **WHEN** `button`, `tree-view`, and `tab-view` pages are created
- **THEN** they MUST all use the same component detail page type
- **AND** they MUST differ by catalog data and sample factories rather than by independent page layouts

### Requirement: Sample cards host live preview and code
Gallery sample cards SHALL present a consistent card shell containing sample metadata, a live preview widget, and an optional code snippet view.

#### Scenario: Sample card renders live preview
- **WHEN** a component sample card is created
- **THEN** it MUST contain a live preview widget produced by the sample factory
- **AND** the preview widget MUST be hosted inside the card rather than replacing the whole page

#### Scenario: Sample card exposes code snippet
- **WHEN** a sample definition contains a code snippet
- **THEN** the sample card MUST expose a code snippet area
- **AND** the snippet MUST be readable without requiring external files

### Requirement: Seeded component samples are representative
The first Gallery content implementation SHALL seed representative samples for `Button`, `TreeView`, and `TabView` to validate basic input, collection, and navigation component content patterns.

#### Scenario: Button page has button samples
- **WHEN** the `button` component page is shown
- **THEN** it MUST include live examples for at least standard, accent, disabled, and icon button usage

#### Scenario: TreeView page has hierarchy sample
- **WHEN** the `tree-view` component page is shown
- **THEN** it MUST include a live hierarchy sample that demonstrates expandable items

#### Scenario: TabView page has tab sample
- **WHEN** the `tab-view` component page is shown
- **THEN** it MUST include a live tab sample that demonstrates selected content hosting

### Requirement: Content pages support theme refresh
Gallery content pages SHALL refresh their visible colors, text, cards, and sample surfaces when the Fluent theme changes.

#### Scenario: Theme update reaches content page
- **WHEN** a Gallery content page receives `onThemeUpdated()`
- **THEN** the page MUST update its own visual surface
- **AND** it MUST propagate theme refresh to child Fluent controls that participate in the sample or page chrome
