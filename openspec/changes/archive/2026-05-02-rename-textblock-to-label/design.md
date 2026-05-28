## Context

`src/view/textfields/TextBlock.{h,cpp}` implements a static Fluent text label built on `QLabel`. It exposes the `fluentTypography` property, applies theme typography, and updates `QPalette::WindowText` when the theme changes.

`src/view/textfields/TextEdit.{h,cpp}` is a separate rich/multi-line text editing component built around an internal `QTextEdit`. It is not part of this rename and must remain available as `TextEdit`.

## Goals / Non-Goals

**Goals:**

- Make `view::textfields::Label` the canonical public name for the current static `TextBlock` behavior.
- Preserve the static label API: text constructors, inherited `QLabel` API, `fluentTypography`, `setFluentTypography()`, `typographyChanged`, and theme refresh behavior.
- Remove the old `TextBlock` include path and type name instead of keeping a compatibility alias.
- Update production code, tests, and VisualCheck naming to use `Label`.
- Keep `TextEdit` restored and unchanged.

**Non-Goals:**

- Do not rename `TextEdit` or change its rich text editing behavior.
- Do not redesign the static label component.
- Do not introduce `using Label = TextBlock` or any alias that maps `Label` to `TextBlock`.
- Do not rename unrelated local variables named `label`.

## Decisions

### 1. Move static label implementation to `Label`

The actual static text class moves to `Label.h/.cpp` and is declared as `class Label : public QLabel, public FluentElement, public view::QMLPlus`.

The old `TextBlock.h/.cpp` files are removed. This is an intentional breaking rename to avoid redundant compatibility names.

### 2. Keep `TextEdit` separate

`TextEdit` remains the editing component and keeps its existing files, tests, and API. `Label` must not expose editing APIs such as `setPlainText()`, placeholder text, read-only state, auto-height metrics, or scrollbars.

### 3. Test the renamed static label

The renamed textfields test target should use `Label` in the test filename, fixture naming, includes, and construction. Existing `TextEdit` tests remain registered separately.

## Risks / Trade-offs

- Removing `TextBlock` breaks old call sites -> all in-repo call sites are migrated to `Label` in this change.
- `Label` may collide with platform symbols if introduced as an alias -> use a real namespaced class and avoid `using Label = TextBlock`.
- Broad mechanical renaming could accidentally touch `TextEdit` -> build both `test_label` and `test_text_edit` to verify the split.

## Migration Plan

1. Restore `TextEdit.h/.cpp` and `TestTextEdit.cpp` if they were changed during the mistaken rename.
2. Replace `Label.h/.cpp` with the static label implementation from `TextBlock`.
3. Delete `TextBlock.h/.cpp`.
4. Update source and test call sites from `TextBlock` to `Label`.
5. Replace `TestTextBlock.cpp` with `TestLabel.cpp` and register `test_label`.
6. Keep `test_text_edit` registered for the editing component.
7. Build and run `test_label` and `test_text_edit`, then validate the OpenSpec change.
