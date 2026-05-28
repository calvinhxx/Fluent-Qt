## 1. TreeView Motion API

- [x] 1.1 Add TreeView enum types for selected indicator vertical direction and hierarchy transition, with Qt meta-object exposure for tests and callers.
- [x] 1.2 Add additive TreeView query APIs/properties for indicator motion progress, current direction, current hierarchy transition, and animation enablement.
- [x] 1.3 Add private state for previous/current persistent indexes, active indicator index, normalized animation progress, and one reusable indicator animation object.
- [x] 1.4 Ensure duplicate property values and repeated selection of the same item do not emit duplicate signals or restart equivalent animations.

## 2. Motion Classification And Lifecycle

- [x] 2.1 Override or connect the current/selection change path so TreeView computes indicator motion whenever selected/current item changes.
- [x] 2.2 Implement visual direction classification for upward, downward, and neutral transitions using visible row order or visual geometry.
- [x] 2.3 Implement hierarchy depth classification for inward, outward, and same-level transitions by walking model parent indexes.
- [x] 2.4 Clear transient motion state on invalid selection, model reset, selection model replacement, hidden/collapsed endpoints, and TreeView teardown.
- [x] 2.5 Drive indicator progress with Fluent animation duration/easing tokens, and snap synchronously when animation is disabled.

## 3. Fluent Delegate Rendering

- [x] 3.1 Update `FluentTreeItemDelegate` to query TreeView indicator motion state instead of owning all selected-indicator transition classification itself.
- [x] 3.2 Render direction-aware indicator treatments that differ for upward and downward movement while ending at the same final row-local indicator geometry.
- [x] 3.3 Render hierarchy-aware treatments that differ for inward and outward movement while preserving row height, indentation, icon, checkbox, and text layout.
- [x] 3.4 Keep theme colors, disabled handling, selected/hover/pressed backgrounds, chevron rotation, and checkbox behavior compatible with the existing delegate.

## 4. Automated Tests And VisualCheck

- [x] 4.1 Add TreeView tests for initial neutral selection, upward movement, downward movement, inward movement, outward movement, and same-level movement.
- [x] 4.2 Add TreeView tests for invalid index clearing, model reset safety, selection model replacement safety, duplicate signal suppression, and animation-disabled snapping.
- [x] 4.3 Add delegate integration tests proving motion state is consumed without changing final indicator geometry or row layout metrics.
- [x] 4.4 Extend TreeView VisualCheck with a nested Fluent sample that can trigger upward, downward, inward, and outward indicator transitions.
- [x] 4.5 Keep the VisualCheck primary layout on `AnchorLayout` and use project Fluent components such as `Button`, `Label`, and related custom controls where equivalents exist.
- [x] 4.6 Preserve the `SKIP_VISUAL_TEST` guard so automated runs do not open an interactive window.

## 5. Validation

- [x] 5.1 Build the focused TreeView test target with the active CMake preset/build directory.
- [x] 5.2 Run the TreeView test binary with `SKIP_VISUAL_TEST=1`.
- [x] 5.3 Run a focused `ctest` pattern covering TreeView and adjacent collection view regressions where practical.
- [x] 5.4 Optionally run the manual TreeView VisualCheck without `SKIP_VISUAL_TEST=1` to inspect the two requested indicator motion effects.
