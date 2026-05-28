## Why

`organize-view-foundation-structure` introduced canonical `src/view/foundation/` paths, but the view root still contains compatibility forwarding headers and `src/view/internal/overlay/` forwarding headers. The project is still early enough to complete the structural cleanup now, before stale include paths become a long-term convention.

## What Changes

- **BREAKING**: Remove legacy forwarding headers `src/view/FluentElement.h` and `src/view/QMLPlus.h`.
- **BREAKING**: Remove legacy overlay forwarding headers under `src/view/internal/overlay/`.
- Update all project-owned source and tests to include `view/foundation/FluentElement.h`, `view/foundation/QMLPlus.h`, and `view/foundation/overlay/...` directly.
- Update development guidance and overlay documentation so legacy include paths are no longer described as supported compatibility entry points.
- Preserve existing namespaces, component inheritance contracts, runtime behavior, and overlay behavior; this change is structural include cleanup only.

## Capabilities

### New Capabilities
- `view-foundation-canonical-includes`: Defines canonical-only include behavior for view foundation mixins and overlay helpers after legacy forwarding headers are removed.

### Modified Capabilities
- None.

## Impact

- Affected code: public and test includes referencing `view/FluentElement.h`, `view/QMLPlus.h`, or `view/internal/overlay/...`; legacy forwarding header files; docs that mention legacy compatibility.
- API compatibility: source-level breaking change for consumers that still include removed legacy paths.
- Runtime behavior: no intended behavior changes.
- Validation: broad build and focused foundation/overlay/component tests are required because public include headers are removed and many component headers will change include paths.