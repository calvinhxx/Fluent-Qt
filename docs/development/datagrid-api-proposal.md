# DataGrid API Contract

> **Status:** Accepted contract, shipped in FluentQt 1.7.0

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Accepted component contracts

[← Field API Contract](field-api-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [MultiSelectComboBox API Contract →](multi-select-combobox-api-proposal.md)
<!-- docs-nav:top:end -->

`fluent::collections::DataGrid` is a Fluent table surface built on
`QTableView`. It presents large two-dimensional models, column interaction,
selection, and delegate-based editing without becoming a data source, form
engine, or spreadsheet.

## Core boundary

DataGrid owns presentation and interaction state. The application remains
authoritative for:

- `QAbstractItemModel` data, headers, flags, sorting, filtering, paging, and
  persistence;
- a caller-supplied `QItemSelectionModel`;
- a caller-supplied `QAbstractItemDelegate`, including painting, editors,
  validation, and commit behavior; and
- domain schemas, loading, serialization, and business rules.

The view does not copy the complete model into an internal store. Normal cells
are delegate-painted, and editor widgets exist only while an index is being
edited.

## Public surface

DataGrid inherits `QTableView`, `FluentElement`, and `QMLPlus`. It reuses Qt
APIs wherever they already carry the contract:

- model, selection model, delegate, headers, sorting, grid, word wrapping, row
  and column sizing, and edit triggers remain standard item-view APIs;
- `selectionBehavior` remains the Qt item/row/column choice;
- `headerData()` and `QHeaderView` remain the column-label, visibility,
  resize, reorder, and sort boundary; and
- model flags and delegate behavior remain authoritative for editing.

FluentQt adds:

- the shared `fluent::collections::SelectionMode`;
- `placeholderText` and `isShowingPlaceholder()`;
- border and background visibility;
- scroll chaining; and
- borrowed horizontal and vertical Fluent overlay scroll bars.

New properties follow the getter/setter/changed-signal pattern and remain
silent when their effective value does not change.

## Scale and lifetime

- Initial display, resize, and scroll work must follow visible cells plus a
  bounded margin, not total model size.
- Retained delegates, editors, indexes, and helper objects must not scale with
  the total row or column count.
- Models, selection models, and delegates remain caller-owned. Replacement or
  destruction disconnects the old object without extending its lifetime.
- Model reset, row/column changes, layout changes, and replacement must
  invalidate stale logical cells, editors, focus targets, and accessibility
  cache entries.
- Fluent overlay scroll bars mirror native ranges and remain borrowed internal
  children.

## Selection and columns

- Current index, selected indexes, activation, and row/cell selection remain
  distinct Qt concepts.
- Keyboard and pointer input use the same model and selection state.
- Header resize, reorder, hide/show, and sort requests stay in `QHeaderView`
  and the caller model; DataGrid adds no second column store.
- Persisting column layout is an application responsibility.
- Model-provided accessible text and descriptions do not replace display or
  edit values.

## Editing and validation

DataGrid is read-only until the caller enables inherited edit triggers, and an
index remains editable only when its model flags allow it.

- The default path keeps `QStyledItemDelegate` editor, commit, cancel, and
  geometry semantics.
- Enter, F2, Tab, and Escape travel through the real item-view/delegate path.
- A failed `setData(..., Qt::EditRole)` does not become a successful commit
  and does not cause DataGrid to synthesize a replacement value.
- Validation state may travel through application-defined model roles and a
  caller delegate. DataGrid reserves no public validation-role range.
- Only the active index may own a transient editor. Commit, cancel, model
  reset, model replacement, and view destruction release it.

`Field` is a semantic reference for validation presentation, not a widget to
install in every table cell.

## Accessibility

DataGrid exposes logical table, header, and cell semantics without creating a
child widget per cell.

- Caller-provided accessible names and descriptions remain authoritative.
- The placeholder becomes an automatic description only when no application
  description is present.
- Logical cells expose model-provided names, descriptions, focus, selection,
  read-only/editable state, and supported actions.
- Structural model changes produce the matching table-model accessibility
  change and invalidate cached logical interfaces.
- Platform-wide hierarchy enumeration that is unstable in the underlying Qt
  plugin remains a manual platform diagnostic rather than a FluentQt-specific
  automated gate.

## Cross-language and platform delivery

The installed C++ header, PySide6 facade, native and Python Gallery samples,
and WebAssembly Gallery share this model/view contract. Exact exports, routes,
and sample inventory come from generated manifests and catalogs rather than
hand-maintained counts in this document.

Python retains wrappers for caller-supplied models, selection models, and
delegates while they are installed, but Qt ownership does not transfer.

## Verification

The maintained `test_data_grid` target covers large-model bounds, model and
selection lifetime, structural changes, headers, sorting, keyboard selection,
editing, rejected commits, accessibility, scroll behavior, theme rendering,
RTL, and high-DPI presentation. Installed-package, PySide6, catalog, Gallery,
and WebAssembly gates protect the public delivery boundary.

## Deferred from the MVP

- Spreadsheet formulas, merged cells, pivoting, grouping, and tree tables
- Frozen panes, batch clipboard, fill handles, and Excel-style range editing
- Built-in filtering UI, pagination, remote-data adapters, CSV import/export,
  and application persistence
- A FluentQt-owned column schema duplicating model header roles
- Built-in validation roles that could collide with application roles

Each addition needs a concrete use case and a separate compatible contract; it
is not an implicit DataGrid responsibility.

<!-- docs-nav:bottom:start -->
---
[← Field API Contract](field-api-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [MultiSelectComboBox API Contract →](multi-select-combobox-api-proposal.md)
<!-- docs-nav:bottom:end -->
