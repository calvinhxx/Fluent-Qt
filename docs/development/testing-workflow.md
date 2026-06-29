# Testing Workflow

Use this workflow when choosing Qt/GTest/CTest validation commands, filtering
tests by CTest labels, running or skipping VisualCheck tests, adding test
targets with `add_qt_test_module`, or synchronizing new component directories
with README, CMake, and agent instructions.

## CTest Labels

- Register Qt component tests with `add_qt_test_module(test_<name> Test<Name>.cpp
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
./build/vcpkg-osx/tests/components/<category>/<test_target> --gtest_filter="*VisualCheck*"
```

- For deterministic snapshot generation, run a migrated VisualCheck binary with
  `VISUAL_SNAPSHOT=1`:

```bash
VISUAL_SNAPSHOT=1 ./build/vcpkg-osx/tests/components/textfields/test_label --gtest_filter="LabelTest.VisualCheck"
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

## App Visual Geometry Verification

See [App Visual Geometry Verification](app-visual-geometry-verification.md) for
the canonical app-only workflow.

- Use geometry assertions for application-level UI under `app/` when layout
  contracts can be expressed as numbers: center alignment, fixed sizes, edge
  offsets, spacing, containment, and logical icon dimensions.
- Do not apply this as a blanket rule to stable reusable components under
  `src/components/`; keep component tests focused on their existing API and
  behavior contracts unless a component-specific visual issue explicitly needs
  geometry evidence.
- Give visually important app-owned widgets stable object names before writing
  geometry tests. Use scoped names such as `GalleryTitleBar.BackButton` and
  `GalleryTitleBar.SearchBox` so tests do not depend on implementation-local
  pointers.
- Use helpers from `tests/support/VisualGeometryTestUtils.h` for named-widget
  lookup, geometry assertions, and opt-in dumps from app/gallery tests.
  Assertion failures should be readable without opening a screenshot.
- Keep geometry dump output disabled during normal CTest runs. Enable it only
  when investigating an app layout issue:

```bash
FLUENT_QT_GEOMETRY_DUMP=1 ctest --preset vcpkg-osx -L '^test_gallery_shell_framework$' --output-on-failure
```

- Use VisualCheck or screenshots after geometry checks for subjective polish:
  typography, color, material, animation, perceived balance, and brand fidelity.

## Component Directories

- `src/components/` should only contain directories with implemented components.
- Do not keep empty placeholder directories.
- Create a new component directory only when the first component in that
  category lands.
- When adding or removing a component directory, update the README overview,
  tests CMake, and `.github/copilot-instructions.md`.

## Validation Defaults

- Configure with `cmake --preset vcpkg-osx` when CMake structure or test
  discovery changes.
- Build focused targets with `cmake --build --preset vcpkg-osx --target
  <test_target>`.
- Prefer focused CTest label runs after changing a test target:

```bash
ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure
```
