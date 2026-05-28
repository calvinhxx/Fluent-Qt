## Why

`TextBlock` is the textfields module's static Fluent text label component. The requested public API direction is to expose this component as `Label`, which better matches how callers use it.

`TextEdit` is a separate rich/multi-line text editing component and must remain unchanged.

## What Changes

- Rename the static `TextBlock` component to `Label`.
- Move the static label implementation to `src/view/textfields/Label.h/.cpp`.
- Remove the old `TextBlock.h/.cpp` public entry instead of keeping a compatibility alias or shim.
- Update production and test call sites to include `view/textfields/Label.h` and construct `view::textfields::Label`.
- Restore and keep `TextEdit` as the rich text editing component.
- Avoid introducing `using Label = TextBlock`; `Label` is a real class, not an alias.
- **BREAKING** existing code that includes `view/textfields/TextBlock.h` or constructs `view::textfields::TextBlock` must migrate to `Label`.

## Capabilities

### New Capabilities

- `label`: Specifies the canonical static text label component API and behavior after renaming `TextBlock`.

### Modified Capabilities

- `qt-version-compatibility`: Clarify that the rename must not reintroduce a broad `Label` alias from `TextBlock`.

## Impact

- Source headers and implementation in `src/view/textfields/`: `TextBlock.h/.cpp` are replaced by `Label.h/.cpp`.
- `TextEdit.h/.cpp` remain as the editing component.
- Tests in `tests/views/textfields/`: `TestTextBlock.cpp` is replaced by `TestLabel.cpp`, and `TestTextEdit.cpp` remains a separate test target.
- CMake/source discovery and test registration are updated for the renamed files.
- No new third-party dependencies are required.
