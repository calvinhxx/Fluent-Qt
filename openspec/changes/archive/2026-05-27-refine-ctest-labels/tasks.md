## 1. CTest Label Infrastructure

- [x] 1.1 Inspect current `tests/CMakeLists.txt` discovery output and confirm how `${module_name}_TESTS` names are formed after `gtest_discover_tests`.
- [x] 1.2 Update `add_qt_test_module` generated label include script to iterate discovered tests and preserve existing base labels per test.
- [x] 1.3 Add conservative per-test semantic label rules for `visual`, `interactive`, `animation`, `slow`, `platform_windows`, and `platform_macos`.
- [x] 1.4 Keep `SKIP_VISUAL_TEST=1` on discovered tests and verify direct binary execution behavior is unchanged.

## 2. Representative Coverage

- [x] 2.1 Ensure at least one discovered VisualCheck test receives both `visual` and `interactive` labels.
- [x] 2.2 Ensure at least one representative animation-named test receives the `animation` label without renaming broad unrelated tests.
- [x] 2.3 Add or identify representative tests for `slow`, `platform_windows`, and `platform_macos` labels if existing names do not exercise those rules.
- [x] 2.4 Verify base labels such as `qt`, `unit`, category, target, and component remain present on semantically labeled tests.

## 3. Documentation

- [x] 3.1 Update README test workflow documentation with anchored label examples for `visual`, `interactive`, `animation`, `slow`, `platform_windows`, and `platform_macos`.
- [x] 3.2 Update repository test workflow guidance so future tests follow the naming and label-filter conventions.

## 4. Validation

- [x] 4.1 Reconfigure with `cmake --preset vcpkg-osx` if CTest discovery metadata needs regeneration.
- [x] 4.2 Run focused CTest metadata checks, including `ctest --preset vcpkg-osx -N -L '^visual$'` and representative filters for the other semantic labels.
- [x] 4.3 Run a focused non-visual CTest slice and confirm VisualCheck tests still skip under automated CTest.
- [x] 4.4 Run `openspec validate refine-ctest-labels --strict` and confirm `openspec status --change refine-ctest-labels` reports apply-ready.