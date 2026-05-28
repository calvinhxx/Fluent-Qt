## Context

`src/view/navigation/` exists but is currently empty, while the project already has Fluent self-painted controls in other categories. Breadcrumb is a natural first navigation component because it is compact, does not require a full shell/navigation model, and appears in both the Windows UI Kit and WinUI Gallery as a reusable path navigation pattern.

Figma node `29604:363` documents Breadcrumb variants in the Windows UI Kit. The standard size is a 20px-tall inline path using 14px Segoe UI Variable body text, 16px separator slots, and Segoe Fluent Icons chevron glyph `E974`. The large custom size is a 40px-tall title-path variant using 28px semibold display text, 24px separator/overflow slots, secondary text color for ancestor items, and primary text color for the active page. Overflow variants use ellipsis glyph `E712` and include `Overflow=None`, `Overflow=Beginning`, and `Overflow=Middle`.

WinUI Gallery's `BreadcrumbBarPage` shows two behavior patterns. A string array can be assigned directly as `ItemsSource`, and a custom item type can be rendered with a template. Its interactive sample handles `ItemClicked` by removing all items after the clicked index, so selecting an ancestor truncates the path to that ancestor.

This change adapts those references to Qt Widgets without adding a templating framework. The v1 component should be self-painted, expose path item data through C++ APIs, and provide item activation signals so applications can update their model or let the control truncate the trailing path.

## Goals / Non-Goals

**Goals:**

- Add `view::navigation::Breadcrumb` as a QWidget-based Fluent component using `QWidget + FluentElement + view::QMLPlus`.
- Support standard and large sizes matching the Figma metrics and typography.
- Provide path item APIs for strings and richer item records with text, user data, enabled state, and accessible name.
- Support item activation, optional automatic trailing truncation, and signals containing the clicked index and item.
- Implement no-overflow, beginning-overflow, and middle-overflow display modes with an ellipsis item for hidden crumbs.
- Paint text, chevrons, ellipsis, hover, pressed, focused, disabled, Light, and Dark states using design tokens and Segoe Fluent Icons.
- Provide keyboard navigation, focus rectangles, accessible names/descriptions, automated tests, and VisualCheck coverage.

**Non-Goals:**

- Do not implement a general WinUI `DataTemplate` or arbitrary QWidget-per-item template system in v1.
- Do not implement a full navigation shell, route resolver, history stack, or page container.
- Do not require `QSS`, `QPalette`, or third-party dependencies for visual styling.
- Do not implement animated overflow menus in v1; the ellipsis can expose hidden items through a simple signal and/or existing menu/flyout composition later.
- Do not change existing components outside test category registration.

## Decisions

### 1. Implement as one self-painted QWidget

`Breadcrumb` will inherit `QWidget`, `FluentElement`, and `view::QMLPlus`. Items, separators, and overflow affordances are represented by internal layout records and painted in `paintEvent()`; pointer and keyboard interaction use hit rectangles computed during layout.

Rationale: Breadcrumb is a compact path control with many small visual states. A self-painted widget keeps Figma metrics stable, avoids nested child buttons for every crumb, and matches this repository's component style. Composing `Button` children was considered, but it would make precise text baselines, separator slots, overflow measurement, and focus traversal harder to keep consistent.

### 2. Use a small item model instead of templates

The public API should support common path use cases without introducing a model/view dependency:

- `setItems(const QStringList&)`
- `setItems(const QVector<BreadcrumbItem>&)`
- `items()`, `itemCount()`, `itemAt(int)`
- `appendItem`, `insertItem`, `removeItemAt`, `clearItems`
- per-item enabled, text, data, and accessible name fields

Signals should include `itemClicked(int index)`, `itemActivated(int index, const BreadcrumbItem& item)`, and `itemsChanged()`. The component should expose `autoTruncateOnItemClick`, defaulting to `false` or `true` based on final API review; the spec requires the behavior to be available and testable. Keeping truncation optional lets applications use Breadcrumb as a pure signal source or as a WinUI Gallery-style path mutator.

### 3. Layout is measured from text and fixed icon slots

For standard size, text crumbs use a 20px height, Body typography, 4px corner radius, 16px separator slots, and 12px chevron/ellipsis glyphs where appropriate. For large size, text crumbs use a 36px text area inside a 40px row, Title typography, 5px text corner radius, 24px separator/overflow slots, and 16px chevrons or 24px ellipsis glyphs following Figma.

Layout computes a display sequence: visible text crumbs, separator records, and optional ellipsis record. The active/current crumb is the final visible item and uses primary text; ancestors use secondary text. Disabled items and disabled controls use disabled semantic colors. Text is elided inside its available crumb rect when width is constrained.

### 4. Overflow is deterministic and width-driven

`BreadcrumbOverflowMode` should include `None`, `Beginning`, and `Middle`.

- `None`: measure all items and elide text if necessary.
- `Beginning`: preserve the trailing path and replace one or more leading hidden ancestors with an ellipsis item after the first visible edge needed by the final layout.
- `Middle`: preserve the first and trailing active path, replacing middle hidden items with an ellipsis item.

The ellipsis item uses Segoe Fluent Icons glyph `E712`. Activating it emits `overflowActivated(hiddenIndexes)` and, if a simple popup/menu is implemented in v1, shows hidden item choices using existing menu/flyout primitives. Tests should validate the hidden-index calculation even if visual popup behavior remains minimal.

### 5. Input and focus follow path navigation expectations

Pointer hover/press states are tracked per visible interactive record. Clicking an enabled text crumb emits activation signals and may truncate trailing items. Clicking separators does not activate navigation. Clicking ellipsis emits overflow activation. Keyboard support should move focus across visible interactive crumbs with Left/Right, activate with Enter/Space, move to first/last with Home/End, and skip disabled items.

### 6. Tests and VisualCheck live in a new navigation category

Add `tests/views/navigation/` with `TestBreadcrumb.cpp` and `CMakeLists.txt`, then register it from `tests/views/CMakeLists.txt`. Automated tests cover public API, layout/overflow calculations, input, truncation, disabled behavior, theme safety, and accessibility. VisualCheck should show standard and large Breadcrumbs, Light/Dark, no/beginning/middle overflow, disabled items, and a WinUI Gallery-style folder path that truncates when ancestors are selected.

## Risks / Trade-offs

- [Risk] Exact WinUI brush names do not map one-to-one to project tokens. -> Mitigation: use existing `themeColors()` semantic text/subtle/focus tokens and validate metrics/states through VisualCheck.
- [Risk] Overflow behavior can be surprising when width is extremely small. -> Mitigation: keep deterministic minimum display rules, always preserve the active crumb when possible, and expose hidden-index helpers for tests.
- [Risk] Optional auto-truncation could conflict with applications that own path state. -> Mitigation: make truncation configurable and always emit activation signals so callers can choose model-driven updates.
- [Risk] A future templated breadcrumb may need richer item rendering. -> Mitigation: keep v1 item data stable and avoid exposing internal paint records as API.
