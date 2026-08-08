#!/usr/bin/env python3

"""Keep the top-level CI workflow free of C++ and PySide6 implementation."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"

EXPECTED_JOBS = {
    "ci.yml": {"plan", "cpp", "python", "ci-gate", "release-ready"},
    "ci-cpp.yml": {"plan", "build", "integration"},
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
    python = contents["ci-python.yml"]
    python_release = contents["python-release.yml"]

    if len(orchestrator.splitlines()) > 260:
        errors.append("ci.yml must remain a compact orchestration-only workflow")
    for required in (
        "uses: ./.github/workflows/ci-cpp.yml",
        "uses: ./.github/workflows/ci-python.yml",
        "name: CI Gate",
        "name: Release ready",
        "actions: read",
    ):
        if required not in orchestrator:
            errors.append(f"ci.yml is missing orchestration contract: {required}")
    for forbidden in ("cmake --build", "install-qt-action", "PySide6==", "vcpkg-"):
        if forbidden in orchestrator:
            errors.append(f"ci.yml contains module implementation detail: {forbidden}")

    for name, module in (("ci-cpp.yml", cpp), ("ci-python.yml", python)):
        if "workflow_call:" not in module:
            errors.append(f"{name} must be a reusable workflow")
        if "needs.plan.outputs.should_build" in module:
            errors.append(f"{name} must not depend on orchestrator classification outputs")

    if ".github/ci-cpp-matrix.json" not in cpp:
        errors.append("ci-cpp.yml must own the C++ matrix catalog")
    for forbidden in ("pip install PySide6", "shiboken6_generator==", "pyside6_release:"):
        if forbidden in cpp:
            errors.append(f"ci-cpp.yml contains PySide6 execution detail: {forbidden}")

    if "bindings/pyside6/wheel-matrix.json" not in python:
        errors.append("ci-python.yml must own the PySide6 wheel matrix catalog")
    for required in (
        "actions: read",
        "name: Platform status / ${{ matrix.display_name }}",
        ".github/scripts/verify-pyside-platform-artifacts.py",
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
        "display_name: Linux x64",
        "display_name: Linux ARM64",
        "display_name: Windows x64",
        "display_name: Windows ARM64",
        "display_name: macOS x64",
        "display_name: macOS ARM64",
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
        "attestations: true",
        "skip-existing: ${{ inputs.recovery }}",
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
