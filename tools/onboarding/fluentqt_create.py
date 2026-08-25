#!/usr/bin/env python3

"""Generate a maintained FluentQt consumer starter in a new directory."""

from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import re
import tempfile
from typing import Iterable, Mapping


CREATOR_VERSION = "0.2.0"
ONBOARDING_ROOT = Path(__file__).resolve().parent
STARTERS_ROOT = ONBOARDING_ROOT / "starters"
STARTER_MANIFEST = STARTERS_ROOT / "manifest.json"


def _default_project_templates() -> Path:
    container = Path(__file__).resolve().parents[2]
    candidates = (
        container
        / ".agents"
        / "skills"
        / "build-fluentqt-gui"
        / "assets"
        / "project-structure-templates.json",
        container / "assets" / "project-structure-templates.json",
    )
    return next((path for path in candidates if path.is_file()), candidates[0])


PROJECT_TEMPLATES = _default_project_templates()
PLACEHOLDER = re.compile(r"@[A-Z][A-Z0-9_]*@")
IDENTIFIER = re.compile(r"^[a-z][a-z0-9-]*$")
ACCENT = re.compile(r"^#[0-9A-Fa-f]{6}$")


class CreateError(RuntimeError):
    """Raised when a starter cannot be generated safely."""


def _read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CreateError(f"Could not read {path}: {error}") from error


def _starter_id(language: str, starter: str) -> str:
    return f"{language}-{starter}"


def _derive_identifier(target: Path) -> str:
    value = target.name.lower().replace("_", "-")
    value = re.sub(r"[^a-z0-9-]+", "-", value)
    value = re.sub(r"-+", "-", value).strip("-")
    if value and value[0].isdigit():
        value = f"app-{value}"
    return value


def _validate_inputs(
    *, target: Path, application: str, identifier: str, accent: str
) -> None:
    if target.exists():
        raise CreateError(f"Target already exists: {target}")
    if not application or any(ord(character) < 32 for character in application):
        raise CreateError("Application name must be a non-empty single line.")
    if "@" in application:
        raise CreateError("Application name cannot contain '@'.")
    if not IDENTIFIER.fullmatch(identifier):
        raise CreateError(
            "Identifier must start with a lowercase letter and contain only "
            "lowercase letters, numbers, or hyphens."
        )
    if not ACCENT.fullmatch(accent):
        raise CreateError("Accent must use #RRGGBB format.")


def _replacement_values(
    *, application: str, identifier: str, accent: str
) -> dict[str, str]:
    module = identifier.replace("-", "_")
    target = module
    cpp_name = application.replace("\\", "\\\\").replace('"', '\\"')
    python_name = json.dumps(application, ensure_ascii=False)
    return {
        "@APP_NAME@": application,
        "@APP_NAME_CPP@": cpp_name,
        "@APP_NAME_PY@": python_name,
        "@APP_ID@": identifier,
        "@APP_TARGET@": target,
        "@APP_NAMESPACE@": module,
        "@APP_MODULE@": module,
        "@ACCENT@": accent.upper(),
    }


def _render(value: str, replacements: Mapping[str, str]) -> str:
    rendered = value
    for marker, replacement in replacements.items():
        rendered = rendered.replace(marker, replacement)
    unresolved = PLACEHOLDER.search(rendered)
    if unresolved:
        raise CreateError(f"Unresolved starter placeholder: {unresolved.group(0)}")
    return rendered


def _render_relative_path(path: Path, replacements: Mapping[str, str]) -> Path:
    parts = [_render(part, replacements) for part in path.parts]
    if parts[-1].endswith(".in"):
        parts[-1] = parts[-1][:-3]
    rendered = Path(*parts)
    if rendered.is_absolute() or ".." in rendered.parts:
        raise CreateError(f"Starter path escapes the project: {rendered}")
    return rendered


def _architecture_manifest(
    *,
    application: str,
    language: str,
    profile: str,
    module: str,
    shell_files: list[str],
    templates_path: Path,
) -> tuple[dict, list[str], list[str]]:
    templates = _read_json(templates_path)
    template_id = f"{language}-{profile}"
    try:
        template = copy.deepcopy(templates["templates"][template_id])
    except KeyError as error:
        raise CreateError(f"Missing architecture template: {template_id}") from error

    source_directories = list(template["directories"])
    layers = copy.deepcopy(template["layers"])
    if language == "pyside6":
        source_directories = [f"{module}/{value}" for value in source_directories]
        layers = {
            name: [f"{module}/{value}" for value in values]
            for name, values in layers.items()
        }
        shell_files = [f"{module}/{value}" for value in shell_files]

    manifest = {
        "schema_version": 1,
        "application": application,
        "template": template_id,
        "language": language,
        "profile": profile,
        "source_root": "src",
        "tests_root": "tests",
        "layers": layers,
        "shell_files": shell_files,
        "allowed_source_root_files": [],
        "budgets": template["budgets"],
        "dependency_rules": {
            "domain_forbids_ui": True,
            "application_forbids_ui": True,
            "ui_forbids_process_ownership": True,
            "app_is_composition_root": True,
        },
    }
    return manifest, source_directories, list(template["test_directories"])


def _template_definition(
    *, language: str, starter: str, manifest_path: Path
) -> tuple[str, dict]:
    manifest = _read_json(manifest_path)
    template_id = _starter_id(language, starter)
    try:
        definition = manifest["templates"][template_id]
    except KeyError as error:
        raise CreateError(f"Unknown starter: {template_id}") from error
    return template_id, definition


def _planned_files(
    *, template_root: Path, replacements: Mapping[str, str]
) -> list[tuple[Path, Path]]:
    files: list[tuple[Path, Path]] = []
    for source in sorted(template_root.rglob("*")):
        if source.is_symlink():
            raise CreateError(f"Starter templates cannot contain symlinks: {source}")
        if not source.is_file():
            continue
        relative = source.relative_to(template_root)
        files.append((source, _render_relative_path(relative, replacements)))
    if not files:
        raise CreateError(f"Starter template is empty: {template_root}")
    return files


def create_project(
    *,
    target: Path,
    application: str,
    identifier: str,
    language: str,
    starter: str,
    accent: str,
    dry_run: bool = False,
    starters_root: Path = STARTERS_ROOT,
    starter_manifest: Path = STARTER_MANIFEST,
    project_templates: Path = PROJECT_TEMPLATES,
) -> dict[str, object]:
    destination = target.expanduser().resolve()
    display_name = application.strip()
    app_id = identifier.strip()
    _validate_inputs(
        target=destination,
        application=display_name,
        identifier=app_id,
        accent=accent,
    )
    template_id, definition = _template_definition(
        language=language,
        starter=starter,
        manifest_path=starter_manifest,
    )
    replacements = _replacement_values(
        application=display_name,
        identifier=app_id,
        accent=accent,
    )
    template_root = starters_root / definition["directory"]
    files = _planned_files(template_root=template_root, replacements=replacements)
    module = replacements["@APP_MODULE@"]
    architecture, source_directories, test_directories = _architecture_manifest(
        application=display_name,
        language=language,
        profile=definition["profile"],
        module=module,
        shell_files=list(definition["shell_files"]),
        templates_path=project_templates,
    )
    relative_files = [path.as_posix() for _, path in files]
    relative_files.append(".fluentqt/architecture.json")
    relative_files.sort()
    next_steps = [
        _render(step, replacements) for step in definition["next_steps"]
    ]
    report: dict[str, object] = {
        "schema_version": 1,
        "creator_version": CREATOR_VERSION,
        "status": "planned" if dry_run else "created",
        "template": template_id,
        "language": language,
        "profile": definition["profile"],
        "application": display_name,
        "identifier": app_id,
        "target": str(destination),
        "files": relative_files,
        "next_steps": next_steps,
    }
    if dry_run:
        return report

    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=f".{destination.name}-fluentqt-", dir=destination.parent
    ) as temporary:
        stage = Path(temporary) / "project"
        stage.mkdir()
        for directory in source_directories:
            (stage / "src" / directory).mkdir(parents=True, exist_ok=True)
        for directory in test_directories:
            (stage / "tests" / directory).mkdir(parents=True, exist_ok=True)
        for source, relative in files:
            output = stage / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            content = _render(source.read_text(encoding="utf-8"), replacements)
            if str(destination) in content:
                raise CreateError(f"Generated file contains an absolute target path: {relative}")
            output.write_text(content, encoding="utf-8")
        architecture_path = stage / ".fluentqt" / "architecture.json"
        architecture_path.parent.mkdir(parents=True, exist_ok=True)
        architecture_path.write_text(
            json.dumps(architecture, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        os.replace(stage, destination)
    return report


def _print_report(report: Mapping[str, object], output_format: str) -> None:
    if output_format == "json":
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return
    verb = "Would create" if report["status"] == "planned" else "Created"
    print(f"{verb} {report['application']} at {report['target']}")
    print(f"Starter: {report['template']} ({len(report['files'])} files)")
    print("Next:")
    for step in report["next_steps"]:
        print(f"  {step}")


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", nargs="?", help="New project directory")
    parser.add_argument("--name", help="Application display name")
    parser.add_argument("--id", dest="identifier", help="Lowercase project identifier")
    parser.add_argument(
        "--language", choices=("cpp", "pyside6"), default="cpp"
    )
    parser.add_argument(
        "--starter", choices=("existing-qt", "workbench"), default="workbench"
    )
    parser.add_argument("--accent", default="#3A63E8")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--format", choices=("human", "json"), default="human")
    parser.add_argument("--list-templates", action="store_true")
    parser.add_argument("--version", action="version", version=CREATOR_VERSION)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    if args.list_templates:
        manifest = _read_json(STARTER_MANIFEST)
        if args.format == "json":
            print(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True))
        else:
            for template_id, definition in manifest["templates"].items():
                print(f"{template_id}: {definition['description']}")
        return 0
    if not args.target:
        raise SystemExit("target is required unless --list-templates is used")

    target = Path(args.target)
    identifier = args.identifier or _derive_identifier(target)
    application = args.name or target.name.replace("-", " ").replace("_", " ").title()
    try:
        report = create_project(
            target=target,
            application=application,
            identifier=identifier,
            language=args.language,
            starter=args.starter,
            accent=args.accent,
            dry_run=args.dry_run,
        )
    except CreateError as error:
        if args.format == "json":
            print(json.dumps({"schema_version": 1, "error": str(error)}, indent=2))
        else:
            print(f"Error: {error}")
        return 1
    _print_report(report, args.format)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
