## Context

`DrawerView` should bring the QML `Drawer` mental model into this Qt Widgets Fluent component library: an edge-attached panel that can host arbitrary QWidget content, slide between hidden and open positions, and optionally behave as a modal light-dismiss overlay. Existing `Popup` and `Flyout` cover centered or anchor-based floating surfaces, while `SplitView` and `NavigationView` cover persistent layout chrome. DrawerView fills the gap for temporary edge panels without changing those components.

## Goals / Non-Goals

**Goals:**

- Add `view::collections::DrawerView` under `src/view/collections/` using the standard `QWidget + FluentElement + view::QMLPlus` pattern.
- Support QML Drawer-like edge placement: left, right, top, and bottom.
- Expose open/close lifecycle, `isOpen`, normalized `position` progress, `interactive`, `dragMargin`, drawer length, modal/dim behavior, close policy, and animation enablement.
- Host one arbitrary QWidget content item and lay it out inside the visible drawer panel.
- Keep the drawer in the same top-level QWidget rather than creating a `QDialog` or native window.
- Provide deterministic geometry helpers and automated tests for edge placement, content parenting, lifecycle signals, light dismiss, drag gestures, theme switching, and animation-disabled execution.
- Provide a VisualCheck sample that demonstrates all four edges, modal dimming, content hosting, and interactive gestures.

**Non-Goals:**

- Do not implement navigation item models, routing, selected-item state, or settings/search/back commands.
- Do not replace `Popup`, `Flyout`, `SplitView`, `StackView`, or `NavigationView` APIs.
- Do not depend on Qt Quick/QML runtime types.
- Do not add native platform chrome, separate OS windows, or new third-party dependencies.
- Do not build a general overlay manager shared by all popup-like components in this change.

## Decisions

### 1. DrawerView is a standalone same-window widget

`DrawerView` inherits `QWidget`, `FluentElement`, and `view::QMLPlus`. When opened, it resolves the top-level widget from its original parent and reparents/moves itself as a child of that top-level. It stays a normal QWidget and MUST NOT set `Qt::Window` or `Qt::Dialog` flags.

This follows the same same-window overlay direction as `Popup`, but keeps DrawerView standalone because drawer geometry, edge dragging, and partial-open `position` behavior are different enough from card/flyout positioning.

### 2. Geometry is edge + length + position

DrawerView exposes an edge enum:

```cpp
enum class DrawerEdge { Left, Right, Top, Bottom };
```

The drawer has one open-axis `drawerLength`. Left/right drawers use the length as width and fill the top-level height. Top/bottom drawers use the length as height and fill the top-level width. `position` is clamped to `[0.0, 1.0]`, where `0.0` is fully hidden and `1.0` is fully open. The component computes a panel rect from the top-level content rect, edge, drawer length, and position. The two corners opposite the attached edge use a configurable `outerCornerRadius`, defaulting to the Fluent overlay radius.

### 3. Content hosting is intentionally simple

The component owns at most one content widget through `setContentWidget(QWidget*)`. The assigned widget is reparented into DrawerView, shown while the drawer is visible, and laid out to the fully visible panel content rect. Replacing content detaches the old widget without deleting it.

### 4. Modal, dim, and light-dismiss stay local

DrawerView exposes modal/dim booleans and a close policy flag set:

```cpp
enum class DrawerClosePolicyFlag {
    NoAutoClose,
    CloseOnPressOutside,
    CloseOnEscape
};
```

When `modal` is true, an internal scrim widget covers the top-level below the drawer and blocks background pointer input. When `dim` is true, the scrim paints a smoke-style translucent fill from Fluent material tokens and its opacity follows `position`, so open/close and drag transitions fade the dim layer smoothly. For non-modal drawers, background widgets remain interactive; DrawerView uses a top-level event filter to implement outside-click and Escape light-dismiss without owning the whole window surface.

### 5. Animation is position-based

Opening and closing animate the `position` property with a `QPropertyAnimation` using Fluent animation duration/easing tokens. `animationEnabled=false` completes state changes synchronously, which keeps automated tests deterministic. The drawer slides only along its edge axis; content is clipped by the drawer bounds rather than scaled.

### 6. Interactive dragging mirrors QML Drawer, but stays conservative

When `interactive` is true, a pointer press inside the open drawer or within `dragMargin` of the configured edge can start a drag. Drag movement updates `position` directly. Release chooses open or closed using a simple halfway threshold. Disabled drawers and `interactive=false` ignore drag gestures.

### 7. Painting uses Fluent tokens

DrawerView paints its panel background, border stroke, and shadow/elevation with existing theme/design token access. It does not use QSS/QPalette for drawer surfaces. The scrim uses Fluent smoke/material colors and tracks theme changes.

## Risks / Trade-offs

- [Risk] Same-window reparenting can be tricky when the original parent is destroyed. -> Mitigation: keep the original parent in a `QPointer`, remove event filters on close/destruction, and close safely if the top-level disappears.
- [Risk] Non-modal outside-click handling with event filters can accidentally consume events. -> Mitigation: outside presses used for light-dismiss should close the drawer but allow the event to continue to the target widget.
- [Risk] Edge dragging while closed requires watching top-level events. -> Mitigation: only install the top-level filter when the drawer has a valid top-level and `interactive` is true; keep drag detection limited to `dragMargin`.
- [Risk] Top/bottom drawers can cover large vertical areas on small windows. -> Mitigation: clamp `drawerLength` to the available top-level size during geometry calculation while preserving the configured value.
