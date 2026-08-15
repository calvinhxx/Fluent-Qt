# Field API Contract (1.7-B)

Status: **Accepted and implemented** on `release/1.7.x`.

`fluent::layout::Field` adds the form chrome around **any existing input
control**: label, required marker, helper text, and validation feedback. It is
not a new editor base class and does not replace LineEdit, PasswordBox,
NumberBox, ComboBox, DatePicker, or TextEdit.

## Owns

- Caption label and optional required indicator
- Editor slot with `fluent::WidgetOwnership` (`Borrowed` default, plus
  `Reparented` / `Owned`)
- Helper text and validation **presentation** (`None` / `Error` / `Warning` /
  `Success`), including a compact semantic status icon
- Wrapped long labels and baseline-aligned required indicators
- Focus handoff (`setFocusProxy`) and caption buddy
- Accessibility name / description from label, helper, and validation message

## Does not own

- Editor value, `QValidator`, IME, or parsing
- Writing the slotted editor's text as part of validation presentation
- A new inheritance tree under the text-field controls

## Ownership

Mirrors Expander:

- `setEditor(QWidget*)` borrows
- `setEditor(QWidget*, WidgetOwnership)` records policy
- `takeEditor()` detaches and transfers a parentless widget to the caller
- `releaseEditor()` applies the recorded policy
- Changing the same editor's ownership mode in place is rejected; call
  `takeEditor()` before reinstalling it with a different mode. C++ and PySide6
  use the same rule.
- Rejects `this` and ancestor widgets; `nullptr` clears the slot

Field itself is not a Tab stop. The editor keeps its own focus and size policy;
Field only forwards explicit focus through `focusProxy()` and wires the caption
buddy. A validation state becomes visible only when `validationMessage` is
non-empty.

## Tests

`tests/components/layout/TestField.cpp` (`test_field`). Contract names use the
`Contract_*` prefix. VisualCheck keeps `SKIP_VISUAL_TEST` and `qApp->exec()`.
