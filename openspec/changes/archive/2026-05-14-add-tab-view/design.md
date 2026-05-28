## Context

`src/view/navigation/` already contains Breadcrumb, and the project follows the same component pattern across the library: widgets inherit a Qt base class, `FluentElement`, and `view::QMLPlus`, then render Fluent states through `paintEvent()` instead of QSS/QPalette. The root library CMake uses recursive source discovery, while navigation tests are registered explicitly in `tests/views/navigation/CMakeLists.txt`.

The Windows UI Kit Figma TabView node shows these implementation anchors: single tabs are 160x32 in the reference matrix, the tab row is 40px tall, tabs have Light/Dark variants, title-bar and content-area placements, active/inactive states, focus, hover, pressed, and close hover/pressed states. The row also has overflow and no-overflow variants, plus referenced controls for the new-tab button, generic button affordances, title bar, and caption controls.

WinUI Gallery's TabView sample demonstrates the behavior surface to adapt: tabs can be added and closed, content is typically a page/frame-like widget, items can also be data-backed, `TabWidthMode` supports `Equal`, `SizeToContent`, and `Compact`, `CloseButtonOverlayMode` supports `Auto`, `OnPointerOver`, and `Always`, keyboard accelerators support new tab, close selected tab, and Ctrl+number tab selection, and the advanced sample supports drag/drop and tear-out windowing. This change implements the core TabView control for Qt Widgets and leaves cross-window tear-out orchestration out of v1.

## Goals / Non-Goals

**Goals:**

- Add `view::navigation::TabView` as a QWidget-based Fluent component using `QWidget + FluentElement + view::QMLPlus`.
- Provide a C++ tab item API for header text, optional icon glyph, optional content widget, closeability, enabled state, and user data.
- Render a Fluent tab row with title-bar and content-area placements, active/inactive tabs, close buttons, add button, overflow controls, focus visuals, and Light/Dark theme support.
- Support selection, add requests, close requests/removal, keyboard navigation, Ctrl+T/Ctrl+W/Ctrl+1..9 style accelerators, and optional in-row drag reordering.
- Support WinUI-inspired `Equal`, `SizeToContent`, and `Compact` width modes and `Auto`, `OnHover`, and `Always` close button overlay modes.
- Host the selected tab's content widget below the tab row when content-area placement is used, while allowing title-bar placement to expose the tab row for window integration.
- Add automated tests and VisualCheck coverage for API behavior, layout metrics, state rendering, overflow, keyboard behavior, reordering, and theme switching.

**Non-Goals:**

- Do not implement WinUI TabView tear-out into new native windows in v1.
- Do not introduce a generic data-template, delegate, or model/view framework for tab headers.
- Do not use `QTabWidget`, QSS, or QPalette to emulate the control visuals.
- Do not implement title-bar caption buttons or platform window chrome changes as part of TabView.
- Do not change existing Breadcrumb behavior or any existing component public API.

## Decisions

### 1. Implement TabView as a self-painted container with an internal content host

`TabView` will inherit `QWidget`, `FluentElement`, and `view::QMLPlus`. The tab strip, buttons, focus rectangles, drag indicators, and overflow affordances will be painted by `TabView::paintEvent()` from cached layout records. Selected tab content will be hosted in an internal child `QWidget` area; non-selected content widgets are hidden and reparented to the host when managed by the control.

Rationale: the tab row has tightly coupled visual states, clipped layout, overflow, and drag behavior. Self-painting the strip keeps Figma metrics stable and matches the rest of the project, while an internal host keeps the selected content ergonomic for Qt applications. Using `QTabWidget` was considered, but it would make WinUI visual states and exact add/close/overflow affordances difficult to control.

### 2. Use a small owned item record instead of child widgets per tab header

The public API should expose a `TabViewItem` record with text, icon glyph, content widget pointer, closable/enabled flags, user data, and accessible name. The control provides convenience methods such as `addTab`, `insertTab`, `removeTab`, `clearTabs`, `tabAt`, `setTabText`, `setTabIconGlyph`, `setTabClosable`, and `setTabContentWidget`.

Content ownership should be explicit and Qt-friendly: adding a content widget reparents it into the TabView content host; removing a tab hides its content and either deletes it with the item or returns it through a `takeTabContentWidget()` style API if preservation is needed during implementation. Tests should pin down the final ownership semantics before broad use.

This avoids a templating system while still supporting the common WinUI Gallery pattern where each tab owns a page-like content widget.

### 3. Mirror the important WinUI modes as strongly typed properties

`TabView` should expose `Q_PROPERTY` values for `selectedIndex`, `tabCount`, `tabWidthMode`, `closeButtonOverlayMode`, `tabPlacement`, `tabsClosable`, `addTabButtonVisible`, `tabReorderEnabled`, and configurable font/icon roles. Font-role strings such as Body or icon roles must be stored in properties/members rather than hard-coded at token lookup sites.

The enums map to the reference behavior:

- `TabWidthMode::Equal`: visible tabs share available row width up to min/max constraints.
- `TabWidthMode::SizeToContent`: each tab measures text/icon/close content, clamped by min/max widths.
- `TabWidthMode::Compact`: inactive tabs compress toward icon-only/minimum width while the selected tab can show its label.
- `CloseButtonOverlayMode::Auto`: close appears for selected/hovered closeable tabs.
- `CloseButtonOverlayMode::OnHover`: close appears only while the tab is hovered or focused.
- `CloseButtonOverlayMode::Always`: close is reserved and visible for every closeable tab.
- `TabPlacement::InTitleBar` and `TabPlacement::InContentArea`: use the same interaction model but different row background/stroke treatments.

### 4. Keep layout deterministic and queryable for tests

Layout should produce stable records for tabs, close buttons, add button, overflow buttons, and content area. The row height is 40px, tab body height is 32px, and the new-tab/button affordances are 40x32 in the reference. Single tab natural width starts from icon + label + close button content and is clamped between minimum, preferred, and maximum widths. Overflow is triggered when the measured row exceeds available width after reserving add/overflow controls.

The component should expose test helpers for tab geometry, close button geometry, add button geometry, overflow geometry, visible tab indexes, and content area geometry. These helpers keep automated tests independent of pixel screenshots while still validating the reference metrics.

### 5. Use scroll-window overflow before popup/menu overflow

When tabs exceed row capacity, v1 should keep a horizontal visible window over the tab list with previous/next overflow buttons and ensure the selected tab is brought into view. This matches a tab-strip mental model and avoids inventing a separate overflow menu before the base control is stable.

Alternative considered: move hidden tabs into a flyout/menu. That can be added later, but it adds menu ownership and item activation complexity not required by the Figma row variants or the basic WinUI Gallery sample.

### 6. Input and signals follow Qt control expectations

Pointer input selects enabled tabs, presses close/add/overflow affordances, and starts reordering when `tabReorderEnabled` is true and the drag threshold is exceeded. Close interaction emits `tabCloseRequested(index)` before mutating so applications can intercept; the default convenience path can remove closeable tabs. Add interaction emits `addTabRequested()` and leaves creation to the application. Selection emits `selectedIndexChanged` and `currentChanged` once per effective change.

Keyboard support should include Left/Right/Home/End movement across visible enabled tabs, Enter/Space activation of focused tab/add/close controls as appropriate, Delete or Ctrl+W for close selected when closable, Ctrl+T for add request, and Ctrl+1..Ctrl+9 for direct tab selection where 9 selects the last tab.

### 7. Visual rendering uses existing tokens and icon font glyphs

Painting should use project semantic colors for layer/background, control fill/stroke, text primary/secondary/disabled, subtle hover/pressed fills, divider strokes, and focus strokes. Icons use Segoe Fluent Icons through existing typography/icon helpers. Close uses the cancel/dismiss glyph, add uses the plus glyph, and overflow buttons use chevron glyphs. Exact glyph constants should come from `Typography::Icons` when available, with missing constants added to the project token headers only if needed by implementation.

### 8. Test and VisualCheck scope is broad enough for a shared control

Add `tests/views/navigation/TestTabView.cpp` and register `test_tab_view`. Automated tests should cover default state, item mutation, selected-index clamping, content visibility, property signals, width modes, close overlay modes, overflow calculations, add/close request signals, keyboard accelerators, drag reorder model updates, disabled tabs, theme refresh, and geometry helpers. VisualCheck should show title-bar/content-area placements, Light/Dark examples, selected/unselected tabs, close modes, compact/size-to-content/equal widths, overflow, disabled tabs, and a WinUI Gallery-inspired document workspace.

## Risks / Trade-offs

- [Risk] Exact WinUI brushes and platform title-bar integration do not map one-to-one to this project. -> Mitigation: use existing semantic design tokens and keep title-bar placement as a visual/layout mode, not a window chrome feature.
- [Risk] Content widget ownership can surprise callers. -> Mitigation: document and test ownership semantics, and provide a safe take/remove path for callers that need to preserve content widgets.
- [Risk] Width modes and overflow can interact poorly at very small widths. -> Mitigation: define deterministic minimum widths, reserve affordance geometry first, and expose visible-index geometry helpers for tests.
- [Risk] Drag reordering can conflict with close/add hit targets. -> Mitigation: start drag only from tab body records after movement exceeds `QApplication::startDragDistance()` and never from close/add/overflow button rectangles.
- [Risk] Built-in keyboard accelerators may conflict with application shortcuts. -> Mitigation: gate accelerator handling behind a configurable property while preserving basic arrow/Home/End focus navigation.