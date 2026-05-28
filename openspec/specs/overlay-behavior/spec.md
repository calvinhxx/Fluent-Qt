# overlay-behavior Specification

## Purpose

Define shared overlay behavior for Popup, Flyout, ComboBox dropdowns, DrawerView,
and future transient surfaces so placement, transparency, dismissal, scrims,
stacking, focus, animation, and validation remain consistent across components.

## Requirements

### Requirement: Overlay windowing model
The project SHALL define a shared overlay windowing model for Popup, Flyout, ComboBox dropdowns, DrawerView, and future transient surfaces.

#### Scenario: Same-window overlay attaches to owning top-level
- **WHEN** a same-window overlay is opened from a descendant widget
- **THEN** it SHALL attach to the owning top-level widget
- **AND** it SHALL NOT create a `Qt::Window`, `Qt::Dialog`, or independent native top-level window

#### Scenario: Native-window overlay requires explicit policy
- **WHEN** an overlay cannot be represented as a same-window overlay
- **THEN** the component SHALL document and test its native-window flags, transparency behavior, focus behavior, and stacking behavior before using an independent top-level window

#### Scenario: Top-level destruction is safe
- **WHEN** the original parent or resolved top-level widget is destroyed while an overlay exists
- **THEN** the overlay SHALL remove event filters, clear dangling pointers, close or detach safely, and avoid accessing destroyed widgets

### Requirement: Overlay geometry separates card, shadow, and content
Overlay components SHALL distinguish outer widget geometry, visible card geometry, and hosted content geometry.

#### Scenario: Shadow margin does not redefine placement
- **WHEN** an overlay is positioned relative to an anchor, edge, or explicit local point
- **THEN** placement calculations SHALL use the visible card geometry
- **AND** shadow margins SHALL NOT shift the logical visible card position seen by callers and tests

#### Scenario: Rounded visual card clips hosted content
- **WHEN** an overlay hosts a child viewport, list, or arbitrary QWidget content
- **THEN** the visible content area SHALL be inset, clipped, masked, or styled so square child backgrounds do not leak through rounded overlay corners

#### Scenario: Hit testing uses visible interactive regions
- **WHEN** a user presses inside the visible card or drawer panel
- **THEN** the overlay SHALL treat the press as inside even if the outer widget includes additional shadow margin
- **AND** outside-press light-dismiss SHALL only trigger for points outside the visible interactive region

### Requirement: Overlay transparent rendering
Overlay components SHALL render transparent outside-card areas consistently across supported Qt versions and platforms.

#### Scenario: Outside-card background remains transparent
- **WHEN** an overlay paints a rounded card or panel with shadow
- **THEN** pixels outside the visible card and shadow SHALL remain transparent over the owning top-level widget

#### Scenario: Overlay visuals use Fluent tokens
- **WHEN** an overlay paints its card, panel, border, shadow, dim scrim, or smoke layer
- **THEN** it SHALL use project Fluent theme tokens and custom painting rather than relying on QSS-only styling for the overlay surface

#### Scenario: Theme update preserves state
- **WHEN** the active Fluent theme changes while an overlay exists
- **THEN** the overlay SHALL repaint card, shadow, border, scrim, and hosted content styling without changing open state, placement, selected value, or hosted content ownership

### Requirement: Overlay light-dismiss policy
Overlay components SHALL share consistent close policy semantics for outside press and Escape handling.

#### Scenario: Escape follows close policy
- **WHEN** an overlay is open and its close policy includes Escape dismissal
- **THEN** pressing Escape in the overlay or owning top-level context SHALL close the overlay exactly once

#### Scenario: Outside press follows close policy
- **WHEN** an overlay is open and its close policy includes outside-press dismissal
- **THEN** pressing outside the visible card or panel SHALL close the overlay exactly once

#### Scenario: NoAutoClose suppresses implicit dismissal
- **WHEN** an overlay close policy is `NoAutoClose`
- **THEN** outside press and Escape SHALL NOT close it
- **AND** only explicit close APIs or equivalent state setters SHALL close it

#### Scenario: Non-modal outside press may continue
- **WHEN** a non-modal overlay closes due to outside press
- **THEN** the implementation SHALL define whether the original event continues to the background target
- **AND** Popup, Flyout, ComboBox dropdown, and non-modal DrawerView SHALL use a consistent documented behavior

### Requirement: Overlay modal scrim behavior
Overlay components SHALL share consistent modal and dim scrim semantics.

#### Scenario: Modal overlay blocks background input
- **WHEN** an overlay opens with modal behavior enabled
- **THEN** a same-window scrim or equivalent blocker SHALL prevent pointer interaction with background widgets behind the overlay

#### Scenario: Dim overlay paints smoke layer
- **WHEN** an overlay opens with dim behavior enabled
- **THEN** the scrim SHALL paint a Fluent smoke or dim layer behind the visible card or panel

#### Scenario: Non-modal overlay leaves background interactive
- **WHEN** an overlay opens with modal behavior disabled
- **THEN** background widgets outside the overlay SHALL remain interactive except for event filtering needed to implement documented light-dismiss behavior

#### Scenario: Scrim lifecycle matches overlay lifecycle
- **WHEN** a modal or dim overlay closes
- **THEN** its scrim SHALL be hidden, destroyed, or detached before background widgets can be left blocked by a stale overlay object

### Requirement: Overlay stacking and focus behavior
Overlay components SHALL manage z-order, focus, and activation without relying on incidental widget creation order.

#### Scenario: Overlay stack order is deterministic
- **WHEN** an overlay opens
- **THEN** the scrim SHALL appear above background widgets and below the visible card or panel
- **AND** hosted content SHALL appear above the card background

#### Scenario: Overlay raises above sibling widgets
- **WHEN** a same-window overlay opens or its top-level widget is resized
- **THEN** the overlay SHALL raise itself and any scrim to the documented stack order

#### Scenario: Closing restores safe focus state
- **WHEN** an overlay closes through explicit close, Escape, outside press, or selection
- **THEN** it SHALL leave focus in a safe state for the owning control or top-level widget
- **AND** it SHALL NOT trap keyboard focus in hidden hosted content

### Requirement: Overlay animation semantics
Overlay components SHALL expose deterministic behavior for enabled and disabled animation paths.

#### Scenario: Animation disabled completes synchronously
- **WHEN** overlay animation is disabled and an overlay opens or closes
- **THEN** all state, visibility, lifecycle signals, scrim state, geometry, and progress properties SHALL settle synchronously

#### Scenario: Animation enabled preserves lifecycle order
- **WHEN** overlay animation is enabled and an overlay opens or closes
- **THEN** lifecycle signals SHALL be emitted in a documented order and final open or closed signals SHALL be emitted exactly once

#### Scenario: Geometry remains stable during opacity-only Popup transitions
- **WHEN** a Popup-style overlay uses opacity animation
- **THEN** its visible card geometry SHALL remain stable while opacity changes

### Requirement: Overlay validation coverage
The project SHALL provide automated tests and VisualCheck coverage for the shared overlay behavior.

#### Scenario: Automated tests cover shared overlay rules
- **WHEN** the affected test targets run with `SKIP_VISUAL_TEST=1`
- **THEN** they SHALL verify same-window attachment, transparent-background-safe geometry, visible card hit testing, modal scrim blocking, non-modal dismissal, z-order, animation-disabled lifecycle, and destruction safety where applicable

#### Scenario: Cross-component tests remain focused
- **WHEN** Popup, Flyout, ComboBox dropdown, or DrawerView behavior is changed to satisfy the overlay contract
- **THEN** the focused test target for that component SHALL verify the component-specific behavior still works

#### Scenario: VisualCheck demonstrates overlay edge cases
- **WHEN** overlay VisualCheck cases run without `SKIP_VISUAL_TEST=1`
- **THEN** they SHALL make rounded corners, transparent outside-card areas, shadow, dim scrim, light-dismiss, z-order, nested content, and theme switching visually inspectable
