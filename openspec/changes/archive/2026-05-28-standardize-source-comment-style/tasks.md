## 1. Documentation

- [x] 1.1 Create `docs/development/comment-style.md` with scope, public API Doxygen format, implementation-comment rules, module density guidance, glossary, and examples.
- [x] 1.2 Link `docs/development/comment-style.md` from `docs/development/README.md`.
- [x] 1.3 Update `.github/copilot-instructions.md` so agent-facing guidance points to the canonical source comment style document.

## 2. Source Demonstration

- [x] 2.1 Update representative comments in `src/design/` to demonstrate design-token/module comment style without changing runtime code.
- [x] 2.2 Update representative comments in `src/view/foundation/` to demonstrate bilingual public API contract comments.
- [x] 2.3 Update representative comments in `src/utils/` to demonstrate operational utility comments and implementation-note boundaries.
- [x] 2.4 Avoid mechanical rewrites across unrelated `src/` files; leave untouched component comments for opportunistic future cleanup.

## 3. Validation

- [x] 3.1 Verify documentation links and examples reference `comment-style.md`, `zh_CN:`, and project-neutral terminology.
- [x] 3.2 Run `openspec validate standardize-source-comment-style --strict`.
- [x] 3.3 Record that no build or Qt test run is required if implementation changes are docs-only or comment-only.

## 4. Additional View Component Coverage

- [x] 4.1 Add bilingual public API comments to `src/view/basicinput/Button.h` as the common button-like component example.
- [x] 4.2 Add bilingual foundation comments to `src/view/foundation/QMLPlus.h` for anchor layout, property binding, and state contracts.
- [x] 4.3 Add bilingual public API comments to `src/view/scrolling/AnnotatedScrollBar.h` for label, ScrollView connection, and visible-label semantics.
- [x] 4.4 Keep the additional pass scoped to representative view headers without rewriting unrelated components.

## 5. Bulk View Header Comment Fill

- [x] 5.1 Mechanically add missing bilingual class comments across public `src/view/**/*.h` headers.
- [x] 5.2 Mechanically add missing bilingual `Q_PROPERTY` comments across public `src/view/**/*.h` headers.
- [x] 5.3 Keep the bulk pass comment-only, with no behavior or declaration changes.
- [x] 5.4 Update proposal, design, and source-comment-style spec to document the broader view-header pass.

## 6. Polished View Header Comment Pass

- [x] 6.1 Replace placeholder class comments with component-specific bilingual contracts matching the `Button.h` style.
- [x] 6.2 Replace generic `Q_PROPERTY` comments with semantic descriptions for affected component APIs.
- [x] 6.3 Verify no placeholder phrases such as `view component`, `视图组件`, or generic ``property`` comments remain under `src/view`.
- [x] 6.4 Re-run OpenSpec validation and the `fluent_qt_lib` build after the polished pass.

## 7. Design And Compatibility Header Polish

- [x] 7.1 Replace Chinese-only design token comments under `src/design/` with English anchors plus `zh_CN` explanations.
- [x] 7.2 Add bilingual public-contract comments to `QtCompat.h` and `WindowChromeCompat.h`.
- [x] 7.3 Add concise bilingual implementation comments for non-obvious platform chrome behavior.
- [x] 7.4 Verify the design and compatibility pass is comment-only and rebuild `fluent_qt_lib`.

## Validation Notes

- `openspec validate standardize-source-comment-style --strict` passed.
- `cmake --build --preset vcpkg-osx --target fluent_qt_lib` passed after the polished view-header pass, confirming MOC and header parsing remain valid.
- The placeholder-comment scan returned no matches after the polished pass.
- A comment-stripped comparison against `HEAD` for `src/view/**/*.h` found no non-comment header differences.
- A comment-stripped comparison against `HEAD` for `src/design/*` and `src/compatibility/*` found no non-comment source differences.
- Qt test runs were not required because the implementation only changes documentation and comments.
