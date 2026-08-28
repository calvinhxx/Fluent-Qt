# Testing Workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[← Build Workflow](build-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Qt Component Test Conventions →](qt-component-test-conventions.md)
<!-- docs-nav:top:end -->

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
- `visual_gate` is the opt-in representative Light/Dark/RTL snapshot compare
  (three checked-in PNGs). It is not part of `ci_fast`, `ci_full`, or
  `local_full`.
- Discovered tests also receive conservative semantic labels based on test-name
  tokens: `visual`, `interactive`, `animation`, `slow`, `platform_windows`,
  and `platform_macos`. VisualCheck tests receive `visual`,
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
ctest --preset vcpkg-osx -N -L '^visual_gate$'
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

## Removing a Test Matrix Axis

When a public mode, platform abstraction, or product variant is removed, do
not delete its whole parameterized fixture until every assertion is classified:

1. Move design-neutral behavior, signal, ownership, and no-op assertions into
   the component's normal `Contract_*` fixture.
2. Keep representative Fluent Light/Dark state distinctions and painted
   geometry/color invariants when the deleted matrix was their only automated
   coverage.
3. Delete only assertions that require the removed axis or its branch-specific
   output. Do not retain a one-value compatibility matrix merely to preserve
   the old fixture shape.
4. Compare the deleted and remaining test names, then confirm that no affected
   component is left with only a skipped/manual `VisualCheck` unless another
   focused target owns its executable contract.

Run the affected component labels and `visual_gate` after the extraction.
VisualCheck remains a separate manual review surface; it does not replace
automated state and pixel invariants.

## Validation Tiers

The public [CI workflow](../../.github/workflows/ci.yml) is an orchestration
layer. It classifies changed paths, selects `fast` or `full`, invokes three
reusable validation modules, and owns only the stable `CI Gate` and
`Release ready` checks:

- [C++ CI module](../../.github/workflows/ci-cpp.yml) owns native Qt builds,
  CTest, CMake consumer integration, native packages, and the validated
  [C++ matrix catalog](../../.github/ci-cpp-matrix.json).
- [PySide6 CI module](../../.github/workflows/ci-python.yml) owns binding
  generation, compatibility baselines, native Python wheels, clean-environment
  tests, and the optional publishable wheel matrix. Fast CI clean-installs the
  core wheel on its Qt 6.2 compatibility lanes. Standard full CI adds Gallery,
  typing, visible-example, and native-window acceptance on representative
  lanes. On an untagged release-ready `main` commit, Release Candidate owns the
  macOS ARM64 CPython 3.11 representative so full CI does not compile it twice.
  Scheduled and manual full runs keep that lane. `python_release_bundle=true`
  additionally builds every declared release wheel, runs the complete binding
  suite on the six extended-acceptance representatives, and clean-installs and
  smoke-tests every other ABI wheel. It then runs manylinux repair and audit,
  assembles the immutable 18-wheel bundle, and reports six explicit
  platform/architecture checks in the Actions UI. The module also owns
  [the wheel matrix](../../bindings/pyside6/wheel-matrix.json).
  Python release scenarios are queued critical-path first: Windows ARM64 with
  CPython 3.11 precedes the other extended-acceptance representatives, and
  secondary CPython rows fill runner capacity afterward. This changes only
  scheduling order; it does not reduce the supported or validated matrix.
- [WebAssembly CI module](../../.github/workflows/ci-wasm.yml) owns the pinned
  Qt 6.9.3 `wasm_singlethread` and Emscripten 3.1.70 toolchain, builds Hello
  World and the C++ Gallery, runs the fast/full Chromium smoke, and stages the
  Pages payload consumed by [the Pages workflow](../../.github/workflows/pages.yml).
  A `main` CI run passes that artifact directly to the reusable Pages deploy;
  the manual Pages entry rebuilds it only for recovery.

Do not add compiler, SDK, package-manager, wheel, or platform steps to the
orchestrator. Add them to the owning reusable workflow and update its catalog.
`.github/scripts/validate-ci-workflow-boundaries.py` enforces that separation.
All three modules upload artifacts into the caller's workflow run. Standard
desktop releases therefore remain independent of PyPI publishing, while an
opted-in full run exposes the immutable bundle to the Python release workflow.
The separate [Release Candidate workflow](../../.github/workflows/release-candidate.yml)
runs for an untagged version on `main`: it invokes the reusable desktop
packaging module and bundle-enabled PySide6 module in parallel, then emits
`Release Candidate ready` only after both commit-bound manifests pass. The tag
workflow promotes those artifacts; it does not rebuild them. On an untagged
release-ready `main` commit, the candidate also owns the fixed macOS ARM64
Python representative omitted from the simultaneous full CI run.

- GitHub Actions `matrix=fast` is the default pull-request and manual validation
  tier. It runs
  the narrow `ci_fast` set on Linux x64 and Windows x64, then compiles the
  library on macOS arm64. Native Linux and Windows ARM64 execution stays in the
  scheduled/manual full tier instead of running for every pull request.
- Pull requests that change only Markdown, `docs/`, `site/`, license, or issue
  template files skip the native build matrix. The stable `CI Gate` job still
  reports success, so branch protection can require one check for every pull
  request without spending hosted-runner time on documentation-only changes.
- GitHub Actions `matrix=full` runs automatically after pushes to `main` and on
  the weekly schedule, and is available manually. Weekly runs also enable the
  complete Python release bundle; ordinary `main` and manual full runs keep it
  disabled unless `python_release_bundle=true` is selected. The automatic
  Release Candidate run owns the pre-tag publication bundle instead. It also
  owns the macOS ARM64 Python representative for that release commit; manual
  and scheduled full runs retain the representative themselves. macOS arm64
  remains the broadest native C++ lane for the curated `ci_full` subset; Linux
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
  `fluent_qt_contract_tests` builds the focused component-contract binaries, and
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
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target fluent_qt_all_tests
ctest --preset vcpkg-osx -L '^local_full$' --output-on-failure --timeout 180
```

```powershell
cmake --preset vcpkg-windows
python tools/dev/fluent_qt_build.py --preset vcpkg-windows --target fluent_qt_all_tests
ctest --preset vcpkg-windows -L '^local_full$' --output-on-failure --timeout 180
```

On native Windows ARM64 with the Qt `msvc2022_arm64` kit, substitute
`vcpkg-windows-arm64` for `vcpkg-windows`. An x64-hosted ARM64 cross-build can
compile the same targets but cannot execute this test preset.

When running a Windows GTest binary outside Qt Creator or a configured CMake
preset environment, validate the loader environment before starting any test
batch:

- Build one process `Path` containing the selected Qt `bin` directory and the
  matching vcpkg Debug/Release runtime directories. Do not keep competing
  process-level `Path` and `PATH` values.
- Run exactly one focused binary with `--gtest_list_tests` first. Continue only
  when that probe exits successfully.
- Automation launchers must suppress Windows loader and fault-reporting dialogs
  (for example with the documented Win32 process error-mode flags) so a missing
  dependency fails in the terminal rather than blocking the desktop.
- After the probe, execute one test binary per process and combine related
  cases with one `--gtest_filter`. Do not launch an unverified CTest batch from
  an inherited shell environment.

```bash
cmake --preset vcpkg-linux
python3 tools/dev/fluent_qt_build.py --preset vcpkg-linux --target fluent_qt_all_tests
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

## Component Contract Baseline

Tests whose names contain `Contract` receive the `contract` label. A desired
behavior that is not yet implemented is named `DISABLED_Contract_*` and also
receives `known_contract_gap`. Known gaps are excluded from `local_full`,
`ci_fast`, and `ci_full`. The current Phase 1 suite has no disabled contract
test; the naming and label remain available for future target-behavior work.

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-linux --target fluent_qt_contract_tests
ctest --preset vcpkg-linux -L '^contract$' -LE '^known_contract_gap$' --output-on-failure
ctest --preset vcpkg-linux -N -L '^known_contract_gap$'
```

If a future known gap is added, run it explicitly with the owning GTest binary
and `--gtest_also_run_disabled_tests`. Run one at a time because a lifetime or
layout gap may terminate the current process. Current accepted contracts and
deferred decisions are in
[Component Contract Baseline](component-contract-baseline.md).

Linux also provides a focused ASan/UBSan preset:

```bash
cmake --preset vcpkg-linux-sanitized
python3 tools/dev/fluent_qt_build.py --preset vcpkg-linux-sanitized --target fluent_qt_contract_tests
ctest --preset vcpkg-linux-sanitized --output-on-failure
```

`FLUENT_QT_ENABLE_SANITIZERS` is opt-in and does not affect release or ordinary
debug builds.

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

- Snapshot files are written to `build/<preset>/visual/` using stable names such
  as `<target>__<suite>__<test>[_variant].png`. Repeated runs overwrite the same
  file. Migrated VisualCheck tests still only verify that a non-empty PNG was
  written. They are not a screenshot farm and do not compare against baselines.
- If both `SKIP_VISUAL_TEST=1` and `VISUAL_SNAPSHOT=1` are set, skip behavior wins
  and no snapshot should be generated.
- VisualCheck tests must guard on `SKIP_VISUAL_TEST`, show the test window, and
  block with `qApp->exec()` until the window closes unless they branch to the
  shared snapshot helper for `VISUAL_SNAPSHOT=1`.
- Do not replace VisualCheck event-loop blocking with `QTest::qWait()`.
- Do not convert every VisualCheck into a baseline compare. The pixel gate below
  is a separate, tiny allowlist.

## Representative visual gate

A 1.7 quality-track gate for three checked-in PNGs under
[tests/visual-baselines/](../../tests/visual-baselines/README.md):

- Button Rest/Hover/Pressed/Focus/Disabled in Light LTR
- The same Button row in Dark LTR
- Compact TreeView in Light RTL

Compare uses exact logical-pixel equality via `tests::support::compareVisualImages`.
A mismatch fails the test and writes `<name>.diff.png` next to the capture under
`build/<preset>/visual/`.

If the approved state includes keyboard focus, assign the target a stable
object name and set `VisualSnapshotOptions::focusObjectName`. The snapshot
helper activates the shown window and restores that focus immediately before
capture, so a background CTest launch cannot silently drop the focus ring.

Default automated CTest still injects `SKIP_VISUAL_TEST=1`, so discovered
`VisualGateTest.*` rows skip. `VisualGate.CompareBaselines` is the row that
diffs against the checked-in PNGs. Both are labeled `visual_gate`; the compare
entry is also `local_desktop`. Neither is in `ci_fast`, `ci_full`, or
`local_full`.

Run the gate on the approval host (macOS arm64 / `vcpkg-osx`, Fusion, bundled
fonts, `QT_SCALE_FACTOR=1`, `QT_FONT_DPI=96`):

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target test_visual_gate
ctest --preset vcpkg-osx -L '^visual_gate$' --output-on-failure
```

Equivalent direct invocation:

```bash
VISUAL_SNAPSHOT=1 VISUAL_COMPARE=1 QT_SCALE_FACTOR=1 QT_FONT_DPI=96 \
  ./build/vcpkg-osx/tests/components/test_visual_gate
```

Regenerate baselines after an intentional visual change:

```bash
VISUAL_SNAPSHOT=1 VISUAL_UPDATE_BASELINE=1 QT_SCALE_FACTOR=1 QT_FONT_DPI=96 \
  ./build/vcpkg-osx/tests/components/test_visual_gate
```

### CI limitation

Hosted runners use `QT_QPA_PLATFORM=offscreen`. Offscreen, Linux, and Windows
pixel output does not match these macOS desktop baselines (font engine, DPI,
platform plugin). The gate therefore:

- Skips on headless `offscreen` / `minimal` platforms
- Skips compare/update unless the process is macOS arm64 + Cocoa + Fusion with
  `QT_SCALE_FACTOR=1`, `QT_FONT_DPI=96`, and no per-screen scale override
- Is excluded from GitHub Actions by the `ci_fast` / `ci_full` label filters and
  by `-LE '^(manual_visual|local_desktop)$'`
- Must not be added as a default-red CI job

Keep the helper tests in `test_qt_test_environment` (synthetic image compare,
missing baseline) on the normal CTest path. Those do not render widgets against
checked-in PNGs.

See [Qt Component Test Conventions](qt-component-test-conventions.md) for
VisualCheck authoring rules.
See [Visual Review](visual-review.md) for manual UI review workflow.

## App Visual Geometry Verification

The [App Visual Geometry Verification](app-visual-geometry-verification.md)
guide owns the app-only scope, object-name convention, assertion helpers,
geometry-dump command, and the boundary between measurable layout checks and
subjective visual review.

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

<!-- docs-nav:bottom:start -->
---
[← Build Workflow](build-workflow.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Qt Component Test Conventions →](qt-component-test-conventions.md)
<!-- docs-nav:bottom:end -->
