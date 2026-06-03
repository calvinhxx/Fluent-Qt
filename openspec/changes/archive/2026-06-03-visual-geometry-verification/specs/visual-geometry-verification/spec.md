## ADDED Requirements

### Requirement: Stable visual widget identifiers
The system SHALL provide stable Qt object names for app-owned widgets that are part of an app visual geometry contract.

#### Scenario: Gallery title bar widgets are discoverable
- **WHEN** a test constructs the gallery window
- **THEN** the title bar back button, menu button, app icon, title label, and search box MUST be discoverable by stable object names
- **AND** those names MUST NOT require access to implementation-local pointers

#### Scenario: Reusable components are not forced into the app contract
- **WHEN** reusable components under `src/components/` already have stable component tests
- **THEN** app visual geometry verification MUST NOT require renaming component-owned child widgets
- **AND** app geometry tests MUST prefer app-owned widget names and public component geometry

### Requirement: Geometry contract assertions
The system SHALL provide app-focused test support for asserting widget geometry contracts with explicit tolerances.

#### Scenario: Vertical center alignment is verified
- **WHEN** a test verifies controls that should be vertically centered inside a parent visual region
- **THEN** the test helper MUST compare widget center Y coordinates against the parent or reference center
- **AND** the helper MUST allow a caller-specified pixel tolerance

#### Scenario: Size and spacing contracts are verified
- **WHEN** a test verifies fixed visual dimensions or spacing between named widgets
- **THEN** the test helper MUST report expected and actual rectangles, sizes, centers, and spacing on failure
- **AND** the failure output MUST be readable without inspecting a screenshot

#### Scenario: High DPI icon dimensions are verified
- **WHEN** a test verifies an icon-bearing widget or pixmap-backed label
- **THEN** the logical widget size MUST be asserted separately from device-pixel backing data when such data is available
- **AND** device pixel ratio differences MUST NOT cause false failures for correct logical geometry

### Requirement: Opt-in geometry dump
The system SHALL support opt-in structured geometry dumps for app visual debugging.

#### Scenario: Dump selected named widgets
- **WHEN** geometry dump output is explicitly enabled for a test or gallery diagnostic run
- **THEN** the system MUST print stable object name, class name, visibility, enabled state, local rectangle, global rectangle when available, size hint, and center for selected widgets
- **AND** normal CTest runs MUST remain quiet unless the dump is enabled

#### Scenario: Dump scoped widget subtree
- **WHEN** a developer requests a dump for a parent widget subtree
- **THEN** the system MUST include descendants in object-tree order
- **AND** unnamed widgets MAY be included only when full subtree output is requested

### Requirement: Geometry-first visual review workflow
The system SHALL document and preserve an app-focused workflow where deterministic geometry evidence is collected before screenshot-based review for layout correctness issues.

#### Scenario: Layout bug investigation
- **WHEN** a developer investigates alignment, sizing, spacing, or containment problems in app/gallery UI
- **THEN** the recommended workflow MUST run focused geometry tests or inspect geometry dump output before relying on screenshot interpretation
- **AND** screenshots MAY still be used for final visual confirmation

#### Scenario: Subjective polish review
- **WHEN** a developer reviews typography, color, material, animation, perceived balance, or brand fidelity
- **THEN** manual VisualCheck or screenshot review MUST remain an accepted verification path
- **AND** geometry tests MUST NOT be treated as sufficient evidence for subjective visual polish
