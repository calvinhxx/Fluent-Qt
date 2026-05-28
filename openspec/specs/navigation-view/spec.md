# navigation-view Specification

## Purpose

The system SHALL provide `view::navigation::NavigationView` as a lightweight Fluent shell layout container that owns chrome/content geometry while leaving application navigation items, selection, commands, and page routing to caller-supplied widgets and `StackContentHost`.

## Requirements

### Requirement: NavigationView public API
The system SHALL provide `view::navigation::NavigationView` as a Qt Widgets Fluent shell layout container. The component MUST inherit `QWidget`, `FluentElement`, and `view::QMLPlus`, and MUST expose pane display mode, pane open state, configurable shell metrics, chrome slot widgets, a `StackContentHost`, theme refresh, and geometry query APIs.

#### Scenario: Default construction
- **WHEN** a `NavigationView` is constructed
- **THEN** it MUST use auto pane display mode, keep the pane open by default, create a non-null `StackContentHost`, expose usable size hints, and set an accessible name

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/navigation/NavigationView.h`
- **THEN** `NavigationView` MUST provide QWidget child containment, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals

### Requirement: Chrome slot model
The system SHALL expose three optional chrome QWidget slots named header, main, and footer. The component MUST lay out these slots, but MUST NOT own navigation item models, item selection, settings/search/back commands, or nested item expansion behavior.

#### Scenario: Set chrome widgets
- **WHEN** callers assign header, main, or footer chrome widgets
- **THEN** each widget MUST become a child of `NavigationView`, receive chrome slot geometry, and emit the matching changed signal once per effective replacement

#### Scenario: Replace chrome widget
- **WHEN** callers replace a chrome widget
- **THEN** the previous widget MUST be detached from `NavigationView` without being deleted and the new widget MUST receive the slot geometry

#### Scenario: Application-owned chrome content
- **WHEN** applications need lists, commands, search, settings, indicators, selection, or nested expansion
- **THEN** they MUST compose those behaviors inside the supplied chrome widgets rather than relying on `NavigationView` item APIs

### Requirement: Side shell layout
The system SHALL support side shell layouts for expanded, compact, and minimal pane presentations.

#### Scenario: Left expanded layout
- **WHEN** the resolved pane mode is `Left`
- **THEN** chrome MUST reserve the configured expanded pane width with a 320px default, content MUST start to the right of chrome, header MUST sit at the top of chrome, footer MUST sit at the bottom, and main MUST fill the remaining chrome height

#### Scenario: Left compact layout
- **WHEN** the resolved pane mode is `LeftCompact`
- **THEN** closed chrome MUST reserve the configured compact pane width with a 48px default, content MUST start to the right of the compact rail, and opened chrome MUST use the configured expanded pane width

#### Scenario: Left minimal layout
- **WHEN** the resolved pane mode is `LeftMinimal`
- **THEN** closed chrome MUST be empty and content MUST fill the control, while opened chrome MUST use the configured expanded pane width without moving content

#### Scenario: Side resize
- **WHEN** the NavigationView is resized in a side mode
- **THEN** it MUST recompute chrome, content, header, main, footer, and `StackContentHost` geometry without mutating application chrome content

### Requirement: Top shell layout
The system SHALL support a top shell layout.

#### Scenario: Top chrome geometry
- **WHEN** the resolved pane mode is `Top`
- **THEN** chrome MUST reserve the configured top bar height with a 48px default and content MUST start below chrome

#### Scenario: Top chrome slot order
- **WHEN** header, main, and footer chrome widgets are assigned in top mode
- **THEN** header MUST be placed first, main MUST be placed after header, footer MUST be aligned to the right edge, and unused space between main and footer MUST behave as stretch

### Requirement: Auto pane display mode
The system SHALL resolve automatic pane display mode from available width using caller-configurable thresholds.

#### Scenario: Auto expanded threshold
- **WHEN** pane display mode is `Auto` and width is at or above the expanded threshold
- **THEN** resolved display mode MUST use `Left`

#### Scenario: Auto compact threshold
- **WHEN** pane display mode is `Auto` and width is below the expanded threshold but at or above the compact threshold
- **THEN** resolved display mode MUST use `LeftCompact`

#### Scenario: Auto minimal threshold
- **WHEN** pane display mode is `Auto` and width is below the compact threshold
- **THEN** resolved display mode MUST use `LeftMinimal`

#### Scenario: Custom thresholds and metrics
- **WHEN** callers set threshold, pane width, compact width, or top bar height properties
- **THEN** the component MUST normalize invalid values, recompute shell geometry, and emit changed signals once per effective change

### Requirement: StackContentHost content surface
The system SHALL use `StackContentHost` as the content surface. Applications MUST add and switch pages through `contentHost()`.

#### Scenario: Content host geometry
- **WHEN** the layout is recomputed
- **THEN** `contentHost()` MUST receive the current content rectangle geometry

#### Scenario: Application-owned page switching
- **WHEN** applications insert pages and change the current index on `contentHost()`
- **THEN** `NavigationView` MUST preserve the content host geometry and MUST NOT infer page selection from chrome widgets

### Requirement: Fluent visual shell
The system SHALL paint only shell-level surfaces.

#### Scenario: Shell background
- **WHEN** the component is painted
- **THEN** it MUST paint the canvas background, content background, chrome background, and chrome/content divider using Fluent semantic colors

#### Scenario: Theme refresh
- **WHEN** the theme is updated
- **THEN** the component MUST refresh the `StackContentHost` and chrome widgets without changing shell geometry

### Requirement: Tests and VisualCheck
The system SHALL include NavigationView source and tests in the normal CMake build.

#### Scenario: Build target
- **WHEN** `fluent_qt_lib` is built
- **THEN** `src/view/navigation/NavigationView.cpp` and `src/view/navigation/NavigationView.h` MUST participate in compilation

#### Scenario: NavigationView test target
- **WHEN** `test_navigation_view` is built
- **THEN** it MUST compile and link against `fluent_qt_lib`

#### Scenario: VisualCheck sample
- **WHEN** the VisualCheck test is run without `SKIP_VISUAL_TEST`
- **THEN** it MUST show an application-composed chrome using the header, main, and footer slots plus a `StackContentHost` content page
