#!/usr/bin/env python3
"""Run the cheap, deterministic gates before opening a stable-release PR."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VERSION_RE = re.compile(
    r"project\s*\(\s*FluentQt\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)"
)
STABLE_TAG_RE = re.compile(r"^v[0-9]+\.[0-9]+\.[0-9]+$")


class PreflightError(RuntimeError):
    pass


@dataclass(frozen=True)
class ReleaseContext:
    version: str
    branch: str
    base_ref: str
    head_sha: str
    previous_tag: str
    target_tag: str
    notes_path: Path


def run(
    command: list[str],
    *,
    root: Path,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=root,
        check=False,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def git_output(root: Path, *args: str) -> str:
    result = run(["git", *args], root=root, capture=True)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise PreflightError(detail or f"git {' '.join(args)} failed")
    return result.stdout.strip()


def project_version(root: Path) -> str:
    try:
        text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    except OSError as error:
        raise PreflightError(f"cannot read CMakeLists.txt: {error}") from error
    match = VERSION_RE.search(text)
    if not match:
        raise PreflightError("cannot resolve the FluentQt project version")
    return match.group(1)


def expected_branch(version: str) -> str:
    major, minor, _patch = version.split(".")
    return f"release/{major}.{minor}.x"


def latest_stable_tag(root: Path) -> str:
    tags = git_output(root, "tag", "--merged", "HEAD", "--sort=-version:refname")
    for tag in tags.splitlines():
        if STABLE_TAG_RE.fullmatch(tag):
            return tag
    raise PreflightError("no previous stable release tag is reachable from HEAD")


def collect_release_context(root: Path, base_ref: str) -> ReleaseContext:
    version = project_version(root)
    branch = git_output(root, "symbolic-ref", "--short", "HEAD")
    required_branch = expected_branch(version)
    if branch != required_branch:
        raise PreflightError(
            f"version {version} must be promoted from {required_branch}, not {branch}"
        )
    if git_output(root, "status", "--porcelain"):
        raise PreflightError("worktree is not clean; commit or remove local changes first")

    git_output(root, "rev-parse", "--verify", f"{base_ref}^{{commit}}")
    head_sha = git_output(root, "rev-parse", "HEAD^{commit}")
    ancestry = run(
        ["git", "merge-base", "--is-ancestor", base_ref, "HEAD"], root=root
    )
    if ancestry.returncode == 1:
        raise PreflightError(
            f"{branch} does not contain current {base_ref}; rebase before opening the PR"
        )
    if ancestry.returncode != 0:
        raise PreflightError(f"could not compare {branch} with {base_ref}")

    target_tag = f"v{version}"
    target = run(
        ["git", "show-ref", "--verify", "--quiet", f"refs/tags/{target_tag}"],
        root=root,
    )
    if target.returncode == 0:
        raise PreflightError(f"{target_tag} already exists")
    if target.returncode not in (0, 1):
        raise PreflightError(f"could not inspect refs/tags/{target_tag}")

    notes_path = root / "docs" / "releases" / f"{target_tag}.md"
    try:
        notes = notes_path.read_text(encoding="utf-8").strip()
    except OSError as error:
        raise PreflightError(f"cannot read curated notes at {notes_path}: {error}") from error
    if not notes:
        raise PreflightError(f"curated notes are empty: {notes_path}")

    return ReleaseContext(
        version=version,
        branch=branch,
        base_ref=base_ref,
        head_sha=head_sha,
        previous_tag=latest_stable_tag(root),
        target_tag=target_tag,
        notes_path=notes_path,
    )


def lightweight_checks(
    context: ReleaseContext, output_path: Path
) -> list[tuple[str, list[str]]]:
    python = sys.executable
    return [
        ("project metadata", [python, ".github/scripts/validate-project-metadata.py"]),
        ("desktop package matrix", [python, ".github/scripts/validate-package-matrix.py"]),
        ("Python wheel matrix", [python, ".github/scripts/validate-pyside-wheel-matrix.py"]),
        ("CI workflow boundaries", [python, ".github/scripts/validate-ci-workflow-boundaries.py"]),
        ("release preflight tests", [python, "scripts/release/test_preflight.py"]),
        ("release-branch freshness tests", [python, ".github/scripts/test_check_release_branch_freshness.py"]),
        ("AI assets", [python, "tools/ai/validate_ai_assets.py", "--project-root", "."]),
        ("onboarding doctor", [python, "tools/onboarding/test_fluentqt_doctor.py"]),
        ("onboarding create", [python, "tools/onboarding/test_fluentqt_create.py"]),
        ("onboarding trial", [python, "tools/onboarding/test_fluentqt_trial.py"]),
        ("Gallery wheel builder", [python, "bindings/pyside6/gallery/tests/test_wheel_builder.py"]),
        (
            "curated changelog",
            [
                python,
                "scripts/release/generate_changelog.py",
                "--from",
                context.previous_tag,
                "--to",
                "HEAD",
                "--public-notes",
                str(context.notes_path),
                "--require-curated",
                "--check",
                "--output",
                str(output_path),
            ],
        ),
    ]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fail fast on stable-release mistakes without compiling Qt."
    )
    parser.add_argument("--base-ref", default="origin/main")
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="Fetch origin and tags before checking the release branch.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.refresh:
        print("[preflight] Refreshing origin and tags...")
        fetched = run(["git", "fetch", "--prune", "--tags", "origin"], root=ROOT)
        if fetched.returncode != 0:
            return fetched.returncode

    try:
        context = collect_release_context(ROOT, args.base_ref)
    except PreflightError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory(prefix="fluentqt-release-preflight-") as temp:
        output_path = Path(temp) / "release-notes.md"
        checks = lightweight_checks(context, output_path)
        for index, (label, command) in enumerate(checks, start=1):
            print(f"[preflight {index}/{len(checks)}] {label}", flush=True)
            result = run(command, root=ROOT)
            if result.returncode != 0:
                print(f"error: {label} failed", file=sys.stderr)
                return result.returncode

    print(
        f"Release preflight passed for {context.target_tag} at "
        f"{context.head_sha[:12]} ({context.previous_tag}..HEAD)."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
