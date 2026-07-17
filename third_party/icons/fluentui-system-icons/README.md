# Fluent UI System Icons source font

- Upstream: <https://github.com/microsoft/fluentui-system-icons>
- Release: `1.1.328`
- License: MIT; see `LICENSE.txt` and `NOTICE.txt`.

Vendored upstream files:

| File | SHA-256 |
| --- | --- |
| `FluentSystemIcons-Regular.ttf` | `8ebc946d0de5ff7cd8fa653020f7c4046c71e27574ac6a9013aa727021290c32` |
| `FluentSystemIcons-Regular.json` | `9c782fa7f81a5a631ef68fa618305fd946d0fd0697372f11988116ee5852ae70` |

`tools/fonts/generate_typography_assets.py` retains the complete upstream
Regular catalog, renames the runtime family to `FluentQt Icons`, and adds the
semantic codepoint shortcuts used by FluentQt controls. If an upstream icon
occupies one of those shortcut positions, it is moved to an unused
supplementary private-use codepoint and the generated catalog is updated; no
official glyph is dropped.
