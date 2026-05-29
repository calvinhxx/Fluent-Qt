# Component API Consistency Audit

Date: 2026-05-26
Change: `audit-component-api-consistency`

## Scope

This audit covers public component headers under `src/components/**`, focused tests
under `tests/components/**`, and relevant OpenSpec specs under `openspec/specs/**`.
It records compatibility-safe fixes applied in this change and defers breaking
or broad migrations to follow-up proposals.

The reusable component API checklist lives in
[Component API Conventions](component-api-conventions.md). This document is the
historical audit report for the `audit-component-api-consistency` change.

The second pass covers 56 public component headers and 57 public component
classes. `src/components/menus_toolbars/Menu.h` contains two public classes,
`FluentMenu` and `FluentMenuItem`; `fluent::FluentElement`, `fluent::QMLPlus`, and private
headers remain supporting infrastructure rather than component inventory.

Core infrastructure such as `fluent::FluentElement`, `fluent::QMLPlus`, and design tokens is
treated as supporting API rather than a component category.

Namespace follow-up: `rename-component-namespace-to-fluent` deliberately moves
the reusable component API from `view::...` to `fluent::...` with no compatibility
aliases, typedefs, or forwarding namespaces. Future API audits should flag any
active `view::...` component spelling outside archived history or explicit
migration notes.

## Inventory Summary

| Category | Public components audited | Focused tests | Relevant specs found |
| --- | --- | --- | --- |
| `basicinput` | Button, CheckBox, RadioButton, Slider, ComboBox, ColorPicker, ToggleSwitch, ToggleButton, SplitButton, ToggleSplitButton, DropDownButton, HyperlinkButton, RepeatButton, RatingControl | Present for listed components | combobox-dropdown-flyout |
| `collections` | ListView, GridView, FlowView, TreeView, FlipView, SplitView, StackView, DrawerView | Present for listed components | flow-view, gridview-drag-reorder, listview-wheel-input, listview-indicator-motion, tree-view, flipview-wheel-input, split-view, stack-view, drawer-view |
| `date_time` | CalendarView, CalendarDatePicker, DatePicker, TimePicker | Present for listed components | calendar-date-picker, calendar-view-pager, date-picker, time-picker |
| `dialogs_flyouts` | Dialog, ContentDialog, Popup, Flyout, TeachingTip | Present for listed components | dialog-winui3-polish, popup-overlay, flyout, teaching-tip |
| `menus_toolbars` | FluentMenu, FluentMenuItem, MenuBar | MenuBar focused test present; menu classes are currently exercised through MenuBar and DropDownButton tests | menu-bar |
| `navigation` | Breadcrumb, NavigationView, Pivot, SelectorBar, StackContentHost, TabView | Present for listed public components except StackContentHost as standalone host | breadcrumb, navigation-view, pivot, selector-bar, tab-view |
| `scrolling` | ScrollBar, ScrollView, AnnotatedScrollBar, PipsPager | Present for listed components | scroll-view, annotated-scrollbar, pips-pager |
| `status_info` | ToolTip, InfoBar, InfoBadge, ProgressBar, ProgressRing | Present for listed components | tooltip-animation, info-bar, info-badge, progress-bar, progress-ring |
| `textfields` | Label, LineEdit, TextEdit, AutoSuggestBox, PasswordBox, NumberBox | Present for listed components | label, auto-suggest-box, password-box, number-box |
| `windowing` | Window, TitleBar | Window focused test present | fluent-window, window-platform-compatibility |

## Findings

| ID | Severity | Category | Component path | Rationale | Action |
| --- | --- | --- | --- | --- | --- |
| API-001 | Medium | Open state naming | `src/components/date_time/DatePicker.h`, `src/components/date_time/TimePicker.h`, `src/components/date_time/CalendarDatePicker.h` | Button-like picker entries exposed specific getters (`isDropDownOpen`, `isCalendarOpen`) while nearby `DropDownButton` uses `isOpen`. | Applied compatible `isOpen()` aliases and focused tests. Existing specific getters remain public. |
| API-002 | Medium | Repeated setter tests | `tests/components/date_time/TestDatePicker.cpp` | `DatePicker::setSelectedDate(...)` already suppresses duplicate signals, but the focused test did not assert the no-op behavior. | Added repeated selected-date and repeated clear assertions. |
| API-003 | Low | Nullable values | Date/time pickers | `DatePicker`, `TimePicker`, and `CalendarDatePicker` use invalid `QDate()` or `QTime()` as empty selected values. Existing tests cover defaults and clears, but the convention was not documented durably. | Documented the nullable value convention in [Component API Conventions](component-api-conventions.md). |
| API-004 | Medium | Open state breadth | `src/components/basicinput/SplitButton.h`, `src/components/basicinput/ToggleSplitButton.h` | Split buttons own a menu and have primary/secondary hit zones, but they do not expose an open-state property. Adding one may require QMenu lifecycle semantics and is broader than this audit. | Deferred to follow-up recommendation `standardize-splitbutton-open-state` if callers need observable menu state. |
| API-005 | Medium | Popup/flyout state naming | `src/components/dialogs_flyouts/Popup.h`, `src/components/dialogs_flyouts/Flyout.h`, `src/components/dialogs_flyouts/ContentDialog.h`, `src/components/dialogs_flyouts/TeachingTip.h` | Overlay components mix Qt visibility, popup open state, light-dismiss, modal, and hosted-content semantics. A cosmetic rename could hide real behavioral differences. | Deferred to follow-up recommendation `standardize-overlay-open-state-semantics`. |
| API-006 | Low | Collection selection naming | `src/components/collections/ListView.h`, `src/components/collections/TreeView.h`, `src/components/collections/GridView.h`, `src/components/collections/FlowView.h` | Collection views intentionally differ: item-view based components use Qt model/delegate contracts, while ListView/TreeView expose component-specific enum names. | Marked intentional; document selection/current/item ownership expectations rather than force a rename. |
| API-007 | Low | Caller-owned composition | `src/components/navigation/NavigationView.h`, `src/components/navigation/StackContentHost.h`, `src/components/navigation/TabView.h` | Navigation components act as shells/hosts and should not absorb application page choice or content ownership. | Marked intentional; keep caller-owned composition boundaries. |
| API-008 | Low | Header documentation | Broad `src/components/**` | Some public properties lack explanatory header comments, especially where names are inherited from WinUI concepts. | Documented project-level checklist now; individual comments can be added when touching the owning component. |
| API-009 | Medium | Boolean getter aliases | `src/components/collections/ListView.h`, `src/components/collections/GridView.h`, `src/components/collections/FlowView.h`, `src/components/collections/TreeView.h`, `src/components/collections/FlipView.h`, `src/components/navigation/TabView.h`, `src/components/status_info/ProgressRing.h` | Several public bool getters used noun-style names such as `borderVisible()`, `backgroundVisible()`, `showNavigationButtons()`, or `addTabButtonVisible()` while nearby components already use `is*`, `are*`, or `has*` for state queries. | Applied compatible alias getters and focused tests. Existing getters remain public. |
| API-010 | Low | Open setter alias | `src/components/basicinput/DropDownButton.h` | `DropDownButton` exposes `isOpen()` but only had `setOpen(bool)`, while other open-state components expose `setIsOpen(bool)`. | Applied compatible `setIsOpen(bool)` alias and focused test. Existing `setOpen(bool)` remains public. |
| API-011 | Medium | Popup property notify gaps | `src/components/dialogs_flyouts/Popup.h`, `src/components/dialogs_flyouts/TeachingTip.h`, `src/components/dialogs_flyouts/ContentDialog.h` | Some overlay properties do not expose NOTIFY signals, but adding these signals should be paired with overlay-state semantics and binding tests rather than rushed into an API audit sweep. | Deferred to follow-up recommendation `standardize-overlay-open-state-semantics`. |

## Intentional Deviations

- `ListView`, `GridView`, `FlowView`, and `TreeView` keep separate selection/reorder APIs because their Qt bases and model/delegate responsibilities differ.
- `NavigationView` and `StackContentHost` keep caller-owned page/content composition to preserve shell boundaries.
- `Popup`, `Flyout`, `ContentDialog`, and `TeachingTip` keep specialized open and dismissal semantics until a dedicated overlay-state migration specifies common terms.
- `CalendarDatePicker`, `DatePicker`, and `TimePicker` keep specific legacy getters while adding the common `isOpen()` alias for compatibility.
- Existing noun-style boolean getters remain public for source compatibility while clearer aliases are added for new code.

## Applied Fixes

- Added `isOpen()` compatibility aliases to `DatePicker`, `TimePicker`, and `CalendarDatePicker`.
- Added focused tests proving the aliases track the existing open-state getters.
- Added focused `DatePicker` tests for repeated `setSelectedDate(...)` and repeated `clearSelectedDate()` no-op signal behavior.
- Added compatible boolean getter aliases across audited visible/enabled state APIs:
  - `ListView`: `isBorderVisible()`, `isBackgroundVisible()`, `isViewportHovered()`, `isSectionEnabled()`, `isSelectedIndicatorAnimationEnabled()`.
  - `GridView`: `isBorderVisible()`, `isViewportHovered()`.
  - `FlowView`: `isBorderVisible()`, `isViewportHovered()`.
  - `TreeView`: `isBorderVisible()`, `isBackgroundVisible()`, `isViewportHovered()`.
  - `FlipView`: `areNavigationButtonsVisible()`, `isPageIndicatorVisible()`.
  - `TabView`: `areTabsClosable()`, `isAddTabButtonVisible()`, `isTabReorderEnabled()`, `areKeyboardAcceleratorsEnabled()`.
  - `ProgressRing`: `isBackgroundVisible()`.
- Added `DropDownButton::setIsOpen(bool)` as a compatible alias for `setOpen(bool)`.
- Added focused tests for the new aliases in DropDownButton, ListView, GridView, FlowView, TreeView, FlipView, TabView, and ProgressRing.
- Published the durable checklist as [Component API Conventions](component-api-conventions.md) so future work can use it without depending on an agent skill path.

## Deferred Follow-Ups

- `standardize-splitbutton-open-state`: decide whether `SplitButton` and `ToggleSplitButton` should expose observable menu open state.
- `standardize-overlay-open-state-semantics`: align Popup/Flyout/Dialog/ContentDialog/TeachingTip naming only after light-dismiss, modal, and visibility semantics are specified.
- `add-overlay-property-notify-signals`: add missing NOTIFY signals for Popup/TeachingTip/Dialog properties after overlay-state semantics are settled.
- `add-menu-focused-tests`: add direct tests for `FluentMenu` and `FluentMenuItem` font style properties rather than relying only on MenuBar/DropDownButton coverage.
- `add-component-api-static-checks`: consider a later static or meta-object based checker after the checklist stabilizes.
- `document-public-property-comments`: add targeted header comments for ambiguous public properties as components are touched.

## Validation Notes

- Date/time picker code changes were validated with focused builds and direct test binaries.
- Alias sweep code changes were validated with focused builds and CTest label filters for `test_dropdown_button`, `test_list_view`, `test_grid_view`, `test_flow_view`, `test_tree_view`, `test_flip_view`, `test_tab_view`, and `test_progress_ring`: 289 tests passed, 0 failed, 8 VisualCheck tests skipped through `SKIP_VISUAL_TEST`.
- Broad category audits that produced report-only findings did not need automated test changes because no production behavior was modified.
