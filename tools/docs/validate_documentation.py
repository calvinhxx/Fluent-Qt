#!/usr/bin/env python3

"""Validate the FluentQt documentation tree and local Markdown links."""

from __future__ import annotations

import argparse
from pathlib import Path, PurePath
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
GENERATED_NAV_RE = re.compile(
    r"^<!-- docs-nav:(top|bottom):start -->\n.*?^<!-- docs-nav:\1:end -->$",
    re.MULTILINE | re.DOTALL,
)
HTML_COMMENT_RE = re.compile(r"<!--.*?-->", re.DOTALL)
CODE_FENCE_RE = re.compile(r"^[ \t]*(?:>[ \t]*)*(`{3,}|~{3,})(.*)$")
INLINE_CODE_RE = re.compile(r"(?<![\\`])(`+)(?!`)[\s\S]*?(?<!`)\1(?!`)")
LINK_DESTINATION_RE = re.compile(r"(?<=\]\()[ \t]*(?:<[^>\n]*>|[^\s)]+)")
REFERENCE_DESTINATION_RE = re.compile(
    r"^ {0,3}\[[^\]\n]+\]:[ \t]*(?:<[^>\n]*>|\S+)", re.MULTILINE
)
REFERENCE_ID_RE = re.compile(r"(?<=\])\[[^\]\n]*\]")
URL_RE = re.compile(r"\b(?:https?://|mailto:)[^\s<>\"']+")
HTML_DESTINATION_RE = re.compile(r"\b(?:href|src)\s*=\s*([\"']).*?\1")


def relative_posix_path(path: PurePath, root: PurePath) -> str:
    """Return a root-relative path with navigation-manifest separators."""

    return path.relative_to(root).as_posix()


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

    # The index still lists unstaged deletions. Navigation and link checks
    # report any remaining references to those missing documents.
    return [
        project_root / relative
        for relative in output.splitlines()
        if relative and (project_root / relative).is_file()
    ]


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


def english_document_cjk_lines(text: str, relative: str) -> list[int]:
    """Find CJK text outside the documented exceptions, preserving line numbers."""

    if relative in {"README.md", "docs/SUMMARY.md"} or relative.endswith(".zh-CN.md"):
        return []

    def blank(value: str) -> str:
        return re.sub(r"[^\n]", " ", value)

    # Generated navigation is checked against the manifest separately. A link
    # to an explicitly translated page may contain its translated title.
    text = GENERATED_NAV_RE.sub(lambda match: blank(match.group()), text)
    lines: list[str] = []
    fence = ""
    for line in text.splitlines(keepends=True):
        marker = CODE_FENCE_RE.match(line)
        if fence:
            if (
                marker
                and marker[1][0] == fence[0]
                and len(marker[1]) >= len(fence)
                and not marker[2].strip()
            ):
                fence = ""
            lines.append(blank(line))
        elif marker and not (marker[1][0] == "`" and "`" in marker[2]):
            fence = marker[1]
            lines.append(blank(line))
        else:
            lines.append(line)

    text = "".join(lines)
    for pattern in (
        INLINE_CODE_RE,
        HTML_COMMENT_RE,
        LINK_DESTINATION_RE,
        REFERENCE_DESTINATION_RE,
        REFERENCE_ID_RE,
        HTML_DESTINATION_RE,
        URL_RE,
    ):
        text = pattern.sub(lambda match: blank(match.group()), text)

    violations: list[int] = []
    in_glossary = False
    in_translation_table = False
    for number, line in enumerate(text.splitlines(), start=1):
        if relative == "docs/development/comment-style.md":
            if re.match(r"^#{1,2} ", line):
                in_glossary = line.strip() == "## Glossary"
                in_translation_table = False
            cells = line.strip().split("|")
            if in_glossary and len(cells) == 5 and not cells[0] and not cells[-1]:
                if [cell.strip() for cell in cells[1:-1]] == ["English", "Chinese", "Notes"]:
                    in_translation_table = True
                if in_translation_table:
                    # Only the translation column is exempt, not the guide or
                    # the explanatory notes in the same table.
                    cells[2] = ""
                    line = "|".join(cells)
            else:
                in_translation_table = False
        if CJK_RE.search(line):
            violations.append(number)
    return violations


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
                display_path = relative_posix_path(path, project_root)
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
            relative_posix_path(path, project_root / "docs")
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
        relative = relative_posix_path(path, project_root)

        if len(FENCE_RE.findall(text)) % 2:
            errors.append(f"unbalanced fenced code block: {relative}")

        for line in cjk_hard_wrap_lines(text):
            errors.append(f"hard-wrapped CJK prose: {relative}:{line}-{line + 1}")

        for line in english_document_cjk_lines(text, relative):
            errors.append(
                f"CJK prose in English document: {relative}:{line} "
                "(use English or a .zh-CN.md translation)"
            )

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
