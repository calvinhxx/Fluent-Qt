#!/usr/bin/env python3
"""Verify the versioning and deprecation policy for the PySide6 API."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Any


PYSIDE_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = PYSIDE_ROOT.parents[1]
DEFAULT_MANIFEST = PYSIDE_ROOT / "api-manifest.json"
DEFAULT_PROJECT_FILE = PROJECT_ROOT / "CMakeLists.txt"
SEMVER_PATTERN = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
PROJECT_VERSION_PATTERN = re.compile(
    r"\bproject\s*\(\s*FluentQt\s+VERSION\s+"
    r"(\d+\.\d+\.\d+)\b",
    re.IGNORECASE,
)
REQUIRED_VERSION_VARIABLES = {"__api_version__", "__version__"}
DEPRECATION_FIELDS = {
    "symbol",
    "deprecated_in",
    "remove_in",
    "replacement",
    "reason",
}


def parse_semver(value: Any, context: str) -> tuple[int, int, int]:
    if not isinstance(value, str):
        raise ValueError(f"{context} must be a SemVer string")
    match = SEMVER_PATTERN.fullmatch(value)
    if match is None:
        raise ValueError(f"{context} must use MAJOR.MINOR.PATCH SemVer")
    return tuple(int(part) for part in match.groups())


def read_project_version(project_file: Path) -> tuple[str, tuple[int, int, int]]:
    try:
        contents = project_file.read_text(encoding="utf-8")
    except OSError as error:
        raise ValueError(f"cannot read project file: {error}") from error
    match = PROJECT_VERSION_PATTERN.search(contents)
    if match is None:
        raise ValueError("cannot find FluentQt project VERSION")
    version = match.group(1)
    return version, parse_semver(version, "project version")


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read valid manifest JSON: {error}") from error
    if not isinstance(manifest, dict):
        raise ValueError("manifest root must be an object")
    return manifest


def public_symbols(manifest: dict[str, Any]) -> set[str]:
    symbols: set[str] = set()
    for section in ("classes", "enums", "functions", "variables"):
        values = manifest.get(section)
        if not isinstance(values, list):
            continue
        symbols.update(
            f"fluentqt.{name}" for name in values if isinstance(name, str)
        )

    methods = manifest.get("methods")
    if isinstance(methods, dict):
        for class_name, names in methods.items():
            if not isinstance(class_name, str) or not isinstance(names, list):
                continue
            symbols.update(
                f"fluentqt.{class_name}.{name}"
                for name in names
                if isinstance(name, str)
            )
    return symbols


def validate_manifest(
    manifest: dict[str, Any],
    project_version: tuple[int, int, int],
) -> list[str]:
    errors: list[str] = []
    if manifest.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    expected_api_version = f"{project_version[0]}.{project_version[1]}"
    if manifest.get("api_version") != expected_api_version:
        errors.append(
            f"api_version must match the project major/minor "
            f"({expected_api_version!r})"
        )

    for section in ("classes", "enums", "functions", "variables"):
        values = manifest.get(section)
        if not isinstance(values, list) or not all(
            isinstance(value, str) and value for value in values
        ):
            errors.append(f"{section} must be an array of non-empty strings")
            continue
        if len(values) != len(set(values)):
            errors.append(f"{section} must not contain duplicates")

    variables = manifest.get("variables")
    if isinstance(variables, list):
        missing_variables = sorted(REQUIRED_VERSION_VARIABLES - set(variables))
        if missing_variables:
            errors.append(
                "variables must expose the version contract: "
                + ", ".join(missing_variables)
            )

    symbols = public_symbols(manifest)
    deprecations = manifest.get("deprecations")
    if not isinstance(deprecations, list):
        return [*errors, "deprecations must be an array"]

    seen_symbols: set[str] = set()
    for index, entry in enumerate(deprecations):
        context = f"deprecations[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{context} must be an object")
            continue
        missing = sorted(DEPRECATION_FIELDS - entry.keys())
        extra = sorted(entry.keys() - DEPRECATION_FIELDS)
        if missing:
            errors.append(f"{context} is missing fields: {', '.join(missing)}")
        if extra:
            errors.append(f"{context} has unknown fields: {', '.join(extra)}")
        if missing:
            continue

        symbol = entry["symbol"]
        if not isinstance(symbol, str) or symbol not in symbols:
            errors.append(f"{context}.symbol must name a manifest API symbol")
        elif symbol in seen_symbols:
            errors.append(f"duplicate deprecation symbol: {symbol}")
        else:
            seen_symbols.add(symbol)

        try:
            deprecated_in = parse_semver(
                entry["deprecated_in"], f"{context}.deprecated_in"
            )
            remove_in = parse_semver(entry["remove_in"], f"{context}.remove_in")
        except ValueError as error:
            errors.append(str(error))
            continue

        if deprecated_in > project_version:
            errors.append(f"{context}.deprecated_in cannot be in the future")
        if remove_in <= project_version:
            errors.append(
                f"{context}.remove_in has arrived; remove the API and ledger entry"
            )
        if remove_in[0] <= deprecated_in[0]:
            errors.append(
                f"{context}.remove_in must be a later major release"
            )

        replacement = entry["replacement"]
        if replacement is not None and (
            not isinstance(replacement, str)
            or replacement not in symbols
            or replacement == symbol
        ):
            errors.append(
                f"{context}.replacement must be null or another manifest symbol"
            )
        reason = entry["reason"]
        if not isinstance(reason, str) or not reason.strip():
            errors.append(f"{context}.reason must be a non-empty string")

    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--project-file", type=Path, default=DEFAULT_PROJECT_FILE)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        project_version_text, project_version = read_project_version(
            args.project_file
        )
        manifest = load_manifest(args.manifest)
    except ValueError as error:
        print(f"PySide6 API policy verification failed: {error}", file=sys.stderr)
        return 1

    errors = validate_manifest(manifest, project_version)
    if errors:
        for error in errors:
            print(f"{args.manifest}: {error}", file=sys.stderr)
        return 1

    print(
        "Verified PySide6 API {0} against FluentQt {1} with {2} active "
        "deprecation(s).".format(
            manifest["api_version"],
            project_version_text,
            len(manifest["deprecations"]),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
