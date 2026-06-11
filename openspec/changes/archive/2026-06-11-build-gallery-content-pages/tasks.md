## 1. Content Data Model

- [x] 1.1 Add app-layer content metadata structs for page kind, route ID, title, description, category ID, related routes, and sample IDs.
- [x] 1.2 Add a static Gallery content catalog with seeded entries for Home, Basic input, Collections, Navigation, `button`, `tree-view`, and `tab-view`.
- [x] 1.3 Add lookup helpers that resolve content entries by route ID and verify seeded content route IDs exist in `GalleryNavigationViewModel`.
- [x] 1.4 Add sample metadata and preview factory records for representative `button`, `tree-view`, and `tab-view` samples.

## 2. Page Factory and Routing

- [x] 2.1 Add a Gallery page factory that creates Home, category, component, Settings, or fallback page widgets from a route ID.
- [x] 2.2 Update `GalleryWindow::applyRoute()` to delegate page creation to the factory while preserving content host replacement and navigation history behavior.
- [x] 2.3 Keep `PlaceholderPage` as a fallback for known routes without content metadata in this phase.
- [x] 2.4 Keep Settings routed to `SettingsPage` and out of the content catalog main component model.

## 3. Shared Content Page Widgets

- [x] 3.1 Add a scrollable `GalleryContentPage` base shell with title, optional subtitle, section layout, route ID exposure, and theme refresh.
- [x] 3.2 Add `GalleryCategoryPage` with category overview text and component cards that expose target route IDs.
- [x] 3.3 Add `GalleryComponentPage` with Overview, Examples, API notes, and related-content sections driven by catalog data.
- [x] 3.4 Add `GallerySampleCard` that hosts sample title, description, live preview widget, optional options panel, and code snippet area.
- [x] 3.5 Add `GalleryCodeBlock` or equivalent app-layer code snippet presentation with read-only text and copy affordance.

## 4. Seeded Component Content

- [x] 4.1 Implement `Button` samples for standard, accent, disabled, and icon button usage using public `Button` APIs.
- [x] 4.2 Implement `TreeView` sample content with a basic expandable hierarchy using public `TreeView` APIs.
- [x] 4.3 Implement `TabView` sample content that demonstrates selected content hosting using public `TabView` APIs.
- [x] 4.4 Add category overview cards for Basic input, Collections, and Navigation that link to the seeded component pages.
- [x] 4.5 Tune right-side page spacing, typography, card radius, and responsive widths against the existing shell visual language.

## 5. Build Integration

- [x] 5.1 Register new app model/view source files in the Gallery executable build.
- [x] 5.2 Register any new gallery test source files in `tests/gallery/CMakeLists.txt`.
- [x] 5.3 Ensure the reusable library target does not absorb Gallery app-layer content files.

## 6. Tests and Validation

- [x] 6.1 Add tests for content catalog seeded route lookup and navigation route consistency.
- [x] 6.2 Add tests that selecting Basic input, Collections, and Navigation routes creates category overview pages.
- [x] 6.3 Add tests that selecting `button`, `tree-view`, and `tab-view` creates component pages with expected route IDs and sample counts.
- [x] 6.4 Add tests that sample cards contain live preview widgets and expose code snippets where defined.
- [x] 6.5 Update existing placeholder routing tests to expect real content pages for seeded routes and fallback pages for unseeded routes.
- [x] 6.6 Add a focused theme refresh test for at least one content page and one sample card.
- [x] 6.7 Build `test_gallery_shell_framework` and any new gallery content test target.
- [x] 6.8 Run `ctest --preset vcpkg-osx -L '^test_gallery' --output-on-failure` or the focused gallery labels available after test registration.
- [x] 6.9 Run `openspec validate build-gallery-content-pages --strict`.
