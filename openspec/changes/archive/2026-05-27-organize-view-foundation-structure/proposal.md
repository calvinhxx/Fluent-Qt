## Why

`src/view` currently mixes component categories with core mixins, private implementation headers, and shared overlay helpers. That makes it harder to see which files are reusable view foundation, which files are public component APIs, and which files are internal implementation details before the project is renamed or moved.

## What Changes

- Introduce a `src/view/foundation/` area for shared view-layer infrastructure.
- Move `FluentElement` and `QMLPlus` implementation files into the foundation area while preserving their public include compatibility.
- Move `FluentElement_p.h` into a private foundation subdirectory so private implementation is no longer presented beside public component entry points.
- Move same-window overlay helpers from `src/view/internal/overlay/` into `src/view/foundation/overlay/`.
- Keep existing component category directories directly under `src/view/`; do not add a `src/view/components/` wrapper in this change.
- Preserve existing namespaces, component inheritance contracts, runtime behavior, and test-facing APIs.
- Update docs, CMake/source references, and focused tests for the new layout.

## Capabilities

### New Capabilities
- `view-foundation-structure`: Defines how view-layer foundation files, private implementation details, overlay helpers, and component category directories are organized and kept compatible.

### Modified Capabilities
- None.

## Impact

- Affected code: `src/view/FluentElement*`, `src/view/QMLPlus*`, `src/view/internal/overlay/*`, overlay users, and CMake source discovery if needed.
- Affected docs: architecture/development notes that describe overlay helper location or component foundation conventions.
- Compatibility: existing includes such as `view/FluentElement.h`, `view/QMLPlus.h`, and existing overlay helper includes should remain usable through forwarding headers during the transition.
- Tests: focused foundation tests and overlay-consuming component tests should validate that the move does not change behavior.
