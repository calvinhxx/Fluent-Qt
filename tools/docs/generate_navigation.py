#!/usr/bin/env python3

"""Generate FluentQt documentation contents and leaf-page navigation."""

from __future__ import annotations

import argparse
import json
import posixpath
from pathlib import Path
import re


TOP_START = "<!-- docs-nav:top:start -->"
TOP_END = "<!-- docs-nav:top:end -->"
BOTTOM_START = "<!-- docs-nav:bottom:start -->"
BOTTOM_END = "<!-- docs-nav:bottom:end -->"

TOP_RE = re.compile(
    rf"\n?{re.escape(TOP_START)}.*?{re.escape(TOP_END)}\n?",
    re.DOTALL,
)
BOTTOM_RE = re.compile(
    rf"\n?{re.escape(BOTTOM_START)}.*?{re.escape(BOTTOM_END)}\n?",
    re.DOTALL,
)
H1_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)
RELEASE_RE = re.compile(r"^releases/v(?P<version>\d+\.\d+\.\d+)\.md$")


def load_manifest(project_root: Path) -> dict[str, object]:
    path = project_root / "docs/navigation.json"
    return json.loads(path.read_text(encoding="utf-8"))


def page_title(relative: str, text: str) -> str:
    match = H1_RE.search(text)
    if match:
        return re.sub(r"\s+\([^)]*\)$", "", match.group(1)).strip()
    release = RELEASE_RE.match(relative)
    if release:
        return f"Fluent-Qt {release.group('version')}"
    return Path(relative).stem.replace("-", " ").title()


def relative_link(source: str, target: str) -> str:
    source_path = posixpath.normpath(posixpath.join("docs", source))
    target_path = posixpath.normpath(posixpath.join("docs", target))
    source_dir = posixpath.dirname(source_path) or "."
    return posixpath.relpath(target_path, source_dir)


def strip_navigation(text: str) -> str:
    text = TOP_RE.sub("\n", text)
    text = BOTTOM_RE.sub("\n", text)
    return text.strip() + "\n"


def ensure_release_heading(relative: str, text: str) -> str:
    if H1_RE.search(text):
        return text
    match = RELEASE_RE.match(relative)
    if not match:
        return text
    version = match.group("version")
    return (
        f"# Fluent-Qt {version}\n\n"
        "> **Status:** Historical release note\n\n"
        f"{text.lstrip()}"
    )


def insertion_index(lines: list[str]) -> int:
    heading = next((index for index, line in enumerate(lines) if line.startswith("# ")), 0)
    cursor = heading + 1
    while cursor < len(lines) and not lines[cursor].strip():
        cursor += 1
    if cursor < len(lines) and lines[cursor].startswith("> **Status:**"):
        while cursor < len(lines) and lines[cursor].startswith(">"):
            cursor += 1
        while cursor < len(lines) and not lines[cursor].strip():
            cursor += 1
    return cursor


def navigation_block(
    *,
    relative: str,
    section_title: str,
    section_index: str,
    group_title: str,
    previous: tuple[str, str] | None,
    following: tuple[str, str] | None,
    bottom: bool,
) -> str:
    home = relative_link(relative, "README.md")
    contents = relative_link(relative, "SUMMARY.md")
    section = relative_link(relative, section_index)
    breadcrumb = (
        f"[Documentation]({home}) › "
        f"[{section_title}]({section}) › {group_title}"
    )

    jumps: list[str] = []
    if previous:
        title, target = previous
        jumps.append(f"[← {title}]({relative_link(relative, target)})")
    jumps.append(f"[Contents]({contents})")
    jumps.append(f"[{section_title} index]({section})")
    if following:
        title, target = following
        jumps.append(f"[{title} →]({relative_link(relative, target)})")

    start = BOTTOM_START if bottom else TOP_START
    end = BOTTOM_END if bottom else TOP_END
    parts = [start]
    if bottom:
        parts.append("---")
    else:
        parts.extend((breadcrumb, ""))
    parts.extend((" · ".join(jumps), end))
    return "\n".join(parts)


def render_page(
    *,
    relative: str,
    text: str,
    section_title: str,
    section_index: str,
    group_title: str,
    previous: tuple[str, str] | None,
    following: tuple[str, str] | None,
) -> str:
    clean = ensure_release_heading(relative, strip_navigation(text))
    lines = clean.rstrip().splitlines()
    top = navigation_block(
        relative=relative,
        section_title=section_title,
        section_index=section_index,
        group_title=group_title,
        previous=previous,
        following=following,
        bottom=False,
    )
    bottom = navigation_block(
        relative=relative,
        section_title=section_title,
        section_index=section_index,
        group_title=group_title,
        previous=previous,
        following=following,
        bottom=True,
    )
    at = insertion_index(lines)
    while at > 0 and not lines[at - 1].strip():
        del lines[at - 1]
        at -= 1
    lines[at:at] = ["", top, ""]
    return "\n".join(lines).rstrip() + f"\n\n{bottom}\n"


def navigation_entries(
    manifest: dict[str, object], project_root: Path
) -> tuple[list[dict[str, str]], dict[str, str]]:
    entries: list[dict[str, str]] = []
    titles: dict[str, str] = {}
    docs_root = project_root / "docs"

    for section in manifest["sections"]:
        section_title = section["title"]
        section_index = section["index"]
        index_text = (docs_root / section_index).read_text(encoding="utf-8")
        titles[section_index] = page_title(section_index, index_text)
        for group in section["groups"]:
            for relative in group["pages"]:
                text = (docs_root / relative).read_text(encoding="utf-8")
                titles[relative] = page_title(relative, text)
                entries.append(
                    {
                        "path": relative,
                        "section_title": section_title,
                        "section_index": section_index,
                        "group_title": group["title"],
                    }
                )
    return entries, titles


def render_summary(
    manifest: dict[str, object], titles: dict[str, str]
) -> str:
    lines = [
        "# Documentation contents",
        "",
        "> **Status:** Current guide (generated index)",
        "",
        "[Documentation home](README.md) · "
        "[Search docs](https://github.com/calvinhxx/Fluent-Qt/search?q=language%3AMarkdown&type=code)",
        "",
        "This is the complete reader-facing tree. Generated catalogs and JSON",
        "schemas remain linked from their owning guides instead of appearing as",
        "standalone reading tasks.",
        "",
    ]
    for section in manifest["sections"]:
        index = section["index"]
        lines.append(f"- [{section['title']}]({index})")
        for group in section["groups"]:
            lines.append(f"  - **{group['title']}**")
            for relative in group["pages"]:
                lines.append(f"    - [{titles[relative]}]({relative})")
    return "\n".join(lines) + "\n"


def expected_outputs(project_root: Path) -> dict[Path, str]:
    manifest = load_manifest(project_root)
    docs_root = project_root / "docs"
    entries, titles = navigation_entries(manifest, project_root)
    outputs = {
        docs_root / manifest["summary"]: render_summary(manifest, titles),
    }

    for index, entry in enumerate(entries):
        previous = None
        following = None
        if (
            index > 0
            and entries[index - 1]["section_index"] == entry["section_index"]
            and entries[index - 1]["group_title"] == entry["group_title"]
        ):
            previous_path = entries[index - 1]["path"]
            previous = (titles[previous_path], previous_path)
        if (
            index + 1 < len(entries)
            and entries[index + 1]["section_index"] == entry["section_index"]
            and entries[index + 1]["group_title"] == entry["group_title"]
        ):
            following_path = entries[index + 1]["path"]
            following = (titles[following_path], following_path)

        path = docs_root / entry["path"]
        outputs[path] = render_page(
            relative=entry["path"],
            text=path.read_text(encoding="utf-8"),
            section_title=entry["section_title"],
            section_index=entry["section_index"],
            group_title=entry["group_title"],
            previous=previous,
            following=following,
        )
    return outputs


def generate(project_root: Path, *, check: bool) -> list[str]:
    stale: list[str] = []
    for path, expected in expected_outputs(project_root).items():
        actual = path.read_text(encoding="utf-8") if path.exists() else ""
        if actual == expected:
            continue
        stale.append(str(path.relative_to(project_root)))
        if not check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(expected, encoding="utf-8")
    return stale


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = args.project_root.resolve()
    stale = generate(root, check=args.check)
    if args.check and stale:
        for relative in stale:
            print(f"stale documentation navigation: {relative}")
        return 1
    if stale:
        print(f"Updated documentation navigation in {len(stale)} files.")
    else:
        print("Documentation navigation is current.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
