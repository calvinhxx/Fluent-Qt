## 1. Compatibility Layer

- [x] 1.1 Add `src/compatibility/WindowChromeCompat.h` with platform-neutral options, state, and method declarations.
- [x] 1.2 Implement common/fallback behavior in `WindowChromeCompat.cpp`, including safe no-op configuration and failure-returning system move/resize.
- [x] 1.3 Add Windows-specific implementation guarded inside compatibility sources for custom chrome flags, native hit-test, resize borders, caption area, and drag exclusion rects.
- [x] 1.4 Add macOS-specific compatibility behavior that preserves native window controls and avoids enabling Windows custom chrome.
- [x] 1.5 Add Qt5/Qt6 native event result/signature helpers in compatibility code so `src/view/windowing/` does not contain `QT_VERSION_CHECK`.

## 2. Windowing UI Components

- [x] 2.1 Implement TitleBar as a system-reserved leading area plus external content slot.
- [x] 2.2 Add `src/view/windowing/TitleBar.h/.cpp` with 32px height, top-level AnchorLayout content hosting, drag exclusion rect derivation, and theme-aware painting.
- [x] 2.3 Add `src/view/windowing/Window.h/.cpp` deriving from `QWidget`, `FluentElement`, and `view::QMLPlus`.
- [x] 2.4 Implement Window content host APIs, including `setContentWidget(QWidget*)`, `contentWidget()`, and `contentHost()`.
- [x] 2.5 Keep `windowTitle` and optional window icon on the native Window while allowing callers to provide their own titlebar content widget.
- [x] 2.6 Apply default minimum size from `Breakpoints::MinWindowWidth` and `Breakpoints::MinWindowHeight`.

## 3. Window Behavior Integration

- [x] 3.1 Keep Window minimize, maximize/restore, and close operations available as slots for external titlebar controls.
- [x] 3.2 Keep maximize/restore behavior synchronized with Window state changes.
- [x] 3.3 Configure `WindowChromeCompat` after layout changes with titlebar rect, resize border width, and external titlebar control exclusion rects.
- [x] 3.4 Delegate Window native events to `WindowChromeCompat` and preserve platform-neutral UI code.
- [x] 3.5 Implement titlebar blank-area system move through compatibility fallback where native hit-test is unavailable.
- [x] 3.6 Ensure Window does not install `QGraphicsOpacityEffect` on the top-level widget.

## 4. Build Integration

- [x] 4.1 Ensure root CMake includes new `src/view/windowing/` and `src/compatibility/WindowChromeCompat*.cpp` files through the existing source discovery.
- [x] 4.2 Add `tests/views/windowing/CMakeLists.txt` and register it from `tests/views/CMakeLists.txt`.
- [x] 4.3 Ensure platform-specific compatibility sources compile on non-target platforms by guarding native includes inside compatibility files.

## 5. Automated Tests

- [x] 5.1 Add `tests/views/windowing/TestWindow.cpp` covering default construction, minimum size, titlebar/content host existence, and top-level show smoke behavior.
- [x] 5.2 Add tests for `setContentWidget()` insertion and replacement without leaving stale layout items.
- [x] 5.3 Add tests for title synchronization from `setWindowTitle()`.
- [x] 5.4 Add tests for theme switching updating Window and TitleBar without crashes.
- [x] 5.5 Add tests for external titlebar actions using observable Window state or overridable test hooks.
- [x] 5.6 Add compatibility tests verifying `src/view/windowing/` has no platform macros, Qt version checks, or native platform headers.
- [x] 5.7 Add compatibility tests for fallback `WindowChromeCompat` behavior on the current platform.
- [x] 5.8 Add Windows-only tests for hit-test classification when running on Windows; skip them on other platforms.

## 6. VisualCheck And Validation

- [x] 6.1 Add Window VisualCheck with a button that opens a real Window using custom titlebar content and content host layout.
- [ ] 6.2 On Windows, manually verify external caption controls, titlebar drag, edge resize, double-click maximize/restore, and Aero Snap.
- [ ] 6.3 On macOS, manually verify native traffic lights remain available and Windows caption buttons are not shown by default.
- [x] 6.4 Run the focused windowing test target with `SKIP_VISUAL_TEST=1`.
- [x] 6.5 Run relevant existing tests for `test_qt_compat`, dialogs/flyouts, and menus to catch regressions around top-level window behavior.
- [x] 6.6 Run `ctest --output-on-failure` with VisualCheck skipped before marking implementation complete.
