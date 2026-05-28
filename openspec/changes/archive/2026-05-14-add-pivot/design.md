## Context

`src/view/navigation/` already contains Breadcrumb, NavigationView, and a TabView implementation that has been split into `TabStrip`, `StackContentHost`, and `TabViewItem`. That split is useful context, but Pivot should not become a thin alias for TabView: TabView is a document/workspace tab control with close buttons, add-tab affordance, reordering, and width modes, while Pivot is a lightweight page-category switcher whose headers are owned by the Pivot itself.

WinUI Gallery's Pivot sample is intentionally small: an email section label sits above a Pivot whose items are multiple `<PivotItem Header="...">` children, and each item owns page content. The code-behind only initializes the page, which means the control's base behavior is selection and content switching rather than application routing. For this Qt Widgets library, the user-requested structure is `external Label -> Pivot -> header(Pivot-owned) -> content(QStackLayout-style page management)`.

## Goals / Non-Goals

**Goals:**

- Add `view::navigation::Pivot` as a Qt Widgets Fluent component using `QWidget + FluentElement + QMLPlus` inheritance.
- Implement a Pivot-owned header strip that renders item headers, selected indicator, overflow affordances, hover/pressed/disabled color states, and Light/Dark theme updates.
- Implement page/content management through a `QStackedLayout`-based host so pivot items can own QWidget pages and the selected page is shown below the header.
- Provide item APIs for header text, optional icon glyph, content widget, enabled state, user data, accessible name, and page ownership/take semantics.
- Support pointer selection, keyboard navigation, selected-index changes, current-page synchronization, overflow, accessibility text, tests, and VisualCheck.

**Non-Goals:**

- Do not implement TabView behaviors such as close buttons, add-tab button, drag reordering, title-bar placement, or document workspace width modes.
- Do not introduce a general XAML-style `PivotItem` template system or arbitrary QWidget-per-header delegates in v1.
- Do not implement routing, URL navigation, or history; applications can react to `currentChanged`/`selectedIndexChanged` if they need routing.
- Do not use QSS/QPalette for Pivot component visuals, except test window/background setup where existing test conventions allow it.
- Do not change existing Breadcrumb, NavigationView, TabView, TabStrip, or StackContentHost public behavior.

## Decisions

### 1. Build Pivot as a composite QWidget with a private header surface

`Pivot` will inherit `QWidget`, `FluentElement`, and `QMLPlus`. It owns a private header layout/paint surface rather than using `TabStrip` directly. The header records include pivot item headers, optional icon rects, text rects, selected indicator rects, overflow buttons, and hit-test rectangles. Labels such as `EMAIL` are intentionally outside the Pivot and can be anchored in tests or product UI with `AnchorLayout`.

Rationale: `TabStrip` carries document-tab features that are wrong for Pivot: add, close, reordering, compact inactive tab widths, title-bar placement, and close overlay modes. A private Pivot header keeps the public API and visual model focused on WinUI Pivot semantics while still allowing implementation reuse of small helper ideas from TabStrip.

Alternative considered: implement Pivot as a configured `TabView`. That would be faster, but it would leak TabView concepts into Pivot tests and APIs and would make the user-requested `Pivot` structure less explicit.

### 2. Use a small `PivotItem` record and stable index-based APIs

The public item record should include header text, optional icon glyph, content widget pointer, enabled state, user data, and accessible name. APIs should include `addItem`, `insertItem`, `removeItem`, `takePageWidget`, `setItemHeader`, `setItemEnabled`, `setItemIconGlyph`, `setItemData`, and `setItemAccessibleName`.

Selection is index-based because Pivot items are ordered pages and do not need TabView's document identity/reorder model. Invalid indexes fail safely without changing selection or page state.

### 3. Content host follows QStackedLayout page management

Pivot should use a `QStackedLayout`-based content host, reusing `StackContentHost` for page-host behavior and stack transition animation. Selection changes should animate only the incoming page with a fixed, short left-to-right slide-in transition while hiding the outgoing page immediately, so the user sees one consistent motion without two page contents overlapping in the same area or a large content panel sweeping across the view. Insert item inserts a page slot, remove item removes a page slot, move is not required in v1, and selected index updates the current stacked page while hiding every non-current page after the transition completes.

The page ownership policy should be explicit and testable: assigning a content widget reparents it into the host; removing an item hides and detaches the page; `takePageWidget(index)` returns the page to the caller without deleting it.

### 4. Header layout is horizontal, deterministic, and overflow-aware

Header layout arranges pivot item headers horizontally using Body typography. The selected header receives an accent indicator at the bottom edge. Headers size to content by default and should not render text ellipses; constrained text may clip only as a last resort while overflow controls keep additional headers reachable.

Geometry helpers should expose header row, item header, selected indicator, overflow, and content rectangles for tests. These helpers reduce reliance on screenshots while pinning the intended metrics.

### 5. Input follows Pivot page-switching expectations

Pointer activation on an enabled header changes selected index and current page, then emits `selectedIndexChanged`, `currentChanged`, and `itemActivated` once per effective selection. Keyboard navigation uses Left/Right/Home/End to move focus across enabled visible headers, Enter/Space to select, and optional Ctrl+Tab/Ctrl+Shift+Tab cycling if it does not conflict with application shortcuts.

Disabled items remain visible but cannot be selected by pointer, keyboard, or programmatic selection. When the selected item is removed or disabled, selection clamps to a neighboring enabled item or `-1` if none remain.

### 6. Visuals use Fluent tokens with Pivot-specific painting only

The header surface is custom-painted with `themeColors()` and token-backed fonts. Header font role and icon font family are member-backed configurable properties to avoid hard-coded token lookups at paint sites. Icons use Segoe Fluent Icons through existing typography helpers. Pivot item headers do not paint button borders, hover backgrounds, or focus rectangles; selected, hover, pressed, disabled, and theme states are differentiated through text/icon color plus the selected accent indicator without changing geometry.

The content area is a normal child region managed by the stacked host and should use the layer/canvas background token through `FluentElement` updates.

### 7. Tests and VisualCheck stay in navigation

Add `tests/views/navigation/TestPivot.cpp` and register `test_pivot` in `tests/views/navigation/CMakeLists.txt`. Automated tests should cover defaults, item/page APIs, content host synchronization, ownership/take behavior, selection clamping, geometry, overflow, pointer/keyboard interactions, disabled behavior, theme refresh, and accessibility. VisualCheck should show the WinUI Gallery email-style Pivot with an externally anchored `Label`, many-header overflow, disabled item, optional icon metadata coverage, page switching, and Light/Dark theme switching.

## Risks / Trade-offs

- [Risk] Pivot and TabView can drift visually if they duplicate too much header code. -> Mitigation: reuse small helper patterns and token choices, but keep public behavior separate.
- [Risk] Reusing `StackContentHost` could bring TabView-specific animation semantics into Pivot. -> Mitigation: use a fixed short left-to-right incoming-only slide transition and make the host hide non-current pages before and after stack changes.
- [Risk] Overflow behavior can be surprising on very narrow widths. -> Mitigation: keep selected header visible whenever possible and expose deterministic visible/hidden index helpers for tests.
- [Risk] Content widget ownership can surprise callers. -> Mitigation: document and test reparent/detach/take semantics explicitly.
- [Risk] Exact WinUI Pivot brush names may not map one-to-one to existing tokens. -> Mitigation: use existing semantic text, accent, divider, and disabled tokens and validate through VisualCheck.
