## ADDED Requirements

### Requirement: DrawerView public API
The system SHALL provide `view::collections::DrawerView` as a Qt Widgets Fluent edge drawer component. The component MUST inherit `QWidget`, `FluentElement`, and `view::QMLPlus`, and MUST expose QML Drawer-inspired properties for edge placement, open state, normalized position, drawer length, modality, dimming, close policy, interactivity, drag margin, outer corner radius, animation enablement, content widget hosting, theme refresh, and geometry queries.

#### Scenario: Default construction
- **WHEN** a `DrawerView` is constructed
- **THEN** it MUST be closed, have `position()` equal to 0.0, use the left edge, have a positive default drawer length, use a positive default outer corner radius, be modal and dimmed by default, enable interaction and animation by default, include outside-press and Escape close policies, expose usable size hints, and remain hidden until opened

#### Scenario: Component inheritance pattern
- **WHEN** code includes `src/view/collections/DrawerView.h`
- **THEN** `DrawerView` MUST provide QWidget child containment, Fluent theme access, and QMLPlus anchors/binding support

#### Scenario: Repeated property value
- **WHEN** a property setter receives the value already stored by the component
- **THEN** the component MUST NOT emit duplicate changed signals or restart animations

#### Scenario: Disabled component
- **WHEN** the DrawerView is disabled
- **THEN** pointer drag gestures and light-dismiss pointer handling MUST NOT change `isOpen()` or `position()`

### Requirement: Same-window open and close lifecycle
The system SHALL show DrawerView as a same-window overlay attached to the top-level widget resolved from its original parent. DrawerView MUST NOT create a `QDialog`, native window, or independent top-level window.

#### Scenario: Open attaches to top-level widget
- **WHEN** `open()` is called on a closed DrawerView with a descendant QWidget parent
- **THEN** DrawerView MUST become a child of the resolved top-level widget, MUST NOT have `Qt::Window` or `Qt::Dialog` flags, MUST become visible, and MUST animate or set `position()` toward 1.0

#### Scenario: Open lifecycle signals
- **WHEN** `open()` transitions a closed DrawerView to open
- **THEN** DrawerView MUST emit `aboutToShow()` before becoming visible, emit `isOpenChanged(true)` exactly once, and emit `opened()` when the drawer is fully open

#### Scenario: Close lifecycle signals
- **WHEN** `close()` transitions an open DrawerView to closed
- **THEN** DrawerView MUST emit `aboutToHide()` before the closing transition, animate or set `position()` toward 0.0, hide after the transition completes, emit `isOpenChanged(false)` exactly once, and emit `closed()` exactly once

#### Scenario: Animation disabled lifecycle
- **WHEN** `animationEnabled` is false and `open()` or `close()` is called
- **THEN** DrawerView MUST complete the transition synchronously, set `position()` to 1.0 or 0.0 immediately, and emit the same lifecycle signals without waiting for timers

#### Scenario: Parent destruction safety
- **WHEN** the original parent or resolved top-level widget is destroyed while a DrawerView exists
- **THEN** DrawerView MUST remove installed event filters, clear dangling pointers, and avoid accessing destroyed widgets

### Requirement: Edge placement and geometry
The system SHALL support left, right, top, and bottom drawer edges. Drawer geometry MUST be deterministic from the top-level content rect, selected edge, configured drawer length, and normalized position.

#### Scenario: Left edge geometry
- **WHEN** edge is `Left`, drawer length is 320, the top-level size is 800x600, and `position()` is 1.0
- **THEN** the drawer panel geometry MUST be x=0, y=0, width=320, height=600

#### Scenario: Right edge geometry
- **WHEN** edge is `Right`, drawer length is 320, the top-level size is 800x600, and `position()` is 1.0
- **THEN** the drawer panel geometry MUST be x=480, y=0, width=320, height=600

#### Scenario: Top edge geometry
- **WHEN** edge is `Top`, drawer length is 240, the top-level size is 800x600, and `position()` is 1.0
- **THEN** the drawer panel geometry MUST be x=0, y=0, width=800, height=240

#### Scenario: Bottom edge geometry
- **WHEN** edge is `Bottom`, drawer length is 240, the top-level size is 800x600, and `position()` is 1.0
- **THEN** the drawer panel geometry MUST be x=0, y=360, width=800, height=240

#### Scenario: Partial position geometry
- **WHEN** the drawer is on the left edge with length 320 and `position()` is 0.5
- **THEN** the visible panel MUST be offset halfway from the hidden position toward the open position while retaining its full drawer length for child layout and painting

#### Scenario: Drawer length normalization
- **WHEN** callers set a drawer length less than one or larger than the available top-level axis length
- **THEN** DrawerView MUST preserve a positive configured value, clamp effective geometry to the available top-level axis during layout, and emit `drawerLengthChanged(int)` only for effective property changes

#### Scenario: Top-level resize
- **WHEN** the top-level widget is resized while DrawerView is open or partially open
- **THEN** DrawerView MUST recompute overlay, panel, scrim, and content geometry without changing `edge()`, configured drawer length, or open state

### Requirement: Content widget hosting
The system SHALL allow DrawerView to host one arbitrary QWidget content item inside the drawer panel.

#### Scenario: Set content widget
- **WHEN** `setContentWidget(widget)` is called with a non-null QWidget
- **THEN** the widget MUST become a child of DrawerView, be shown when the drawer is visible, be laid out to the drawer content rectangle, and `contentWidgetChanged(widget)` MUST emit once

#### Scenario: Replace content widget
- **WHEN** a second content widget replaces an existing content widget
- **THEN** the previous widget MUST be detached from DrawerView without being deleted, the new widget MUST become the hosted content, and content geometry MUST update

#### Scenario: Clear content widget
- **WHEN** `setContentWidget(nullptr)` is called
- **THEN** DrawerView MUST detach the previous content widget without deleting it and keep open/close behavior working with an empty drawer surface

#### Scenario: Content geometry query
- **WHEN** tests query `contentGeometry()` after layout
- **THEN** DrawerView MUST return the latest content rectangle for the hosted widget and an empty rectangle only when no valid layout is available

### Requirement: Modal, dim, and close policy behavior
The system SHALL support QML Drawer-style modal and light-dismiss behavior using same-window event handling.

#### Scenario: Modal drawer blocks background input
- **WHEN** DrawerView opens with `modal` true
- **THEN** an internal scrim MUST cover the top-level area behind the drawer and background widgets MUST NOT receive pointer input through the scrim

#### Scenario: Dimmed drawer paints smoke overlay
- **WHEN** DrawerView opens with `dim` true
- **THEN** the scrim MUST paint a Fluent smoke/material overlay behind the drawer panel

#### Scenario: Non-modal drawer keeps background interactive
- **WHEN** DrawerView opens with `modal` false
- **THEN** background widgets outside the drawer MUST remain interactive except for event-filtered light-dismiss handling required by the close policy

#### Scenario: Outside press light-dismiss
- **WHEN** DrawerView is open, close policy includes `CloseOnPressOutside`, and the user presses outside the drawer panel
- **THEN** DrawerView MUST close and the original outside press MUST be allowed to continue to the target widget when the drawer is non-modal

#### Scenario: Escape light-dismiss
- **WHEN** DrawerView is open, close policy includes `CloseOnEscape`, and Escape is pressed in the drawer or its top-level widget
- **THEN** DrawerView MUST close

#### Scenario: No auto close policy
- **WHEN** close policy is `NoAutoClose`
- **THEN** outside presses and Escape MUST NOT close the drawer; only explicit `close()` or `setIsOpen(false)` may close it

### Requirement: Interactive drag behavior
The system SHALL support pointer drag gestures that update DrawerView position when `interactive` is true.

#### Scenario: Edge drag opens closed drawer
- **WHEN** DrawerView is closed, `interactive` is true, and the user presses within `dragMargin` of the configured top-level edge then drags inward
- **THEN** DrawerView MUST become visible, update `position()` proportionally to the drag distance, and settle open when released beyond the halfway threshold

#### Scenario: Drawer drag closes open drawer
- **WHEN** DrawerView is open, `interactive` is true, and the user drags the drawer panel outward toward its hidden edge
- **THEN** DrawerView MUST update `position()` proportionally to the drag distance and settle closed when released below the halfway threshold

#### Scenario: Interactive disabled
- **WHEN** `interactive` is false
- **THEN** edge presses and drawer drags MUST NOT change `position()` or open/close state

#### Scenario: Drag margin normalization
- **WHEN** callers set `dragMargin` to a negative value
- **THEN** DrawerView MUST normalize it to zero and emit `dragMarginChanged(int)` only once per effective change

#### Scenario: Outer corner radius configuration
- **WHEN** callers set `outerCornerRadius` to a positive value
- **THEN** DrawerView MUST round only the two drawer corners opposite the attached edge and emit `outerCornerRadiusChanged(int)` once per effective change
- **WHEN** callers set `outerCornerRadius` to a negative value
- **THEN** DrawerView MUST normalize it to zero

### Requirement: Fluent visuals, animation, and theme refresh
The system SHALL render DrawerView using Fluent design tokens and update visuals safely across theme changes.

#### Scenario: Drawer panel visual
- **WHEN** DrawerView is visible
- **THEN** it MUST paint the drawer panel background, edge stroke, and shadow/elevation using Fluent semantic tokens and MUST NOT rely on QSS or QPalette for the drawer surface

#### Scenario: Position animation
- **WHEN** `open()` or `close()` starts with `animationEnabled` true
- **THEN** DrawerView MUST animate `position()` using Fluent animation duration and easing tokens and emit `positionChanged(qreal)` as the value changes

#### Scenario: Dim opacity follows drawer position
- **WHEN** DrawerView is dimmed and `position()` changes during open, close, or drag
- **THEN** the scrim opacity MUST follow the normalized drawer position so the dim overlay fades in and out instead of appearing or disappearing abruptly

#### Scenario: Theme update
- **WHEN** the Fluent theme changes while DrawerView exists
- **THEN** `onThemeUpdated()` MUST refresh drawer and scrim visuals and repaint without changing `isOpen()`, `position()`, content widget, or configured edge

#### Scenario: Size hints
- **WHEN** `sizeHint()` or `minimumSizeHint()` is queried
- **THEN** DrawerView MUST return non-empty values suitable for embedding in tests and layouts, based on drawer length and content hints where available

### Requirement: Tests and VisualCheck
The system SHALL include DrawerView source, automated tests, and VisualCheck coverage in the normal CMake build.

#### Scenario: Build target
- **WHEN** `fluent_qt_lib` is built
- **THEN** `src/view/collections/DrawerView.cpp` and `src/view/collections/DrawerView.h` MUST participate in compilation through the existing source discovery

#### Scenario: DrawerView test target
- **WHEN** tests are configured
- **THEN** `tests/views/collections/CMakeLists.txt` MUST register `test_drawer_view`, and the target MUST compile and link against `fluent_qt_lib`

#### Scenario: Automated tests
- **WHEN** `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/collections/test_drawer_view` is run
- **THEN** non-VisualCheck tests MUST cover default properties, open/close lifecycle, animation-disabled transitions, all edge geometries, content parenting/replacement, modal and non-modal close policies, interactive drag behavior, duplicate-signal suppression, top-level resize, disabled behavior, and theme refresh safety

#### Scenario: VisualCheck sample
- **WHEN** `test_drawer_view --gtest_filter="*VisualCheck*"` is run manually without `SKIP_VISUAL_TEST=1`
- **THEN** VisualCheck MUST display a Fluent sample window with controls to open left/right/top/bottom drawers, toggle modal/dim/interactive options, show hosted QWidget content, and switch Light/Dark theme

#### Scenario: VisualCheck automation guard
- **WHEN** `SKIP_VISUAL_TEST=1` is set
- **THEN** the DrawerView VisualCheck test MUST skip without opening an interactive window
