## Context

The current repository has a small `src/viewmodel/ViewModel` class with observable properties such as `text`, `enabled`, `title`, and `visible`. Its header documents it as a helper for UT/property-binding scenarios, but the top-level CMake source glob includes every file under `src/`, so the helper is compiled into `fluent_qt_lib`.

That makes the source tree communicate the wrong boundary:

```text
src/viewmodel/ViewModel
    purpose: test binding helper
    build result: production library source

tests/support
    purpose: shared test runtime helpers
    build result: linked only into test binaries
```

The existing `tests/support` area already owns Qt-aware GoogleTest process setup and VisualCheck helpers. If a shared property-binding model is useful beyond one test file, that is the right home.

## Goals / Non-Goals

**Goals:**

- Remove test-only model code from the production `src/` tree.
- Keep `fluent_qt_lib` focused on reusable component/runtime code.
- Preserve a path for shared test binding models under `tests/support/` when more than one test needs the same helper.
- Update comment-style guidance so it no longer describes `src/viewmodel/` as an active production source area.
- Validate that the move does not change `QMLPlus` binding behavior or Qt test registration behavior.

**Non-Goals:**

- Do not introduce a production MVVM framework or formal runtime data-binding layer.
- Do not redesign `QMLPlus` property binding.
- Do not migrate unrelated local test-only mock classes unless they directly benefit from the shared helper.
- Do not change public component APIs.

## Decisions

### Decision: Treat the current `ViewModel` as test support, not production API

The current class exists to simplify UT property binding, and no production component depends on it. The implementation should either be deleted if unused or moved into `tests/support/` with a test-specific name.

Alternative considered: keep `src/viewmodel` and polish comments. That preserves the status quo but keeps a test fixture in the runtime library and makes the `src/` module map less professional.

### Decision: Prefer explicit test-support naming

If retained, the helper should use a name such as `BindingTestModel` or `PropertyBindingTestModel` in a `tests::support` namespace. Avoid a generic production-sounding name like `ViewModel`.

Alternative considered: keep the class name `ViewModel` after moving it. That would reduce edit churn, but the name still suggests a reusable product-layer abstraction rather than a fixture.

### Decision: Keep test-specific models local when they are specialized

`TestQMLPlus.cpp` currently has a local `QMLPlusViewModel` with `statusText` and `isOnline` properties. During implementation, decide whether the shared helper genuinely improves that test. If the test needs specialized property names, the local mock can remain local.

Alternative considered: force all tests onto one generic helper. That risks making tests less readable and couples unrelated component scenarios to one broad fixture object.

### Decision: Remove `src/viewmodel` from source-comment scope after migration

The source-comment contract should describe active production areas. Once the test-only model is removed from `src/`, `docs/development/comment-style.md` and `source-comment-style` requirements should point test helper comments to test support guidance instead of preserving a stale `src/viewmodel` row.

## Risks / Trade-offs

- Test helper is accidentally made unavailable to a test target -> ensure `add_qt_test_module(...)` includes `tests/support` sources or headers as needed and build focused targets.
- Moving a `QObject` with `Q_OBJECT` into shared test support can affect MOC wiring -> prefer adding the helper source through `FLUENT_QT_QT_GTEST_SUPPORT_SOURCES` or keeping it header-only only if the MOC path is verified.
- Deleting the helper may be cleaner than moving it, but future binding tests may reintroduce duplicate models -> only move it if there is immediate reuse or a clear support role.
- Documentation can drift if `src/viewmodel` is removed but docs still mention it -> update `comment-style.md` and source-comment spec in the same change.

## Migration Plan

1. Inspect whether any test includes `src/viewmodel/ViewModel.h`.
2. If unused, remove `src/viewmodel/ViewModel.h` and `src/viewmodel/ViewModel.cpp`; if reused or worth preserving, move it to `tests/support/BindingTestModel.h/.cpp`.
3. Wire any moved helper into test builds without adding it to `fluent_qt_lib`.
4. Update source-comment guidance and OpenSpec main specs to remove stale `src/viewmodel` production scope.
5. Validate `fluent_qt_lib` and focused tests such as `test_qml_plus` and `test_qt_test_environment`.
