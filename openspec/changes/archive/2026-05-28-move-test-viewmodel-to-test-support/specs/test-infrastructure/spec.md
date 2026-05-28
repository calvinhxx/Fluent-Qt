## ADDED Requirements

### Requirement: Test-only helper models live in test support
The test infrastructure SHALL keep reusable test-only QObject models and fixture helpers under the test tree rather than compiling them into the production component library.

#### Scenario: Shared binding model is test-owned
- **WHEN** a reusable QObject model exists only to support property binding, fixture setup, or visual/unit tests
- **THEN** it MUST live under `tests/support/` or a specific `tests/views/**` test file
- **AND** it MUST NOT live under `src/` or be compiled into `fluent_qt_lib`

#### Scenario: Test helper naming is explicit
- **WHEN** a shared test-only model is introduced under `tests/support/`
- **THEN** its name MUST make the test-support role clear, such as `BindingTestModel` or `PropertyBindingTestModel`
- **AND** it MUST NOT use broad production-layer names such as `ViewModel` unless the class is promoted to a documented runtime API

#### Scenario: Specialized local mocks stay local
- **WHEN** a test needs component-specific property names, signals, or setup behavior
- **THEN** the helper MAY remain local to that test file or fixture
- **AND** the shared test support layer MUST NOT force unrelated tests to use a generic model

#### Scenario: Test support wiring stays out of runtime library
- **WHEN** a shared test helper uses `QObject` or `Q_OBJECT`
- **THEN** the test CMake wiring MUST make MOC/build behavior valid for test targets
- **AND** the helper MUST be linked only into relevant test binaries through test support sources or explicit test target sources
