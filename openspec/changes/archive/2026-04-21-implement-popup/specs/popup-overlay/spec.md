## ADDED Requirements

### Requirement: Popup is a same-window overlay attached to the top-level widget

The Popup class SHALL be implemented as a QWidget-based overlay, not as a QDialog. When opened, it SHALL attach itself to the top-level widget of its original parent and appear above sibling widgets without creating a new OS top-level window.

#### Scenario: Opening a Popup reparents it to the top-level widget
- WHEN a Popup is created with any descendant widget as parent and open() is called
- THEN the popup parentWidget() SHALL become the top-level widget of the original parent
- AND the popup SHALL NOT become a Qt::Window or Qt::Dialog top-level window

#### Scenario: Popup stays hidden until explicitly opened
- WHEN a Popup is constructed and the parent widget is shown
- THEN the popup SHALL remain hidden until open() is called

### Requirement: Popup exposes lightweight open/close API and signals

The Popup class SHALL expose the following public API:

- bool isOpen() const
- void setIsOpen(bool)
- void open()
- void close()
- signals aboutToShow(), aboutToHide(), opened(), closed(), isOpenChanged(bool)

Popup SHALL NOT expose dialog-style result semantics such as exec() or done(int).

#### Scenario: open() emits show lifecycle signals
- WHEN open() is called on a closed popup
- THEN aboutToShow() SHALL be emitted before the popup becomes visible
- AND opened() SHALL be emitted after the popup is fully considered open
- AND isOpenChanged(true) SHALL be emitted exactly once

#### Scenario: close() emits hide lifecycle signals
- WHEN close() is called on an open popup
- THEN aboutToHide() SHALL be emitted before the popup hides
- AND closed() SHALL be emitted exactly once after closing completes
- AND isOpenChanged(false) SHALL be emitted exactly once

### Requirement: Popup supports QML-style relative positioning via widget-local coordinates

The Popup class SHALL expose a single positioning API:

- void setPosition(QWidget* relativeTo, const QPoint& localPos)

The given localPos SHALL be interpreted in the local coordinate system of relativeTo. Popup SHALL map that point into the coordinate space of the top-level widget. If relativeTo itself is the top-level widget, this SHALL behave like explicit top-level placement. If no position is set, Popup SHALL default to centered placement in the top-level widget.

Coordinates SHALL refer to the visible card top-left corner. Popup may internally compensate for shadow margins before moving the underlying widget.

#### Scenario: Relative widget coordinates are mapped into the top-level widget
- WHEN setPosition(button, QPoint(12, 18)) is called and the popup is opened
- THEN the popup visible card top-left SHALL align with the point obtained by mapping QPoint(12, 18) from button into the top-level widget

#### Scenario: Top-level widget may be used for fixed placement
- WHEN setPosition(topLevelWidget, QPoint(50, 80)) is called and the popup is opened
- THEN the popup visible card SHALL appear at the fixed position (50, 80) within the top-level widget

#### Scenario: Popup defaults to centered placement when no position is set
- WHEN open() is called without any prior setPosition() call
- THEN the popup SHALL be centered in the top-level widget

### Requirement: Popup supports closePolicy-based light-dismiss behavior

Popup SHALL support a ClosePolicy flag set containing:

- NoAutoClose
- CloseOnPressOutside
- CloseOnEscape

The default closePolicy SHALL include CloseOnPressOutside and CloseOnEscape.

#### Scenario: Outside mouse press closes popup when CloseOnPressOutside is enabled
- WHEN an open popup receives a mouse press outside its geometry and CloseOnPressOutside is enabled
- THEN the popup SHALL close
- AND the original mouse event SHALL still be allowed to propagate to background widgets

#### Scenario: Escape closes popup when CloseOnEscape is enabled
- WHEN an open popup receives Qt::Key_Escape and CloseOnEscape is enabled
- THEN the popup SHALL close

#### Scenario: NoAutoClose keeps popup open
- WHEN closePolicy is set to NoAutoClose and the user clicks outside or presses Escape
- THEN the popup SHALL remain open unless close() is called explicitly

### Requirement: Popup uses Fluent theme tokens for self-painted visuals

Popup SHALL render itself using Fluent theme tokens and custom paintEvent logic. It SHALL NOT rely on QSS or QPalette.

The visual structure SHALL include:

- rounded overlay corners
- shadow drawn from Fluent elevation tokens
- background fill drawn from overlay background tokens
- border stroke drawn from overlay stroke tokens

#### Scenario: Theme changes repaint popup visuals
- WHEN the active theme changes while a popup exists
- THEN onThemeUpdated() SHALL trigger a repaint of the popup visuals

### Requirement: Popup provides opacity-based enter and exit animation

Popup SHALL expose popupProgress and animationEnabled properties. When animationEnabled is true, open and close transitions SHALL animate popupProgress between 0.0 and 1.0 using themeAnimation timing and easing.

The transition effect SHALL be opacity-only. The implementation SHALL avoid scale-based rendering transforms for popup contents.

#### Scenario: Opening animates popupProgress from 0 to 1
- WHEN open() is called with animationEnabled set to true
- THEN popupProgress SHALL animate toward 1.0
- AND popupProgressChanged SHALL be emitted during the transition

#### Scenario: Disabling animation completes opening synchronously
- WHEN animationEnabled is false and open() is called
- THEN popupProgress SHALL become 1.0 immediately
- AND opened() SHALL be emitted without waiting for an animation to finish

### Requirement: Popup supports optional modal and dim behavior using a scrim

Popup SHALL support two boolean properties:

- modal
- dim

When modal is true, Popup SHALL create an internal scrim widget attached to the top-level widget and use it to block background pointer interaction. When dim is also true, the scrim SHALL paint a smoke-style translucent fill. When the popup closes, the scrim SHALL be removed.

#### Scenario: Modal popup creates a blocking scrim
- WHEN a popup is opened with modal set to true
- THEN a scrim widget SHALL exist over the top-level widget background
- AND background widgets SHALL not receive pointer input through the scrim

#### Scenario: Non-modal popup has no scrim
- WHEN a popup is opened with modal set to false
- THEN no blocking scrim SHALL be created
