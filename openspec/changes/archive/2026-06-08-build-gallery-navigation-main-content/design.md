## Context

The Gallery shell is already in place: `GalleryWindow` owns a `NavigationView`, the main chrome is supplied by `GalleryNavigationPane`, the footer chrome contains `Settings`, and the content host swaps placeholder pages. The remaining gap is the left pane main content. It is currently seeded as a flat hand-written list that only partially matches the component groups under `src/components/` and uses `expandable`/`depth` as visual hints rather than a real navigation model.

This change should move the Gallery app toward its own component catalog, not a direct clone of WinUI Gallery. The source taxonomy is the repository's component directory layout. The right side remains placeholder-only so that navigation structure can be implemented and tuned independently from sample content.

## Goals / Non-Goals

**Goals:**

- Build the `NavigationView` main chrome from a Gallery-owned model that reflects the public component categories under `src/components/`.
- Keep `Settings` in the footer chrome and out of the main navigation model.
- Represent categories and component entries as structured navigation nodes rather than a flat list with text-based chevrons.
- Preserve placeholder pages for category and component routes.
- Add tests that verify category coverage, component route availability, footer isolation, and route activation.

**Non-Goals:**

- No real component sample cards, source snippets, favorites, recent items, or search results.
- No runtime filesystem scanning of `src/components/`.
- No reusable component API changes under `src/components/`.
- No redesign of `NavigationView` pane geometry, display modes, or content hosting.
- No full WinUI Gallery data model import.

## Decisions

### Use a static Gallery component catalog, not runtime directory scanning

The catalog should live in the Gallery app layer and list the categories/components that the shell exposes. It can be authored from the current `src/components/` structure and updated as components are added.

Alternatives considered:

- Runtime filesystem scanning: rejected because the Gallery should work from packaged builds where source folders may not exist.
- Embedding the data directly in `GalleryNavigationPane`: rejected because it couples catalog ownership to rendering and makes tests less direct.

### Keep component catalog, navigation tree, and rendering separate

The app layer should have three responsibilities:

- Catalog data: public component categories and entries derived from `src/components/`.
- Navigation tree/view-model: Home, section headers, category nodes, component child nodes, selected route, and expanded state.
- Pane rendering: widgets, indentation, chevrons, selection visuals, and route activation signals.

This keeps `GalleryWindow` focused on coordinating route selection and placeholder page creation.

### Map source directories to user-facing category titles

The first-pass main navigation should include these user-facing categories:

- `basicinput/` -> Basic input
- `collections/` -> Collections
- `date_time/` -> Date & time
- `dialogs_flyouts/` -> Dialogs & flyouts
- `menus_toolbars/` -> Menus & toolbars
- `navigation/` -> Navigation
- `scrolling/` -> Scrolling
- `status_info/` -> Status & info
- `textfields/` -> Text fields
- `windowing/` -> Windowing

`foundation/` is infrastructure and should not appear as a main Gallery category in this phase.

### Preserve shallow placeholder routing

Every selectable route, including component entries, should still resolve to a placeholder page whose title matches the selected route. This lets the navigation tree be tested and manually tuned without blocking on sample content.

### Keep `NavigationView` reusable and app-agnostic

`NavigationView` should continue to own pane geometry, footer/main/header chrome placement, responsive modes, and content hosting. Gallery-specific category data, selection state, and row rendering belong under `app/`.

## Risks / Trade-offs

- Static catalog drift -> Add focused tests for required category/component route IDs and update the catalog alongside new components.
- Large left pane feels dense -> Start with expanded category support and tune indentation/spacing visually after the model is testable.
- Component names may not match friendly display text -> Store explicit display titles in catalog entries rather than deriving titles mechanically from filenames.
- Some headers are infrastructure-like (`StackContentHost`, `Window`) -> Catalog entries can include visibility flags or be omitted from the first pass while preserving category-level coverage.

## Migration Plan

1. Introduce the catalog/tree model while keeping `GalleryWindow::selectRoute()` behavior placeholder-based.
2. Update `GalleryNavigationPane` to render structured nodes and keep footer rendering separate.
3. Replace current flat seed data with component-category main content and Settings footer data.
4. Extend gallery tests to assert the new left-pane shape and routing behavior.
5. Validate with the focused Gallery shell test target and strict OpenSpec validation.

Rollback is straightforward: revert the Gallery app-layer changes because no reusable component APIs or persisted data formats are changed.

## Open Questions

- Should component child nodes be initially visible under all categories, or should categories start collapsed except for the selected route's category?
- Should infrastructure-like public headers such as `StackContentHost` appear as component entries now, or remain hidden until sample pages exist?
- Should component route IDs use source names (`auto-suggest-box`) or class names normalized from headers (`autosuggestbox`)?