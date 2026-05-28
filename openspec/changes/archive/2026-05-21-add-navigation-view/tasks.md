## 1. Shell Component API

- [x] 1.1 Add `src/view/navigation/NavigationView.h` with `QWidget + FluentElement + view::QMLPlus` inheritance, a single `DisplayMode` enum, properties, signals, chrome slot accessors, `StackContentHost` access, and geometry query APIs.
- [x] 1.2 Add `src/view/navigation/NavigationView.cpp` with constructor setup, focus/accessibility defaults, size hints, layout invalidation, resize/theme handling, and shell painting.
- [x] 1.3 Support `Auto`, `Left`, `LeftCompact`, `LeftMinimal`, and `Top` display modes with `effectiveDisplayMode` reporting for the active layout mode.
- [x] 1.4 Implement duplicate-signal suppression for repeated property values.
- [x] 1.5 Wire `NavigationView` into the existing source/test discovery or CMake test target.

## 2. Chrome Slots

- [x] 2.1 Expose application-supplied `headerChromeWidget`, `mainChromeWidget`, and `footerChromeWidget` slots.
- [x] 2.2 Reparent assigned chrome widgets into `NavigationView`, apply computed geometry, and detach replaced widgets without deleting them.
- [x] 2.3 Lay side chrome vertically as header, main, and footer.
- [x] 2.4 Lay top chrome horizontally as header, main, stretch, and footer.
- [x] 2.5 Keep navigation items, selection, commands, indicators, nested expansion, search, settings, and routing outside `NavigationView`.

## 3. Content Host

- [x] 3.1 Own and expose a `StackContentHost` via `contentHost()`.
- [x] 3.2 Assign the content host to the computed content rectangle after every shell layout recomputation.
- [x] 3.3 Leave page insertion, replacement, and current page selection to application code using `StackContentHost`.

## 4. Layout And Metrics

- [x] 4.1 Implement side expanded layout with configurable expanded pane width and content offset.
- [x] 4.2 Implement compact layout with a compact rail when closed and expanded chrome when opened.
- [x] 4.3 Implement minimal layout with no reserved chrome when closed and overlay chrome when opened.
- [x] 4.4 Implement top layout with configurable top bar height and content below the bar.
- [x] 4.5 Resolve `Auto` mode from configurable compact and expanded width thresholds.
- [x] 4.6 Expose configurable expanded pane width, compact pane width, and top bar height with normalization.
- [x] 4.7 Expose deterministic chrome, pane, top bar, content, header, main, and footer geometry query helpers.

## 5. Painting And Theme Integration

- [x] 5.1 Paint only shell-level canvas, chrome, content, and divider surfaces using Fluent semantic colors.
- [x] 5.2 Refresh `StackContentHost` and chrome widgets on theme update without mutating application-owned chrome/content state.
- [x] 5.3 Avoid item-row painting, built-in indicators, expansion animations, or command rendering inside `NavigationView`.

## 6. Tests And VisualCheck

- [x] 6.1 Add `tests/views/navigation/TestNavigationView.cpp` with component construction, property, geometry, theme, and `StackContentHost` tests.
- [x] 6.2 Add chrome slot tests for side vertical layout and top horizontal layout with footer alignment.
- [x] 6.3 Add auto/compact/minimal/top geometry tests and configurable metric tests.
- [x] 6.4 Add a VisualCheck sample that composes header/main/footer chrome and content pages at the application layer.
- [x] 6.5 Register `test_navigation_view` in `tests/views/navigation/CMakeLists.txt`.

## 7. Validation

- [x] 7.1 Validate the OpenSpec change with `openspec validate add-navigation-view --strict`.
- [x] 7.2 Build incrementally with `cmake --build build --target test_navigation_view`.
- [x] 7.3 Run automated tests with `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_navigation_view`.
- [x] 7.4 Run `git diff --check` for NavigationView source, tests, and OpenSpec files.
- [ ] 7.5 Optionally run the manual NavigationView VisualCheck without `SKIP_VISUAL_TEST=1` to inspect the app-composed chrome.
