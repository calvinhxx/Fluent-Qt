## 1. Correct Rename Target

- [x] 1.1 Restore `src/view/textfields/TextEdit.h` and `TextEdit.cpp` as the rich text editing component.
- [x] 1.2 Restore `tests/views/textfields/TestTextEdit.cpp` and keep `test_text_edit` registered.
- [x] 1.3 Replace the mistaken multi-line `Label` implementation with the static label implementation from `TextBlock`.

## 2. Label / TextBlock Source Rename

- [x] 2.1 Add `src/view/textfields/Label.h` with `class Label : public QLabel, public FluentElement, public view::QMLPlus`.
- [x] 2.2 Add `src/view/textfields/Label.cpp` with the former `TextBlock` constructors, typography setter, and theme refresh behavior.
- [x] 2.3 Delete `src/view/textfields/TextBlock.h` and `TextBlock.cpp` instead of keeping a compatibility alias or shim.
- [x] 2.4 Ensure `Label.h` does not contain `using Label = TextBlock` or `using TextBlock = Label`.

## 3. Call Site Migration

- [x] 3.1 Update production includes from `view/textfields/TextBlock.h` to `view/textfields/Label.h`.
- [x] 3.2 Update production types and forward declarations from `TextBlock` to `Label`.
- [x] 3.3 Update tests and VisualCheck examples from `TextBlock` to `Label`.
- [x] 3.4 Keep unrelated local variables named `label` intact.

## 4. Tests and Docs

- [x] 4.1 Replace `tests/views/textfields/TestTextBlock.cpp` with `TestLabel.cpp`.
- [x] 4.2 Register `test_label` and `test_text_edit` in `tests/views/textfields/CMakeLists.txt`.
- [x] 4.3 Update `readme.md` to describe `Label` as the static text label and `TextEdit` as the editing component.
- [ ] 4.4 Optionally run the `Label` VisualCheck manually on a machine with a display.

## 5. Validation

- [x] 5.1 Configure the build after deleting `TextBlock` files.
- [x] 5.2 Build `test_label` and `test_text_edit`.
- [x] 5.3 Run automated tests with VisualCheck skipped for `test_label`.
- [x] 5.4 Run automated tests with VisualCheck skipped for `test_text_edit`.
- [x] 5.5 Run `openspec validate rename-textblock-to-label`.
