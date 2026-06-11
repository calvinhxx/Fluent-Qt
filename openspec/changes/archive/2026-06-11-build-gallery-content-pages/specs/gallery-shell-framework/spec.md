## MODIFIED Requirements

### Requirement: Navigation switches colored placeholder content
The Gallery shell framework SHALL route left navigation activation to the right content area. Routes covered by Gallery content metadata MUST show route-specific content pages, while routes that are not yet covered MAY continue to use visually distinct placeholder fallback pages.

#### Scenario: Selecting a seeded navigation entry updates content
- **WHEN** a user activates a seeded category or component entry in the left pane
- **THEN** the right content area MUST switch to a content page for that entry
- **AND** the content page MUST expose the selected route ID for tests and diagnostics

#### Scenario: Fallback pages remain visually distinguishable
- **WHEN** a user activates a known navigation entry that has no content metadata in this phase
- **THEN** the right content area MAY switch to a placeholder page for that entry
- **AND** the placeholder page MUST include a `Label` naming the selected route
- **AND** different fallback placeholder pages MUST use distinct background or accent colors sufficient for manual visual review of route changes

### Requirement: Gallery shell scope stays intentionally shallow
The Gallery shell framework SHALL support first-pass structured content pages and representative live samples while deferring full Gallery product features.

#### Scenario: Representative real samples are allowed
- **WHEN** the shell content phase is delivered
- **THEN** seeded component pages MAY contain live component sample cards and local code snippets
- **AND** those samples MUST remain app-layer content that uses public component APIs

#### Scenario: Advanced content behaviors are deferred
- **WHEN** the shell content phase is delivered
- **THEN** component pages MUST NOT contain search result lists, favorites, recent items, automatic source parsing, generated API documentation, or exhaustive sample coverage for every catalog component
- **AND** those behaviors MUST remain available for later Gallery changes

### Requirement: Gallery shell is covered by focused tests
The Gallery shell framework SHALL include focused automated tests for the shell and right-side content contracts.

#### Scenario: Shell construction is tested
- **WHEN** Gallery tests run in the existing Qt/GTest infrastructure
- **THEN** they MUST verify that the shell can be constructed without showing interactive visual UI
- **AND** they MUST verify expected navigation entries are available

#### Scenario: Content routing is tested
- **WHEN** Gallery tests activate representative category and component navigation entries programmatically
- **THEN** they MUST verify the selected route changes
- **AND** they MUST verify the right content area reflects the selected route with the expected page type

### Requirement: Gallery main navigation exposes component routes
The Gallery shell SHALL expose selectable component routes under their component category routes while keeping the route model independent from right-side sample implementation.

#### Scenario: Component entries are available under source categories
- **WHEN** the Gallery navigation model is queried
- **THEN** representative component route IDs from each user-facing component category MUST be available
- **AND** each component route MUST retain its parent category relationship

#### Scenario: Selecting seeded component route shows content page
- **WHEN** a user activates `button`, `tree-view`, or `tab-view` in the left navigation pane
- **THEN** the selected route MUST update to that component route
- **AND** the right content area MUST show a component content page for that component route

#### Scenario: Selecting unseeded component route can fall back
- **WHEN** a user activates a component entry that has no content metadata in this phase
- **THEN** the selected route MUST update to that component route
- **AND** the right content area MAY show a placeholder fallback page for that component route

### Requirement: Footer navigation remains Settings-only
The Gallery shell SHALL keep Settings in the `NavigationView` footer chrome and keep footer navigation separate from component catalog main content.

#### Scenario: Footer contains Settings route
- **WHEN** the Gallery window is constructed
- **THEN** the footer navigation pane MUST expose the Settings route
- **AND** the Settings route MUST continue to navigate to the Settings page

#### Scenario: Footer is excluded from component catalog
- **WHEN** component category and component route IDs are enumerated
- **THEN** the Settings route MUST NOT be included as a component catalog entry
