## ADDED Requirements

### Requirement: spdlog dependency is managed by vcpkg
The project SHALL consume spdlog through the existing vcpkg manifest workflow rather than vendoring spdlog source files into the repository.

#### Scenario: Configure with vcpkg preset
- **WHEN** a developer configures the project using a provided vcpkg CMake preset
- **THEN** CMake MUST resolve spdlog through `find_package(spdlog CONFIG REQUIRED)`
- **AND** the component library MUST link against a spdlog CMake target
- **AND** Qt discovery MUST continue to use the existing Qt package discovery flow

#### Scenario: Missing spdlog package
- **WHEN** the vcpkg toolchain is used but spdlog cannot be resolved
- **THEN** CMake MUST fail during configuration with a package resolution error
- **AND** the failure MUST occur before compiling project sources

### Requirement: Project logging facade wraps spdlog
The project SHALL provide a lightweight logging facade so new project and component logs can be written without scattering direct spdlog usage across widget code.

#### Scenario: Component emits a project log message
- **WHEN** component or support code emits a log through the project logging facade
- **THEN** the message MUST be routed to the configured spdlog logger
- **AND** the call site MUST NOT need to construct or register a spdlog logger directly

#### Scenario: Logging is initialized repeatedly
- **WHEN** logging initialization is called more than once in the same process
- **THEN** initialization MUST remain safe
- **AND** the project MUST NOT register duplicate default loggers or duplicate sinks

### Requirement: Runtime log level can be controlled externally
The project logging foundation SHALL support runtime log level configuration without requiring source code changes.

#### Scenario: Environment log level is set
- **WHEN** a supported log level is provided through the project logging environment configuration
- **THEN** the project logger MUST use that level for subsequent log filtering

#### Scenario: Environment log level is absent
- **WHEN** no project log level override is configured
- **THEN** the project logger MUST use a conservative default level suitable for normal test and local runs

#### Scenario: Environment log level is invalid
- **WHEN** an unsupported log level value is provided
- **THEN** logging initialization MUST fall back to the default level
- **AND** the process MUST continue running

### Requirement: Qt messages are bridgeable into project logging
The project SHALL provide a Qt message handler bridge that can route Qt logging output into the project logger.

#### Scenario: Qt warning is emitted after bridge installation
- **WHEN** the Qt message bridge is installed
- **AND** code emits a Qt warning message
- **THEN** the message MUST be routed through the project logger at a warning-equivalent level

#### Scenario: Qt fatal message is emitted after bridge installation
- **WHEN** the Qt message bridge is installed
- **AND** code emits a Qt fatal message
- **THEN** the fatal message MUST be routed through the project logger at a fatal-equivalent level
- **AND** fatal termination behavior MUST be preserved

### Requirement: Shared Qt test startup initializes logging
The shared Qt GoogleTest entry point SHALL initialize project logging for each test binary.

#### Scenario: Test binary starts normally
- **WHEN** a Qt widget test binary starts through the shared GoogleTest main
- **THEN** project logging MUST be initialized before tests execute
- **AND** direct binary execution MUST continue to run GoogleTest normally

#### Scenario: CTest runs with VisualCheck skipped
- **WHEN** CTest runs a discovered Qt widget test with `SKIP_VISUAL_TEST=1`
- **THEN** offscreen setup MUST continue to happen before `QApplication` is created
- **AND** logging initialization MUST NOT prevent VisualCheck tests from being skipped automatically

### Requirement: Future log anchors follow stable diagnostic conventions
The project SHALL document conventions for adding future log anchors so log output remains useful for human debugging and AI-assisted analysis.

#### Scenario: New component diagnostic anchor is added
- **WHEN** a future change adds a component log anchor
- **THEN** the log message MUST include a stable component or subsystem name
- **AND** it MUST include relevant structured key/value details for state, geometry, indices, ranges, or failure reasons where applicable

#### Scenario: Placeholder diagnostic message is proposed
- **WHEN** a future log message contains only vague text such as `here` or `xxx`
- **THEN** the message MUST be replaced with a descriptive event and relevant diagnostic fields before the change is accepted
