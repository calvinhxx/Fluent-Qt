# MultiSelectComboBox API Contract

> **Status:** Accepted contract, shipped in FluentQt 1.7.2

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Accepted component contracts

[← DataGrid API Contract](datagrid-api-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Editing Command Router API Contract →](editing-command-router-proposal.md)
<!-- docs-nav:top:end -->

Issue: [#37 — 新增多选下拉框](https://github.com/calvinhxx/Fluent-Qt/issues/37)

`fluent::basicinput::MultiSelectComboBox` is a separate model-backed
multi-selection component. The existing `ComboBox` remains single-select and
source-compatible.

This separation avoids making `currentIndex`, editable free-form text, and
multiple selected rows compete as one value contract. The implementation still
reuses FluentQt's same-window overlays, item views, inputs, tokens, and
model/view infrastructure.

## Public boundary

Exact declarations live in the installed
[`MultiSelectComboBox.h`](../../src/components/basicinput/MultiSelectComboBox.h)
header.

| Area | Public contract |
|---|---|
| Base | `QWidget + FluentElement + QMLPlus`; no misleading `QComboBox` or command-button value API |
| Data | Caller-owned `QAbstractItemModel`, display column, and root index |
| Value | `QItemSelectionModel`, selected rows/indexes, selected count, and row query |
| Presentation | Placeholder, optional local search, select-all visibility, and maximum visible rows |
| Lifecycle | Logical `isOpen`, open/close operations, and effective-change signals |
| Mutation | Replace selected rows, clear selection, or select all source rows under the active root |

The component does not maintain an internal string collection and does not
offer `addItem()`, `addItems()`, free-form values, or persistent item widgets.
Simple applications can supply `QStringListModel`; richer applications retain
their existing model.

## Model and ownership

- The application owns the source model supplied to `setModel()`.
- The component creates and owns a default selection model for the active
  source model.
- A caller-supplied selection model remains caller-owned and must reference the
  same source model. A mismatch is rejected without changing state.
- `setSelectionModel(nullptr)` restores a component-owned selection model.
- Replacing or destroying the source model detaches an external selection
  model, installs an empty internal selection model, closes the popup, and
  emits only effective changes.
- Selection identity is row-based under `rootModelIndex()`; labels come from
  `modelColumn()`. Duplicate labels therefore remain distinct.
- Disabled or non-selectable rows are never toggled by pointer, keyboard,
  programmatic replacement, or select-all.
- Model insertion, removal, movement, reset, and layout changes cannot leave
  stale indexes or an incorrect selected count.

A private proxy may implement search, but public indexes and selection always
belong to the caller's source model.

## Selection and search

Selection commits immediately. Closing with Escape does not roll it back.

| Action | Result |
|---|---|
| Click, Space, or Enter on an option | Toggle the option and keep the popup open |
| Click outside or press Escape | Close and retain selection |
| `clearSelection()` | Clear selected rows under the active root |
| `selectAll()` | Select every enabled/selectable source row under the active root |
| Built-in select-all | Toggle enabled/selectable rows in the filtered result |
| Change the filter | Keep hidden selections and recompute visible select-all state |

The select-all row is a separate tri-state control, not synthetic model row
zero. It is unchecked, partial, checked, or disabled according to the filtered
selectable rows.

Search is an optional case-insensitive substring filter over the display column:

- it is disabled by default;
- its query is transient and clears when the popup closes;
- hidden selections remain selected and contribute to `selectedCount`;
- it never creates values or mutates the source model; and
- remote search, fuzzy ranking, and custom callbacks remain application-owned
  through a supplied model.

## Closed-field presentation

- No selection shows `placeholderText`.
- Labels that fit are shown in source-row order with the locale's compact list
  separator.
- When labels do not fit, the control uses a translatable selected-count summary
  and elides only as a last resort.
- The field remains one standard control row high; tags, chips, and wrapping
  are outside this contract.
- Painted text is presentation only. Model indexes remain the value API.
- Text, checkbox, search, and chevron layout mirror correctly in RTL.

## Popup and keyboard behavior

The dropdown is a non-modal, non-dimming, same-window surface with outside-press
and Escape close behavior.

- `isOpen` is the logical requested state, not animation progress.
- Repeated open or close requests are no-ops.
- Moving the owner to another top-level recreates or reparents the popup safely.
- Closing restores focus to the trigger unless focus deliberately moved.
- Opening with search enabled focuses the search editor; otherwise it focuses
  the first selected or selectable option.

| Context | Keys |
|---|---|
| Closed field | Space, Enter, Alt+Down, or F4 opens |
| Search editor | Text/IME filters; Down enters results; Escape closes; Ctrl/Cmd+A selects query text |
| Option list | Up/Down/Home/End moves current focus without changing selection |
| Option list | Space or Enter toggles the focused option |
| Option list | Ctrl/Cmd+A toggles filtered selectable options |
| Popup | Escape closes; Tab closes and continues host focus order |

Pointer and keyboard paths use the same source-selection operation so signals,
count, tri-state presentation, and accessibility events cannot drift.

## Accessibility

- The trigger exposes a button-menu entry with popup state, selected-count
  value, and a show-popup action.
- Application-provided accessible name and description remain authoritative.
- The popup item view exposes a multi-select list with one logical option per
  model row; checkboxes are delegate-painted rather than child widgets.
- Current focus and selection remain separate.
- The search editor retains native editable-text and IME semantics.
- The select-all header is one accessible tri-state checkbox outside the list.
- Effective selection, count, popup, active option, and model changes emit the
  matching events; repaint and repeated setters remain silent.

## Rendering and scale

- The popup uses a delegate for checkbox, icon, text, hover, focus, disabled,
  and selected states; it does not create a widget per row.
- Opening an unfiltered list queries data proportional to visible rows plus a
  bounded margin rather than scanning the model for width.
- Local search and explicit select-all may perform O(n) model work, but they do
  not retain O(n) widgets or copied business values.
- Structural tests use a large counting model to guard bounded access and
  retained-object behavior without brittle wall-clock thresholds.
- Light/Dark, high DPI, RTL, long CJK/English text, disabled options, narrow
  width, empty results, and reduced animation remain visual-review states.

## Verification

The maintained `test_multi_select_combo_box` target covers defaults, model and
selection ownership, structural changes, source/proxy mapping, pointer and
keyboard parity, search, select-all, popup lifecycle, focus return,
accessibility, and bounded large-model work.

Installed-header, PySide6 API-policy, generated catalog, Gallery, and
WebAssembly gates protect the public C++/Python delivery boundary. Run the
focused binary with `--gtest_filter="*VisualCheck*"` for manual presentation
review; automated CTest keeps interactive cases behind `SKIP_VISUAL_TEST=1`.

## Deferred behavior

- changing `ComboBox` into a dual single/multiple component;
- free-form values, token creation, removable chips, or wrapping content;
- asynchronous search, fuzzy ranking, custom filter callbacks, or loading rows;
- hierarchical and cascading selection;
- persistent application-owned item widgets;
- Apply/Cancel transactions or Escape rollback;
- maximum- or required-selection business validation; and
- a public delegate API before a concrete compatible extension case exists.

Each addition needs a separate public contract and must preserve the existing
model and selection semantics.

<!-- docs-nav:bottom:start -->
---
[← DataGrid API Contract](datagrid-api-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Editing Command Router API Contract →](editing-command-router-proposal.md)
<!-- docs-nav:bottom:end -->
