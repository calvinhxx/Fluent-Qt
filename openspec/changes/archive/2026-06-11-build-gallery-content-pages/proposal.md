## Why

The Gallery shell now has a stable window shell, title bar, search affordance, left navigation, compact mode, and footer Settings route, but the right content area still stops at placeholder pages. The next step is to make selected categories and components useful by introducing structured content pages with live examples while keeping the reusable component library independent from Gallery-specific behavior.

## What Changes

- Add a Gallery app-layer content page system for Home, component category routes, and component detail routes.
- Add a static content catalog that enriches existing component navigation data with descriptions, page metadata, and sample definitions.
- Add reusable right-side page widgets for category overview pages, component detail pages, example cards, and code snippet presentation.
- Replace placeholder routing for component category and component routes with real content pages.
- Seed the first implementation with representative real sample pages for `Button`, `TreeView`, and `TabView`, plus category overview pages for their categories.
- Keep Settings as a separate footer-owned page.
- Preserve app-layer ownership: no Gallery-specific catalog, sample, or page APIs are added to reusable components under `src/components/`.
- Defer search result pages, favorites, recent items, exhaustive API documentation, and automatic source parsing.

## Capabilities

### New Capabilities
- `gallery-content-pages`: Defines structured Gallery right-side content pages, category overviews, component detail pages, live sample cards, and code snippet presentation.

### Modified Capabilities
- `gallery-shell-framework`: Replace the placeholder-only right content contract with route-based real content page hosting for Gallery category and component routes while keeping the shell/navigation boundaries intact.

## Impact

- Affected app code: `app/model/`, `app/view/`, `app/viewmodel/`, and `app/view/GalleryWindow.*`.
- Affected tests: focused Gallery shell/content tests under `tests/gallery/`.
- Reusable component APIs under `src/components/` remain unchanged.
- Build system may need new app/view source files and test source registration.
- No new third-party dependencies are expected.
