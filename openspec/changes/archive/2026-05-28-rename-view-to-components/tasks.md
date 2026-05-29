## 1. Baseline And Inventory

- [x] 1.1 Record the current active references to `src/view`, `tests/views`, and `view/` includes outside archived OpenSpec history.
- [x] 1.2 Confirm the current component category directories under `src/view/` and matching test directories under `tests/views/`.
- [x] 1.3 Confirm root-level `app/model`, `app/view`, and `app/viewmodel` remain application-layer directories and are not part of `fluent_qt_lib`.

## 2. Source And Test Tree Rename

- [x] 2.1 Move `src/view/` to `src/components/` without changing component class names or C++ namespaces.
- [x] 2.2 Move `tests/views/` to `tests/components/` while preserving test source names and category structure.
- [x] 2.3 Remove any empty placeholder directories left behind by the rename.
- [x] 2.4 Update repository source groups, CMake paths, and any explicit test output paths to use the renamed component tree.

## 3. Include And Build Wiring

- [x] 3.1 Update project-owned source and test includes from `view/...` to `components/...`.
- [x] 3.2 Update foundation and overlay includes to `components/foundation/...` and `components/foundation/overlay/...`.
- [x] 3.3 Verify no permanent forwarding headers under `src/view/` are needed or left behind.
- [x] 3.4 Verify `fluent_qt_lib` compiles component, design, compatibility, utility, and resource code without compiling root-level `app/` code.
- [x] 3.5 Preserve all existing `view::...` namespaces and `Q_DECLARE_METATYPE` declarations.

## 4. Tests And VisualCheck Paths

- [x] 4.1 Update tests CMake registration from `tests/views` to `tests/components`.
- [x] 4.2 Preserve `add_qt_test_module(...)` usage for all component test targets.
- [x] 4.3 Verify CTest labels still include target, component, category, `qt`, `unit`, and semantic labels such as `visual` and `interactive`.
- [x] 4.4 Update direct VisualCheck and snapshot command examples to use `tests/components` binary paths.
- [x] 4.5 Preserve `SKIP_VISUAL_TEST=1` automated skip behavior and direct binary VisualCheck behavior.

## 5. Documentation And OpenSpec Synchronization

- [x] 5.1 Update README and AGENTS.md component path guidance to `src/components` and `tests/components`.
- [x] 5.2 Update `docs/development/` and `docs/architecture/` active guidance from view paths to component paths.
- [x] 5.3 Update live OpenSpec specs under `openspec/specs/` so active requirements use `src/components`, `tests/components`, and `components/...` include paths.
- [x] 5.4 Leave archived OpenSpec changes as historical records unless an archived reference is copied into active guidance.
- [x] 5.5 Re-run repository searches for `src/view`, `tests/views`, and active `view/` include paths, and classify any remaining hits as archived history, namespace references, or explicit migration notes.

## 6. Validation

- [x] 6.1 Run `openspec validate rename-view-to-components --strict`.
- [x] 6.2 Run `openspec validate --all --strict` after live spec synchronization.
- [x] 6.3 Configure with `cmake --preset vcpkg-osx` if CMake source discovery or test discovery changed.
- [x] 6.4 Build `fluent_qt_lib`.
- [x] 6.5 Build focused representative component test targets from foundation, basicinput, navigation, dialogs_flyouts, collections, and windowing.
- [x] 6.6 Run focused CTest label validation for representative component targets with VisualCheck skipped.
- [x] 6.7 Document any unrelated pre-existing failures separately from migration regressions.
