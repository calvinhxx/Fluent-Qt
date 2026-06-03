## 1. Test Support Infrastructure

- [x] 1.1 Add reusable named-widget lookup helpers for Qt tests, including clear failure output when an expected object name is missing.
- [x] 1.2 Add geometry assertion helpers for center alignment, size, spacing, containment, and rectangle comparison with explicit pixel tolerance.
- [x] 1.3 Add optional geometry dump helpers that can print selected named widgets and scoped widget subtrees.
- [x] 1.4 Ensure geometry dump output is opt-in and does not add noise to normal CTest runs.

## 2. Stable Visual Identifiers

- [x] 2.1 Add stable object names to gallery title bar controls, including back button, menu button, app icon, title label, and search box.
- [x] 2.2 Keep reusable component-owned child object names outside this app-focused geometry contract.
- [x] 2.3 Keep app object names scoped and descriptive so tests do not depend on unnamed Qt platform children.

## 3. Geometry Contract Coverage

- [x] 3.1 Update gallery shell tests to assert title bar control vertical centering, fixed heights, icon logical size, and expected spacing.
- [x] 3.2 Avoid applying new geometry requirements to stable reusable component tests.
- [x] 3.3 Ensure gallery app icon checks compare logical geometry separately from device-pixel backing data.
- [x] 3.4 Make geometry assertion failures include enough rectangle and center data to debug without opening a screenshot.

## 4. Workflow Documentation

- [x] 4.1 Update visual review/testing documentation to describe the geometry-first workflow.
- [x] 4.2 Document when to use geometry assertions, geometry dumps, screenshots, and manual VisualCheck.
- [x] 4.3 Add example commands for focused geometry tests and opt-in geometry dump runs.

## 5. Validation

- [x] 5.1 Build the focused gallery shell test target with `cmake --build --preset vcpkg-osx`.
- [x] 5.2 Run focused CTest labels for the updated geometry tests with `ctest --preset vcpkg-osx --output-on-failure`.
- [x] 5.3 Run `openspec validate visual-geometry-verification --strict` and resolve any spec issues.
