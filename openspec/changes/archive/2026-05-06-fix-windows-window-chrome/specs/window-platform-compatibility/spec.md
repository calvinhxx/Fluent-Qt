## ADDED Requirements

### Requirement: Windows system move and resize SHALL report only real native starts
On Windows, `WindowChromeCompat` MUST only return success from `beginSystemMove()` or `beginSystemResize()` when the target widget is an eligible visible top-level window and the native system move or resize operation has been requested through a valid native window handle. If the widget is hidden, non-top-level, missing a native handle, configured without custom chrome, or otherwise ineligible for a native operation, the method MUST return `false` without changing capture state or sending native non-client commands.

#### Scenario: Hidden widget does not start system move
- **WHEN** `WindowChromeCompat` is constructed for a hidden QWidget and `beginSystemMove()` is called
- **THEN** it MUST return `false` and MUST NOT send a Windows non-client move command

#### Scenario: Hidden widget does not start system resize
- **WHEN** `WindowChromeCompat` is constructed for a hidden QWidget and `beginSystemResize(Qt::LeftEdge, pos)` is called
- **THEN** it MUST return `false` and MUST NOT send a Windows non-client resize command

#### Scenario: Eligible top-level custom chrome window can start native operations
- **WHEN** a visible top-level Windows custom chrome Window has a valid native handle and the pointer is in an eligible drag or resize area
- **THEN** the compatibility layer MUST be able to request the corresponding native move or resize operation while preserving OS Snap and resize behavior

### Requirement: Windows custom chrome SHALL preserve DWM frame affordances
On Windows, custom chrome MUST expand the client area into the titlebar while keeping DWM-managed caption/frame styles needed for rounded corners, border/shadow, animations, maximize geometry, and Snap behavior. The visible Windows caption buttons MUST be self-drawn Qt controls owned by `Window`.

#### Scenario: DWM frame style is preserved
- **WHEN** a Windows custom chrome Window is shown
- **THEN** it MUST use expanded client area titlebar flags
- **AND** it MUST NOT use a fully frameless Qt window flag
- **AND** the HWND MUST retain native caption and thick-frame styles needed by DWM

#### Scenario: Custom TitleBar shares native caption row
- **WHEN** a Windows custom chrome Window is shown
- **THEN** the custom Qt TitleBar content MUST occupy the native caption row within normal resize-border and DPI tolerance
- **AND** it MUST NOT render below a separate Windows system titlebar row

#### Scenario: Windows uses WinUI-style Qt-owned titlebar controls
- **WHEN** a Windows custom chrome Window is shown
- **THEN** the leading titlebar area MUST be available for custom Qt content
- **AND** the right-side minimize, maximize/restore, and close controls MUST be visible Qt controls
- **AND** the Qt Window title property MUST remain available for custom TitleBar content
- **AND** the trailing self-drawn caption-button area MUST remain reserved and reachable

#### Scenario: Self-drawn caption buttons remain reachable
- **WHEN** a Windows custom chrome Window lays out TitleBar content
- **THEN** the self-drawn caption-button area MUST be reserved as non-draggable application content space
- **AND** caption-button hit-test results MUST remain client space so Qt receives button clicks

#### Scenario: Titlebar double-click uses native maximize restore
- **WHEN** a user double-clicks an eligible non-interactive TitleBar area on Windows
- **THEN** the Window MUST maximize or restore through the native titlebar path

### Requirement: Windows maximized custom chrome SHALL correct client frame geometry
On Windows custom chrome windows, the compatibility layer MUST account for the native maximized frame, resize border, monitor work area, and Qt device-pixel scaling so maximized windows present the Fluent client area flush with the usable screen bounds. The implementation MUST NOT leave thick black margins around the visible content after maximizing.

#### Scenario: Maximized custom chrome fills the usable work area
- **WHEN** a Windows custom chrome Window is shown and then maximized
- **THEN** the visible Fluent window content MUST fill the monitor work area without large black margins around the client content

#### Scenario: Maximized hit-test remains usable
- **WHEN** a Windows custom chrome Window is maximized
- **THEN** titlebar hit-test, drag exclusion hit-test, and restore-from-titlebar behavior MUST remain usable

#### Scenario: Restoring clears maximized frame adjustment
- **WHEN** a maximized Windows custom chrome Window is restored to normal state
- **THEN** any maximized-only frame or client-area adjustment MUST be removed so the normal window geometry, resize borders, and Fluent border paint are correct
