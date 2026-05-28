## 1. Include Inventory And Migration

- [x] 1.1 Inventory all project-owned references to `view/FluentElement.h`, `view/QMLPlus.h`, and `view/internal/overlay/...` across `src`, `tests`, docs, OpenSpec artifacts, and repository guidance.
- [x] 1.2 Replace source includes in `src/**` with canonical `view/foundation/FluentElement.h`, `view/foundation/QMLPlus.h`, and `view/foundation/overlay/...` paths.
- [x] 1.3 Replace test includes in `tests/**` with canonical foundation paths.
- [x] 1.4 Confirm no project-owned source or tests still include removed legacy paths.

## 2. Remove Legacy Forwarders

- [x] 2.1 Delete `src/view/FluentElement.h` and `src/view/QMLPlus.h` forwarding headers.
- [x] 2.2 Delete `src/view/internal/overlay/` forwarding headers and remove the empty compatibility directory.
- [x] 2.3 Confirm `src/view` root contains no direct foundation forwarding files and no `internal/overlay` compatibility bucket.

## 3. Documentation And Guidance

- [x] 3.1 Update `.github/copilot-instructions.md` to describe canonical foundation includes as required paths instead of preferred paths with legacy compatibility.
- [x] 3.2 Update `docs/development/component-api-conventions.md` to remove legacy include guidance and require canonical foundation include paths.
- [x] 3.3 Update `docs/architecture/overlay-behavior.md` to remove compatibility forwarding language for `src/view/internal/overlay/`.
- [x] 3.4 Ensure new OpenSpec artifacts document the breaking source-compatibility impact and migration path.

## 4. Validation

- [x] 4.1 Build the library and focused foundation/overlay targets with `cmake --build --preset vcpkg-osx --target test_anchor_layout test_fluent_element test_qml_plus test_popup test_flyout test_combo_box test_drawer_view`.
- [x] 4.2 Run focused tests with `SKIP_VISUAL_TEST=1` where applicable for foundation and overlay-consuming targets.
- [ ] 4.3 Run broad `ctest --preset vcpkg-osx --output-on-failure` because public include headers were removed across the component tree.
	- Attempted: 810/811 runnable tests passed; blocked by existing `QtCompat.ComponentSourcesDoNotContainScatteredQtVersionGuards` failures in `FlowView` and Qt-version-guarded tests, unrelated to legacy foundation include removal.
- [x] 4.4 Run legacy include path searches to confirm `view/FluentElement.h`, `view/QMLPlus.h`, and `view/internal/overlay/` are gone from project-owned source, tests, docs, and guidance except historical OpenSpec context where intentionally retained.
- [x] 4.5 Run `openspec validate remove-view-foundation-legacy-forwarders --strict`.
- [x] 4.6 Run `git diff --check` and inspect the final diff for accidental behavior edits.