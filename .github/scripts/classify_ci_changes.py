#!/usr/bin/env python3

"""Classify pull-request paths for the native and PySide6 CI matrices."""

from __future__ import annotations

import sys
from dataclasses import dataclass


DOCUMENTATION_PREFIXES = (
    "docs/",
    "site/",
    ".github/ISSUE_TEMPLATE/",
)

PYSIDE_PREFIXES = (
    "app/",
    "bindings/",
    "cmake/",
    "include/",
    "src/",
    "third_party/",
)

PYSIDE_ROOT_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "resources.qrc",
    "vcpkg.json",
}

PYSIDE_CI_FILES = {
    ".github/scripts/install-linux-dependencies.sh",
    ".github/scripts/setup-shiboken-clang.py",
    ".github/scripts/test_validate_pyside_wheel_matrix.py",
    ".github/scripts/validate-pyside-wheel-matrix.py",
    ".github/workflows/ci.yml",
    ".github/workflows/ci-python.yml",
}


@dataclass(frozen=True)
class ChangeClassification:
    should_build: bool
    should_build_pyside: bool


def is_documentation_path(path: str) -> bool:
    """Return whether a path is covered by the documentation-only gate."""
    return (
        path.endswith(".md")
        or path == "LICENSE"
        or path.startswith("LICENSE.")
        or path.startswith(DOCUMENTATION_PREFIXES)
    )


def affects_pyside(path: str) -> bool:
    """Return whether a non-documentation path can affect PySide6 artifacts."""
    return (
        path in PYSIDE_ROOT_FILES
        or path in PYSIDE_CI_FILES
        or path.startswith(PYSIDE_PREFIXES)
        or path.endswith(".qrc")
    )


def classify_changes(paths: list[str]) -> ChangeClassification:
    """Classify a non-empty list of repository-relative changed paths."""
    normalized = [path.strip() for path in paths if path.strip()]
    if not normalized:
        raise ValueError("No changed files were provided")

    build_paths = [path for path in normalized if not is_documentation_path(path)]
    return ChangeClassification(
        should_build=bool(build_paths),
        should_build_pyside=any(affects_pyside(path) for path in build_paths),
    )


def main() -> int:
    try:
        result = classify_changes(sys.stdin.read().splitlines())
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"should_build={str(result.should_build).lower()}")
    print(f"should_build_pyside={str(result.should_build_pyside).lower()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
