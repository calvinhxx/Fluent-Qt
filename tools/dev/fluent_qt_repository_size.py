#!/usr/bin/env python3
"""Check tracked content bytes; Git history and ignored build caches are separate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def inventory(root: Path, ref: str | None = None) -> dict[str, int]:
    if ref:
        records = subprocess.check_output(["git", "ls-tree", "-rlz", ref], cwd=root).split(b"\0")
        result = {}
        for record in records:
            if not record:
                continue
            metadata, name = record.split(b"\t", 1)
            _, kind, _, size = metadata.split()
            if kind == b"blob":
                result[name.decode()] = int(size)
        return result
    # Exclude untracked experiments and ignored caches. Missing tracked files
    # are pending deletions and do not contribute to the proposed worktree.
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).split(b"\0")
    return {name.decode(): (root / name.decode()).lstat().st_size
            for name in names if name and (root / name.decode()).is_file()}


def ignored(root: Path, names: list[str]) -> set[str]:
    # .gitignore is the single list of generated outputs and maintainer inputs.
    # --no-index also reports tracked paths, which is exactly what must fail.
    result = subprocess.run(["git", "check-ignore", "--no-index", "-z", "--stdin"], cwd=root,
                            input=b"".join(name.encode() + b"\0" for name in names),
                            capture_output=True)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.decode(errors="replace").strip() or "git check-ignore failed")
    return {name.decode() for name in result.stdout.split(b"\0") if name}


def check(files: dict[str, int], budget: dict, ignored_names: set[str] = frozenset()) -> dict:
    violations = []
    total = sum(files.values())
    if total > budget["max_total_bytes"]:
        violations.append(f"Tracked content exceeds {budget['max_total_bytes']} bytes: {total}")
    for name, size in sorted(files.items()):
        if name in ignored_names:
            violations.append(f"Tracked file matches .gitignore (generated output or maintainer input): {name}")
        allowance = budget["large_files"].get(name)
        limit = allowance["max_bytes"] if allowance else budget["max_file_bytes"]
        if size > limit:
            violations.append(f"{name}: {size} bytes exceeds {limit}; review the asset and its budget")
    return {"files": len(files), "tracked_content_bytes": total,
            "measurement": "file content only; excludes .git, filesystem allocation and untracked files",
            "largest": [{"path": p, "bytes": n} for p, n in sorted(files.items(), key=lambda x: -x[1])[:10]],
            "violations": violations}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=ROOT)
    parser.add_argument("--ref", help="Measure a committed Git tree (checked against the current .gitignore rules)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    budget = json.loads((args.project_root / "tools/dev/repository-size-budget.json").read_text())
    if budget.get("schema_version") != 1:
        parser.error("Unsupported repository size budget")
    files = inventory(args.project_root, args.ref)
    result = check(files, budget, ignored(args.project_root, list(files)))
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Tracked file content: {result['tracked_content_bytes'] / 1_000_000:.2f} MB in {result['files']} files; excludes .git.")
        for violation in result["violations"]:
            print(violation)
    return int(bool(result["violations"]))


if __name__ == "__main__":
    raise SystemExit(main())
