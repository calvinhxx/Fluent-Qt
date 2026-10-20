# CodeBlock

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Form state and validation](form-state.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Spatial: compose existing widgets in depth →](spatial-view.md)
<!-- docs-nav:top:end -->

`fluent::textfields::CodeBlock` displays read-only source code. It combines the
existing Expander, Label, Button and ScrollView, with no editor or parsing-library
dependency. Include `<FluentQt/TextFields.h>` or `<FluentQt/FluentQt.h>`.

```cpp
using fluent::textfields::CodeBlock;
auto* code = new CodeBlock("auto answer = 42;", parent);
code->setLanguage("cpp");
code->setMaxVisibleLines(8);
```

```python
code = fluentqt.CodeBlock("answer = 42", parent)
code.setLanguage("python")
code.setMaxVisibleLines(8)
```

## Display and copy

The four code-specific properties are `code`, `language`, `wordWrap` and
`maxVisibleLines`. Wrapping is enabled by default; the default height limit is
16 rendered lines. Set the limit to `0` to fit the entire source. With wrapping
disabled, long lines scroll horizontally. The inherited Expander API controls
folding; CodeBlock starts expanded.

C++, Python and QML receive syntax colors. Language identifiers are normalized:
`c++` and `cxx` become `cpp`, `py` becomes `python`, and an empty identifier,
`text` or `plaintext` becomes `plain`. Other identifiers display plain text.
High contrast uses the theme's primary text color. Highlighting is intended for
reading snippets, not language analysis or diagnostics.

The header copy button and `copy()` copy the original source. A selected passage
can be copied with the standard keyboard shortcut or context menu. Tabs, CRLF,
repeated spaces and real zero-width characters survive copying; synthetic wrap
opportunities do not enter the clipboard. Source is always escaped before it is
rendered, including unknown languages.

`copyFinished(bool success)` reports completion of a copy attempt. Desktop builds
use Qt's clipboard. WebAssembly uses the browser clipboard promise, so failure is
reported when clipboard permission or the required user gesture is unavailable.
Destroying the component cancels its completion callback. The library provides
brief button feedback; applications can connect the signal for additional feedback.

## Gallery and bindings

C++, PySide6 and WebAssembly run the same native renderer and source-selection
mapping. `GalleryCodeBlock` adds source-language, key/full-example and file
selection around the public component. These Gallery catalog concerns are not
part of CodeBlock's API.

Gallery's `code-block` route demonstrates selection/copy/folding and bounded
scrolling with a wrap toggle. Gallery source panels also reuse CodeBlock, with
unlimited visible lines and an initially collapsed state.

<!-- docs-nav:bottom:start -->
---
[← Form state and validation](form-state.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Spatial: compose existing widgets in depth →](spatial-view.md)
<!-- docs-nav:bottom:end -->
