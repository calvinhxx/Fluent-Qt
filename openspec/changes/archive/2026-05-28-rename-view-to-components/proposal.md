## Why

The reusable widget library currently lives under `src/view/`, while the project also has an application-layer `app/view/` area for Gallery work. Renaming the reusable component source tree to `src/components/` makes the architecture boundary explicit before the Gallery application grows around model/view/viewmodel structure.

## What Changes

- **BREAKING**: Rename the reusable component source tree from `src/view/` to `src/components/`.
- **BREAKING**: Rename matching component tests from `tests/views/` to `tests/components/`.
- Update public include paths in project code, tests, docs, and OpenSpec specs from `view/...` to `components/...`.
- Preserve component behavior, public class names, Qt inheritance, properties, signals, and the current `view::...` C++ namespace during this change.
- Keep `app/` as the application-layer home for Gallery model/view/viewmodel work; do not move Gallery code into the component library.
- Update validation docs, CTest labels, VisualCheck guidance, and agent instructions so future component work uses the new component paths.

## Capabilities

### New Capabilities
- `component-source-layout`: Defines the canonical source, include, test, and application-boundary layout for reusable Fluent Qt components.

### Modified Capabilities
- `component-api-consistency`: Update component API audit and convention requirements from `src/view/**` and `tests/views/**` to the new component tree.
- `test-infrastructure`: Update component test registration and CTest labeling requirements from `tests/views/**` to `tests/components/**`.
- `developer-workflow-docs`: Update reusable workflow documentation requirements so README, development docs, and agent guidance reference `src/components/` and `tests/components/`.
- `view-foundation-structure`: Replace the old `src/view/foundation/` canonical home with `src/components/foundation/` while preserving foundation and overlay behavior.
- `source-comment-style`: Update source comment guidance coverage from `src/view/**` to `src/components/**`.

## Impact

- Source paths under `src/view/**`, including foundation, component categories, overlay helpers, and windowing components.
- Test paths under `tests/views/**`, test labels, focused CTest examples, and VisualCheck instructions.
- Project includes, forwarding headers if retained, CMake source discovery, Qt Creator grouping, and build/test target output paths.
- Documentation and OpenSpec references in README, AGENTS.md, `docs/development/`, `docs/architecture/`, and live capability specs.
- No dependency changes and no intended runtime behavior changes.
