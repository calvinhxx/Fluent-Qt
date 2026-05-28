## ADDED Requirements

### Requirement: QtCompat.h SHALL 封装滚轮与原生手势事件 API 差异

`QtCompat.h` MUST provide centralized helpers for Qt version differences used by ScrollView zoom input:

- `fluentWheelPosition(const QWheelEvent*)` MUST return the wheel event local position as `QPointF` on all supported Qt versions.
- `fluentNativeGesturePosition(const FluentNativeGestureEvent*)` MUST return the native gesture local position as `QPointF` on all supported Qt versions that expose native gesture events.
- Production view code MUST use these helpers instead of local `QT_VERSION_CHECK` branches for `QWheelEvent::position()` / `posF()` or `QNativeGestureEvent::position()` / `localPos()`.

#### Scenario: ScrollView zoom wheel position is version-independent

- **WHEN** `ScrollView` handles Ctrl+wheel zoom input on Qt 5.15 or Qt 6.2+
- **THEN** it MUST obtain the zoom anchor through `fluentWheelPosition`
- **AND** `src/view/scrolling/ScrollView.cpp` MUST NOT contain a local `QT_VERSION_CHECK` branch for wheel event position APIs

#### Scenario: ScrollView native gesture position is version-independent

- **WHEN** `ScrollView` handles `QEvent::NativeGesture` zoom input on Qt 5.15 or Qt 6.2+
- **THEN** it MUST obtain the zoom anchor through `fluentNativeGesturePosition`
- **AND** `src/view/scrolling/ScrollView.cpp` MUST NOT contain a local `QT_VERSION_CHECK` branch for native gesture position APIs

### Requirement: QtCompat.h SHALL 封装测试所需的原生手势构造差异

`QtCompat.h` MUST provide a centralized way for tests to determine whether synthetic native gesture events can be constructed and to construct the event when supported.

The helper surface MUST:

- Avoid requiring component tests to conditionally include `QNativeGestureEvent` or `QPointingDevice`.
- Keep Qt 5.15 builds valid when native gesture event construction is unavailable.
- Preserve explicit test skip behavior for native gesture tests on unsupported Qt versions.

#### Scenario: TestScrollView has no conditional native gesture includes

- **WHEN** `tests/views/scrolling/TestScrollView.cpp` is compiled on any supported Qt version
- **THEN** it MUST include `compatibility/QtCompat.h` for native gesture test helpers
- **AND** it MUST NOT contain local conditional includes for `QNativeGestureEvent` or `QPointingDevice`

#### Scenario: Native gesture tests skip through compatibility capability

- **WHEN** the supported Qt version cannot synthesize the native gesture event used by the test
- **THEN** `TestScrollView` MUST skip native gesture tests through a compatibility capability helper or macro
- **AND** the skip reason MUST remain clear to the test reader

### Requirement: Component code SHALL NOT introduce scattered Qt version guards outside compatibility boundaries

Project source and tests MUST keep Qt version checks inside compatibility boundaries except for explicit compatibility self-tests. Ordinary view component code and component tests MUST use `QtCompat.h` helpers when a Qt API difference is already supported by the compatibility layer.

Allowed boundaries include:

- `src/compatibility/QtCompat.h`
- `src/compatibility/WindowChromeCompat.h`
- `src/compatibility/WindowChromeCompat_*.cpp`
- focused Qt compatibility tests such as `tests/views/TestQtCompat.cpp`

#### Scenario: Audit catches component-level QT_VERSION_CHECK usage

- **WHEN** the compatibility audit test scans `src/view` and component tests
- **THEN** any `QT_VERSION_CHECK` usage outside the allowlisted compatibility boundaries MUST fail the test
- **AND** the failure message MUST identify the offending file path

#### Scenario: Compatibility boundary version checks remain allowed

- **WHEN** the compatibility audit test scans the project
- **THEN** intentional version checks inside `QtCompat.h` and platform compatibility files MUST remain allowed
- **AND** the audit MUST NOT require removing platform feature checks from `WindowChromeCompat_mac.cpp`
