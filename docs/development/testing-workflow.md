# Testing Workflow

Use this workflow when choosing Qt/GTest/CTest validation commands, filtering
tests by CTest labels, running or skipping VisualCheck tests, adding test
targets with `add_qt_test_module`, or synchronizing new component directories
with OpenSpec, README, CMake, and agent instructions.

## CTest Labels

- Register Qt view tests with `add_qt_test_module(test_<name> Test<Name>.cpp
  [extra sources...])`.
- The helper applies these labels to discovered tests: `qt`, `unit`,
  source-directory labels, target name, and component name.
- Discovered tests also receive conservative semantic labels based on test-name
  tokens: `visual`, `interactive`, `animation`, `slow`, `platform_windows`, and
  `platform_macos`. VisualCheck tests receive both `visual` and `interactive`.
- Use anchored label filters so substring matches do not select adjacent
  components:

```bash
ctest --preset vcpkg-osx -L '^navigation$'
ctest --preset vcpkg-osx -L '^date_time$'
ctest --preset vcpkg-osx -L '^test_date_picker$'
ctest --preset vcpkg-osx -N -L '^visual$'
ctest --preset vcpkg-osx -L '^animation$' --output-on-failure
ctest --preset vcpkg-osx -N -L '^platform_macos$'
```

- Use established tokens in new test names when a semantic label should apply:
  `VisualCheck`, `Interactive`, `Animation`/`Animated`, `Slow`, `Windows`/`Win32`,
  or `MacOS`/`MacOs`/`Darwin`/`Cocoa`.
- Semantic labels are additive. VisualCheck tests still keep `qt`, `unit`,
  category, target, and component labels.

## VisualCheck

- Automated CTest runs inject `SKIP_VISUAL_TEST=1`; VisualCheck tests should
  skip in that mode.
- For manual UI review, run the test binary directly:

```bash
./build/vcpkg-osx/tests/views/<category>/<test_target> --gtest_filter="*VisualCheck*"
```

- For deterministic snapshot generation, run a migrated VisualCheck binary with
  `VISUAL_SNAPSHOT=1`:

```bash
VISUAL_SNAPSHOT=1 ./build/vcpkg-osx/tests/views/textfields/test_label --gtest_filter="LabelTest.VisualCheck"
```

- Snapshot files are written to `build/vcpkg-osx/visual/` using stable names such
  as `<target>__<suite>__<test>[_variant].png`. Repeated runs overwrite the same
  file. This phase only verifies that a non-empty PNG is generated; it does not
  perform pixel diffing or baseline approval.
- If both `SKIP_VISUAL_TEST=1` and `VISUAL_SNAPSHOT=1` are set, skip behavior wins
  and no snapshot should be generated.

- VisualCheck tests must guard on `SKIP_VISUAL_TEST`, show the test window, and
  block with `qApp->exec()` until the window closes unless they branch to the
  shared snapshot helper for `VISUAL_SNAPSHOT=1`.
- Do not replace VisualCheck event-loop blocking with `QTest::qWait()`.
- See [Qt Component Test Conventions](qt-component-test-conventions.md) for
  VisualCheck authoring rules.
- See [Visual Review](visual-review.md) for manual UI review workflow.

## Component Directories

- `src/view/` should only contain directories with implemented components.
- Do not keep empty placeholder directories.
- Create a new component directory only when the first component in that
  category lands.
- When adding or removing a component directory, update the relevant OpenSpec
  specs, README overview, tests CMake, and `.github/copilot-instructions.md`.

## Validation Defaults

- Configure with `cmake --preset vcpkg-osx` when CMake structure or test
  discovery changes.
- Build focused targets with `cmake --build --preset vcpkg-osx --target
  <test_target>`.
- Prefer focused CTest label runs after changing a test target:

```bash
ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure
```

- For OpenSpec-driven changes, also run the relevant `openspec validate ...
  --strict`; telemetry errors from `edge.openspec.dev` are noise if the command
  exits successfully.
