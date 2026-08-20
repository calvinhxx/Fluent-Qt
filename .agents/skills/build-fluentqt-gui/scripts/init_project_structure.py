#!/usr/bin/env python3
"""Initialize a FluentQt application architecture manifest and directory layout."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


SKILL_ROOT = Path(__file__).resolve().parent.parent
TEMPLATES_PATH = SKILL_ROOT / "assets" / "project-structure-templates.json"


def _inside(root: Path, value: str, label: str) -> tuple[Path, str]:
    candidate = (root / value).resolve()
    try:
        relative = candidate.relative_to(root)
    except ValueError as error:
        raise SystemExit(f"{label} must stay inside project root: {value}") from error
    return candidate, relative.as_posix()


def _load_template(language: str, profile: str) -> tuple[str, dict[str, Any]]:
    data = json.loads(TEMPLATES_PATH.read_text(encoding="utf-8"))
    template_id = f"{language}-{profile}"
    try:
        return template_id, data["templates"][template_id]
    except KeyError as error:
        raise SystemExit(f"Unknown project template: {template_id}") from error


def _manifest(
    *,
    template_id: str,
    template: dict[str, Any],
    application: str,
    source_root: str,
    tests_root: str,
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "application": application,
        "template": template_id,
        "language": template["language"],
        "profile": template["profile"],
        "source_root": source_root,
        "tests_root": tests_root,
        "layers": template["layers"],
        "shell_files": [],
        "allowed_source_root_files": [],
        "budgets": template["budgets"],
        "dependency_rules": {
            "domain_forbids_ui": True,
            "application_forbids_ui": True,
            "ui_forbids_process_ownership": True,
            "app_is_composition_root": True,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create the standard FluentQt project layout and architecture manifest."
    )
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--application", required=True)
    parser.add_argument("--language", choices=("cpp", "pyside6"), required=True)
    parser.add_argument("--profile", choices=("lite", "full"), required=True)
    parser.add_argument("--source-root", default="src")
    parser.add_argument("--tests-root", default="tests")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    project_root = Path(args.project_root).expanduser().resolve()
    source_root, source_relative = _inside(project_root, args.source_root, "source root")
    tests_root, tests_relative = _inside(project_root, args.tests_root, "tests root")
    manifest_path = project_root / ".fluentqt" / "architecture.json"
    template_id, template = _load_template(args.language, args.profile)

    if manifest_path.exists() and not args.force:
        raise SystemExit(
            f"Architecture manifest already exists: {manifest_path}. Use --force to replace it."
        )

    source_directories = [source_root / item for item in template["directories"]]
    test_directories = [tests_root / item for item in template["test_directories"]]
    all_directories = [source_root, tests_root, manifest_path.parent]
    all_directories.extend(source_directories)
    all_directories.extend(test_directories)

    manifest = _manifest(
        template_id=template_id,
        template=template,
        application=args.application,
        source_root=source_relative,
        tests_root=tests_relative,
    )

    print(f"template: {template_id}")
    for directory in all_directories:
        print(f"directory: {directory.relative_to(project_root)}")
    print(f"manifest: {manifest_path.relative_to(project_root)}")

    if args.dry_run:
        return 0

    project_root.mkdir(parents=True, exist_ok=True)
    for directory in all_directories:
        directory.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
