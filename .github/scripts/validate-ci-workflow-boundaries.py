#!/usr/bin/env python3

"""Keep the top-level CI workflow free of C++ and PySide6 implementation."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"

EXPECTED_JOBS = {
    "ci.yml": {"plan", "cpp", "python", "wasm", "ci-gate", "release-ready"},
    "ci-cpp.yml": {"plan", "build", "integration"},
    "ci-wasm.yml": {"build"},
    "ci-python.yml": {
        "plan",
        "pyside6_linux",
        "pyside6_windows",
        "pyside6_macos",
        "pyside6_release",
        "pyside6_release_bundle",
        "pyside6_platform_summary",
    },
    "python-release.yml": {
        "preflight",
        "prepare",
        "publish_testpypi",
        "verify_testpypi",
        "publish_pypi",
        "verify_pypi",
    },
    "desktop-release-candidate.yml": {"plan", "package", "assemble"},
    "release-candidate.yml": {"plan", "desktop", "python", "ready"},
    "release.yml": {
        "preflight",
        "desktop_candidate",
        "source-package",
        "python_candidate",
        "publish",
        "publish_python",
    },
    "pages.yml": {"wasm", "deploy"},
}


def read_workflow(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8")


def job_ids(contents: str) -> set[str]:
    try:
        jobs = contents.split("\njobs:\n", 1)[1]
    except IndexError:
        return set()
    return set(re.findall(r"^  ([A-Za-z0-9_-]+):$", jobs, re.MULTILINE))


def job_section(contents: str, job_id: str) -> str:
    match = re.search(
        rf"^  {re.escape(job_id)}:\n(?P<body>.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
        contents,
        re.MULTILINE | re.DOTALL,
    )
    return match.group(0) if match else ""


def validate_boundaries() -> list[str]:
    errors: list[str] = []
    contents: dict[str, str] = {}
    for name, expected_jobs in EXPECTED_JOBS.items():
        try:
            contents[name] = read_workflow(name)
        except OSError as error:
            errors.append(f"unable to read {name}: {error}")
            continue
        actual_jobs = job_ids(contents[name])
        if actual_jobs != expected_jobs:
            errors.append(
                f"{name} jobs must be {sorted(expected_jobs)}, got {sorted(actual_jobs)}"
            )

    if errors:
        return errors

    orchestrator = contents["ci.yml"]
    cpp = contents["ci-cpp.yml"]
    wasm = contents["ci-wasm.yml"]
    python = contents["ci-python.yml"]
    python_release = contents["python-release.yml"]
    desktop_candidate = contents["desktop-release-candidate.yml"]
    release_candidate = contents["release-candidate.yml"]
    release = contents["release.yml"]
    pages = contents["pages.yml"]

    if len(orchestrator.splitlines()) > 300:
        errors.append("ci.yml must remain a compact orchestration-only workflow")
    for required in (
        "uses: ./.github/workflows/ci-cpp.yml",
        "uses: ./.github/workflows/ci-python.yml",
        "uses: ./.github/workflows/ci-wasm.yml",
        "name: CI Gate",
        "name: Release ready",
        "python_release_bundle:",
        'python_release_bundle="true"',
        "build_release_bundle: ${{ needs.plan.outputs.python_release_bundle == 'true' }}",
        "run_compatibility_validation: true",
        "release_max_parallel: 4",
        "Require a current main base for release pull requests",
        ".github/scripts/check-release-branch-freshness.py",
        "actions: read",
    ):
        if required not in orchestrator:
            errors.append(f"ci.yml is missing orchestration contract: {required}")
    if orchestrator.count('python_release_bundle="true"') != 1:
        errors.append(
            "ci.yml must reserve the complete Python bundle for scheduled validation; "
            "the release-candidate workflow owns stable promotion artifacts"
        )
    for forbidden in (
        "cmake --build",
        "install-qt-action",
        "PySide6==",
        "vcpkg-",
        "emsdk",
        "playwright",
    ):
        if forbidden in orchestrator:
            errors.append(f"ci.yml contains module implementation detail: {forbidden}")

    for name, module in (
        ("ci-cpp.yml", cpp),
        ("ci-python.yml", python),
        ("ci-wasm.yml", wasm),
    ):
        if "workflow_call:" not in module:
            errors.append(f"{name} must be a reusable workflow")
        if "needs.plan.outputs.should_build" in module:
            errors.append(f"{name} must not depend on orchestrator classification outputs")

    for required in (
        "Qt 6.9.3 / Emscripten 3.1.70",
        "wasm_singlethread",
        "aqtinstall==3.3.0",
        "playwright==1.58.0",
        "cmake --preset wasm",
        "cmake --build --preset wasm --parallel",
        "Verify installed FluentQt WebAssembly consumer",
        "build/wasm-installed-consumer",
        ".github/scripts/run-wasm-browser-smoke.py",
        ".github/scripts/stage-wasm-pages.py",
        "name: fluentqt-wasm-pages",
    ):
        if required not in wasm:
            errors.append(f"ci-wasm.yml is missing browser validation contract: {required}")
    for forbidden in ("VCPKG_ROOT", "PySide6==", "shiboken6_generator=="):
        if forbidden in wasm:
            errors.append(f"ci-wasm.yml contains unrelated module detail: {forbidden}")

    for required in (
        "uses: ./.github/workflows/ci-wasm.yml",
        "mode: full",
        "name: fluentqt-wasm-pages",
        "path: build/pages",
        "needs: wasm",
    ):
        if required not in pages:
            errors.append(f"pages.yml is missing WebAssembly deployment contract: {required}")

    if ".github/ci-cpp-matrix.json" not in cpp:
        errors.append("ci-cpp.yml must own the C++ matrix catalog")
    cpp_build = job_section(cpp, "build")
    if "max-parallel: 4" not in cpp_build:
        errors.append(
            "ci-cpp.yml build matrix must cap parallel action downloads at 4"
        )
    for forbidden in ("pip install PySide6", "shiboken6_generator==", "pyside6_release:"):
        if forbidden in cpp:
            errors.append(f"ci-cpp.yml contains PySide6 execution detail: {forbidden}")

    if "bindings/pyside6/wheel-matrix.json" not in python:
        errors.append("ci-python.yml must own the PySide6 wheel matrix catalog")
    if "build_release_bundle:" not in python:
        errors.append("ci-python.yml must expose the optional Python release-bundle input")
    if "run_compatibility_validation:" not in python:
        errors.append("ci-python.yml must expose its representative compatibility lanes")
    if "release_max_parallel:" not in python:
        errors.append("ci-python.yml must expose its release-wheel concurrency cap")
    if python.count("inputs.build_release_bundle") < 4:
        errors.append(
            "ci-python.yml must gate matrix selection, release jobs, bundle, and summary"
        )
    pyside_release = job_section(python, "pyside6_release")
    if "max-parallel: ${{ inputs.release_max_parallel }}" not in pyside_release:
        errors.append(
            "ci-python.yml release matrix must honor release_max_parallel"
        )
    for job_id in ("pyside6_linux", "pyside6_windows"):
        if "if: ${{ inputs.run_compatibility_validation }}" not in job_section(
            python, job_id
        ):
            errors.append(
                f"ci-python.yml {job_id} must honor run_compatibility_validation"
            )
    if "inputs.run_compatibility_validation" in job_section(
        python, "pyside6_macos"
    ):
        errors.append(
            "ci-python.yml pyside6_macos must always build the publishable "
            "macOS ARM64 CPython 3.11 wheel"
        )
    for required in (
        "actions: read",
        "name: Verify Python candidate platform coverage",
        ".github/scripts/verify-pyside-platform-artifacts.py",
        'verification_scope="release"',
        '--scope "$verification_scope"',
        ".github/scripts/select-pyside-release-matrix.py",
        ".github/scripts/assemble-pyside-release-bundle.py",
        "Prioritized representative scenarios:",
        "matrix.extended_acceptance == true",
        "fluentqt-pyside6-qt624-cp310-linux-x64",
        "fluentqt-pyside6-qt624-cp310-windows-x64",
        "fluentqt-pyside6-qt693-cp311-macos-arm64",
        "name: PySide6 compatibility / Linux x64 / CPython 3.10 / Qt 6.2.4",
        "name: PySide6 compatibility / Windows x64 / CPython 3.10 / Qt 6.2.4",
        "name: PySide6 release / macOS ARM64 / CPython 3.11 / Qt 6.9.3",
        "name: Assemble canonical Python release bundle",
        "name: fluentqt-python-release-bundle",
        "Linux x64|linux|x64",
        "Linux ARM64|linux|arm64",
        "Windows x64|windows|x64",
        "Windows ARM64|windows|arm64",
        "macOS x64|macos|x64",
        "macOS ARM64|macos|arm64",
    ):
        if required not in python:
            errors.append(f"ci-python.yml is missing platform summary: {required}")
    for contract, expected_count in (
        ("Test core wheel in a clean virtual environment (fast)", 2),
        ("needs.plan.outputs.mode != 'full'", 2),
        ("Run extended installed-wheel acceptance", 1),
    ):
        actual_count = python.count(contract)
        if actual_count != expected_count:
            errors.append(
                f"ci-python.yml must contain {expected_count} occurrence(s) of "
                f"{contract!r}, found {actual_count}"
            )
    for forbidden in ("VCPKG_BINARY_SOURCES", "fluent_qt_ci_full_tests", "Library integration"):
        if forbidden in python:
            errors.append(f"ci-python.yml contains C++ matrix detail: {forbidden}")

    for required in (
        "workflow_call:",
        "source_ref:",
        "source_commit:",
        "package_set:",
        ".github/package-matrix.json",
        "name: fluentqt-desktop-package-${{ matrix.id }}",
        "name: fluentqt-desktop-release-candidate",
        ".github/scripts/assemble-desktop-release-candidate.py assemble",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        "for attempt in 1 2 3",
        "smoke-launch-macos-gallery.sh",
        "diagnostics-desktop-release-${{ matrix.id }}-${{ github.run_attempt }}",
    ):
        if required not in desktop_candidate:
            errors.append(
                "desktop-release-candidate.yml is missing packaging contract: "
                f"{required}"
            )
    if "workflow_dispatch:" in desktop_candidate or "push:" in desktop_candidate:
        errors.append(
            "desktop-release-candidate.yml must remain reusable-only"
        )

    for required in (
        "name: Release Candidate",
        "branches:\n      - main",
        "workflow_dispatch:",
        "uses: ./.github/workflows/desktop-release-candidate.yml",
        "uses: ./.github/workflows/ci-python.yml",
        "build_release_bundle: true",
        "run_compatibility_validation: false",
        "release_max_parallel: 6",
        "name: Release Candidate ready",
        "refs/tags/$tag^{tag}",
        "docs/releases/$tag.md",
        "name: fluentqt-desktop-release-candidate",
        "name: fluentqt-python-release-bundle",
        "name: fluentqt-release-candidate-receipt",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        ".source.ci_run_id == $run_id",
        "cancel-in-progress: true",
    ):
        if required not in release_candidate:
            errors.append(
                f"release-candidate.yml is missing promotion contract: {required}"
            )
    for forbidden in ("contents: write", "id-token: write", "gh release"):
        if forbidden in release_candidate:
            errors.append(
                f"release-candidate.yml may not publish external state: {forbidden}"
            )

    for required in (
        "workflow_dispatch:",
        "name: Publish to TestPyPI",
        "name: Publish to PyPI",
        "environment:\n      name: ${{ matrix.environment_name }}",
        "uses: pypa/gh-action-pypi-publish@release/v1",
        "name: fluentqt-python-release-bundle",
        "name: fluentqt-python-core-publish-candidate",
        "name: fluentqt-python-gallery-publish-candidate",
        "dist/fluentqt-*.whl",
        "dist/fluentqt_gallery-*.whl",
        ".github/scripts/verify-python-package-index.py",
        ".github/scripts/install-python-release-from-index.py",
        "attestations: true",
        "skip-existing: ${{ needs.preflight.outputs.stage == 'all' || needs.preflight.outputs.recovery == 'true' }}",
        "- all",
        "needs.preflight.outputs.stage == 'all'",
        "source_tag:",
        "SOURCE_TAG: ${{ inputs.source_tag }}",
        "source_run_id:",
        "SOURCE_RUN_ID: ${{ inputs.source_run_id }}",
        "source_run_attempt:",
        "SOURCE_RUN_ATTEMPT: ${{ inputs.source_run_attempt }}",
        "source_run_id and source_run_attempt are reserved for stage=all.",
        '"$candidate_name" == "Release"',
        '"$candidate_path" == ".github/workflows/release.yml"',
        '"$candidate_name" == "Release Candidate"',
        '"$candidate_path" == ".github/workflows/release-candidate.yml"',
        "source_ref: ${{ steps.resolve.outputs.source_ref }}",
        "source_tag is only valid for manual TestPyPI recovery.",
        "TestPyPI source_tag recovery requires a published stable GitHub Release.",
        "ref: ${{ needs.preflight.outputs.source_ref }}",
        "for discovery_attempt in {1..12}",
        "Release evidence is not fully visible yet; retrying in 10 seconds.",
        'candidate_name" != "CI full"',
        'candidate_path" != ".github/workflows/ci.yml"',
    ):
        if required not in python_release:
            errors.append(
                f"python-release.yml is missing publication contract: {required}"
            )
    if "workflow_call:" in python_release:
        errors.append(
            "python-release.yml must remain a top-level Trusted Publisher workflow"
        )
    if python_release.count("id-token: write") != 2:
        errors.append(
            "python-release.yml must grant id-token: write to exactly two matrix publish jobs"
        )
    for required in (
        "actions: write",
        "name: Build Python release candidate",
        "uses: ./.github/workflows/ci-python.yml",
        "build_release_bundle: true",
        "name: Resolve immutable release candidate",
        "name: fluentqt-desktop-release-candidate",
        "run-id: ${{ needs.preflight.outputs.candidate_run_id }}",
        "needs.preflight.outputs.promote_candidate == 'true'",
        "A manual stable publication must dispatch release.yml from $tag.",
        "name: Dispatch synchronized Python publication",
        "gh workflow run python-release.yml",
        "-f stage=all",
        'source_run_id="$(jq -r',
        'source_run_attempt="$(jq -r',
        "-f source_run_id=\"${{ steps.candidate.outputs.source_run_id }}\"",
        "-f source_run_attempt=\"${{ steps.candidate.outputs.source_run_attempt }}\"",
        "gh run watch",
        "--exit-status",
    ):
        if required not in release:
            errors.append(f"release.yml is missing Python publication orchestration: {required}")
    stable_publish = job_section(release, "publish")
    for required in (
        "needs: [preflight, desktop_candidate, source-package, python_candidate]",
        "always()",
        "needs.desktop_candidate.result == 'skipped'",
        "needs.desktop_candidate.result == 'success'",
        "needs.source-package.result == 'success'",
        "needs.preflight.outputs.promote_candidate == 'true'",
        "needs.preflight.outputs.promote_candidate != 'true'",
        "needs.python_candidate.result == 'success'",
        ".github/scripts/assemble-desktop-release-candidate.py verify",
        "name: release-source",
    ):
        if required not in stable_publish:
            errors.append(
                "release.yml stable publish must promote or build a complete candidate: "
                f"{required}"
            )
    for required in (
        "find release-dist -maxdepth 1 -type f -print0 | sort -z",
        "mapfile -d '' release_assets",
        '"${release_assets[@]}"',
    ):
        if required not in stable_publish:
            errors.append(
                f"release.yml is missing scoped release asset handling: {required}"
            )
    if "name: diagnostics-desktop-release-${{ matrix.id }}-${{ github.run_attempt }}" not in desktop_candidate:
        errors.append(
            "desktop-release-candidate.yml must keep diagnostics outside the candidate namespace"
        )
    for forbidden in (
        "name: release-diagnostics-${{ matrix.id }}",
        "release-dist/*",
    ):
        if forbidden in release:
            errors.append(
                f"release.yml may publish unintended release assets: {forbidden}"
            )
    if "id-token: write" in release:
        errors.append("release.yml must dispatch the top-level publisher, not receive OIDC")
    publisher_contracts = {
        "publish_testpypi": (
            "environment_name: testpypi",
            "environment_name: testpypi-gallery",
        ),
        "publish_pypi": (
            "environment_name: pypi",
            "environment_name: pypi-gallery",
        ),
    }
    for job_id, environments in publisher_contracts.items():
        section = job_section(python_release, job_id)
        if not section:
            errors.append(f"python-release.yml is missing job {job_id}")
            continue
        if "id-token: write" not in section:
            errors.append(f"{job_id} must receive the short-lived OIDC permission")
        for required in (
            "distribution: FluentQt",
            "distribution: FluentQt-Gallery",
            "candidate_artifact: fluentqt-python-core-publish-candidate",
            "candidate_artifact: fluentqt-python-gallery-publish-candidate",
            "name: ${{ matrix.candidate_artifact }}",
            "packages-dir: publisher-candidate/",
            *environments,
        ):
            if required not in section:
                errors.append(f"{job_id} is missing scoped publisher contract: {required}")
        for forbidden in ("actions/checkout", ".github/scripts/", "run:"):
            if forbidden in section:
                errors.append(
                    f"{job_id} must not execute repository code: {forbidden}"
                )
    for forbidden in (
        "PYPI_API_TOKEN",
        "TEST_PYPI_API_TOKEN",
        "secrets.PYPI",
        "password:",
    ):
        if forbidden in python_release:
            errors.append(
                f"python-release.yml must not use long-lived publishing credentials: {forbidden}"
            )

    return errors


def main() -> int:
    errors = validate_boundaries()
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print("Validated modular CI workflow boundaries.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
