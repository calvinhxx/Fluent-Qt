#!/usr/bin/env python3

"""Build a deterministic, installable build-fluentqt-gui Agent Skill zip."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
from typing import Iterable
import zipfile


SKILL_NAME = "build-fluentqt-gui"
FIXED_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
VERSION_PATTERN = re.compile(
    r"project\(FluentQt\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)"
)
PACKAGE_VERSION_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._+-]*")
REQUIRED_SKILL_FILES = (
    "SKILL.md",
    "LICENSE.txt",
    "agents/openai.yaml",
    "assets/benchmarks/agent-run-workspace.json",
    "assets/composition-recipes.json",
    "assets/fluentqt-ai-catalog.json",
    "assets/project-structure-templates.json",
    "references/art-direction.md",
    "references/component-selection.md",
    "references/design-intelligence.md",
    "references/experience-differentiation.md",
    "references/iconography.md",
    "references/performance-lifecycle.md",
    "references/premium-shell.md",
    "references/product-reference-patterns.md",
    "references/project-architecture.md",
    "references/product-copy.md",
    "references/signature-surface.md",
    "references/theme-system.md",
    "references/visual-evidence-contract.md",
    "references/visual-refinement.md",
    "scripts/init_design_brief.py",
    "scripts/init_project_structure.py",
    "scripts/init_visual_evidence.py",
    "scripts/query_catalog.py",
    "scripts/render_design_board.py",
    "scripts/render_visual_review.py",
    "scripts/validate_design_brief.py",
    "scripts/validate_project_structure.py",
    "scripts/validate_visual_evidence.py",
)


def project_version(project_root: Path) -> str:
    contents = (project_root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = VERSION_PATTERN.search(contents)
    if match is None:
        raise ValueError("Could not read FluentQt version from CMakeLists.txt")
    return match.group(1)


def skill_files(skill_root: Path) -> list[Path]:
    files = [
        path
        for path in skill_root.rglob("*")
        if path.is_file()
        and "__pycache__" not in path.parts
        and path.suffix not in {".pyc", ".pyo"}
        and path.name != ".DS_Store"
    ]
    return sorted(files, key=lambda path: path.relative_to(skill_root).as_posix())


def validate_skill_root(skill_root: Path) -> None:
    missing = [
        relative
        for relative in REQUIRED_SKILL_FILES
        if not (skill_root / relative).is_file()
    ]
    if missing:
        raise ValueError("Skill source is incomplete: " + ", ".join(missing))
    skill_text = (skill_root / "SKILL.md").read_text(encoding="utf-8")
    if not skill_text.startswith(
        "---\nname: build-fluentqt-gui\ndescription:"
    ):
        raise ValueError("Skill source has invalid SKILL.md frontmatter")


def _zip_info(archive_path: str, *, executable: bool) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(archive_path, FIXED_TIMESTAMP)
    info.create_system = 3
    mode = 0o100755 if executable else 0o100644
    info.external_attr = mode << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def build_skill_package(
    project_root: Path, output_dir: Path, version: str | None = None
) -> Path:
    project_root = project_root.resolve()
    output_dir = output_dir.resolve()
    skill_root = project_root / ".agents" / "skills" / SKILL_NAME
    validate_skill_root(skill_root)

    resolved_version = version or project_version(project_root)
    if PACKAGE_VERSION_PATTERN.fullmatch(resolved_version) is None:
        raise ValueError(f"Invalid package version: {resolved_version!r}")
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / f"{SKILL_NAME}-{resolved_version}.zip"
    archive.unlink(missing_ok=True)

    with zipfile.ZipFile(
        archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as package:
        for source in skill_files(skill_root):
            relative = source.relative_to(skill_root).as_posix()
            package.writestr(
                _zip_info(
                    f"{SKILL_NAME}/{relative}",
                    executable=source.suffix == ".py",
                ),
                source.read_bytes(),
            )
    return archive


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=default_root)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--version")
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    archive = build_skill_package(
        args.project_root, args.output_dir, version=args.version
    )
    print(archive)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
