# Inter source fonts

- Upstream: <https://github.com/rsms/inter>
- Release: `v4.1`
- License: SIL Open Font License 1.1; see `LICENSE.txt`.

Vendored hinted static TrueType files:

| File | SHA-256 |
| --- | --- |
| `Inter-Regular.ttf` | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
| `Inter-SemiBold.ttf` | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` |
| `InterDisplay-SemiBold.ttf` | `0310d7a325896129730c6c8cf9a6e0f81ee258bedf77b1ff059b2a7b75f74e02` |

The upstream Reserved Font Name is not used by the generated application
fonts. `tools/fonts/generate_typography_assets.py` renames these static faces to
`FluentQt UI Text`, `FluentQt UI Heading`, and `FluentQt UI Display` while
preserving their TrueType hint programs. Only the renamed derivatives are
embedded in the runtime resource file.
