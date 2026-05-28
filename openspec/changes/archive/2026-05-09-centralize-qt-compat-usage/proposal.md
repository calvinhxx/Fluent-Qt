## Why

`QtCompat.h` already exists to centralize Qt 5.15+ / Qt 6.2+ differences, but newer ScrollView code and tests reintroduced local `QT_VERSION_CHECK` guards and conditional Qt includes. This makes the code harder to review and weakens the compatibility contract because each component can drift into its own Qt-version handling.

## What Changes

- Extend `src/compatibility/QtCompat.h` with helpers for currently duplicated event APIs:
  - wheel event local position (`QWheelEvent::position()` vs `posF()`)
  - native gesture event type availability and local position (`QNativeGestureEvent::position()` vs `localPos()`)
  - test-only native gesture construction for Qt 6.2+ while providing a harmless fallback for Qt 5.15
- Refactor `ScrollView` and `TestScrollView` to include `QtCompat.h` and remove component-level `QT_VERSION_CHECK` branches for these APIs.
- Add a focused audit/test guard so future scattered `QT_VERSION_CHECK` usage in `src/view` and component tests is caught, while allowing compatibility-layer files and platform-specific compatibility files to keep necessary version checks.
- Leave platform compatibility implementation files such as `WindowChromeCompat_mac.cpp` alone where they are intentionally the compatibility boundary for platform or Qt feature flags.
- No public widget behavior changes are intended.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `qt-version-compatibility`: Clarify and enforce that Qt version API differences for view components and tests are exposed through `QtCompat.h`, including wheel/native gesture helpers used by ScrollView.

## Impact

- Affected files:
  - `src/compatibility/QtCompat.h`
  - `src/view/scrolling/ScrollView.cpp`
  - `tests/views/scrolling/TestScrollView.cpp`
  - `tests/views/TestQtCompat.cpp` or another focused compatibility audit test
- Public APIs remain source-compatible for consumers of widgets.
- Build targets affected: `fluent_qt_lib`, `test_scroll_view`, `test_qt_compat`.
