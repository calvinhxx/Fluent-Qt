## 1. Catalog and Model

- [x] 1.1 Define Gallery app-layer catalog records for component categories and component entries without changing `src/components/` APIs.
- [x] 1.2 Seed the static component catalog from the current `src/components/` taxonomy for Basic input, Collections, Date & time, Dialogs & flyouts, Menus & toolbars, Navigation, Scrolling, Status & info, Text fields, and Windowing.
- [x] 1.3 Exclude `foundation/` infrastructure and footer Settings from the component catalog.
- [x] 1.4 Define structured navigation nodes that distinguish section headers, category routes, component routes, and footer routes.

## 2. Navigation View Model

- [x] 2.1 Replace the flat `GalleryNavigationViewModel` seed with navigation tree construction from the static component catalog.
- [x] 2.2 Provide main-pane and footer-pane node collections so Settings remains footer-only.
- [x] 2.3 Provide route lookup helpers that return category/component parent relationships and placeholder metadata for every selectable route.
- [x] 2.4 Preserve the default Home route and placeholder-compatible route IDs for existing shell behavior.

## 3. Pane Rendering

- [x] 3.1 Update `GalleryNavigationPane` to consume structured navigation nodes instead of inferring hierarchy from text or `depth` alone.
- [x] 3.2 Render category and component rows with consistent indentation, icon glyphs, selection state, and expandable affordances.
- [x] 3.3 Emit route activation for category and component entries while preserving footer Settings activation.
- [x] 3.4 Keep `NavigationView` reusable and free of Gallery-specific catalog or route responsibilities.

## 4. Window Routing

- [x] 4.1 Wire `GalleryWindow` to the new main/footer navigation model outputs.
- [x] 4.2 Keep right-side content as `PlaceholderPage` for category and component routes.
- [x] 4.3 Keep `SettingsPage` routing unchanged for the footer Settings route.

## 5. Tests and Validation

- [x] 5.1 Add or update Gallery tests that verify required component categories are present in the main pane and `foundation/` is absent.
- [x] 5.2 Add or update Gallery tests that verify representative component route IDs exist under their expected categories.
- [x] 5.3 Add or update Gallery tests that verify Settings appears only in the footer pane.
- [x] 5.4 Add or update Gallery tests that activate a representative component route and verify placeholder content updates.
- [x] 5.5 Validate with `cmake --build --preset vcpkg-osx --target test_gallery_shell_framework`.
- [x] 5.6 Validate with `ctest --preset vcpkg-osx -L '^test_gallery_shell_framework$' --output-on-failure`.
- [x] 5.7 Run `openspec validate build-gallery-navigation-main-content --strict`.