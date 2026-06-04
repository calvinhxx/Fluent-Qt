## 1. Build Wiring

- [x] 1.1 Add an optional `FLUENT_QT_BUILD_GALLERY` CMake option and include `app/` only when enabled.
- [x] 1.2 Add `app/CMakeLists.txt` for the `fluent_qt_gallery` executable linked against `fluent_qt_lib` and Qt Widgets.
- [x] 1.3 Add `app/main.cpp` that initializes the Qt application and opens the Gallery shell window.

## 2. Shell Structure

- [x] 2.1 Create the Gallery app folder structure under `app/gallery/` for shell, model, and placeholder page code.
- [x] 2.2 Implement the top-level Gallery window using the existing Fluent windowing component.
- [x] 2.3 Add a title/header area with app title text and a visible search input affordance labelled for controls and samples.
- [x] 2.4 Add a main shell layout with a persistent left navigation area and right content area.

## 3. Navigation Model and Pane

- [x] 3.1 Define app-local navigation records with stable IDs, display titles, grouping metadata, optional icon glyphs, and placeholder colors.
- [x] 3.2 Seed first-pass navigation entries for Home, Fundamentals, Design, Accessibility, Controls, Settings, and the required Controls groups.
- [x] 3.3 Implement a left navigation pane that renders the seeded entries and exposes activation events.
- [x] 3.4 Track the selected navigation entry and update visual selection state when the active route changes.

## 4. Placeholder Content

- [x] 4.1 Implement a placeholder page factory that creates a colored content surface for a navigation entry.
- [x] 4.2 Ensure each placeholder page includes a Fluent `Label` naming the selected route.
- [x] 4.3 Wire navigation activation to replace or switch the right content area with the matching placeholder page.
- [x] 4.4 Keep real component samples, code snippets, recent items, favorites, and search results out of this phase.

## 5. Tests and Validation

- [x] 5.1 Add `tests/gallery/` CMake wiring guarded by `FLUENT_QT_BUILD_GALLERY`.
- [x] 5.2 Add tests that construct the Gallery shell without showing interactive UI.
- [x] 5.3 Add tests that verify required navigation entries are available.
- [x] 5.4 Add tests that activate representative navigation entries and verify placeholder content changes.
- [x] 5.5 Validate with `cmake --build --preset vcpkg-osx --target fluent_qt_gallery test_gallery_shell_framework`.
- [x] 5.6 Validate with focused CTest label or target execution for the Gallery shell tests.
- [x] 5.7 Run `openspec validate gallery-shell-framework --strict`.