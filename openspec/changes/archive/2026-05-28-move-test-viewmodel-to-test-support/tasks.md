## 1. Source Ownership Cleanup

- [x] 1.1 Confirm whether any project-owned source or test includes `src/viewmodel/ViewModel.h`.
- [x] 1.2 Remove `src/viewmodel/ViewModel.h` and `src/viewmodel/ViewModel.cpp` if the helper is unused.
- [x] 1.3 If reuse is needed, move the helper to `tests/support/BindingTestModel.h/.cpp` or an equivalent explicit test-support name under `tests::support`.
- [x] 1.4 Ensure any moved helper is built only into test binaries and not into `fluent_qt_lib`.

## 2. Test Integration

- [x] 2.1 Decide whether `tests/views/TestQMLPlus.cpp` should keep its local `QMLPlusViewModel` or use a shared test-support helper.
- [x] 2.2 Update test includes and CMake support source wiring if a shared helper is retained.
- [x] 2.3 Keep specialized component-specific mock models local when shared support would make the test less clear.

## 3. Documentation And Specs

- [x] 3.1 Update `docs/development/comment-style.md` to remove `src/viewmodel/` from production source module guidance.
- [x] 3.2 Add or update guidance that test-only helper model comments belong under `tests/support/` or local test fixtures.
- [x] 3.3 Sync the durable `test-infrastructure` and `source-comment-style` spec updates when archiving the change.

## 4. Validation

- [x] 4.1 Run `openspec validate move-test-viewmodel-to-test-support --strict`.
- [x] 4.2 Build `fluent_qt_lib` to confirm production source glob/MOC behavior remains valid.
- [x] 4.3 Build and run focused tests for `test_qml_plus` and `test_qt_test_environment` with VisualCheck skipped.
- [x] 4.4 Verify `rg -n "src/viewmodel|ViewModel.h|src/viewmodel/ViewModel" src tests docs openspec/specs` no longer reports stale live references except archived OpenSpec history if applicable.
