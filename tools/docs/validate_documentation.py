#!/usr/bin/env python3

"""Validate the FluentQt documentation tree and local Markdown links."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys
import urllib.parse

from generate_navigation import generate as generate_navigation
from generate_navigation import load_manifest


REQUIRED_INDEXES = (
    "docs/README.md",
    "docs/SUMMARY.md",
    "docs/ai/README.md",
    "docs/architecture/README.md",
    "docs/community/README.md",
    "docs/design-languages/README.md",
    "docs/development/README.md",
    "docs/releases/README.md",
)

INLINE_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
REFERENCE_LINK_RE = re.compile(r"^\s*\[[^\]]+\]:\s*(\S+)", re.MULTILINE)
HTML_LINK_RE = re.compile(r"(?:href|src)=[\"']([^\"']+)[\"']")
FENCE_RE = re.compile(r"^\s*```", re.MULTILINE)
CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")
FENCE_LINE_RE = re.compile(r"^\s*(?:```|~~~)")
LIST_ITEM_RE = re.compile(r"^\s*(?:[-+*]|\d+[.)])\s+")
NON_PROSE_RE = re.compile(
    r"^\s*(?:#{1,6}\s|>|\||<!--|-->|<[/!?A-Za-z]|(?:---+|===+|___+)\s*$)"
)


def markdown_files(project_root: Path) -> list[Path]:
    """Return tracked and new Markdown files, with a source-tree fallback."""

    try:
        output = subprocess.check_output(
            (
                "git",
                "-C",
                str(project_root),
                "ls-files",
                "--cached",
                "--others",
                "--exclude-standard",
                "*.md",
            ),
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        excluded = {".git", ".venv", "build", "dist", "node_modules"}
        return sorted(
            path
            for path in project_root.rglob("*.md")
            if not any(part in excluded or part.startswith("build-") for part in path.parts)
        )

    return [project_root / relative for relative in output.splitlines() if relative]


def local_target(raw_target: str) -> str | None:
    """Return a decoded local path, or None for external and anchor-only links."""

    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    target = target.split(' "', 1)[0].split(" '", 1)[0]
    if not target or target.startswith(
        ("#", "http://", "https://", "mailto:", "app://", "data:")
    ):
        return None
    return urllib.parse.unquote(target.split("#", 1)[0].split("?", 1)[0]) or None


def cjk_hard_wrap_lines(text: str) -> list[int]:
    """Return 1-based lines where CJK prose continues on the next source line."""

    lines = text.splitlines()
    kinds: list[str | None] = []
    in_fence = False
    for line in lines:
        if FENCE_LINE_RE.match(line):
            in_fence = not in_fence
            kinds.append(None)
            continue
        if in_fence:
            kinds.append(None)
            continue

        stripped = line.lstrip()
        indentation = len(line) - len(stripped)
        if (
            not stripped
            or indentation >= 4
            or NON_PROSE_RE.match(line)
            or REFERENCE_LINK_RE.match(line)
        ):
            kinds.append(None)
        elif LIST_ITEM_RE.match(line):
            kinds.append("list")
        else:
            kinds.append("prose")

    hard_wraps: list[int] = []
    for index in range(len(lines) - 1):
        current_kind = kinds[index]
        following_kind = kinds[index + 1]
        if current_kind is None or following_kind is None or following_kind == "list":
            continue
        current = lines[index]
        following = lines[index + 1]
        if current.endswith(("  ", "\\")) or current.rstrip().endswith(
            ("<br>", "<br/>", "<br />")
        ):
            continue
        if CJK_RE.search(current) and CJK_RE.search(following):
            hard_wraps.append(index + 1)
    return hard_wraps


def validate(project_root: Path) -> list[str]:
    project_root = project_root.resolve()
    errors: list[str] = []

    for relative in REQUIRED_INDEXES:
        if not (project_root / relative).is_file():
            errors.append(f"missing documentation index: {relative}")

    manifest_path = project_root / "docs/navigation.json"
    if not manifest_path.is_file():
        errors.append("missing documentation navigation manifest: docs/navigation.json")
    else:
        manifest = load_manifest(project_root)
        listed = {manifest["home"], manifest["summary"]}
        for section in manifest["sections"]:
            listed.add(section["index"])
            for group in section["groups"]:
                listed.update(group["pages"])

        listed_files_exist = True
        for relative in sorted(listed):
            path = (project_root / "docs" / relative).resolve()
            try:
                display_path = path.relative_to(project_root)
            except ValueError:
                errors.append(f"document escapes project root: docs/{relative}")
                listed_files_exist = False
                continue
            if not path.is_file():
                errors.append(
                    f"navigation.json points to a missing document: {display_path}"
                )
                listed_files_exist = False
                continue
            if "> **Status:**" not in path.read_text(encoding="utf-8"):
                errors.append(f"missing document status: {display_path}")

        actual = {
            str(path.relative_to(project_root / "docs"))
            for path in (project_root / "docs").rglob("*.md")
        }
        internal = {relative for relative in listed if not relative.startswith("../")}
        for relative in sorted(actual - internal):
            errors.append(f"document is missing from navigation.json: docs/{relative}")

        if listed_files_exist:
            for relative in generate_navigation(project_root, check=True):
                errors.append(f"stale documentation navigation: {relative}")

    for path in markdown_files(project_root):
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(project_root)

        if len(FENCE_RE.findall(text)) % 2:
            errors.append(f"unbalanced fenced code block: {relative}")

        for line in cjk_hard_wrap_lines(text):
            errors.append(f"hard-wrapped CJK prose: {relative}:{line}-{line + 1}")

        targets = [match.group(1) for match in INLINE_LINK_RE.finditer(text)]
        targets.extend(match.group(1) for match in REFERENCE_LINK_RE.finditer(text))
        targets.extend(match.group(1) for match in HTML_LINK_RE.finditer(text))
        for raw_target in targets:
            target = local_target(raw_target)
            if target is None:
                continue
            resolved = (
                project_root / target.lstrip("/")
                if target.startswith("/")
                else path.parent / target
            )
            if not resolved.exists():
                errors.append(f"broken local link: {relative} -> {raw_target}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run documentation-validator regression tests before validation",
    )
    args = parser.parse_args()
    project_root = args.project_root.resolve()

    if args.self_test:
        test_result = subprocess.run(
            (sys.executable, str(Path(__file__).with_name("test_validate_documentation.py"))),
            check=False,
        )
        if test_result.returncode:
            return test_result.returncode

    errors = validate(project_root)
    if errors:
        for error in errors:
            print(f"error: {error}")
        return 1

    print(
        "Documentation validation passed: "
        f"{len(REQUIRED_INDEXES)} indexes and "
        f"{len(markdown_files(project_root))} Markdown files."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
