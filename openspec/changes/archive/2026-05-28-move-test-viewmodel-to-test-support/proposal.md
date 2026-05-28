## Why

`src/viewmodel/ViewModel` is currently a test-oriented binding helper, but the root source glob compiles it into `fluent_qt_lib` as if it were production API. Moving or removing it from `src/` keeps runtime modules focused on reusable controls, design tokens, compatibility shims, and utilities while leaving test fixtures in the test tree.

## What Changes

- Remove the current test-only `ViewModel` implementation from the production `src/viewmodel/` module.
- If a reusable binding model is still useful for tests, rehome it under `tests/support/` with an explicit test-support name such as `BindingTestModel` or `PropertyBindingTestModel`.
- Update CMake/test wiring so shared test support can own the helper without adding it to `fluent_qt_lib`.
- Update source comment guidance so it no longer treats `src/viewmodel/` as an active production module after the move.
- Keep existing component and `QMLPlus` behavior unchanged; this is a source organization cleanup, not a binding feature change.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `test-infrastructure`: shared Qt test support may own reusable test fixture models, and production sources must not carry test-only helper models.
- `source-comment-style`: source comment guidance must reflect the removal of `src/viewmodel/` as a production source area and document test-support helper comments under test infrastructure guidance instead.

## Impact

- Affected source: `src/viewmodel/ViewModel.h`, `src/viewmodel/ViewModel.cpp`, root source glob behavior by removal from `src/`.
- Affected tests/support: optional new shared test binding model under `tests/support/`.
- Affected docs/specs: `docs/development/comment-style.md`, `openspec/specs/test-infrastructure/spec.md`, and `openspec/specs/source-comment-style/spec.md`.
- Validation should include `fluent_qt_lib` build and focused `QMLPlus`/test support targets to confirm MOC, bindings, and test helper wiring remain valid.
