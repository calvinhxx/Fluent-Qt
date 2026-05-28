## 1. Inventory

- [x] 1.1 Inventory all includes and references to `view/FluentElement.h`, `view/QMLPlus.h`, `FluentElement_p.h`, and `view/internal/overlay/*`.
- [x] 1.2 Confirm no existing component source depends directly on private `FluentElement` internals.
- [x] 1.3 Confirm the current build uses recursive source discovery or update source lists if explicit file paths are introduced.

## 2. Foundation Layout

- [x] 2.1 Create `src/view/foundation/`, `src/view/foundation/private/`, and `src/view/foundation/overlay/`.
- [x] 2.2 Move `FluentElement.h`, `FluentElement.cpp`, `QMLPlus.h`, and `QMLPlus.cpp` into `src/view/foundation/`.
- [x] 2.3 Move `FluentElement_p.h` into `src/view/foundation/private/`.
- [x] 2.4 Move overlay helper headers from `src/view/internal/overlay/` into `src/view/foundation/overlay/` without changing the `view::overlay` namespace.

## 3. Compatibility and Includes

- [x] 3.1 Add forwarding headers at `src/view/FluentElement.h` and `src/view/QMLPlus.h`.
- [x] 3.2 Add forwarding headers for the existing `src/view/internal/overlay/*.h` include paths.
- [x] 3.3 Update moved implementation files to include canonical foundation/private paths.
- [x] 3.4 Update project-owned overlay users touched by the migration to prefer `view/foundation/overlay/...` include paths.
- [x] 3.5 Update focused tests or add lightweight compile coverage so legacy forwarding headers remain verified.

## 4. Documentation

- [x] 4.1 Update `docs/architecture/overlay-behavior.md` to describe `src/view/foundation/overlay/` as the canonical helper location.
- [x] 4.2 Update development guidance that describes component foundation files, private headers, or canonical include style.
- [x] 4.3 Avoid project-folder-specific naming in new docs and comments.

## 5. Validation

- [x] 5.1 Run `openspec validate organize-view-foundation-structure --strict`.
- [x] 5.2 Build focused test targets for foundation and overlay users: `test_anchor_layout`, `test_fluent_element`, `test_qml_plus`, `test_popup`, `test_flyout`, `test_combo_box`, and `test_drawer_view`.
- [x] 5.3 Run the same focused tests with `SKIP_VISUAL_TEST=1` where VisualCheck cases are present.
- [x] 5.4 Inspect `git diff --stat` and confirm the change is structural, with no intentional runtime overlay behavior changes.
