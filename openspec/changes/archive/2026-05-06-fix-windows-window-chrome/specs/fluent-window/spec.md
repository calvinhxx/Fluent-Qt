## ADDED Requirements

### Requirement: Maximized Fluent Window SHALL fill its content area
When `view::windowing::Window` is maximized on Windows custom chrome, the Window layout, titlebar, and content host MUST occupy the usable maximized area exposed by the platform compatibility layer. The Window MUST keep painting Fluent background and border state consistently so the user does not see large black margins outside the content.

#### Scenario: Maximized Window content host fills available geometry
- **WHEN** a `view::windowing::Window` with content is shown and maximized on Windows
- **THEN** the titlebar and content host MUST be laid out across the available maximized geometry without leaving large empty black bands

#### Scenario: Window state changes resync chrome geometry
- **WHEN** a Window changes between normal and maximized states
- **THEN** Window MUST resync `WindowChromeCompat` options with the current titlebar rect, resize border, drag exclusions, and native titlebar safe-area insets

#### Scenario: Non-Windows behavior remains unchanged
- **WHEN** the same Window is shown on macOS or fallback platforms
- **THEN** the existing native macOS controls or Qt fallback window behavior MUST remain unchanged
