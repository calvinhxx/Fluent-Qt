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

Compiler, SDK, package-manager, and platform steps belong in the owning module,
not the orchestrator. Modules upload artifacts into the caller's run. The
[Pages workflow](../../.github/workflows/pages.yml) consumes the payload from
main CI; its manual entry rebuilds for recovery.

Use CMake aggregate targets rather than duplicating lists in YAML:
`fluent_qt_ci_fast_tests`, `fluent_qt_ci_full_tests`, and
`fluent_qt_ci_windows_platform_tests`. When registering a test, choose its
membership in `FLUENT_QT_CI_FAST_TARGETS`, `FLUENT_QT_CI_FULL_TARGETS`, or the
local-only set. `fluent_qt_all_tests` and `fluent_qt_contract_tests` serve local
host and focused contract validation.

CI may pin build parallelism to its measured runner capacity. Local builds use
the [adaptive wrapper](build-workflow.md).

## Build release artifacts once

For an untagged release version on `main`,
[Release Candidate](../../.github/workflows/release-candidate.yml) builds desktop
packages and the complete Python publication bundle in parallel. It emits
`Release Candidate ready` only after both commit-bound manifests pass. The tag
workflow promotes those artifacts without rebuilding them.

On that release commit, Release Candidate also owns the macOS ARM64 CPython
3.11 representative omitted from simultaneous main CI. Scheduled and manual
full runs retain it. Bundle-enabled Python runs validate all declared wheels;
their ordering prioritizes the expensive acceptance lanes without reducing
matrix coverage. Consult the wheel matrix for supported ABIs and the
[Python publishing runbook](../../bindings/pyside6/PUBLISHING.md) for promotion,
index verification, and recovery.

## Validate CI changes

External actions use full commit SHAs with readable version comments;
container actions use image digests. Dependabot maintains revisions. The
boundary validator checks both workflows and starter templates.

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
