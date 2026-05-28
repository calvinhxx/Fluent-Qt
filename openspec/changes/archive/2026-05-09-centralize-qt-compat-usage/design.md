## Context

`src/compatibility/QtCompat.h` already centralizes several Qt 5/Qt 6 differences: enter event types, mouse event coordinates, view item option initialization, color component pointer types, and test enter event construction. The current scan found new local compatibility branches in scrolling code:

- `src/view/scrolling/ScrollView.cpp` directly includes `QNativeGestureEvent` and branches on `QWheelEvent::position()` vs `posF()` and `QNativeGestureEvent::position()` vs `localPos()`.
- `tests/views/scrolling/TestScrollView.cpp` conditionally includes `QNativeGestureEvent` and `QPointingDevice`, constructs native gesture events only under Qt 6.2+, and skips tests with local version guards.
- `tests/views/windowing/TestWindow.cpp` and `src/compatibility/WindowChromeCompat_mac.cpp` use Qt version checks for macOS expanded client area behavior. Those are platform compatibility boundaries and should not be treated the same as ordinary component code.

The root cause is that `QtCompat.h` did not yet expose helpers for wheel/native gesture APIs when ScrollView zoom support was added, so the implementation fell back to inline `QT_VERSION_CHECK` blocks.

## Goals / Non-Goals

**Goals:**

- Move ScrollView-related Qt API differences into `QtCompat.h`.
- Remove local Qt version branches from `ScrollView.cpp` and `TestScrollView.cpp` where helper functions/macros can cover the difference.
- Add tests or audit coverage so future component code does not reintroduce scattered `QT_VERSION_CHECK` guards.
- Preserve Qt 5.15+ and Qt 6.2+ source compatibility.

**Non-Goals:**

- Do not redesign ScrollView zoom or native gesture behavior.
- Do not remove legitimate version checks from compatibility-layer files such as `QtCompat.h` or `WindowChromeCompat_*`.
- Do not introduce a new compatibility .cpp file or runtime abstraction layer.
- Do not require Qt native gesture tests to run on Qt versions that cannot construct the relevant event type.

## Decisions

1. Add inline helpers to `QtCompat.h` instead of adding per-component utility functions.

   Rationale: this keeps the existing zero-runtime-cost compatibility pattern and makes the intended usage visible to every component. The alternative was adding private helpers in ScrollView, but that keeps the same compatibility knowledge in component code.

2. Use small, typed helper functions for event positions.

   Proposed helpers:
   - `QPointF fluentWheelPosition(const QWheelEvent* event)`
   - `QPointF fluentNativeGesturePosition(const FluentNativeGestureEvent* event)`

   Rationale: position helpers preserve readable component code and avoid macros for normal expressions. The return type should be `QPointF` because both ScrollView zoom anchoring and Qt 6 APIs use floating-point positions.

3. Use a compatibility alias plus test construction macro/function for native gesture events.

   Proposed surface:
   - `using FluentNativeGestureEvent = QNativeGestureEvent` when the target Qt version supports the needed public type
   - a compile-time capability macro such as `FLUENT_HAS_NATIVE_GESTURE_EVENT_CONSTRUCTOR`
   - `FLUENT_MAKE_NATIVE_GESTURE_EVENT(...)` or an inline helper for tests, with a Qt 5 fallback path that does not require including unavailable classes

   Rationale: production code only receives `QEvent*` and casts after checking `QEvent::NativeGesture`; tests need construction support. The compatibility layer should keep conditional includes out of test files.

4. Add an allowlisted audit test rather than banning every `QT_VERSION_CHECK` string globally.

   Rationale: the project has legitimate compatibility boundaries:
   - `src/compatibility/QtCompat.h`
   - `src/compatibility/WindowChromeCompat.h`
   - `src/compatibility/WindowChromeCompat_mac.cpp`
   - Qt compatibility self-tests

   The audit should fail only when ordinary `src/view` components or component tests add new scattered version guards. A strict global grep would create false positives and encourage awkward workarounds.

## Risks / Trade-offs

- [Risk] Native gesture constructors differ across Qt minor versions and are harder to wrap cleanly than mouse positions. -> Mitigation: keep the test construction API narrow and cover only the constructor shape needed by `TestScrollView`.
- [Risk] An audit test can become too broad and fail on intentional compatibility work. -> Mitigation: use a small allowlist and document that new exceptions belong in compatibility modules, not component files.
- [Risk] Qt 5 cannot synthesize the same native gesture events as Qt 6.2+. -> Mitigation: keep the skip behavior through a compatibility capability helper so the conditional remains centralized.
- [Risk] Moving includes into `QtCompat.h` can increase transitive includes. -> Mitigation: include only required Qt headers and keep helpers inline/simple.
