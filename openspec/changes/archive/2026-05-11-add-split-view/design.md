## Context

`src/view/collections/` contains view-style controls such as `ListView`, `GridView`, `TreeView`, and `FlipView`, but there is no container for resizable panes. Raw `QSplitter` can provide basic dragging, yet it does not follow this library's `QWidget + FluentElement + view::QMLPlus` component pattern, does not expose QML SplitView-style fill/preferred sizing semantics, and would require QSS or style subclassing to match Fluent handle states.

Reference behavior comes from two places. Qt Quick Controls `SplitView` defines the splitter-container model: horizontal/vertical layout, draggable handles, per-item minimum/preferred/maximum sizes, exactly one fill item receiving leftover space, a read-only resizing state, and `saveState()` / `restoreState()` for persisted preferred sizes. WinUI `SplitView` defines the pane/content product pattern used by WinUI Gallery: a pane plus main content, left/right placement, `IsPaneOpen`, `DisplayMode`, `OpenPaneLength`, `CompactPaneLength`, and Fluent pane background/overlay expectations.

This change targets the QML SplitView layout primitive while keeping WinUI Gallery-inspired examples and Fluent visuals. It is not a NavigationView replacement.

## Goals / Non-Goals

**Goals:**

- Add `view::collections::SplitView` as a custom Qt Widgets Fluent container.
- Support horizontal and vertical multi-pane layouts with draggable handles between visible panes.
- Provide explicit widget-side APIs equivalent to QML attached properties: minimum, preferred, maximum, and fill per pane.
- Track and expose resizing state, handle hover/pressed state, and pane size changes through signals.
- Provide state persistence through `saveState()` and `restoreState()` using preferred sizes, orientation, and fill pane index.
- Render handles using theme tokens without QSS/QPalette-driven styling.
- Include tests and VisualCheck for QML-style three-pane splitters and WinUI Gallery-style pane/content scenarios.

**Non-Goals:**

- Do not implement WinUI `NavigationView` menu selection, hamburger button, or full adaptive navigation shell.
- Do not implement overlay/light-dismiss `SplitViewDisplayMode` behavior in v1; WinUI-inspired examples can compose existing `Popup`/overlay controls later if needed.
- Do not use `QSplitter` as the public base class; the component owns layout and handle painting directly.
- Do not introduce a QML attached-property system for Qt Widgets; use explicit per-pane methods and optional simple option structs.
- Do not support nested splitter persistence across arbitrary object names in v1; state applies to the current pane order/count.

## Decisions

### 1. Implement as QWidget container, not QSplitter subclass

`SplitView` will inherit `QWidget`, `FluentElement`, and `view::QMLPlus`. It will maintain an ordered list of pane records containing the child widget, current preferred size, min/max bounds, fill flag, visibility, and cached geometry. Handles are virtual rectangles painted by the parent and handled through mouse events.

Rationale: this matches existing collection controls and keeps handle hit testing, visual thickness, hover/pressed colors, and QML-style fill logic under our control. A `QSplitter` subclass was considered, but its stretch-factor/size model differs from QML SplitView's preferred/fill semantics and would make exact handle visuals harder without style hacks.

### 2. Use explicit pane APIs instead of complex custom meta-types

The public API should stay small and inspectable:

- `addPane(QWidget* pane, const SplitViewPaneOptions& options = {})`
- `insertPane(int index, QWidget* pane, const SplitViewPaneOptions& options = {})`
- `removePane(QWidget*)` / `removePaneAt(int)`
- `paneAt(int)`, `paneCount()`, `indexOf(QWidget*)`
- `setPaneMinimumSize(QWidget*, int)`, `setPanePreferredSize(QWidget*, int)`, `setPaneMaximumSize(QWidget*, int)`, `setPaneFill(QWidget*, bool)`
- `paneMinimumSize()`, `panePreferredSize()`, `paneMaximumSize()`, `isPaneFill()`
- `setFillPaneIndex(int)`, `fillPaneIndex()`
- `saveState()` / `restoreState(const QByteArray&)`

`SplitViewPaneOptions` can be a lightweight value type used only as method input. Signals should pass simple values (`int`, `QWidget*`, `QByteArray`) rather than complex pane structures.

### 3. Layout mirrors QML SplitView preferred/fill behavior

For the active orientation, each visible pane gets a one-dimensional length. Non-fill panes start from their preferred size clamped to min/max. The fill pane receives remaining space after handles and non-fill panes are laid out, then is also clamped. If no pane is explicitly fill, the last visible pane fills by default. If multiple panes are marked fill, the first visible fill pane wins for horizontal and vertical layouts, matching QML's “one fill item” model.

When dragging a handle, the implementation updates the preferred sizes of the two panes adjacent to the handle. The sum of those two pane lengths remains stable except when min/max constraints are hit. The layout then re-runs so the fill pane and all constrained panes remain valid.

### 4. Handles have separate hit and visual metrics

QML SplitView notes that handle visuals should not own input and that hit size can be larger than visual size. The Qt Widgets version will expose `handleWidth` for hit geometry and `handleVisualThickness` for the painted line. Defaults should be touch/mouse friendly while visually quiet, e.g. 8px hit width and 2px visual thickness. Hover/pressed states use `themeColors().subtleSecondary` / accent-ish foreground, and disabled state uses disabled text/stroke tokens.

### 5. State persistence is order-based and conservative

`saveState()` returns a compact `QByteArray` containing a version, orientation, fill pane index, pane count, and current preferred sizes. `restoreState()` validates version and pane count before applying sizes. It returns `false` without partial mutation if the state does not match the current pane count or is malformed.

This is enough for app settings scenarios and avoids coupling to object names or user-defined IDs in v1.

### 6. VisualCheck demonstrates both references

The VisualCheck should include two scenes:

- A QML-style three-pane splitter where the center pane fills remaining space and the user can drag both handles.
- A WinUI Gallery-inspired navigation/content layout with a left/right pane placement toggle, compact/open pane width controls, and theme toggle. This is an example built from the same resizable-pane primitive, not a full WinUI SplitView display-mode clone.

## Risks / Trade-offs

- [Risk] A custom layout engine may drift from `QSplitter` edge cases. → Mitigation: keep v1 algorithm simple, test min/preferred/max/fill invariants, and expose geometry query helpers for tests.
- [Risk] Dragging constrained panes can feel sticky when min/max bounds are hit. → Mitigation: clamp deltas and keep cursor feedback stable; tests should cover attempts to drag beyond constraints.
- [Risk] `saveState()` can restore the wrong layout if panes are reordered. → Mitigation: require matching pane count in v1 and document order-based state; return `false` on mismatch.
- [Risk] WinUI `SplitView` users may expect overlay/light-dismiss modes. → Mitigation: name this capability as a SplitView container with explicit non-goals; reserve display-mode extensions for a later change.

## Migration Plan

1. Add `SplitView.h/.cpp` under `src/view/collections/`.
2. Add `TestSplitView.cpp` and register `test_split_view` in collection tests.
3. Implement pane management and deterministic layout before handle interaction.
4. Add handle painting/input, then persistence and VisualCheck.
5. Validate with `cmake --build build --target test_split_view` and `SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_split_view`.

## Open Questions

- Whether a future WinUI-specific `NavigationSplitView` wrapper should be introduced for `IsPaneOpen`, compact pane length, overlay, and light-dismiss behavior.
- Whether pane state should later support stable application-defined IDs instead of order-only persistence.