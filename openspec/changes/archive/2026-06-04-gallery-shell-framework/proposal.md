## Why

The Gallery effort needs a small, stable application frame before adding real component samples. The immediate goal is to recreate the WinUI Gallery-style layout direction from the provided reference image while keeping the first implementation intentionally shallow and easy to validate.

## What Changes

- Add a new Gallery application shell framework under `app/` with a top-level window, title/header area, left navigation pane, and right content host.
- Model the first screen after the reference image: a persistent left navigation rail/list, a top search/header affordance, and a large right-side content area.
- Use colored `Label` placeholder pages in the right content area instead of real component sample pages for this phase.
- Provide navigation entries for the major Gallery groups so the shell structure can be reviewed before component content is built.
- Add focused tests that verify the Gallery shell can be constructed, exposes expected navigation entries, and switches placeholder content when navigation changes.
- Avoid implementing real component sample cards, code snippets, favorites, recent items, or search result behavior in this phase.

## Capabilities

### New Capabilities
- `gallery-shell-framework`: Covers the initial Gallery application frame, left navigation structure, top header/search affordance, and colored placeholder content pages.

### Modified Capabilities
- None.

## Impact

- Adds an optional Gallery executable target to the CMake project.
- Adds app-layer code under `app/` without changing public component APIs under `src/components/`.
- Adds Gallery-specific tests under `tests/gallery/` using the existing Qt/GTest test infrastructure.
- Reuses existing Fluent Qt components such as `Window`, `NavigationView`, `AutoSuggestBox`, `Label`, and `Button` where they fit the shell.