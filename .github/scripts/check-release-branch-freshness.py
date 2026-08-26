#!/usr/bin/env python3

"""Fail early when a release pull request no longer contains current main."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from urllib.parse import quote


def gh_json(endpoint: str) -> object:
    result = subprocess.run(
        ["gh", "api", endpoint],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(detail or f"gh api {endpoint} failed")
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"gh api returned invalid JSON for {endpoint}") from error


def current_base_sha(repository: str, base_ref: str) -> str:
    endpoint = f"repos/{repository}/commits/{quote(base_ref, safe='')}"
    payload = gh_json(endpoint)
    if not isinstance(payload, dict) or not isinstance(payload.get("sha"), str):
        raise RuntimeError("GitHub did not return the current base commit")
    return payload["sha"]


def behind_count(repository: str, base_sha: str, head_sha: str) -> int:
    endpoint = f"repos/{repository}/compare/{base_sha}...{head_sha}"
    payload = gh_json(endpoint)
    value = payload.get("behind_by") if isinstance(payload, dict) else None
    if not isinstance(value, int) or value < 0:
        raise RuntimeError("GitHub returned an invalid behind_by value")
    return value


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True)
    parser.add_argument("--base-ref", required=True)
    parser.add_argument("--head-sha", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        base_sha = current_base_sha(args.repository, args.base_ref)
        behind_by = behind_count(args.repository, base_sha, args.head_sha)
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    if behind_by:
        print(
            f"error: release branch is {behind_by} commit(s) behind "
            f"{args.base_ref}; rebase before starting expensive matrices",
            file=sys.stderr,
        )
        return 1
    print(f"Release branch contains current {args.base_ref} commit {base_sha}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
