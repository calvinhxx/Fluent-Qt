## 1. Label Elide API

- [x] 1.1 Add a `textElideMode` property, getter, setter, and change signal to `Label`.
- [x] 1.2 Store the full Label text separately from the rendered QLabel text.
- [x] 1.3 Ensure default `Qt::ElideNone` preserves existing constructor and `text()` behavior.

## 2. Elision Rendering State

- [x] 2.1 Implement an internal elision recomputation helper using current font metrics and content width.
- [x] 2.2 Update the rendered QLabel text when full text, font, typography, geometry, elide mode, or theme changes.
- [x] 2.3 Track whether the current rendered text is actually elided.
- [x] 2.4 Preserve existing Fluent typography and theme refresh behavior.

## 3. Hover ToolTip Behavior

- [x] 3.1 Add lazy ownership of `view::status_info::ToolTip` inside `Label`.
- [x] 3.2 Show the tooltip with full text only on hover enter when the current Label is actually elided.
- [x] 3.3 Hide the tooltip on hover leave and when recomputation determines the Label is no longer elided.
- [x] 3.4 Position the tooltip near the Label without changing caller-managed Label geometry.

## 4. Tests

- [x] 4.1 Add unit coverage for default elide mode and unchanged text behavior.
- [x] 4.2 Add unit coverage for constrained `Qt::ElideRight` rendering while `Label::text()` returns full text.
- [x] 4.3 Add unit coverage that hover shows full-text ToolTip only when the Label is actually elided.
- [x] 4.4 Add unit coverage that resize, text changes, and typography/theme changes recompute elision state.
- [x] 4.5 Keep existing Label constructor, typography, theme, and VisualCheck tests valid.

## 5. Verification

- [x] 5.1 Build and run the `test_label` target.
- [x] 5.2 Run OpenSpec status/validation for `add-label-elide-tooltip`.
