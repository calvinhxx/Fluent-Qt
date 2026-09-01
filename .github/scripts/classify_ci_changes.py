#!/usr/bin/env python3

"""Classify pull-request paths and select fail-closed native test coverage."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass


DOCUMENTATION_PREFIXES = (
    "docs/",
    "site/",
    ".agents/skills/",
    ".github/ISSUE_TEMPLATE/",
    "tools/ai/",
    "tools/docs/",
    "tools/site/",
)

DOCUMENTATION_ROOT_FILES = {
    "llms.txt",
}

NON_DOCUMENTATION_PATHS = {
    "docs/development/component-api-policy.json",
    "docs/development/technical-debt-roadmap.md",
    "docs/development/visual-evidence-inventory.json",
    "site/api/catalog.json",
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

PYSIDE_ONBOARDING_FILES = {
    "tools/onboarding/create-report.schema.json",
    "tools/onboarding/first-window-report.schema.json",
    "tools/onboarding/fluentqt",
    "tools/onboarding/fluentqt_create.py",
    "tools/onboarding/fluentqt_trial.py",
    "tools/onboarding/starters/manifest.json",
    "tools/onboarding/test_fluentqt_create.py",
    "tools/onboarding/test_fluentqt_trial.py",
}

PYSIDE_ONBOARDING_PREFIXES = (
    "tools/onboarding/starters/pyside6-",
)

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

GITHUB_PULL_FILES_LIMIT = 3000

CPP_COMPONENT_TEST_GROUPS = frozenset(
    {
        "basicinput",
        "collections",
        "date_time",
        "dialogs_flyouts",
        "layout",
        "menus_toolbars",
        "navigation",
        "scrolling",
        "status_info",
        "textfields",
        "windowing",
    }
)

CPP_COMPONENT_PATH_PREFIXES = (
    "src/components/",
    "tests/components/",
)

CPP_TEST_IRRELEVANT_PREFIXES = (
    ".agents/",
    ".github/ISSUE_TEMPLATE/",
    "app/",
    "bindings/",
    "docs/",
    "examples/",
    "platforms/",
    "site/",
    "tools/",
)

CPP_TEST_IRRELEVANT_ROOT_FILES = {
    "LICENSE",
    "llms.txt",
}

@dataclass(frozen=True)
class ChangeClassification:
    should_build: bool
    should_build_pyside: bool
    should_build_wasm: bool


@dataclass(frozen=True)
class CppTestSelection:
    """A workflow-safe C++ test selection derived only from static allowlists."""

    scope: str
    groups: tuple[str, ...] = ()

    @property
    def label_regex(self) -> str:
        if self.scope == "none":
            return ""
        if self.scope == "all":
            return "^qt$"
        return "^(" + "|".join(self.groups) + ")$"

    @property
    def targets(self) -> tuple[str, ...]:
        if self.scope == "none":
            return ()
        if self.scope == "all":
            return ("fluent_qt_all_tests",)
        return tuple(f"fluent_qt_{group}_tests" for group in self.groups)

    def output_values(self) -> tuple[str, str, str]:
        """Return values only after matching an exact static-output contract."""
        if self.scope not in {"none", "selected", "all"}:
            raise ValueError("Unknown C++ test scope")
        if self.scope != "selected" and self.groups:
            raise ValueError("Only selected C++ test scope may name groups")
        if self.scope == "selected" and (
            not self.groups
            or list(self.groups) != sorted(set(self.groups))
            or any(group not in CPP_COMPONENT_TEST_GROUPS for group in self.groups)
        ):
            raise ValueError("Selected C++ test groups are not allowlisted")

        return self.scope, self.label_regex, " ".join(self.targets)


def is_documentation_path(path: str) -> bool:
    """Return whether a path is covered by the documentation-only gate."""
    if path in NON_DOCUMENTATION_PATHS:
        return False
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
        or path in PYSIDE_ONBOARDING_FILES
        or path.startswith(PYSIDE_PREFIXES)
        or path.startswith(PYSIDE_ONBOARDING_PREFIXES)
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


def _all_cpp_tests() -> CppTestSelection:
    return CppTestSelection("all")


def _cpp_tests_for_groups(groups: set[str]) -> CppTestSelection:
    """Resolve trusted component groups to static targets and a label regex."""
    unknown_groups = groups.difference(CPP_COMPONENT_TEST_GROUPS)
    if unknown_groups:
        raise ValueError(
            "Unknown C++ component test groups: "
            + ", ".join(sorted(unknown_groups))
        )
    if not groups:
        return CppTestSelection("none")

    ordered_groups = sorted(groups)
    selection = CppTestSelection("selected", tuple(ordered_groups))
    selection.output_values()
    return selection


def _has_unsafe_path_syntax(path: str) -> bool:
    """Reject path spellings that could hide a wider-impact repository path."""
    return (
        path != path.strip()
        or path.startswith("/")
        or "\\" in path
        or any(ord(character) < 32 or ord(character) == 127 for character in path)
        or any(part in {"", ".", ".."} for part in path.split("/"))
    )


def _component_group_for_path(path: str) -> str | None:
    """Return a precise component group, or an empty marker for a broad path."""
    for prefix in CPP_COMPONENT_PATH_PREFIXES:
        if not path.startswith(prefix):
            continue
        remainder = path.removeprefix(prefix)
        group, separator, nested_path = remainder.partition("/")
        if (
            not separator
            or not nested_path
            or group not in CPP_COMPONENT_TEST_GROUPS
        ):
            return ""
        return group
    return None


def select_cpp_tests(paths: list[str]) -> CppTestSelection:
    """Select component tests, failing closed for any broad-impact path."""
    normalized = [path for path in paths if path]
    if not normalized:
        raise ValueError("No changed files were provided")

    groups: set[str] = set()
    for path in normalized:
        if not isinstance(path, str) or _has_unsafe_path_syntax(path):
            return _all_cpp_tests()
        if is_documentation_path(path):
            continue
        if path in CPP_TEST_IRRELEVANT_ROOT_FILES or path.startswith(
            CPP_TEST_IRRELEVANT_PREFIXES
        ):
            continue

        group = _component_group_for_path(path)
        if group is None or not group:
            return _all_cpp_tests()
        groups.add(group)

    return _cpp_tests_for_groups(groups)


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


def github_changed_paths(
    value: object, expected_count: int
) -> tuple[str, ...] | None:
    """Validate slurped PR-file pages and return None when API data is incomplete."""
    if (
        not isinstance(expected_count, int)
        or isinstance(expected_count, bool)
        or expected_count < 1
    ):
        raise ValueError("Expected pull-request changed_files must be positive")
    if not isinstance(value, list) or not all(
        isinstance(page, list) for page in value
    ):
        raise ValueError("GitHub pull-request files must be a list of pages")

    current_paths: list[str] = []
    previous_paths: list[str] = []
    for page in value:
        for item in page:
            if not isinstance(item, dict):
                raise ValueError("GitHub pull-request file entries must be objects")
            filename = item.get("filename")
            if not isinstance(filename, str) or not filename:
                raise ValueError("GitHub pull-request file entry has no filename")
            current_paths.append(filename)
            previous = item.get("previous_filename")
            if previous is not None:
                if not isinstance(previous, str) or not previous:
                    raise ValueError(
                        "GitHub pull-request previous_filename must be non-empty"
                    )
                previous_paths.append(previous)

    api_is_incomplete = (
        expected_count > GITHUB_PULL_FILES_LIMIT
        or len(current_paths) != expected_count
        or len(set(current_paths)) != len(current_paths)
    )
    if api_is_incomplete:
        return None
    return tuple([*current_paths, *previous_paths])


def classify_github_file_pages(
    value: object, expected_count: int
) -> ChangeClassification:
    """Classify slurped PR-file pages, failing closed on API truncation."""
    paths = github_changed_paths(value, expected_count)
    if paths is None:
        return ChangeClassification(True, True, True)
    return classify_changes(list(paths))


def select_cpp_tests_from_github_file_pages(
    value: object, expected_count: int
) -> CppTestSelection:
    """Select tests from PR-file pages, using all tests if the API is incomplete."""
    paths = github_changed_paths(value, expected_count)
    if paths is None:
        return _all_cpp_tests()
    return select_cpp_tests(list(paths))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--github-files-json", action="store_true")
    parser.add_argument("--expected-count", type=int)
    args = parser.parse_args()
    try:
        if args.github_files_json:
            if args.expected_count is None:
                raise ValueError(
                    "--expected-count is required with --github-files-json"
                )
            github_files = json.loads(sys.stdin.read())
            result = classify_github_file_pages(github_files, args.expected_count)
            cpp_tests = select_cpp_tests_from_github_file_pages(
                github_files, args.expected_count
            )
        else:
            if args.expected_count is not None:
                raise ValueError(
                    "--expected-count requires --github-files-json"
                )
            paths = sys.stdin.read().splitlines()
            result = classify_changes(paths)
            cpp_tests = select_cpp_tests(paths)
    except (ValueError, RecursionError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"should_build={str(result.should_build).lower()}")
    print(f"should_build_pyside={str(result.should_build_pyside).lower()}")
    print(f"should_build_wasm={str(result.should_build_wasm).lower()}")
    cpp_scope, cpp_label_regex, cpp_targets = cpp_tests.output_values()
    print(f"cpp_test_scope={cpp_scope}")
    print(f"cpp_test_label_regex={cpp_label_regex}")
    print(f"cpp_test_targets={cpp_targets}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
