#!/usr/bin/env python3
"""Check or format only explicitly selected FluentQt C++ files."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EXPECTED_CLANG_FORMAT_VERSION = "15.0.0"
CPP_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"})


def parse_clang_format_version(output: str) -> str | None:
    """Return the LLVM formatter version, ignoring wrapper version banners."""
    match = re.search(r"(?:^|\n)clang-format version (\d+(?:\.\d+){1,2})\b", output)
    return match.group(1) if match else None


def default_formatter_command() -> str:
    """Return the repository-specific formatter override or the PATH default."""
    return os.environ.get("FLUENTQT_CLANG_FORMAT") or "clang-format"


def is_cpp_path(path: Path) -> bool:
    return path.suffix.lower() in CPP_SUFFIXES


def _repo_relative(path: Path, *, require_exists: bool) -> Path:
    candidate = path if path.is_absolute() else PROJECT_ROOT / path
    candidate = candidate.resolve()
    try:
        relative = candidate.relative_to(PROJECT_ROOT)
    except ValueError as error:
        raise ValueError(f"path is outside the repository: {path}") from error
    if require_exists and not candidate.is_file():
        raise ValueError(f"file does not exist: {path}")
    return relative


def normalize_cpp_files(paths: Iterable[str], *, require_exists: bool) -> list[Path]:
    selected: set[Path] = set()
    for raw_path in paths:
        relative = _repo_relative(Path(raw_path), require_exists=require_exists)
        if is_cpp_path(relative):
            selected.add(relative)
    return sorted(selected)


def changed_files_from(base: str, head: str = "HEAD") -> list[Path]:
    result = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMR", f"{base}...{head}", "--"],
        cwd=PROJECT_ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(f"could not list files changed from {base}: {detail}")
    return normalize_cpp_files(result.stdout.splitlines(), require_exists=False)


def staged_files() -> list[Path]:
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "--"],
        cwd=PROJECT_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return normalize_cpp_files(result.stdout.splitlines(), require_exists=False)


def working_tree_files() -> list[Path]:
    tracked = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=ACMR", "HEAD", "--"],
        cwd=PROJECT_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "--"],
        cwd=PROJECT_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return normalize_cpp_files(
        [*tracked.stdout.splitlines(), *untracked.stdout.splitlines()],
        require_exists=False,
    )


def resolve_formatter(command: str) -> str:
    executable = shutil.which(command)
    if executable is None:
        raise RuntimeError(
            f"{command!r} was not found; install clang-format "
            f"{EXPECTED_CLANG_FORMAT_VERSION} or pass --clang-format PATH"
        )

    result = subprocess.run(
        [executable, "--version"], check=False, capture_output=True, text=True
    )
    version_output = "\n".join(part for part in (result.stdout, result.stderr) if part)
    version = parse_clang_format_version(version_output)
    if result.returncode != 0 or version != EXPECTED_CLANG_FORMAT_VERSION:
        found = version or version_output.strip() or "unknown"
        raise RuntimeError(
            f"clang-format {EXPECTED_CLANG_FORMAT_VERSION} is required; "
            f"found {found} at {executable}"
        )
    return executable


def git_file_contents(revision: str, path: Path) -> bytes:
    """Read one repository file from HEAD or the staged index."""
    object_name = (
        f":{path.as_posix()}"
        if revision == "INDEX"
        else f"{revision}:{path.as_posix()}"
    )
    result = subprocess.run(
        ["git", "show", object_name],
        cwd=PROJECT_ROOT,
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        detail = result.stderr.decode(errors="replace").strip()
        raise RuntimeError(f"could not read {path} from {revision}: {detail}")
    return result.stdout


def print_fix_hint() -> None:
    print(
        "C++ formatting failed. Check out the affected branch, run "
        "--working-tree --fix, review the whole-file diff, and stage the result again.",
        file=sys.stderr,
    )


def check_git_files(executable: str, files: Sequence[Path], *, revision: str) -> int:
    """Check committed or staged blobs without consulting working-tree copies."""
    mismatches: list[Path] = []
    for path in files:
        original = git_file_contents(revision, path)
        result = subprocess.run(
            [
                executable,
                "--style=file",
                f"--assume-filename={path.as_posix()}",
            ],
            cwd=PROJECT_ROOT,
            check=False,
            input=original,
            capture_output=True,
        )
        if result.returncode != 0:
            detail = result.stderr.decode(errors="replace").strip()
            raise RuntimeError(f"clang-format failed for {path}: {detail}")
        if result.stdout != original:
            mismatches.append(path)

    if mismatches:
        for path in mismatches:
            print(f"{path}: requires clang-format", file=sys.stderr)
        print_fix_hint()
        return 1

    print("C++ formatting passed.")
    return 0


def run_formatter(executable: str, files: Sequence[Path], *, fix: bool) -> int:
    if not files:
        print("No selected C++ files require formatting checks.")
        return 0

    command = [executable, "--style=file"]
    if fix:
        command.append("-i")
    else:
        command.extend(("--dry-run", "--Werror"))
    command.extend(str(path) for path in files)

    print(f"{'Formatting' if fix else 'Checking'} {len(files)} C++ file(s)...")
    result = subprocess.run(command, cwd=PROJECT_ROOT, check=False)
    if result.returncode == 0:
        print("C++ formatting passed." if not fix else "C++ formatting updated.")
    elif not fix:
        print_fix_hint()
    return result.returncode


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Check selected C++ files without imposing a one-time full-repository reformat."
        )
    )
    parser.add_argument("files", nargs="*", help="explicit C++ files to check")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--changed-from",
        metavar="GIT_REF",
        help="check C++ files changed between GIT_REF and the selected head revision",
    )
    parser.add_argument(
        "--head",
        default="HEAD",
        metavar="GIT_REF",
        help="revision to check with --changed-from (default: HEAD)",
    )
    selection.add_argument("--staged", action="store_true", help="check staged C++ files")
    selection.add_argument(
        "--working-tree",
        action="store_true",
        help="check staged and unstaged C++ files relative to HEAD",
    )
    parser.add_argument(
        "--clang-format",
        default=default_formatter_command(),
        metavar="PATH",
        help=(
            "formatter executable (must report version "
            f"{EXPECTED_CLANG_FORMAT_VERSION}; defaults to FLUENTQT_CLANG_FORMAT "
            "or clang-format)"
        ),
    )
    parser.add_argument("--fix", action="store_true", help="format selected files in place")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    selection_count = sum(
        (bool(args.files), bool(args.changed_from), args.staged, args.working_tree)
    )
    if selection_count != 1:
        parser.error("provide explicit files or choose exactly one changed-file mode")
    if args.head != "HEAD" and not args.changed_from:
        parser.error("--head requires --changed-from")
    if args.fix and args.head != "HEAD":
        parser.error("--fix cannot update an arbitrary --head revision; check it out first")
    try:
        git_revision: str | None = None
        if args.changed_from:
            files = changed_files_from(args.changed_from, args.head)
            git_revision = args.head
        elif args.staged:
            files = staged_files()
            git_revision = "INDEX"
        elif args.working_tree:
            files = working_tree_files()
        else:
            files = normalize_cpp_files(args.files, require_exists=True)

        if not files:
            print("No selected C++ files require formatting checks.")
            return 0
        formatter = resolve_formatter(args.clang_format)
        if git_revision is not None and not args.fix:
            print(f"Checking {len(files)} C++ file(s) from {git_revision}...")
            return check_git_files(formatter, files, revision=git_revision)
        return run_formatter(formatter, files, fix=args.fix)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
