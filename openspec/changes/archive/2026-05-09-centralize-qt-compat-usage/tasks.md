## 1. Compatibility Helpers

- [x] 1.1 Extend `src/compatibility/QtCompat.h` with `QWheelEvent` include and `fluentWheelPosition(const QWheelEvent*)`.
- [x] 1.2 Extend `QtCompat.h` with native gesture aliases/helpers that compile on Qt 5.15 and Qt 6.2+.
- [x] 1.3 Add a centralized helper or macro for synthetic native gesture event construction used by tests.
- [x] 1.4 Add or update `tests/views/TestQtCompat.cpp` coverage for the new helper availability and position APIs.

## 2. ScrollView Cleanup

- [x] 2.1 Refactor `src/view/scrolling/ScrollView.cpp` to include `compatibility/QtCompat.h` and use `fluentWheelPosition`.
- [x] 2.2 Refactor `ScrollView.cpp` to use the native gesture compatibility alias and `fluentNativeGesturePosition`.
- [x] 2.3 Remove local `QT_VERSION_CHECK` branches from `ScrollView.cpp` that duplicate compatibility-layer behavior.
- [x] 2.4 Confirm ScrollView zoom and native gesture behavior remains unchanged.

## 3. Test Cleanup

- [x] 3.1 Refactor `tests/views/scrolling/TestScrollView.cpp` to include `compatibility/QtCompat.h` instead of conditional native gesture includes.
- [x] 3.2 Replace local native gesture construction branches with the centralized compatibility helper.
- [x] 3.3 Replace native gesture skip guards with a compatibility capability helper or macro.
- [x] 3.4 Ensure `TestScrollView.cpp` has no local `QT_VERSION_CHECK` branches after cleanup.

## 4. Audit Guard

- [x] 4.1 Add a focused compatibility audit test that scans component source/tests for scattered `QT_VERSION_CHECK`.
- [x] 4.2 Allowlist only compatibility boundary files and focused Qt compatibility tests.
- [x] 4.3 Make audit failures report the offending file path.
- [x] 4.4 Document in the test or helper why `WindowChromeCompat_*` remains an allowed compatibility boundary.

## 5. Verification

- [x] 5.1 Build `test_qt_compat`.
- [x] 5.2 Build `test_scroll_view`.
- [x] 5.3 Run `ctest --test-dir build -R "QtCompat|ScrollView" --output-on-failure`.
- [x] 5.4 Run `rg "QT_VERSION_CHECK|QT_VERSION >=|QT_VERSION <" src tests` and confirm remaining matches are expected compatibility boundaries.
- [x] 5.5 Run `openspec status --change centralize-qt-compat-usage` and confirm all artifacts are complete.
