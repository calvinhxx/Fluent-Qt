## Context

`GridView` currently uses `QListView::IconMode`, wrapping, `setUniformItemSizes(true)`, and a fixed `gridSize()` derived from `cellSize + spacing`. Its tests and drag-reorder animation assume each row maps to a regular grid slot. That is the right shape for fixed-size item galleries, but it is the wrong owner for variable-size flow layout.

The current test/demo `FluentGridItemDelegate` also returns `m_view->gridSize()` from `sizeHint()`, so it intentionally reinforces fixed cell semantics. `FlowView` needs either a new test delegate or model `Qt::SizeHintRole` data that can return different sizes per item.

## Goals / Non-Goals

Goals:

- Add a new `FlowView` component under `view::collections` that follows the library pattern: `QAbstractItemView, public FluentElement, public QMLPlus`.
- Support row-wrapping flow layout with mixed item widths and heights while preserving model order.
- Keep `GridView` unchanged for fixed-size grids.
- Support model/delegate ownership patterns already used by `ListView`, `GridView`, and `TreeView`: the component owns view behavior, while business/test code supplies the model and delegate.
- Provide accurate `visualRect()`, `indexAt()`, scrolling, selection, hover, keyboard navigation, and optional drag reorder for irregular item rectangles.
- Use Fluent theme tokens for viewport background, border, placeholder/header text, scrollbar, selection/hover states delegated through `QStyleOptionViewItem`, and drag feedback.

Non-goals for the first version:

- Masonry/waterfall layout that fills the shortest column.
- Virtualized item widgets or ownership of child widgets per model row.
- Editing support.
- New data models or production item delegates.
- Changing `GridView` API, behavior, or tests.

## Decisions

### Build `FlowView` on `QAbstractItemView`

Use `QAbstractItemView` rather than `QListView` because Qt's `QListView::IconMode` is optimized around regular grid geometry. A custom view can compute variable rectangles directly and still reuse Qt's model/delegate/selection infrastructure.

Alternatives considered:

- Extending `GridView`: rejected because fixed `gridSize`, uniform item sizes, and grid-slot drag animations are central to its current behavior.
- `QScrollArea` plus child widgets: rejected because it would abandon delegate painting, item selection, and the existing collection-view model pattern.

### First version uses row-wrap layout

The initial layout engine preserves model order and places each item left-to-right until the next item would exceed the available width, then starts a new row. Row height is the maximum item height in that row.

```
availableWidth = viewportWidth - margins.left - margins.right

x = margins.left
y = margins.top
rowHeight = 0

for row in model rows:
    size = itemSize(row)
    if x != margins.left and x + size.width > availableWidth:
        x = margins.left
        y += rowHeight + verticalSpacing
        rowHeight = 0

    itemRects[row] = QRect(x, y, size.width, size.height)
    x += size.width + horizontalSpacing
    rowHeight = max(rowHeight, size.height)
```

Masonry can be added later as a separate `layoutMode` once keyboard navigation and drag semantics are specified for column-balanced placement.

### Item size resolution

Resolve item size in this order:

1. `itemSizeRole` if configured and the model returns a valid `QSize`.
2. Delegate `sizeHint(option, index)` when an item delegate is installed.
3. `defaultItemSize` fallback.

Clamp the resolved size to `minimumItemSize` and `maximumItemSize` when those bounds are valid. This keeps pathological model/delegate sizes from breaking layout or scroll range.

### Layout cache and invalidation

Maintain a cached `QVector<QRect>` indexed by model row, plus content size and dirty flags. Recompute layout when:

- the viewport width changes,
- model rows are inserted, removed, moved, reset, or data changes for size-affecting roles,
- delegate changes,
- item-size role or min/max/default size changes,
- spacing or content margins change.

The first version may recompute the full cache for these events. That is simpler and robust for typical UI item counts. The cache design should leave room for later partial relayout from the first changed row.

### Scrolling and painting

Use the native vertical scrollbar as the source of truth and hide it visually behind the existing Fluent `ScrollBar`, matching `GridView`. `horizontalScrollBarPolicy` should remain off for the row-wrap first version.

Painting should only draw rows whose cached rect intersects the viewport rect translated by `verticalOffset()`. The delegate receives a `QStyleOptionViewItem` with the true item rect translated into viewport coordinates and states for enabled, selected, current, and hover; item focus borders are not requested by the view.

### Hit testing and navigation

`visualRect(index)` returns the cached item rect translated by the current scroll offset. `indexAt(point)` searches visible cached rects and returns the row containing the point.

Pointer selection is click based. In multiple or extended selection mode, additional selected items require Control or Meta modified clicks; dragging a pointer rectangle across items must not rubber-band select multiple items.

Starting a reorder drag on an unselected item without Control or Meta clears the previous selection and selects the dragged item, so the drag preview and final selection cannot show an additional selected item that the user did not modifier-select.

Keyboard navigation uses geometry instead of fixed columns:

- Left/Right move to previous/next model row when available.
- Up/Down choose the nearest item whose center is above/below the current item and whose horizontal center is closest.
- Home/End move to first/last model row.

This keeps navigation predictable even when item widths differ.

### Reorder semantics

If `canReorderItems` is enabled, compute the drop slot from the layout that would result after inserting the dragged row set. The drop indicator should be drawn at the final insertion slot, including row-wrap effects, rather than at the original edge of a non-dragged target item.

During drag feedback, non-dragged items may animate toward their final positions. The dragged source area should not be covered with a viewport-colored rectangle, because that can create a white patch over displaced items.

The first version should support `QStandardItemModel` reordering like `GridView` does. For other models, the view may show drag feedback but should not mutate the model unless supported by a future model contract.

### Header, placeholder, and container chrome

Mirror the collection-view chrome already present in `GridView`:

- optional `headerText` above the viewport,
- `placeholderText` centered when the model is empty,
- optional rounded border using Fluent stroke/background tokens,
- Fluent vertical scrollbar overlay.

The header affects viewport margins; it should not be part of the flow layout cache.

## Risks / Trade-offs

- Variable item geometry makes drag reorder and keyboard navigation less deterministic than fixed grid slots. Mitigation: define geometry-based nearest-neighbor rules and cover them with focused tests.
- Full relayout on every size-affecting model change may be more expensive than GridView's built-in layout. Mitigation: keep the first implementation simple, then optimize partial relayout only if tests or demos expose real performance issues.
- Delegate `sizeHint()` may depend on view width and cause relayout loops. Mitigation: size hints should be read with a stable option width and clamped; relayout should be scheduled rather than recursively triggered during paint.
- Reusing `FluentGridItemDelegate` directly would preserve fixed `gridSize()` behavior. Mitigation: create a FlowView-specific test delegate or use `Qt::SizeHintRole` data in tests.

## Open Questions

- Should masonry/waterfall become `FlowView::LayoutMode::Masonry` later, or a separate `MasonryView` component? Keep this out of the first version.
- Should public item-size customization prefer `itemSizeRole` only, or also expose a callback/delegate hook? First version should prefer Qt model roles and delegate `sizeHint()` to stay idiomatic.