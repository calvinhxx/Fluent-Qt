# MultiSelectComboBox API Contract (#37)

Status: **Implemented for the next minor release**. The class remains a
separate multi-selection component; the existing `ComboBox` API is unchanged.

Issue: [#37 — 新增多选下拉框](https://github.com/calvinhxx/Fluent-Qt/issues/37)

## Verdict

Add a separate `fluent::basicinput::MultiSelectComboBox`. Keep the existing
`ComboBox` single-select and source-compatible.

This is not a `selectionMode` switch on `ComboBox`:

- Qt documents `QComboBox` around one current item and does not support changing
  its popup view to multi-selection.
- FluentQt's `ComboBox` intentionally preserves `QComboBox` current-index,
  editable-text, activation, and close-after-selection behavior.
- Reusing that public base would make `currentIndex`, `currentText`, editable
  free-form text, and multiple selected rows compete as the component's value.
- A separate component can expose one clear model/selection contract without
  breaking existing applications or PySide6 callers.

The implementation should still reuse the existing same-window `Flyout`,
`ListView`, `LineEdit`, design tokens, popup geometry helpers, and model/view
infrastructure.

## User outcome

An application can present a compact field that lets a user:

1. open a dropdown;
2. toggle several options without the dropdown closing after every click;
3. optionally filter a long local option list;
4. select or clear all currently filtered selectable options; and
5. read or replace the selection through Qt model indexes.

The primary scenarios are form fields and local filters with tens to thousands
of options. Remote querying, tag authoring, and free-form values are outside the
first release.

## External behavior references

- [Qt `QComboBox`](https://doc.qt.io/qt-6/qcombobox.html) remains a single-current-item control.
- [Fluent 2 Dropdown](https://fluent2.microsoft.design/components/web/react/core/dropdown/usage)
  keeps a multi-select popup open after an option is toggled and uses checkbox
  visuals for its options.
- [WAI-ARIA Listbox](https://www.w3.org/WAI/ARIA/apg/patterns/listbox/)
  separates focus from selection, uses Space to toggle the focused option, and
  recommends a separate select-all control when that action is important.

These references define interaction intent only. FluentQt remains a native Qt
Widgets implementation and does not copy web markup or styling code.

## Public boundary

### Name and category

- C++ type: `fluent::basicinput::MultiSelectComboBox`
- Header: `components/basicinput/MultiSelectComboBox.h`
- Gallery route: `multi-select-combobox`
- Focused test target: `test_multi_select_combo_box`

### Public base

The proposed public base is `QWidget, FluentElement, QMLPlus`, not `QComboBox`.
This is a deliberate composite-input contract:

- it does not inherit misleading single-current-item APIs;
- it does not expose `QPushButton::text`, `checkable`, or command semantics as
  the value API; and
- a private accessibility adapter can describe the real multi-select field and
  its controlled list.

The trigger remains one focusable surface. Implementation should reuse existing
private surface-painting helpers rather than introduce another public base or
duplicate a second design language. This base decision is frozen at the P0
review before prototype code is promoted.

### MVP surface

The following sketch records the installed API names and responsibilities.

```cpp
class MultiSelectComboBox : public QWidget,
                            public FluentElement,
                            public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model
               READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QItemSelectionModel* selectionModel
               READ selectionModel WRITE setSelectionModel
               NOTIFY selectionModelChanged)
    Q_PROPERTY(int modelColumn
               READ modelColumn WRITE setModelColumn NOTIFY modelColumnChanged)
    Q_PROPERTY(QModelIndex rootModelIndex
               READ rootModelIndex WRITE setRootModelIndex
               NOTIFY rootModelIndexChanged)
    Q_PROPERTY(QString placeholderText
               READ placeholderText WRITE setPlaceholderText
               NOTIFY placeholderTextChanged)
    Q_PROPERTY(bool searchEnabled
               READ isSearchEnabled WRITE setSearchEnabled
               NOTIFY searchEnabledChanged)
    Q_PROPERTY(QString searchPlaceholderText
               READ searchPlaceholderText WRITE setSearchPlaceholderText
               NOTIFY searchPlaceholderTextChanged)
    Q_PROPERTY(bool selectAllVisible
               READ isSelectAllVisible WRITE setSelectAllVisible
               NOTIFY selectAllVisibleChanged)
    Q_PROPERTY(int maximumVisibleItems
               READ maximumVisibleItems WRITE setMaximumVisibleItems
               NOTIFY maximumVisibleItemsChanged)
    Q_PROPERTY(bool isOpen
               READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    Q_PROPERTY(int selectedCount
               READ selectedCount NOTIFY selectedCountChanged)

public:
    explicit MultiSelectComboBox(QWidget* parent = nullptr);
    ~MultiSelectComboBox() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* model);

    QItemSelectionModel* selectionModel() const;
    void setSelectionModel(QItemSelectionModel* selectionModel);

    QList<int> selectedRows() const;
    QModelIndexList selectedIndexes() const;
    int selectedCount() const;
    bool isRowSelected(int row) const;

    bool isOpen() const;

public slots:
    void setSelectedRows(const QList<int>& rows);
    void clearSelection();
    void selectAll();
    void open();
    void close();
    void setIsOpen(bool open);

signals:
    void modelChanged(QAbstractItemModel* model);
    void selectionModelChanged(QItemSelectionModel* selectionModel);
    void modelColumnChanged(int column);
    void rootModelIndexChanged(const QModelIndex& index);
    void selectionChanged(const QItemSelection& selected,
                          const QItemSelection& deselected);
    void selectedCountChanged(int count);
    void placeholderTextChanged(const QString& text);
    void searchEnabledChanged(bool enabled);
    void searchPlaceholderTextChanged(const QString& text);
    void selectAllVisibleChanged(bool visible);
    void maximumVisibleItemsChanged(int count);
    void isOpenChanged(bool open);
};
```

No `addItem`, `addItems`, internal string store, custom item widget, or
free-form text API is included in the MVP. Simple applications can use a
`QStringListModel`; richer applications retain their existing
`QAbstractItemModel`. Convenience mutation APIs can be considered later without
changing the selection contract.

## Model and ownership contract

- The application owns the `QAbstractItemModel` supplied to `setModel()`.
- The component creates and owns a default `QItemSelectionModel` for the active
  model.
- A caller-supplied selection model remains caller-owned and must reference the
  same source model. A mismatched selection model is rejected without changing
  state.
- `setSelectionModel(nullptr)` restores a component-owned selection model.
- Replacing or destroying the source model detaches an external selection
  model, installs an empty internal selection model, closes the popup, and
  emits only the effective model/selection/count changes.
- Selection is row-based under `rootModelIndex()` and uses `modelColumn()` for
  display. Duplicate labels remain distinct because indexes, not strings, are
  the identity.
- Disabled or non-selectable model rows are never toggled by pointer, keyboard,
  `setSelectedRows()`, or select-all.
- Model reset, insertion, removal, move, and layout changes must not leave stale
  persistent indexes or an incorrect selected count.

The private filtered popup may use a `QSortFilterProxyModel`, but public
selection indexes always belong to the caller's source model. A private proxy
selection bridge maps selection in both directions; no second business-data
store is created.

## Selection behavior

Selection changes are committed immediately. Closing with Escape is not an
undo operation.

| Action | Result |
| --- | --- |
| Click or press Space/Enter on an option | Toggle that option and keep the popup open |
| Click outside or press Escape | Close the popup and retain selection |
| `clearSelection()` | Clear all selected rows under the active root |
| `selectAll()` | Select all enabled/selectable source rows under the active root, independent of the current search |
| Built-in select-all checkbox | Toggle all enabled/selectable rows in the current filtered result |
| Filter changes | Keep hidden selections; recompute the visible select-all state |

The built-in select-all row is not model row zero. It is a separate tri-state
control above the option list:

- unchecked: no filtered selectable row is selected;
- partially checked: some filtered selectable rows are selected;
- checked: all filtered selectable rows are selected; and
- disabled: the filtered result contains no selectable rows.

This keeps application data free of synthetic command rows and gives the
select-all action an independent accessible name and state.

## Search behavior

- `searchEnabled` defaults to `false`; the caller opts into the extra input.
- Search is a case-insensitive substring match over `Qt::DisplayRole` in
  `modelColumn()` for the MVP.
- The query is transient UI state, cleared whenever the popup closes, and is
  not part of the selected value.
- Hidden selections remain selected and still contribute to `selectedCount`.
- Search does not add free-form values and does not mutate the source model.
- Local filtering is expected to inspect source rows. It may be debounced, but
  retained objects must stay proportional to the popup viewport and selected
  ranges, not total rows.
- Remote/asynchronous search and caller-provided filter callbacks are deferred.
  Applications needing those behaviors supply an already-filtered model and
  leave built-in search disabled.

## Closed-field presentation

- No selection: show `placeholderText`; the default is empty.
- Selected labels fit: show them in source-row order, separated by the current
  locale's compact list separator.
- Labels do not fit: show a translatable selected-count summary and elide only
  as a last resort.
- The field stays one standard control row high; tags/chips and wrapping are
  not part of the MVP.
- The full selected values remain available through model indexes and the
  accessible value. Painting text is never the business-data API.
- The trailing chevron, field surface, focus cue, and popup gap follow existing
  `ComboBox` tokens. RTL mirrors text, checkbox, search, and chevron placement.

## Popup and open-state contract

The dropdown composes `Flyout` and follows the canonical same-window overlay
contract:

- non-modal and non-dim;
- `CloseOnPressOutside | CloseOnEscape`;
- no native `Qt::Window`, `Qt::Dialog`, or `Qt::Tool`;
- `isOpen` is the logical requested state, not animation progress or temporary
  widget visibility;
- repeated open/close requests are no-ops;
- moving the owner to another top-level recreates or reparents the popup safely;
  and
- closing returns focus to the trigger unless focus deliberately moved
  elsewhere.

Opening with search enabled focuses the search editor. Opening without search
focuses the first selected option, or the first selectable option when the
selection is empty.

## Keyboard contract

| Context | Keys |
| --- | --- |
| Closed field | Space, Enter, Alt+Down, or F4 opens |
| Search editor | Text/IME filters; Down moves to results; Escape closes; Ctrl/Cmd+A selects search text |
| Option list | Up/Down/Home/End moves current focus without changing selection |
| Option list | Space or Enter toggles the focused option |
| Option list | Ctrl+A or Cmd+A toggles all filtered selectable options |
| Popup | Escape closes without clearing selection; Tab closes and continues the host focus order |

Pointer and keyboard paths use the same source-selection operation so signals,
tri-state state, display text, and accessibility events cannot drift.

## Accessibility contract

The component is classified as **Adapter** before public release.

- The trigger exposes a button-menu style entry with has-popup,
  expanded/collapsed, selected-count value, and a show-popup action.
- Application-provided root name and description remain authoritative.
- The real popup `ListView` exposes a multi-select list with one logical option
  per model row; visual checkboxes are delegate-painted states, not persistent
  child widgets.
- Focus/current option and selected options remain separate.
- The search editor controls the result list and retains native editable-text
  and IME semantics.
- The select-all header is one real tri-state checkbox outside the listbox.
- Selection, count, popup, active descendant, and model structure events fire
  after effective changes only. Paint, resize, theme refresh, filtering that
  preserves semantic state, and repeated setters remain silent.
- Closing the same-window popup restores an eligible trigger focus target.

Focused `Contract_Accessibility*` tests must query roles, values, states,
actions, child relationships, focus return, event counts, and caller-owned
text. Visual snapshots are not accessibility evidence.

## Rendering and scale

- Reuse `ListView`, but disable its accent selection pill in this popup so the
  checkbox remains the single selection indicator.
- Use a delegate to paint checkbox, icon, text, hover, current focus, disabled,
  and selected states; do not create a widget per option.
- Opening an unfiltered list must query model data proportional to visible rows
  plus a bounded sampling margin. Do not scan every row merely to calculate the
  popup width.
- Local search and explicit select-all may perform O(n) model work, but must not
  retain O(n) widgets or copied business values.
- Structural counting-model tests use at least 10,000 rows. They assert access
  and object bounds rather than brittle wall-clock thresholds.
- Theme, Light/Dark, high DPI, RTL, long CJK/English labels, disabled options,
  narrow width, empty results, and reduced animation are part of visual review.

## Delivery plan

### P0 — Contract freeze

Deliverables:

- review this proposal against issue #37;
- confirm the separate component name and public base;
- confirm that built-in select-all targets filtered results;
- freeze model ownership, signal order, search reset, and immediate-commit
  behavior; and
- record the intended public delivery as C++ plus PySide6 in the same release
  slice.

Exit: no unresolved choice can change the class name, selected-value model,
ownership, or keyboard behavior.

### P1 — Private native prototype

Deliverables:

- compile `MultiSelectComboBox` in the library but omit it from installed
  headers, aggregate headers, bindings, Gallery catalogs, and generated AI
  guidance;
- implement source model, internal/external selection model, source/proxy
  mapping, row toggling, display summary, and same-window popup lifecycle;
- add structural model/selection/open-state tests; and
- prove that unfiltered open/scroll work stays viewport-bounded.

Exit: the focused target passes on the host Qt version with no public surface.

### P2 — Search, select-all, keyboard, and accessibility

Deliverables:

- add transient local filtering and the tri-state select-all header;
- finish pointer/keyboard parity and focus return;
- add the private accessibility adapter and inventory entry; and
- complete Light/Dark/RTL/long-text/empty/disabled VisualCheck scenarios.

Exit: all `Contract_*` tests pass, the accessibility inventory validator is
clean, and manual visual review accepts the interaction.

### P3 — Public C++ and Gallery delivery

Deliverables:

- add the public header to `cmake/FluentQtInstallHeaders.cmake` and
  `<FluentQt/BasicInput.h>`;
- add a dedicated Gallery component/card image and source-aligned samples for
  basic selection, search plus filtered select-all, and caller-owned model
  updates;
- update README component overview, API audit, content/catalog guidance, and
  generated AI assets; and
- validate native install/package consumption through `<FluentQt/FluentQt.h>`.

Exit: installed C++ consumers use only documented public headers and the three
Gallery examples match their displayed snippets.

### P4 — PySide6 and WebAssembly parity

Deliverables:

- export the class through Shiboken, `fluentqt.basicinput`, top-level
  `fluentqt`, stubs, and `api-manifest.json`;
- cover model/selection-model ownership, replacement, Python subclassing,
  signals, garbage collection, and the three Gallery examples;
- build the WebAssembly Gallery and exercise search, multi-selection,
  select-all, Escape, and focus in the browser; and
- run binding policy, generated-contract, AI-catalog, and WASM smoke gates.

Exit: the component is either public in both C++ and PySide6 or remains private
in both. There is no accidental C++-only release.

## Focused acceptance tests

At minimum, `tests/components/basicinput/TestMultiSelectComboBox.cpp` covers:

1. defaults and repeated-setter no-op signals;
2. caller-owned model and model destruction;
3. internal and external selection-model ownership and mismatch rejection;
4. selected-row normalization, duplicate labels, disabled rows, and count
   signals;
5. insert/remove/move/reset while selected and while open;
6. model column and root-index mapping;
7. item click/Space toggle without popup close;
8. outside/Escape/programmatic close and owner top-level migration;
9. search filtering with hidden selection persistence and query reset;
10. filtered tri-state select-all and empty-result behavior;
11. keyboard current-versus-selection behavior and focus return;
12. accessible role/name/value/state/action/event contracts;
13. bounded unfiltered open/scroll model access with a 10,000-row counting
    model; and
14. VisualCheck coverage for Light/Dark, RTL, long text, narrow width, disabled
    options, partial selection, and search.

Focused validation after implementation:

```bash
cmake --build --preset vcpkg-osx --target test_multi_select_combo_box --parallel
ctest --preset vcpkg-osx -L '^test_multi_select_combo_box$' --output-on-failure
python3 tools/quality/validate_accessibility_inventory.py --project-root .
python3 bindings/pyside6/tools/verify_api_policy.py
python3 tools/ai/generate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
```

Run the focused binary directly with `--gtest_filter="*VisualCheck*"` for human
review; automated CTest continues to use `SKIP_VISUAL_TEST=1`.

## Deferred from the MVP

- changing existing `ComboBox` into a dual single/multiple component;
- free-form values, token/tag creation, removable chips, or wrapping trigger
  content;
- asynchronous/remote search, fuzzy ranking, custom filter callbacks, or
  loading/error rows;
- hierarchical multi-selection or parent/child cascading selection;
- application-owned item widgets, menus used as the value model, or persistent
  checkbox widgets per row;
- Apply/Cancel transactions or Escape rollback;
- maximum-selection rules, required-selection validation, and business-domain
  validation; and
- a public delegate API before the default checkbox-row accessibility contract
  is stable.

Each deferred feature needs a concrete scenario and a separate compatible API
proposal.
