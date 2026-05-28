## Why

TreeView already presents a Fluent selected-row accent, but the indicator transition is a simple appear animation that does not communicate movement direction or hierarchy changes in nested trees. A more expressive indicator motion will make TreeView feel closer to WinUI navigation/tree patterns while keeping selection behavior predictable.

## What Changes

- Add TreeView selected indicator motion semantics that can distinguish upward vs. downward selection movement.
- Add hierarchy-aware indicator motion semantics that can distinguish moving deeper into child items from moving outward to parent/sibling levels.
- Keep TreeView model ownership, delegate injection, item content, and application navigation decisions outside the component.
- Expose enough deterministic motion state for Fluent delegates and tests without requiring screenshot-only verification.
- Add automated and VisualCheck coverage for direction-aware and hierarchy-aware indicator transitions.

## Capabilities

### New Capabilities
- `tree-view`: Fluent TreeView selection indicator behavior, motion state, testing hooks, and VisualCheck coverage.

### Modified Capabilities

## Impact

- Affected source: `src/view/collections/TreeView.h`, `src/view/collections/TreeView.cpp`.
- Affected tests/demo support: `tests/views/collections/TestTreeView.cpp`, `tests/views/collections/FluentTreeItemDelegate.h`, `tests/views/collections/FluentTreeItemDelegate.cpp`.
- No new external dependencies.
- No breaking API changes are planned; new APIs should be additive and default-compatible.
