#!/usr/bin/env python3
"""Generate maintainer changelogs and curated public release notes."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import subprocess
import sys
from collections import OrderedDict


CONVENTIONAL_RE = re.compile(
    r"^(?P<type>[a-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?: (?P<summary>.+)$"
)
BREAKING_RE = re.compile(r"^BREAKING(?: |-)?CHANGE:\s*(?P<text>.*)$")
FOOTER_RE = re.compile(r"^[A-Za-z][A-Za-z-]*: .+")
RELEASE_COMMIT_RE = re.compile(
    r"^chore(?:\([^)]*\))?: v[0-9]+\.[0-9]+\.[0-9]+(?:-rc\.[0-9]+)?$"
)
RELEASE_TAG_RE = re.compile(r"^v[0-9]+\.[0-9]+\.[0-9]+(?:-rc\.[0-9]+)?$")

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
CURATED_PUBLIC_NOTES_DIR = REPO_ROOT / "docs" / "releases"

SECTIONS = OrderedDict(
    [
        ("breaking", "Breaking Changes"),
        ("feat", "Features"),
        ("fix", "Bug Fixes"),
        ("perf", "Performance"),
        ("build_ci", "Build & CI"),
        ("docs", "Documentation"),
        ("refactor", "Refactoring"),
        ("test", "Tests"),
        ("maintenance", "Maintenance"),
        ("revert", "Reverts"),
        ("other", "Other"),
    ]
)

PUBLIC_SECTIONS = OrderedDict(
    [
        ("breaking", "Breaking Changes"),
        ("feat", "Features"),
        ("fix", "Bug Fixes"),
        ("perf", "Performance"),
    ]
)

INTERNAL_KEYWORDS = (
    "automation",
    "build",
    "changelog",
    "ci",
    "cmake",
    "packag",
    "release",
    "smoke",
    "test",
    "validation",
    "workflow",
)

TYPE_TO_SECTION = {
    "feat": "feat",
    "fix": "fix",
    "perf": "perf",
    "build": "build_ci",
    "ci": "build_ci",
    "docs": "docs",
    "refactor": "refactor",
    "test": "test",
    "chore": "maintenance",
    "style": "maintenance",
    "revert": "revert",
}


@dataclasses.dataclass(frozen=True)
class Commit:
    sha: str
    subject: str
    body: str


@dataclasses.dataclass(frozen=True)
class Entry:
    sha: str
    summary: str
    scope: str | None


def run_git(args: list[str]) -> str:
    result = subprocess.run(
        ["git", *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout


def resolve_commit(ref: str) -> str:
    return run_git(["rev-list", "-n", "1", ref]).strip()


def previous_release_tag(ref: str) -> str | None:
    commit = resolve_commit(ref)
    try:
        return run_git(
            [
                "describe",
                "--tags",
                "--match",
                "v[0-9]*.[0-9]*.[0-9]*",
                "--abbrev=0",
                f"{commit}^",
            ]
        ).strip()
    except RuntimeError:
        return None


def commit_date(ref: str) -> str:
    return run_git(["log", "-1", "--format=%cs", ref]).strip()


def read_commits(from_ref: str | None, to_ref: str) -> list[Commit]:
    revision = f"{from_ref}..{to_ref}" if from_ref else to_ref
    output = run_git(
        [
            "log",
            "--no-merges",
            "--reverse",
            "--format=%H%x1f%s%x1f%b%x1e",
            revision,
        ]
    )

    commits: list[Commit] = []
    for record in output.split("\x1e"):
        record = record.strip("\n")
        if not record:
            continue
        parts = record.split("\x1f", 2)
        if len(parts) < 2:
            continue
        sha = parts[0].strip()
        subject = parts[1].strip()
        body = parts[2].strip() if len(parts) > 2 else ""
        commits.append(Commit(sha=sha, subject=subject, body=body))
    return commits


def breaking_footer(body: str) -> str | None:
    lines = body.splitlines()
    for index, line in enumerate(lines):
        match = BREAKING_RE.match(line.strip())
        if not match:
            continue

        text_parts = [match.group("text").strip()]
        for following in lines[index + 1 :]:
            stripped = following.strip()
            if not stripped or FOOTER_RE.match(stripped):
                break
            text_parts.append(stripped)
        text = " ".join(part for part in text_parts if part).strip()
        return text or None
    return None


def format_entry(entry: Entry) -> str:
    prefix = f"{entry.scope}: " if entry.scope else ""
    return f"- {prefix}{entry.summary} (`{entry.sha[:7]}`)"


def format_public_entry(entry: Entry) -> str:
    prefix = f"{entry.scope}: " if entry.scope else ""
    return f"- {prefix}{entry.summary}"


def is_internal_entry(section_key: str, entry: Entry) -> bool:
    if section_key in {"build_ci", "docs", "refactor", "test", "maintenance", "other"}:
        return True
    text = f"{entry.scope or ''} {entry.summary}".lower()
    return any(keyword in text for keyword in INTERNAL_KEYWORDS)


def join_words(words: list[str]) -> str:
    if len(words) == 1:
        return words[0]
    if len(words) == 2:
        return f"{words[0]} and {words[1]}"
    return f"{', '.join(words[:-1])}, and {words[-1]}"


def maintenance_entry(areas: set[str]) -> str:
    ordered_areas: list[str] = []
    if "release" in areas:
        ordered_areas.extend(["release packaging", "CI automation"])
    if "tests" in areas:
        ordered_areas.append("test coverage")
    if "docs" in areas:
        ordered_areas.append("documentation")
    if "internal" in areas:
        ordered_areas.append("internal maintenance")
    if not ordered_areas:
        ordered_areas.append("project maintenance")
    return f"- Improved {join_words(ordered_areas)}."


def maintenance_area(section_key: str, entry: Entry) -> str:
    text = f"{entry.scope or ''} {entry.summary}".lower()
    if section_key == "test" or "test" in text:
        return "tests"
    if section_key == "docs":
        return "docs"
    if section_key == "build_ci" or any(
        keyword in text
        for keyword in (
            "automation",
            "build",
            "changelog",
            "ci",
            "cmake",
            "packag",
            "release",
            "smoke",
            "validation",
            "workflow",
        )
    ):
        return "release"
    return "internal"


def classify(commits: list[Commit]) -> tuple[OrderedDict[str, list[Entry]], list[Commit]]:
    grouped: OrderedDict[str, list[Entry]] = OrderedDict((key, []) for key in SECTIONS)
    unknown: list[Commit] = []

    for commit in commits:
        if RELEASE_COMMIT_RE.match(commit.subject):
            continue

        if commit.subject.startswith("Revert "):
            grouped["revert"].append(Entry(commit.sha, commit.subject, None))
            continue

        match = CONVENTIONAL_RE.match(commit.subject)
        if not match:
            grouped["other"].append(Entry(commit.sha, commit.subject, None))
            unknown.append(commit)
            continue

        commit_type = match.group("type")
        scope = match.group("scope")
        summary = match.group("summary")
        section = TYPE_TO_SECTION.get(commit_type, "other")
        if section == "other":
            unknown.append(commit)

        entry = Entry(commit.sha, summary, scope)
        grouped[section].append(entry)

        breaking_text = breaking_footer(commit.body)
        if match.group("breaking") or breaking_text:
            grouped["breaking"].append(
                Entry(commit.sha, breaking_text or summary, scope)
            )

    return grouped, unknown


def render_maintainer_notes(
    release_name: str,
    release_date: str,
    from_ref: str | None,
    grouped: OrderedDict[str, list[Entry]],
) -> str:
    lines = [f"## {release_name} - {release_date}", ""]
    if from_ref:
        lines.extend([f"_Changes since `{from_ref}`._", ""])

    wrote_section = False
    for section_key, title in SECTIONS.items():
        entries = grouped[section_key]
        if not entries:
            continue
        wrote_section = True
        lines.extend([f"### {title}", ""])
        lines.extend(format_entry(entry) for entry in entries)
        lines.append("")

    if not wrote_section:
        lines.extend(["No notable changes.", ""])

    return "\n".join(lines).rstrip() + "\n"


def render_public_notes(
    release_name: str,
    release_date: str,
    from_ref: str | None,
    grouped: OrderedDict[str, list[Entry]],
) -> str:
    lines = [f"## {release_name} - {release_date}", ""]
    if from_ref:
        lines.extend([f"_Changes since `{from_ref}`._", ""])

    public_grouped: OrderedDict[str, list[Entry]] = OrderedDict(
        (key, []) for key in PUBLIC_SECTIONS
    )
    maintenance_areas: set[str] = set()

    for section_key, entries in grouped.items():
        for entry in entries:
            if section_key == "breaking" or (
                section_key in PUBLIC_SECTIONS
                and not is_internal_entry(section_key, entry)
            ):
                public_grouped[section_key].append(entry)
            else:
                maintenance_areas.add(maintenance_area(section_key, entry))

    wrote_section = False
    for section_key, title in PUBLIC_SECTIONS.items():
        entries = public_grouped[section_key]
        if not entries:
            continue
        wrote_section = True
        lines.extend([f"### {title}", ""])
        lines.extend(format_public_entry(entry) for entry in entries)
        lines.append("")

    if maintenance_areas:
        wrote_section = True
        lines.extend(["### Maintenance", "", maintenance_entry(maintenance_areas), ""])

    if not wrote_section:
        lines.extend(["No notable changes.", ""])

    return "\n".join(lines).rstrip() + "\n"


def default_curated_public_notes(tag: str) -> pathlib.Path | None:
    if not RELEASE_TAG_RE.fullmatch(tag):
        return None
    return CURATED_PUBLIC_NOTES_DIR / f"{tag}.md"


def read_curated_public_notes(path: pathlib.Path) -> str:
    try:
        notes = path.read_text(encoding="utf-8")
    except OSError as error:
        raise RuntimeError(
            f"Could not read curated public notes at {path}: {error}"
        ) from error

    notes = notes.strip()
    if not notes:
        raise RuntimeError(f"Curated public notes are empty: {path}")
    return notes + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate maintainer changelogs and curated public release notes."
    )
    parser.add_argument("--from", dest="from_ref", help="Previous tag or commit.")
    parser.add_argument("--to", dest="to_ref", help="Target tag or commit. Defaults to HEAD.")
    parser.add_argument(
        "--tag",
        help="Release tag. Also used as --to, and auto-detects the previous release tag when --from is omitted.",
    )
    parser.add_argument("--output", help="Write release notes to this file.")
    parser.add_argument(
        "--audience",
        choices=("public", "maintainer"),
        default="public",
        help=(
            "Generate curated public notes when available or a detailed maintainer "
            "changelog. Defaults to public."
        ),
    )
    parser.add_argument(
        "--public-notes",
        metavar="PATH",
        help=(
            "Use this reviewed Markdown file for public notes. With --tag, "
            "docs/releases/<tag>.md is discovered automatically."
        ),
    )
    parser.add_argument(
        "--require-curated",
        action="store_true",
        help="Fail instead of publishing a commit-derived public draft.",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail when commits cannot be classified by Conventional Commit rules.",
    )
    args = parser.parse_args()

    if args.tag and args.to_ref:
        parser.error("--tag cannot be combined with --to")
    if args.audience == "maintainer" and (
        args.public_notes or args.require_curated
    ):
        parser.error(
            "--public-notes and --require-curated only apply to --audience public"
        )
    if args.require_curated and not (args.public_notes or args.tag):
        parser.error("--require-curated requires --tag or --public-notes")
    return args


def main() -> int:
    args = parse_args()
    to_ref = args.tag or args.to_ref or "HEAD"
    from_ref = args.from_ref
    if args.tag and not from_ref:
        from_ref = previous_release_tag(args.tag)

    commits = read_commits(from_ref, to_ref)
    grouped, unknown = classify(commits)
    release_name = args.tag or to_ref
    if args.audience == "maintainer":
        notes = render_maintainer_notes(
            release_name, commit_date(to_ref), from_ref, grouped
        )
    else:
        curated_path: pathlib.Path | None = None
        if args.public_notes:
            curated_path = pathlib.Path(args.public_notes)
        elif args.tag:
            candidate = default_curated_public_notes(args.tag)
            if candidate and candidate.is_file():
                curated_path = candidate

        if curated_path:
            try:
                notes = read_curated_public_notes(curated_path)
            except RuntimeError as error:
                print(f"error: {error}", file=sys.stderr)
                return 2
        elif args.require_curated:
            expected = (
                default_curated_public_notes(args.tag) if args.tag else None
            )
            location = expected or pathlib.Path(args.public_notes or "<unspecified>")
            print(
                f"error: Curated public notes are required but were not found: {location}",
                file=sys.stderr,
            )
            return 2
        else:
            if args.tag:
                print(
                    "warning: No curated public notes were found; generating a "
                    "commit-derived draft for review.",
                    file=sys.stderr,
                )
            notes = render_public_notes(
                release_name, commit_date(to_ref), from_ref, grouped
            )

    if args.output:
        pathlib.Path(args.output).write_text(notes, encoding="utf-8", newline="\n")
    else:
        sys.stdout.write(notes)

    if unknown:
        print("Unclassified commits:", file=sys.stderr)
        for commit in unknown:
            print(f"  {commit.sha[:7]} {commit.subject}", file=sys.stderr)
        if args.check:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
