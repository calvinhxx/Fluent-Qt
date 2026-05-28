## Why

`GridView` is built around same-size cells, which works for icon grids and fixed photo cards but cannot naturally represent variable-size chips, tags, media cards, or dashboard tiles. A dedicated `FlowView` gives the collection layer a first-class row-wrapping surface for mixed item sizes without weakening the existing fixed-grid semantics and tests.

## What Changes

- Add a new `view::collections::FlowView` component for model/delegate driven, variable-size row-wrap layouts.
- Support per-item sizes from the delegate `sizeHint()` and/or a configurable item size role, with min/max clamping and Fluent spacing/margins.
- Provide `visualRect()`, `indexAt()`, selection, hover, keyboard navigation, scrolling, placeholder/header visuals, and theme integration for irregular item geometry.
- Add optional drag reorder support using actual item rectangles instead of fixed grid slots.
- Add focused unit tests and VisualCheck coverage for mixed-width, mixed-height, resize, scrolling, selection, and reorder scenarios.
- Keep `GridView` behavior unchanged; fixed same-size grids remain owned by `GridView`.

## Capabilities

### New Capabilities

- `flow-view`: Covers a Fluent collection view that lays out model items in a row-wrapping flow with variable item sizes, accurate hit testing, scrolling, selection, and optional reorder interactions.

### Modified Capabilities

- None.

## Impact

- New source files under `src/view/collections/FlowView.h` and `src/view/collections/FlowView.cpp`.
- New tests under `tests/views/collections/TestFlowView.cpp`, plus any test-only delegate/model helpers needed to exercise variable item sizes.
- Update `tests/views/collections/CMakeLists.txt` to register `test_flow_view`.
- Potential update to collection documentation/examples to distinguish fixed `GridView` from variable-size `FlowView`.
- No new external dependencies.