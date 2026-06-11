## Context

The Gallery shell already provides the stable application frame: `GalleryWindow` owns a `NavigationView`, `GalleryNavigationPane` renders main and footer navigation, `GalleryNavigationViewModel` supplies structured route data from `GalleryComponentCatalog`, and the content host currently swaps `PlaceholderPage` or `SettingsPage` instances. The next design boundary is the right-side content system.

The current placeholder approach was useful while tuning shell geometry, navigation density, compact mode, flyouts, and footer behavior. It is now the limiting factor: component routes do not demonstrate real controls, category routes do not summarize their components, and future search/favorites features have no content metadata to query.

This change should keep the Gallery app as a consumer of reusable components. It must not add Gallery-specific APIs, sample registries, or documentation hooks under `src/components/`.

## Goals / Non-Goals

**Goals:**

- Introduce an app-layer page factory that maps Gallery route IDs to concrete page widgets.
- Add a structured content catalog that describes right-side pages independently from navigation row rendering.
- Provide reusable page primitives for Home, category overview, component detail, sample cards, and code snippets.
- Replace placeholder pages for component category and component routes with real content pages.
- Seed representative component pages for `Button`, `TreeView`, and `TabView`.
- Keep content page styling consistent with the completed shell and WinUI Gallery-inspired density.
- Add focused tests for route-to-page selection, category overview content, representative sample creation, and reusable component API isolation.

**Non-Goals:**

- No search result page or full search implementation.
- No favorites, recent items, recommendation ranking, or telemetry.
- No automatic source parsing, Doxygen extraction, or runtime filesystem scanning.
- No exhaustive sample coverage for every component in the catalog.
- No reusable component API changes under `src/components/`.
- No redesign of the title bar, left navigation pane, compact flyout behavior, or `NavigationView` layout engine.

## Decisions

### 1. Add a Gallery page factory instead of extending `GalleryWindow::applyRoute()` inline

`GalleryWindow::applyRoute()` should remain a coordinator. It should ask a Gallery-owned page factory to create the correct page for a route, then replace the `NavigationView` content host page as it does today.

Proposed shape:

```text
GalleryWindow::applyRoute(routeId)
  └─ GalleryPageFactory::createPage(routeId, navigationItem, contentCatalog)
       ├─ home             -> GalleryHomePage
       ├─ category route   -> GalleryCategoryPage
       ├─ component route  -> GalleryComponentPage
       └─ settings         -> SettingsPage
```

Rationale: this keeps route/page creation testable and prevents `GalleryWindow` from growing into a content registry.

Alternatives considered:

- Keep all route branching inside `GalleryWindow`: rejected because component pages will add enough cases that the window class would become hard to reason about.
- Let `GalleryNavigationPane` create content pages: rejected because navigation row rendering and right-side content ownership must remain separate.

### 2. Keep navigation catalog and content catalog related but separate

`GalleryComponentCatalog` currently answers "what appears in the left navigation." The new content catalog should answer "what content does the selected route show."

The content catalog should include:

- route ID
- title
- subtitle or short description
- optional category ID
- page kind
- sample IDs for component routes
- related route IDs where useful

The two catalogs can share route IDs, but the content catalog should not drive tree structure. This avoids coupling visual navigation behavior to page layout.

Alternatives considered:

- Merge all page metadata into `GalleryComponentCatalog`: rejected because navigation data and content detail will evolve at different speeds.
- Runtime lookup from component headers or tests: rejected because packaged Gallery builds should not depend on source tree presence.

### 3. Model samples as data plus preview factories

A Gallery sample should be a small record plus a preview factory:

```text
GallerySample
  id
  title
  description
  codeSnippet
  createPreview(parent) -> QWidget*
  createOptionsPanel(parent) -> QWidget* optional
```

The first pass should seed only representative samples:

- `button`: standard/default action, accent action, disabled button, icon button
- `tree-view`: basic hierarchy, selection indicator, expand/collapse behavior
- `tab-view`: content-area tabs, close buttons, add-tab affordance

Rationale: a small factory-based sample model gives real live widgets without requiring every component page to be hand-built from scratch.

Alternatives considered:

- Hand-code every component page: rejected because it creates duplicated layouts and makes future pages inconsistent.
- Build a reflection/template system: rejected as too much infrastructure before the first real pages prove the shape.

### 4. Use common content page primitives

Right-side pages should use a small set of reusable app-layer widgets:

- `GalleryContentPage`: scrollable page shell with title, optional subtitle, and section layout.
- `GalleryCategoryPage`: category overview with component cards linking to component routes.
- `GalleryComponentPage`: detail page with overview, examples, and notes sections.
- `GallerySampleCard`: card with title, description, live preview, optional options panel, and code actions.
- `GalleryCodeBlock`: read-only code snippet view with copy affordance.

These widgets belong under `app/view/`; they can compose existing Fluent components but should not become reusable library controls in this phase.

### 5. Preserve placeholder fallback only as a safety net

Routes without content metadata may still fall back to `PlaceholderPage` during rollout, but category and component routes covered by this change must resolve to real content pages. The fallback should be treated as a temporary compatibility path, not the primary content contract.

### 6. Keep examples visually useful but narrow

The first samples should be enough to validate layout, interaction, theme refresh, and route wiring. They should not attempt to reproduce full WinUI Gallery documentation coverage.

The visual structure should stay close to:

```text
Component title
Short description

Overview
Examples
  Sample card
    preview
    code snippet / copy
API notes
Related
```

## Risks / Trade-offs

- [Risk] Content catalog drift from navigation catalog. -> Mitigation: tests verify every seeded content route is a known navigation route and representative navigation routes resolve to expected page kinds.
- [Risk] Sample factories can become ad hoc and inconsistent. -> Mitigation: centralize card layout and require sample records to use the same preview/code metadata shape.
- [Risk] Component pages may accidentally require component API changes. -> Mitigation: make the proposal and tests explicit that samples use public component APIs only.
- [Risk] Code snippets can become stale. -> Mitigation: keep snippets short and focused on the sample being shown; add tests only for presence, not exact long text.
- [Risk] A full content system could expand the scope. -> Mitigation: seed only `Button`, `TreeView`, and `TabView`; defer search, favorites, and exhaustive docs.

## Migration Plan

1. Add content catalog/page factory skeleton while preserving existing placeholder fallback.
2. Add shared content page primitives and route-to-page tests.
3. Implement category overview page behavior for categories that have content metadata.
4. Implement representative component pages for `Button`, `TreeView`, and `TabView`.
5. Replace covered route creation in `GalleryWindow` with page factory output.
6. Update shell/content tests and run focused Gallery tests.
7. Validate the OpenSpec change strictly.

Rollback is straightforward because all implementation is app-layer. Reverting the page factory and content widgets can restore the previous `PlaceholderPage` path without changing reusable component APIs.

## Open Questions

- Should the first `Home` page include only static overview content, or also show links to the seeded component pages?
- Should code snippets be plain read-only text in v1, or should they use a dedicated styled code block with copy button immediately?
- Should category overview cards show icon-only compact cards or richer cards with descriptions?
