## Why

The Gallery shell currently uses a small hand-written flat navigation list, so it does not yet reflect the component taxonomy that already exists under `src/components/`. Building the left pane from that structure gives the app a durable catalog spine while keeping the right content intentionally shallow until sample pages are ready.

## What Changes

- Replace the Gallery main navigation seed with a component-category navigation model derived from the existing component directory groups.
- Keep `Settings` isolated in the `NavigationView` footer chrome.
- Preserve placeholder routing on the right side for every navigation route in this phase.
- Introduce a modular Gallery app-layer separation between component catalog data, navigation tree/view-model state, and pane rendering.
- Keep reusable component APIs under `src/components/` unchanged.
- Add focused tests for category coverage, footer isolation, route activation, and placeholder behavior.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `gallery-shell-framework`: refine the left navigation contract so the main pane reflects `src/components` categories and component entries while the footer remains Settings-only and content pages remain placeholders.

## Impact

- Affected app code: `app/model/`, `app/viewmodel/`, `app/view/GalleryNavigationPane.*`, and `app/view/GalleryWindow.*`.
- Affected tests: `tests/gallery/` shell/navigation coverage.
- No expected changes to reusable component public APIs under `src/components/`.
- No new third-party dependencies.