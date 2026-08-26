# Accessibility Inventory

This is the review view of the machine-checked
[accessibility-inventory.json](accessibility-inventory.json). The inventory is
keyed by the canonical Gallery component catalog, so every public visible
component has one classification before release.

## Current baseline

| Classification | Components | Meaning |
| --- | ---: | --- |
| Native | 22 | The Qt base class supplies the appropriate role, state, actions, and logical children. |
| Augmented | 9 | Native semantics are retained while FluentQt manages additional text or events. |
| Adapter | 35 | A private accessible interface represents custom-drawn or composite semantics. |
| Gap | 0 | No known inventory contract is open. |
| Not applicable | 4 | The component is presentation-only; semantics belong to its containing control. |

All inventoried gaps are closed. `covered` means
there is deterministic repository evidence for the component boundary; it is
not a claim of platform assistive-technology certification.

## First focused gate: CalendarView

`CalendarView` moved from **Gap** to **Adapter** in the first 1.7-Q slice. Its
private adapter now exposes:

- a table role with 6 × 7 day cells or 4 × 3 month/year cells;
- previous-page, level-title, and next-page actions;
- locale-aware cell names, weekday descriptions, range-disabled state,
  selection, and logical focus;
- press actions that reuse the existing date/month/year behavior; and
- selection, value, focus, and table-reset events with no-op silence.

The adapter does not add installed headers or public methods. Contracts live in
`tests/components/date_time/TestCalendarView.cpp`.

## Second focused gate: logical navigation items

Breadcrumb, Pivot, SelectorBar, TabView, and PipsPager moved from **Gap** to
**Adapter** through one shared private logical-item base. The family now
exposes ordered names, disabled/hidden/off-screen state, logical focus,
selection, and press actions without allocating one widget per item.

TabView additionally exposes add, close, and reorder actions. PipsPager keeps
all pages in the semantic tree even when only a smaller pip window is painted.
Five focused contracts cover roles, names, state, actions, selection interfaces,
effective-change events, and no-op silence in
`tests/components/navigation/TestNavigationAccessibility.cpp`.

## Third focused gate: composite values

ToggleSwitch, RatingControl, NumberBox, ProgressBar, and ProgressRing moved from
**Gap** to **Adapter** through a shared private value boundary. The family now
exposes:

- checkable state and the standard toggle action for ToggleSwitch;
- bounded half-step rating values and increase/decrease actions;
- NumberBox spin-box values without losing editable text, selection, invalid
  state, or unbounded-range semantics; and
- read-only determinate progress values plus busy/animated state only while an
  indeterminate ProgressBar or ProgressRing is observably running.

Five focused contracts cover roles, names/descriptions, bounds, state, actions,
text-interface retention, and effective-change event silence in
`tests/components/TestValueAccessibility.cpp`.

## Fourth focused gate: split actions

SplitButton and ToggleSplitButton moved from **Gap** to **Adapter** through one
private split-button interface. Each control is represented as a `ButtonMenu`
with a primary `press` or `toggle` action and a secondary `showMenu` action,
without allocating accessible child widgets for the painted segments.

Menu presence and the public QMenu `isOpen` state drive has-popup,
expandable, collapsed, and expanded state. Space retains the primary action;
Alt+Down and F4 open the menu through the pointer placement path. Four focused
contracts cover caller-owned text, disabled/no-menu state, checked state,
action separation, keyboard access, menu replacement/destruction, and
changed-state-specific no-op silence in
`tests/components/basicinput/TestSplitButtonAccessibility.cpp`.

## Fifth focused gate: transient surfaces

Popup, Flyout, TeachingTip, and CoachMark moved from **Gap** to **Adapter**
through one private transient-surface interface. Popup and Flyout expose pane
semantics; TeachingTip and CoachMark expose help-balloon semantics while
retaining the real child-widget tree supplied by the caller.

Logical open state drives active/invisible state and dismiss actions without
using fade progress as application state. Modal follows `Popup::isModal()`;
dim remains presentation-only. Flyout reports its anchor relationship, help
surfaces report the target they describe, and caller names/descriptions remain
authoritative. Popup-derived surfaces restore eligible invoker focus, while
CoachMark retains focus and announces a context-help alert.

Five focused contracts cover roles, child semantics, target relations,
open/modal state, Escape/dismiss reasons, focus return, announcements, and
effective-change silence in
`tests/components/dialogs_flyouts/TestTransientAccessibility.cpp`. The Gallery
intro tour additionally supplies step text, focusable actions, a trapped Tab
order, and Escape cleanup.

## Sixth focused gate: complex custom inputs

ColorPicker, DatePicker, TimePicker, AnnotatedScrollBar, and AutoSuggestBox
moved from **Gap** to **Adapter** without changing their public value or
ownership APIs.

- ColorPicker exposes one color chooser, an adjustable hue slider, a keyboard
  operable saturation/brightness surface, a semantic preview, and named native
  channel controls.
- DatePicker and TimePicker expose button-menu roots whose committed values are
  separate from bounded pending-value columns in the flyout.
- AnnotatedScrollBar exposes its bounded value plus one logical jump link for
  every authored annotation, including collision-filtered labels.
- AutoSuggestBox retains editable-text interfaces while adding autocomplete,
  popup state, a controller relation to its real ListView, and active-descendant
  changes without moving keyboard focus away from the editor.

Four focused contracts cover roles, values, bounds, actions, relations,
caller-owned text, keyboard paths, effective-change events, and no-op silence
in `tests/components/TestComplexInputAccessibility.cpp`.

## Seventh focused gate: menu buttons and auxiliary surfaces

DropDownButton, DrawerView, and ToolTip moved from **Gap** to **Adapter** while
keeping their existing QMenu, same-window drawer, and top-level tooltip models.

- DropDownButton now shares the private menu-button adapter with SplitButton,
  exposes one menu action and real Space/Enter/Alt+Down/F4 paths, and reports
  QMenu availability and open state without fake child controls.
- DrawerView exposes pane, expanded, modal, and dismiss state, retains caller
  content as native semantic children, and restores eligible invoker focus.
- ToolTip exposes tooltip text, its described-target relation, and logical
  show/hide state independently of opacity animation frames.

Three focused contracts cover role, caller text, state, actions, relations,
keyboard behavior, focus return, lifecycle events, and no-op silence in
`tests/components/TestAuxiliarySurfaceAccessibility.cpp`.

## Eighth focused gate: collection paging and splitters

FlipView and SplitView moved from **Gap** to **Adapter** without changing their
public APIs or replacing caller-owned content.

- FlipView now exposes a layered-pane root, ordered authored pages, a bounded
  current-page value, current-page text, and orientation-aware previous/next
  actions while filtering its paint-only overlay from the semantic tree.
- SplitView now exposes a splitter root and preserves each pane's native
  subtree. Every visible boundary has a focusable grip with effective value
  bounds, 8 px Arrow movement, 1 px Shift+Arrow movement, and Home/End bounds.
- Four focused contracts cover roles, child preservation, values, actions,
  focus, keyboard resizing, structure changes, effective-change events, and
  no-op silence in
  `tests/components/TestCollectionSurfaceAccessibility.cpp`.

## Ninth focused gate: semantic presentation

HyperlinkButton, InfoBar, and Shimmer moved from **Gap** to **Adapter** without
adding public properties or changing application-owned content.

- HyperlinkButton exposes a link role, target, standard press action, linked
  and traversed state, and resets traversed state when the target changes.
- InfoBar exposes one notification with combined title/message text, severity
  description, polite or assertive update announcements, hosted actions, and
  a dismiss action. Its default icon-only close button is named without
  overriding an application-supplied name.
- Shimmer exposes loading and busy state independently of motion. Disabling
  animation preserves busy state, deactivation removes the semantic skeleton,
  and progress frames emit no accessibility events.
- Five focused contracts cover caller text ownership, link activation,
  visited state, notification updates, dismissal, child structure, busy state,
  event specificity, and no-op silence in
  `tests/components/TestSemanticPresentationAccessibility.cpp`.

## Tenth focused gate: multi-selection dropdown

MultiSelectComboBox enters the inventory as an **Adapter** because its one
focusable field controls a private same-window popup and several selected
model rows. The adapter exposes a button-menu root, selected labels as the
accessible value, expanded state, a show-menu action, and a controller
relation to the real popup ListView. Focused contracts cover its role, value,
action, relation, and open-state behavior in
`tests/components/basicinput/TestMultiSelectComboBox.cpp`.

## Risk-ordered queue

No accessibility inventory gap remains queued for 1.7-Q. New or changed
visible components still need a classification and focused contract before
release.

## Validation

```bash
python3 tools/quality/validate_accessibility_inventory.py --project-root .
ctest --preset vcpkg-osx -R 'AccessibilityInventory|CalendarViewTest\.Contract_Accessibility|NavigationAccessibilityTest\.Contract_Accessibility|ValueAccessibilityTest\.Contract_Accessibility|SplitButtonAccessibilityTest\.Contract_Accessibility|TransientAccessibilityTest\.Contract_Accessibility|ComplexInputAccessibilityTest\.Contract_Accessibility|AuxiliarySurfaceAccessibilityTest\.Contract_Accessibility|CollectionSurfaceAccessibilityTest\.Contract_Accessibility|SemanticPresentationAccessibilityTest\.Contract_Accessibility' --output-on-failure
```

When Python 3.10+ is available, CTest registers
`AccessibilityInventory.Contract_Complete` in the `ci_fast`, `ci_full`,
`contract`, and `local_full` labels. The validator rejects missing, duplicate,
or unknown component IDs, invalid classifications, missing evidence paths, and
open gaps without a next gate.
