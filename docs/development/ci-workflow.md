# CI workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[← Qt Component Test Conventions](qt-component-test-conventions.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Logging Workflow →](logging-workflow.md)
<!-- docs-nav:top:end -->

Use this guide when changing GitHub Actions, validation tiers, or release
artifact routing. Local build and test commands belong to the
[testing workflow](testing-workflow.md); publication order and authorization
belong to [release governance](release-governance.md).

## Select a tier

| Trigger | Validation | Python publication bundle |
|---|---|---|
| Pull request | `fast`; path classification may skip native builds | Disabled |
| Release pull request (`release/*` → `main`, same repository) | `full`; path classification still applies; no `Release ready` | Disabled |
| Push to `main` for a release candidate (curated notes, version not yet tagged) | `full`; no path classification | Built separately by Release Candidate |
| Other push to `main` | `full`; path classification against the previous `main` tip; WebAssembly stays enabled for Pages; no `Release ready` | Disabled |
| Weekly schedule | `full` | Enabled |
| Manual CI | `fast` by default; `full` selectable | Opt in with `python_release_bundle=true`; requires `full` |

[ci.yml](../../.github/workflows/ci.yml) and
[classify_ci_changes.py](../../.github/scripts/classify_ci_changes.py) define
the current triggers and path rules. The classifier must keep build lanes
enabled when it cannot prove a change is documentation-only. The stable
`CI Gate` check still runs when native builds are skipped.

Release pull requests run the full tier before promotion. The representative
Linux job in either tier also runs the C++ tests selected by the changed paths.
Both macOS ARM64 tiers move the installed development SDK and build an external
consumer. The Linux and Windows Python compatibility jobs install the core wheel
and run the first-window trial before full validation adds the Gallery wheel to
the same environment. Only a successful unclassified full run supplies
`Release ready`: a release-candidate push, the weekly schedule, or a manual
full run.

Other pushes to `main`, such as post-release history updates, compare the
pushed commit with the previous `main` tip. Pages deploys the website and Web
Gallery from one run, so these pushes always keep the WebAssembly lane. A new
branch, rewritten history, an identical tree, or an unavailable previous tip
runs the complete matrix. Classification does not re-check a failed earlier
`main` run; the weekly schedule and the next release candidate still run the
full matrix.

`ci_fast` and `ci_full` are curated CTest subsets. `local_full` is the exhaustive
non-manual host set; a full CI run is not an exhaustive local or native visual
review. Real desktop tests and host-bound pixel baselines remain separate.

## Keep workflow ownership explicit

| Owner | Responsibility | Matrix source |
|---|---|---|
| [ci.yml](../../.github/workflows/ci.yml) | Classify paths, choose modules, expose `CI Gate` and `Release ready` | Module outputs |
| [ci-cpp.yml](../../.github/workflows/ci-cpp.yml) | Native Qt builds, CTest, CMake consumers, desktop packages | [C++ matrix](../../.github/ci-cpp-matrix.json) |
| [ci-python.yml](../../.github/workflows/ci-python.yml) | Bindings, compatibility, wheels, clean installs, manylinux audits | [Wheel matrix](../../bindings/pyside6/wheel-matrix.json) |
| [ci-wasm.yml](../../.github/workflows/ci-wasm.yml) | Browser toolchain, Hello World, Gallery, smoke tests, Pages payload | Workflow toolchain and smoke configuration |
| [ci-site.yml](../../.github/workflows/ci-site.yml) | Native component captures, bilingual documents, API reference, website artifact | Gallery catalog and installed headers |

Compiler, SDK, package-manager, and platform steps belong in the owning module,
not the orchestrator. The Wasm module explicitly fetches and verifies the
pinned Qt accessibility sources before offline CMake configuration; its cache
key includes the Qt revision and manifest. The same cache supports the
installed consumer check. Overlay-only helper, manifest and patch changes
select the Wasm lane without PySide or native CTest selection; shared core
and package configuration changes retain their broader lanes.

Modules upload artifacts into the caller's run. The
[Pages workflow](../../.github/workflows/pages.yml) consumes the payload from
main CI; its manual entry rebuilds both the website and Gallery for recovery.

Website generation runs on pull requests as well as `main`. The planning job
regenerates the AI catalog and `site/api/catalog.json` from current source before
running their consistency checks. The website module repeats this preparation
before discovering preview recipes, then generates the
homepage and component documents from the installed headers, Gallery examples,
and editorial translations. Generated pages and captures are not tracked in
Git. Catalog snapshots remain available for tools that run without a build;
their checked-in freshness is not a prerequisite for publication. Generation
does not write commits or push back to the branch.

When preview inputs change, the website module builds the native Gallery with
Spatial and QmlBridge, then captures the real widgets under Xvfb. Each capture
is retried once before the build fails. An exact source, recipe, and Qt version
cache key allows unchanged captures to be reused; every image is checked
against its manifest before reuse. These automated captures do not replace
human desktop visual review. The digest covers the complete library and Gallery
source trees, installed headers, resources, build files, and capture workflow.
Qt version, platform, scale, rendering and locale settings also participate;
the transient Xvfb display number does not. Capture settings are job-wide so
cache lookup, capture and publication validate the same environment.

Pages downloads `fluentqt-site-pages` and `fluentqt-wasm-pages` from the same CI
run. It checks both commit receipts, the website file hashes, and full WASM
validation before deploying them together. The website module is required by
`CI Gate`; stale or failed generation cannot silently publish the old documents.
New components still need Gallery registration, examples, and bilingual prose.
CI generates their pages and previews, not their usage guidance.

Website CI runs the static browser theme contract against that generated artifact
before upload, on every pull request. It checks both homepage languages, three
themes, forced colors, and wide/narrow layouts without loading WebAssembly.
Website styles, homepage templates, and generation helpers therefore use the
required website lane without starting native validation or a WASM build.
The WASM module retains the real embedded Gallery theme and Spatial contracts;
changes to its runtime, shell, or browser helpers still select that module.
The mixed UI/runtime scripts `site/site.js` and `site/spatial-showcase.js` also
keep the WASM lane because they own real iframe lifecycle and host messages.

WASM CI compiles once and builds an external consumer from the installed SDK.
Full validation sends the same staged files to two Chromium runners. One tests
the Gallery, examples, wide viewport and Spatial fallback. The other tests
command menus, the command palette, forms and DataGrid, keeping their failure
diagnostics together. Fast validation uses the Gallery runner alone. Pages
receives the tested files after every selected group passes. Each browser
contract logs its duration.

Use CMake aggregate targets rather than duplicating lists in YAML:
`fluent_qt_ci_fast_tests`, `fluent_qt_ci_full_tests`, and
`fluent_qt_ci_windows_platform_tests`. When registering a test, choose its
membership in `FLUENT_QT_CI_FAST_TARGETS`, `FLUENT_QT_CI_FULL_TARGETS`, or the
local-only set. `fluent_qt_all_tests` and `fluent_qt_contract_tests` serve local
host and focused contract validation.

Build `fluent_qt_ci_full_tests` when selecting `^ci_full$`. Compiling
`fluent_qt_all_tests` for that selection adds local-only binaries that the job
never runs. Test modules link into one executable per test directory and CI
tier, so the aggregates link a few dozen executables rather than one per
module; see [shared test executables](testing-workflow.md#ctest-labels). Exhaustive local builds remain available through `fluent_qt_all_tests`;
the sanitizer lane still builds its separate contract aggregate.

CI may pin build parallelism to its measured runner capacity. Local builds and
matrix builds without an explicit job count use the
[adaptive wrapper](build-workflow.md).

### Measure build changes on the same runner

Dispatch [CI build performance](../../.github/workflows/ci-performance.yml)
on the branch containing the measurement harness. Select a suite and provide
full commit SHAs for `baseline` and `candidate`. This optional workflow measures
builds and tests; it does not replace `CI Gate`, produce release packages, or
publish compiler caches.

The Linux sanitizer and Windows targeted suites alternate clean baseline and
candidate builds in baseline/candidate/candidate/baseline order. The Intel
Gallery suite compares uncached, cold-cache, warm-cache, and uncached builds.
The macOS Python suite compares wrapper PCH off/on/on/off. WASM repeats the
same clean build twice and records each Ninja edge's timing. WASM and Intel
cache comparisons require identical runtime build inputs in both revisions.

Each suite keeps its runner, source path, build path, and parallelism fixed.
Dependency setup is reported separately from compilation. The harness then
removes only linked outputs and rebuilds, rejecting a relink measurement if
any compiled object changes. Baseline test fixtures receive the listed
candidate reliability fixes without changing their test selection. The
`ci-performance-*` artifacts contain the exact variants, phase durations,
cache statistics, and raw logs, including failed phases. Use these paired
measurements to explain build changes; a complete CI run still measures
queueing, artifact transfer, packaging, and browser validation.

### Reuse compiler output and run tests in parallel

The [C++ matrix](../../.github/ci-cpp-matrix.json) sets `ctest_parallel` within
each lane's build capacity. Windows and native display lanes run tests serially.
Tests sharing settings, files or desktop focus must use a CTest `RESOURCE_LOCK`
or `RUN_SERIAL`. See the [testing workflow](testing-workflow.md#ctest-labels).

Linux and macOS lanes use ccache. A lane's `compiler_cache` names the full lane
that owns its snapshot. The cache key combines that lane, runner OS and
architecture, Qt version, a hash of the `c++ --version` line, and the UTC ISO
week. A successful push or scheduled run on `main` saves the owner's snapshot
once a week. Pull requests, including release pull requests, only restore.
The fast tier never runs on `main`. Its Linux x64 and macOS ARM64 lanes therefore
reuse the identically configured full-lane snapshots.

Each owner sets `compiler_cache_max_size`. The matrix validator keeps the total
within the 2048 MB ccache budget, leaving room for the other caches under the
repository's 10 GiB limit. ccache stores entries at zstd level 5; measured on
local Debug objects, this is about 18% smaller than the default level.

Hosted runners reuse the same absolute workspace path, so keys stay stable
without `base_dir`. Do not set it: ccache would pass relative paths to the
compiler, and tests such as `TestQtCompat` locate repository files from
`__FILE__`. Directory hashing is disabled. Launchers are enabled only after
ccache initializes, before the first CMake configure. Cache setup and transfer
failures fall back to ordinary compilation. The installed first-window trial
clears the launchers so that it times an uncached application build.

On macOS runners, ccache's Homebrew installation can also expose a newer `fmt`
in the global compiler include path. CI unlinks that formula from Homebrew's
prefix before configuring the project. Its installed keg remains available to
ccache, while Gallery uses matching headers and libraries from vcpkg.

Windows lanes stay on Visual Studio/MSBuild without a compiler cache. MSBuild
has no CMake compiler launcher. Moving to Ninja would change the generator,
architecture selection and debug-information format of presets shared with
desktop packaging. Track ccache hit rates, `Build` step times and repository
cache use after a warm `main` run before changing a lane's limit.

Full Python validation also builds the optional Spatial binding on one Linux
Qt 6.9 lane. It checks the binding and Gallery contracts offscreen, then OpenGL
resource destruction under Xvfb. Release wheels also enable Spatial on every
platform. Candidate and package-index clean installs require its native API,
stubs, and full Gallery catalog; a missing module must fail instead of passing
through the 2D fallback. These checks do not replace native desktop rendering
and input review.

Run the [local integration preflight](testing-workflow.md#local-integration-preflight)
before pushing. Its Qt-free source/packaging gates are also the first checks in
the CI planning job, so inventory mistakes fail before allocating native
runners. The shared preflight starts with the Qt-free compatibility boundary
checker and its regressions; do not move that gate behind a native/PySide build
or a platform-specific module condition. For a runtime failure, reproduce the failing test with the same
Qt/PySide line and rerun that job before requesting another full matrix. A
same-commit retry can reuse the failed job; a code change needs new validation.
Keep full validation on the final release commit.

## Repository size and font inputs

The planning job checks tracked file content with
`python3 tools/dev/fluent_qt_repository_size.py`. The budget in
`tools/dev/repository-size-budget.json` covers the total and reviewed large
runtime files. The check also rejects any tracked file that matches
`.gitignore`, so generated site assets and upstream font inputs are listed in
one place. Review resource changes before changing that budget. The report
excludes Git history, filesystem allocation, and untracked build caches; use
`--ref` to inspect a committed tree against the current `.gitignore`, or
`--json` for machine-readable results.

Font-generation inputs are cached by their manifest hash. The source-contract
check explicitly prepares verified inputs before the existing typography and
WebAssembly font reproducibility checks. Cached content is always revalidated;
missing or corrupt inputs cannot skip those checks. Ordinary CMake builds do
not fetch font sources.

## Build release artifacts once

For an untagged release version on `main`,
[Release Candidate](../../.github/workflows/release-candidate.yml) builds desktop
packages and the complete Python publication bundle in parallel. It emits
`Release Candidate ready` only after both commit-bound manifests pass. The tag
workflow promotes those artifacts without rebuilding them.

The desktop candidate also installs its Windows ARM64 package on a native
ARM64 runner without a Qt SDK. Architecture, required Spatial DLLs, completed
startup, splash dismissal, and no GPU surface initialization errors are required
before assembly. The retained launch
report explicitly leaves hardware 3D unvalidated; GPU rendering and input need
a capable graphics environment and separate native review.

On an untagged release push with curated notes, Release Candidate owns the
Windows ARM64 installer and the macOS ARM64 stable-ABI wheel. The Windows
candidate requires live interface zoom and verifies the staged ARM64 runtime
before the native installation test. Main CI keeps the Windows ARM64 native
component tests. Full PR, manual, scheduled and already-tagged main runs keep
the independent installer build. Bundle-enabled Python runs build one stable-ABI wheel per
platform with its oldest supported CPython, then install and smoke-test that
same wheel on every newer interpreter in the lane's `test_python_versions`.
The critical Windows ARM64 lane is queued first. Consult the wheel matrix for
supported ABIs and the
[Python publishing runbook](../../bindings/pyside6/PUBLISHING.md) for promotion,
index verification, and recovery.

Both macOS stable-ABI wheel jobs precompile the generated Shiboken module
header once per build. Each wrapper still compiles separately. The compiler
tracks header changes within that build, and each new runner starts from source.

A release push starts Release Candidate and full CI together. They share
hosted-runner capacity, including the account's platform-specific limits. A
matrix's `max-parallel` is an upper bound for that matrix; it neither reserves
runners nor guarantees a share for another workflow. The candidate cap allows
the current dynamic platform lanes to run together, with the fixed macOS ARM64
lane counted separately. The C++ matrix allows up to ten concurrent jobs. Measure queue
time, execution time, and the slowest required job after a matrix change;
timing estimates from the former per-interpreter wheel matrix do not predict
the stable-ABI pipeline.

The main matrix builds in C++ CI, desktop Release Candidate, and Desktop Compat
Packages use `tools/dev/fluent_qt_build.py --jobs auto` when `build_parallel` is
empty, including on Windows. The workflow validator checks all three entry
points, preserves explicit job counts, and requires build failures to propagate.
The Intel macOS Gallery build logs the CPU model, starting load, wall time and
CPU time alongside the selected compiler job count.

Require a successful full `CI Gate` before merging a release-to-main pull
request. Merging earlier sends full-only failures to `main`, where every fix
costs another full run and candidate rebuild.
Configure that required check in the `main` branch rules; workflow source checks
alone do not enforce repository settings. For an external failure, rerun failed
jobs in the same run and commit: successful lanes and their retained artifacts
can be reused. A source change requires validation of the new commit.

## Validate CI changes

External actions use full commit SHAs with readable version comments;
container actions use image digests. Concrete revisions live in workflow YAML,
where Dependabot can update them and their version comments. Do not duplicate
Action SHAs in Python validators or test expectations.

[ci_action_policy.py](../../.github/scripts/ci_action_policy.py) owns the shared
pinning rules and the allowed Actions and invocation counts for the Pages
pipeline. The boundary validator applies that policy to workflows and checks
immutable references in starter templates. Step comparisons accept updated
full SHAs while still checking the Action repository, inputs, permissions, and
artifact provenance. Action upgrades remain subject to PR review and the
owning CI lanes; a valid pin alone does not establish compatibility.

```bash
python3 .github/scripts/test_classify_ci_changes.py
python3 .github/scripts/test_validate_ci_cpp_matrix.py
python3 .github/scripts/test_validate_ci_workflow_boundaries.py
python3 .github/scripts/validate-ci-workflow-boundaries.py
```

Run the tests for any changed release or packaging helper as well. A passing
classifier or matrix check does not prove a native runner or publication path
has executed; record that distinction in the change's validation results.

<!-- docs-nav:bottom:start -->
---
[← Qt Component Test Conventions](qt-component-test-conventions.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Logging Workflow →](logging-workflow.md)
<!-- docs-nav:bottom:end -->
