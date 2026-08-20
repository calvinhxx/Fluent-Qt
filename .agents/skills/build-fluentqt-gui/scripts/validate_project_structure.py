#!/usr/bin/env python3
"""Validate the maintainability contract of a FluentQt application tree."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterable


CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
PYTHON_EXTENSIONS = {".py"}
CPP_UI_PATTERN = re.compile(
    r"#\s*include\s*[<\"](?:QWidget|QFrame|QLayout|QtWidgets|FluentQt|components/|windowing/)"
)
PYTHON_UI_PATTERN = re.compile(r"(?:PySide6\.QtWidgets|PyQt\d?\.QtWidgets|\bfluentqt\b)")
CPP_PROCESS_OWNER_PATTERN = re.compile(
    r"(?:new\s+QProcess\b|QProcess::startDetached\b|\bQProcess\s+[A-Za-z_]\w*\s*[({;])"
)
PYTHON_PROCESS_OWNER_PATTERN = re.compile(
    r"(?:subprocess\.(?:Popen|run|call)\s*\(|QProcess\s*\(|asyncio\.create_subprocess_)"
)


def _load_manifest(project_root: Path, manifest_arg: str) -> tuple[Path, dict]:
    manifest_path = (project_root / manifest_arg).resolve()
    try:
        manifest_path.relative_to(project_root)
    except ValueError as error:
        raise SystemExit("manifest must stay inside project root") from error
    if not manifest_path.is_file():
        raise SystemExit(f"architecture manifest not found: {manifest_path}")
    return manifest_path, json.loads(manifest_path.read_text(encoding="utf-8"))


def _code_files(root: Path, extensions: set[str]) -> Iterable[Path]:
    if not root.is_dir():
        return []
    return (
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in extensions
    )


def _line_count(path: Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as stream:
        return sum(1 for _ in stream)


def _relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def _layer_paths(source_root: Path, manifest: dict, name: str) -> list[Path]:
    return [(source_root / value).resolve() for value in manifest.get("layers", {}).get(name, [])]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check a FluentQt application's layer layout and source budgets."
    )
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--manifest", default=".fluentqt/architecture.json")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    project_root = Path(args.project_root).expanduser().resolve()
    manifest_path, manifest = _load_manifest(project_root, args.manifest)
    errors: list[str] = []
    warnings: list[str] = []

    if manifest.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    language = manifest.get("language")
    profile = manifest.get("profile")
    if language not in {"cpp", "pyside6"}:
        errors.append("language must be cpp or pyside6")
    if profile not in {"lite", "full"}:
        errors.append("profile must be lite or full")

    source_root = (project_root / manifest.get("source_root", "src")).resolve()
    tests_root = (project_root / manifest.get("tests_root", "tests")).resolve()
    for label, root in (("source_root", source_root), ("tests_root", tests_root)):
        try:
            root.relative_to(project_root)
        except ValueError:
            errors.append(f"{label} must stay inside project root")
        if not root.is_dir():
            errors.append(f"{label} does not exist: {_relative(root, project_root)}")

    required_layers = {"app", "application", "ui"}
    if profile == "full":
        required_layers.update({"domain", "infrastructure"})
    layers = manifest.get("layers", {})
    for layer in sorted(required_layers):
        paths = layers.get(layer)
        if not isinstance(paths, list) or not paths:
            errors.append(f"missing layer mapping: {layer}")
            continue
        for value in paths:
            path = (source_root / value).resolve()
            if not path.is_dir():
                errors.append(f"missing layer directory: {_relative(path, project_root)}")

    if profile == "full" and source_root.is_dir():
        extensions = CPP_EXTENSIONS if language == "cpp" else PYTHON_EXTENSIONS
        allowed = set(manifest.get("allowed_source_root_files", []))
        direct_files = [
            path
            for path in source_root.iterdir()
            if path.is_file() and path.suffix.lower() in extensions and path.name not in allowed
        ]
        for path in sorted(direct_files):
            errors.append(
                "full profile forbids code files directly under source_root: "
                + _relative(path, project_root)
            )

    extensions = CPP_EXTENSIONS if language == "cpp" else PYTHON_EXTENSIONS
    source_files = list(_code_files(source_root, extensions))
    budgets = manifest.get("budgets", {})
    source_limit = int(budgets.get("source_file_max_lines", 800 if language == "cpp" else 650))
    shell_limit = int(budgets.get("shell_max_lines", 500 if language == "cpp" else 400))
    member_limit = int(budgets.get("shell_member_max", 48 if language == "cpp" else 40))

    for path in sorted(source_files):
        lines = _line_count(path)
        if lines > source_limit:
            errors.append(
                f"source file exceeds {source_limit} lines: {_relative(path, project_root)} ({lines})"
            )

    shell_entries = manifest.get("shell_files", [])
    if profile == "full" and not shell_entries:
        errors.append("full profile must declare shell_files")
    for value in shell_entries:
        path = (source_root / value).resolve()
        if not path.is_file():
            errors.append(f"shell file does not exist: {_relative(path, project_root)}")
            continue
        lines = _line_count(path)
        if lines > shell_limit:
            errors.append(
                f"shell file exceeds {shell_limit} lines: {_relative(path, project_root)} ({lines})"
            )
        text = _read(path)
        if language == "cpp" and path.suffix.lower() in {".h", ".hh", ".hpp"}:
            members = len(set(re.findall(r"\bm_[A-Za-z_]\w*\b", text)))
            if members > member_limit:
                errors.append(
                    f"shell header exceeds {member_limit} member fields: "
                    f"{_relative(path, project_root)} ({members})"
                )
        if language == "pyside6":
            members = len(set(re.findall(r"\bself\._[A-Za-z_]\w*", text)))
            if members > member_limit:
                errors.append(
                    f"shell module exceeds {member_limit} instance fields: "
                    f"{_relative(path, project_root)} ({members})"
                )

    rules = manifest.get("dependency_rules", {})
    ui_pattern = CPP_UI_PATTERN if language == "cpp" else PYTHON_UI_PATTERN
    for layer_name in ("domain", "application"):
        if not rules.get(f"{layer_name}_forbids_ui", False):
            continue
        for layer_root in _layer_paths(source_root, manifest, layer_name):
            for path in _code_files(layer_root, extensions):
                if ui_pattern.search(_read(path)):
                    errors.append(
                        f"{layer_name} layer imports UI: {_relative(path, project_root)}"
                    )

    if rules.get("ui_forbids_process_ownership", False):
        process_pattern = (
            CPP_PROCESS_OWNER_PATTERN if language == "cpp" else PYTHON_PROCESS_OWNER_PATTERN
        )
        for layer_root in _layer_paths(source_root, manifest, "ui"):
            for path in _code_files(layer_root, extensions):
                if process_pattern.search(_read(path)):
                    errors.append(f"UI owns a process: {_relative(path, project_root)}")

    if profile == "full" and tests_root.is_dir():
        for layer in ("domain", "application", "infrastructure", "ui"):
            if not (tests_root / layer).is_dir():
                warnings.append(f"tests do not mirror layer: {_relative(tests_root / layer, project_root)}")

    print(f"manifest: {_relative(manifest_path, project_root)}")
    for warning in warnings:
        print(f"WARNING: {warning}")
    for error in errors:
        print(f"ERROR: {error}")

    if errors or (args.strict and warnings):
        print(
            f"project structure: FAIL ({len(errors)} error(s), {len(warnings)} warning(s))"
        )
        return 1
    print(f"project structure: PASS ({len(source_files)} source file(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
