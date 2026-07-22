# Testing Workflow

Use this workflow when choosing Qt/GTest/CTest validation commands, filtering
tests by CTest labels, running or skipping VisualCheck tests, adding test
targets with `add_qt_test_module`, or synchronizing new component directories
with README, CMake, and agent instructions.

## CTest Labels

- Register Qt component tests with `add_qt_test_module(test_<name> Test<Name>.cpp
  [extra sources...])`.
- The helper applies these labels to discovered tests: `qt`, `unit`,
  source-directory labels, target name, component name, and validation-tier
  labels.
- `ci_fast` is intentionally tiny and reserved for stable core checks used by
  the default GitHub Actions path.
- `ci_full` is the curated GitHub Actions full-validation subset. It is broad
  enough to cover core helpers, representative components, platform-sensitive
  areas, and app build smoke coverage, but it is not the exhaustive local test
  set. Keep this target list small enough for a cold macOS arm64 runner.
- `local_full` is the exhaustive non-manual Qt/GTest validation set for local
  host runs.
- `manual_visual` identifies tests that must be reviewed by running the binary
  directly. `local_desktop` identifies tests that need a real windowing desktop
  rather than the CI offscreen platform.
- Discovered tests also receive conservative semantic labels based on test-name
  tokens: `visual`, `interactive`, `animation`, `slow`, `platform_windows`,
  `platform_macos`, and `design_macos`. VisualCheck tests receive `visual`,
  `interactive`, `manual_visual`, and `local_desktop`.
- Use anchored label filters so substring matches do not select adjacent
  components:

```bash
ctest --preset vcpkg-osx -L '^navigation$'
ctest --preset vcpkg-osx -L '^date_time$'
ctest --preset vcpkg-osx -L '^test_date_picker$'
ctest --preset vcpkg-osx -N -L '^ci_fast$'
ctest --preset vcpkg-osx -L '^ci_full$' -LE '^(manual_visual|local_desktop)$' --output-on-failure
ctest --preset vcpkg-osx -L '^local_full$' --output-on-failure
ctest --preset vcpkg-osx -N -L '^visual$'
ctest --preset vcpkg-osx -N -L '^manual_visual$'
ctest --preset vcpkg-osx -N -L '^local_desktop$'
ctest --preset vcpkg-osx -L '^animation$' --output-on-failure
ctest --preset vcpkg-osx -N -L '^platform_macos$'
```

Linux runs use the same anchored filters. The `vcpkg-linux` and
`vcpkg-linux-arm64` test presets exclude `local_desktop` by default; use the
matching `*-local-desktop` preset to list tests that need a real X11 or Wayland
desktop session:

```bash
ctest --preset vcpkg-linux -L '^ci_full$' --output-on-failure
ctest --preset vcpkg-linux-local-desktop -N
ctest --preset vcpkg-linux-arm64-local-desktop -N
```

- High-DPI smoke tests have the `high_dpi` label and run at 110%, 125%, 150%,
  175%, 200%, and 300% offscreen scale factors. Build `test_high_dpi` and run
  the anchored label on any host:

```bash
cmake --build --preset vcpkg-linux --target test_high_dpi
ctest --preset vcpkg-linux -L '^high_dpi$' --output-on-failure
```

See [High-DPI Workflow](high-dpi-workflow.md) for application integration and
real mixed-monitor review.

- Use established tokens in new test names when a semantic label should apply:
  `VisualCheck`, `Interactive`, `Animation`/`Animated`, `Slow`, `Windows`/`Win32`,
  or `MacOS`/`MacOs`/`Darwin`/`Cocoa`.
- Semantic labels are additive. VisualCheck tests still keep `qt`, `unit`,
  category, target, and component labels.

## Validation Tiers

- GitHub Actions `matrix=fast` is the default PR/push validation tier. It runs
  the narrow `ci_fast` set on Linux x64 and Windows x64, then compiles the
  library on macOS arm64. Native Linux and Windows ARM64 execution stays in the
  scheduled/manual full tier instead of running for every pull request.
- Pull requests that change only Markdown, `docs/`, `site/`, license, or issue
  template files skip the native build matrix. The stable `CI Gate` job still
  reports success, so branch protection can require one check for every pull
  request without spending hosted-runner time on documentation-only changes.
- GitHub Actions `matrix=full` is the scheduled/manual CI matrix. macOS arm64
  remains the broadest macOS test lane for the curated `ci_full` subset; Linux
  covers Ubuntu 22.04 x64 and ARM64 with distro Qt 6.2.x plus official Qt
  5.15.2 `gcc_64` on x64; macOS x64 is a Gallery build smoke; Windows lanes
  cover targeted x64 and native ARM64 platform tests, Qt 5.15 API, and the
  established ARM64 cross-built installer package. The full run uploads both
  `fluent-qt-gallery-windows-arm64-installer` and
  `fluent-qt-gallery-linux-arm64-deb` for VM review.
- The macOS arm64 full lane uses a limited build parallelism to avoid runner
  memory pressure while compiling and linking multiple Qt/GTest binaries.
- CI build target selection is centralized in CMake:
  `fluent_qt_ci_fast_tests` builds only the fast API/environment test binaries,
  `fluent_qt_ci_full_tests` builds the selected CI-full test binaries, and
  `fluent_qt_ci_windows_platform_tests` builds the focused Windows platform set;
  `fluent_qt_all_tests` builds every registered Qt/GTest binary for local host
  validation. Keep workflow YAML on these aggregate targets instead of
  duplicating long target lists there.
- When adding a new `add_qt_test_module` target, decide whether it belongs in
  `FLUENT_QT_CI_FAST_TARGETS`, `FLUENT_QT_CI_FULL_TARGETS`, or local-only
  `fluent_qt_all_tests`.
- Local host full validation means configuring, building, and running all CTest
  non-manual tests for the current host preset. VisualCheck tests stay in
  `manual_visual`; use `-LE '^local_desktop$'` when running on a headless host:

```bash
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx --target fluent_qt_all_tests --parallel
ctest --preset vcpkg-osx -L '^local_full$' --output-on-failure --timeout 180
```

```powershell
cmake --preset vcpkg-windows
cmake --build --preset vcpkg-windows --target fluent_qt_all_tests --parallel
ctest --preset vcpkg-windows -L '^local_full$' --output-on-failure --timeout 180
```

On native Windows ARM64 with the Qt `msvc2022_arm64` kit, substitute
`vcpkg-windows-arm64` for `vcpkg-windows`. An x64-hosted ARM64 cross-build can
compile the same targets but cannot execute this test preset.

```bash
cmake --preset vcpkg-linux
cmake --build --preset vcpkg-linux --target fluent_qt_all_tests --parallel
ctest --preset vcpkg-linux -L '^local_full$' --output-on-failure --timeout 180
```

On Linux, both architecture-specific test presets intentionally exclude
`local_desktop`, so these commands run the headless-safe `local_full` subset.
Run the matching `*-local-desktop -N` preset separately to discover tests that
need a real desktop, after building the specific test target under review or
the aggregate `fluent_qt_all_tests` target. Automated CTest runs inject
`SKIP_VISUAL_TEST=1`; for real desktop behavior, run the target binary directly
without `SKIP_VISUAL_TEST` and set `QT_QPA_PLATFORM=xcb` or `wayland` when your
session needs an explicit platform.

See [Linux Workflow](linux-workflow.md) for the desktop Linux portability target,
Ubuntu 22.04 reference dependencies, local desktop commands, Qt 5.15.2
official-kit validation, and optional WSL2 filesystem guidance.

## VisualCheck

- Automated CTest runs inject `SKIP_VISUAL_TEST=1`; VisualCheck tests should
  skip in that mode.
- For manual UI review, run the test binary directly:

```bash
./build/vcpkg-osx/tests/components/<category>/<test_target> --gtest_filter="*VisualCheck*"
```

On Linux, use the corresponding `build/vcpkg-linux/...` or
`build/vcpkg-linux-arm64/...` binary path in an X11 or Wayland desktop session.
WSLg is also suitable as an optional local validation host. Run these binaries
directly so `SKIP_VISUAL_TEST` is not inherited from CTest.

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
  tests CMake, and `AGENTS.md`.

## Validation Defaults

- Configure with `cmake --preset vcpkg-osx` when CMake structure or test
  discovery changes.
- Build focused targets with `cmake --build --preset vcpkg-osx --target
  <test_target>`.
- Prefer focused CTest label runs after changing a test target:

```bash
ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure
```
