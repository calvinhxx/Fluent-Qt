## Context

The previous Gallery attempt grew too much app behavior at once. This change restarts with a narrow shell milestone: match the broad layout of the WinUI Gallery reference image, prove the navigation frame works, and use colored placeholder labels for page content until later changes introduce real component samples.

The component library already provides the building blocks needed for the shell: `fluent::windowing::Window`, `fluent::navigation::NavigationView`, `fluent::navigation::StackContentHost`, `fluent::textfields::AutoSuggestBox`, `fluent::textfields::Label`, and button-like controls. `NavigationView` provides pane geometry and content hosting, but it intentionally does not own a Gallery-specific item model, so the app must supply its own navigation data and row widgets.

## Goals / Non-Goals

**Goals:**
- Add an optional Gallery executable that launches a stable shell window.
- Recreate the reference layout at framework level: custom title/header area, left navigation pane, top search affordance, and right content surface.
- Provide first-pass navigation groups such as Home, Fundamentals, Design, Accessibility, Controls, Basic input, Collections, Date & time, Dialogs & flyouts, Navigation, Scrolling, Status & info, Text fields, Windowing, and Settings.
- Show a visually distinct colored placeholder `Label` page for each navigable entry so routing can be reviewed immediately.
- Keep all Gallery-specific state, catalog data, and placeholder page factories under `app/`.
- Add unit tests for construction, navigation data, route switching, and placeholder page selection.

**Non-Goals:**
- No real component sample cards or code snippets.
- No search results page or query filtering behavior beyond showing the search box affordance.
- No favorites, recent items, recently updated lists, hero carousel, external links, or persisted settings.
- No changes to public component APIs under `src/components/`.
- No pixel-perfect Figma reproduction in this phase.

## Decisions

### Decision: Build a vertical shell slice instead of separate model/view phases

The first implementation should include the executable, shell, navigation data, placeholder pages, and tests together. This keeps the main user path reviewable after the first apply step.

Alternative considered: create catalog, routing, and visual shell in separate changes. That was rejected because it can produce individually valid artifacts that do not form a usable app.

### Decision: Keep the Gallery navigation model in `app/`

The app should define simple navigation records with stable IDs, display titles, optional icon glyphs, parent/group information, and placeholder color metadata. `NavigationView` remains a reusable layout component rather than becoming aware of Gallery categories.

Alternative considered: extend `NavigationView` with item data APIs. That would mix application catalog behavior into a library component and make later component API review harder.

### Decision: Use explicit placeholder pages

Each navigation entry should create a right-side placeholder page containing a colored surface and a `Label` naming the route. This makes route changes visible during manual review and testable without real samples.

Alternative considered: keep a single generic placeholder text for all routes. That would make navigation appear wired while hiding whether content actually changes.

### Decision: Add the Gallery as an optional app target

The root build should expose an option such as `FLUENT_QT_BUILD_GALLERY` and add `app/` only when enabled. Tests can be added under `tests/gallery/` and guarded by the same option so normal library validation remains focused.

Alternative considered: always build the Gallery executable. Optional build keeps app iteration from surprising library-only consumers, while still allowing local Gallery validation.

### Decision: Prefer existing Fluent components, but allow small app-only row widgets

Visible shell controls should reuse existing Fluent components where they fit. If the left navigation rows need selection indication or indentation that current components do not provide, implement small app-only widgets under `app/gallery/shell/` rather than changing shared components.

Alternative considered: use raw Qt widgets throughout the shell for speed. That would undercut the purpose of the Gallery as a companion app for this component library.

## Risks / Trade-offs

- Placeholder pages may look too complete and be mistaken for final component pages -> Mitigate by naming the milestone and page text clearly as placeholders in code/tests, while keeping the UI clean.
- Optional CMake wiring can drift from tests -> Mitigate by adding a focused Gallery test target that builds with the app sources.
- Left navigation visuals may need custom painting before a reusable navigation item component exists -> Mitigate by keeping that code app-local and small.
- The first shell may not be pixel-perfect against the Figma/WinUI reference -> Mitigate by treating this change as structural; later polish changes can refine spacing, hero content, and responsive behavior.