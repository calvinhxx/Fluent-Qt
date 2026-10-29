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
| Push to `main` | `full` | Built separately by Release Candidate for an untagged release version |
| Weekly schedule | `full` | Enabled |
| Manual CI | `fast` by default; `full` selectable | Opt in with `python_release_bundle=true`; requires `full` |

[ci.yml](../../.github/workflows/ci.yml) and
[classify_ci_changes.py](../../.github/scripts/classify_ci_changes.py) define
the current triggers and path rules. The classifier must keep build lanes
enabled when it cannot prove a change is documentation-only. The stable
`CI Gate` check still runs when native builds are skipped.

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

Use CMake aggregate targets rather than duplicating lists in YAML:
`fluent_qt_ci_fast_tests`, `fluent_qt_ci_full_tests`, and
`fluent_qt_ci_windows_platform_tests`. When registering a test, choose its
membership in `FLUENT_QT_CI_FAST_TARGETS`, `FLUENT_QT_CI_FULL_TARGETS`, or the
local-only set. `fluent_qt_all_tests` and `fluent_qt_contract_tests` serve local
host and focused contract validation.

CI may pin build parallelism to its measured runner capacity. Local builds and
matrix builds without an explicit job count use the
[adaptive wrapper](build-workflow.md).

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

On that release commit, Release Candidate also owns the macOS ARM64
stable-ABI lane omitted from simultaneous main CI. Scheduled and manual full
runs retain it. Bundle-enabled Python runs build one stable-ABI wheel per
platform with its oldest supported CPython, then install and smoke-test that
same wheel on every newer interpreter in the lane's `test_python_versions`.
The critical Windows ARM64 lane is queued first. Consult the wheel matrix for
supported ABIs and the
[Python publishing runbook](../../bindings/pyside6/PUBLISHING.md) for promotion,
index verification, and recovery.

A release push starts Release Candidate and full CI together. They share
hosted-runner capacity, including the account's platform-specific limits. A
matrix's `max-parallel` is an upper bound for that matrix; it neither reserves
runners nor guarantees a share for another workflow. The candidate cap allows
the current dynamic platform lanes to run together, with the fixed macOS ARM64
lane counted separately. Full CI retains its own matrix caps. Measure queue
time, execution time, and the slowest required job after a matrix change;
timing estimates from the former per-interpreter wheel matrix do not predict
the stable-ABI pipeline.

The main matrix builds in C++ CI, desktop Release Candidate, and Desktop Compat
Packages use `tools/dev/fluent_qt_build.py --jobs auto` when `build_parallel` is
empty, including on Windows. The workflow validator checks all three entry
points, preserves explicit job counts, and requires build failures to propagate.

Require a successful `CI Gate` before merging a release-to-main pull request.
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
