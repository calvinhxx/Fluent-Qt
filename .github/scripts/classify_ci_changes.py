#!/usr/bin/env python3

"""Classify pull-request paths for native, PySide6, and WebAssembly CI."""

from __future__ import annotations

import sys
from dataclasses import dataclass


DOCUMENTATION_PREFIXES = (
    "docs/",
    "site/",
    ".agents/skills/",
    ".github/ISSUE_TEMPLATE/",
    "tools/ai/",
)

DOCUMENTATION_ROOT_FILES = {
    "llms.txt",
}

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

WASM_PREFIXES = (
    "app/",
    "cmake/",
    "examples/hello_world/",
    "include/",
    "platforms/webassembly/",
    "res/",
    "src/",
    "support/logging/",
    "third_party/",
)

WASM_ROOT_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "resources.qrc",
}

WASM_CI_FILES = {
    ".github/scripts/classify_ci_changes.py",
    ".github/scripts/run-wasm-browser-smoke.py",
    ".github/scripts/stage-wasm-pages.py",
    ".github/scripts/test_classify_ci_changes.py",
    ".github/scripts/validate-ci-workflow-boundaries.py",
    ".github/workflows/ci.yml",
    ".github/workflows/ci-wasm.yml",
    ".github/workflows/pages.yml",
}


@dataclass(frozen=True)
class ChangeClassification:
    should_build: bool
    should_build_pyside: bool
    should_build_wasm: bool


def is_documentation_path(path: str) -> bool:
    """Return whether a path is covered by the documentation-only gate."""
    return (
        path.endswith(".md")
        or path == "LICENSE"
        or path.startswith("LICENSE.")
        or path in DOCUMENTATION_ROOT_FILES
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


def affects_wasm(path: str) -> bool:
    """Return whether a non-documentation path can affect browser artifacts."""
    return (
        path in WASM_ROOT_FILES
        or path in WASM_CI_FILES
        or path.startswith(WASM_PREFIXES)
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
        should_build_wasm=any(affects_wasm(path) for path in build_paths),
    )


def main() -> int:
    try:
        result = classify_changes(sys.stdin.read().splitlines())
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"should_build={str(result.should_build).lower()}")
    print(f"should_build_pyside={str(result.should_build_pyside).lower()}")
    print(f"should_build_wasm={str(result.should_build_wasm).lower()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
