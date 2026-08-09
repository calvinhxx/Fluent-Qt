# Noto Sans SC source font

- Upstream: <https://github.com/notofonts/noto-cjk>
- Revision: `f8d157532fbfaeda587e826d4cd5b21a49186f7c`
- Source path: `Sans/SubsetOTF/SC/NotoSansSC-Regular.otf`
- License: SIL Open Font License 1.1; see `LICENSE.txt`.

| File | SHA-256 |
| --- | --- |
| `NotoSansSC-Regular.otf` | `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9` |

`tools/fonts/generate_typography_assets.py --web-fallback` subsets the GB 2312
repertoire and renames the derivative to `FluentQt UI Simplified Chinese`.
`--check-web-fallback` verifies the committed output byte for byte. The
generated face is embedded only by supported WebAssembly builds, where Qt
cannot use host system fonts. Desktop builds continue to use the platform CJK
fallback and do not carry this optional asset.
