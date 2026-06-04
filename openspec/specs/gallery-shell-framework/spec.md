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