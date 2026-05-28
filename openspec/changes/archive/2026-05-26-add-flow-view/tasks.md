## 1. Component Structure and API

- [x] 1.1 Add `src/view/collections/FlowView.h` and `src/view/collections/FlowView.cpp` with `view::collections::FlowView` inheriting `QAbstractItemView, public FluentElement, public QMLPlus`.
- [x] 1.2 Define public enums, properties, signals, and helpers for selection mode, font role, border visibility, header text, placeholder text, default item size, minimum/maximum item size, item size role, horizontal/vertical spacing, content margins, viewport hover, and `canReorderItems`.
- [x] 1.3 Provide collection convenience APIs matching nearby views where appropriate: `selectedIndex()`, `selectedRows()`, `setSelectedIndex()`, `verticalFluentScrollBar()`, and `refreshFluentScrollChrome()`.
- [x] 1.4 Keep `GridView` source, API, and tests unchanged while adding `FlowView` as a separate component.

## 2. Flow Layout Engine

- [x] 2.1 Implement item-size resolution from `itemSizeRole`, delegate `sizeHint()`, and `defaultItemSize`, including min/max clamping and invalid-size fallback.
- [x] 2.2 Implement a row-wrap layout cache that stores one `QRect` per model row, preserves model order, wraps by available viewport width, and uses the tallest item as row height.
- [x] 2.3 Invalidate and recompute layout on viewport resize, model reset, rows inserted/removed/moved, size-affecting data changes, delegate changes, spacing changes, content margin changes, and item-size configuration changes.
- [x] 2.4 Implement `visualRect()`, `indexAt()`, `scrollTo()`, `horizontalOffset()`, `verticalOffset()`, `isIndexHidden()`, `setSelection()`, `visualRegionForSelection()`, and other required `QAbstractItemView` virtuals against the cached geometry.
- [x] 2.5 Synchronize native vertical scrollbar range/page step with computed content height and keep horizontal scrolling disabled for the first row-wrap version.

## 3. Painting, Theme, and Chrome

- [x] 3.1 Paint the viewport background, optional rounded border, empty placeholder, and delegate-rendered visible items using real item rectangles translated by scroll offset.
- [x] 3.2 Populate `QStyleOptionViewItem` with correct enabled, selected, current, hover, focus, and font states before invoking the delegate.
- [x] 3.3 Add optional header text above the flow viewport without making it part of the flow layout cache.
- [x] 3.4 Add and synchronize the Fluent vertical `ScrollBar` overlay, hiding native scrollbars visually.
- [x] 3.5 Implement `onThemeUpdated()` so FlowView chrome, header, placeholder, scrollbar, and drag feedback refresh from Fluent tokens.
- [x] 3.6 Provide accessible-name fallback from `headerText` when no explicit accessible name is set.

## 4. Pointer, Selection, and Keyboard Interaction

- [x] 4.1 Map FlowView selection modes to Qt item-view selection behavior and emit `selectionModeChanged()` when changed.
- [x] 4.2 Implement pointer hover tracking, pointer hit testing through `indexAt()`, click selection, and `itemClicked(int row)` emission.
- [x] 4.3 Implement keyboard navigation for Left/Right previous/next row, Up/Down nearest geometric item above/below, and Home/End first/last row.
- [x] 4.4 Ensure disabled state blocks interactions while preserving paint and geometry behavior.

## 5. Optional Drag Reorder

- [x] 5.1 Implement `canReorderItems` with reorder disabled by default and feedback cleanup when disabled mid-drag.
- [x] 5.2 Render drag pixmaps and source-item ghosting using actual FlowView item rectangles and HiDPI-safe pixmaps.
- [x] 5.3 Compute drop indicator slots from actual item geometry, including end-of-row and end-of-list cases.
- [x] 5.4 Animate non-source item displacement toward their prospective row-wrap positions during drag feedback.
- [x] 5.5 On drop, reorder rows for `QStandardItemModel`, preserve selection by item identity, and emit `itemReordered(int fromIndex, int toIndex)`.

## 6. Tests and VisualCheck

- [x] 6.1 Add `tests/views/collections/TestFlowView.cpp` with focused helpers/models/delegates that provide mixed item sizes through `Qt::SizeHintRole` and delegate `sizeHint()`.
- [x] 6.2 Test defaults, property setters/signals, selection mode mapping, header/placeholder state, accessible-name fallback, and unchanged `GridView` assumptions where needed.
- [x] 6.3 Test row-wrap layout, tallest-row height, resize relayout, item-size role precedence, delegate fallback, default fallback, min/max clamping, `visualRect()`, `indexAt()`, and empty-space hit testing.
- [x] 6.4 Test vertical scroll range, `scrollTo()`, Fluent scrollbar synchronization, theme updates, disabled interaction blocking, pointer click selection, and keyboard navigation.
- [x] 6.5 Test optional drag reorder for mixed-size rows, including geometry-based drop slots, model row mutation, selection preservation, and signal emission.
- [x] 6.6 Add a guarded `TestFlowView.VisualCheck` using `AnchorLayout`, project Fluent components, mixed-width chips/cards, mixed-height cards, scrolling, selection, disabled state, drag-enabled example, and theme switching.
- [x] 6.7 Register `test_flow_view` in `tests/views/collections/CMakeLists.txt` with `add_qt_test_module`.

## 7. Validation

- [x] 7.1 Build the focused target with `cmake --build --preset vcpkg-osx --target test_flow_view`.
- [x] 7.2 Run focused tests with VisualCheck skipped: `SKIP_VISUAL_TEST=1 ./build/vcpkg-osx/tests/views/collections/test_flow_view`.
- [x] 7.3 Run affected collection tests through ctest filter: `ctest --preset vcpkg-osx -R "FlowView|GridView|ListView" --output-on-failure`.
- [x] 7.4 Run `openspec validate add-flow-view --strict` and confirm `openspec status --change add-flow-view` is apply-ready.