#!/usr/bin/env python3

"""Verify that one PySide6 platform/architecture emitted every CI artifact."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / "bindings" / "pyside6" / "wheel-matrix.json"
SUPPORTED_TARGETS = {
    ("linux", "x64"),
    ("linux", "arm64"),
    ("windows", "x64"),
    ("windows", "arm64"),
    ("macos", "x64"),
    ("macos", "arm64"),
}


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def target_scenarios(
    catalog: dict[str, Any], platform: str, arch: str
) -> list[dict[str, Any]]:
    target = (platform, arch)
    if target not in SUPPORTED_TARGETS:
        raise ValueError(f"unsupported platform/architecture: {platform}/{arch}")

    scenarios = catalog.get("scenarios")
    if not isinstance(scenarios, list):
        raise ValueError("wheel matrix must contain a scenarios array")

    selected: list[dict[str, Any]] = []
    for scenario in scenarios:
        if not isinstance(scenario, dict):
            continue
        if (scenario.get("platform"), scenario.get("arch")) != target:
            continue
        if (
            scenario.get("release") is not True
            and scenario.get("compatibility") is not True
        ):
            continue
        selected.append(scenario)

    if not selected:
        raise ValueError(f"wheel matrix has no scenarios for {platform}/{arch}")
    return selected


def expected_artifacts(
    catalog: dict[str, Any], platform: str, arch: str
) -> list[str]:
    scenarios = target_scenarios(catalog, platform, arch)

    artifacts: list[str] = []
    for scenario in scenarios:
        suffix = scenario.get("artifact_suffix")
        if not isinstance(suffix, str) or not suffix:
            raise ValueError(f"scenario {scenario.get('id')!r} has no artifact suffix")
        artifacts.append(f"fluentqt-pyside6-{suffix}")
        if scenario.get("extended_acceptance") is True:
            artifacts.extend(
                (
                    f"fluentqt-pyside6-showcase-{suffix}",
                    f"fluentqt-pyside6-window-chrome-{suffix}",
                )
            )
    return artifacts


def expected_job_names(
    catalog: dict[str, Any], platform: str, arch: str
) -> list[str]:
    names: list[str] = []
    for scenario in target_scenarios(catalog, platform, arch):
        name = scenario.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError(f"scenario {scenario.get('id')!r} has no job name")
        names.append(name)
    return names


def read_artifact_names(path: Path) -> set[str]:
    return {
        line.strip()
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    }


def read_job_results(path: Path) -> list[tuple[str, str]]:
    results: list[tuple[str, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            name, conclusion = line.rsplit("\t", 1)
        except ValueError as error:
            raise ValueError(f"invalid job-result line: {line!r}") from error
        results.append((name, conclusion))
    return results


def matching_job_conclusions(
    expected_name: str, results: list[tuple[str, str]]
) -> list[str]:
    suffix = f" / {expected_name}"
    return [
        conclusion
        for actual_name, conclusion in results
        if actual_name == expected_name or actual_name.endswith(suffix)
    ]


def append_step_summary(
    display_name: str, scenario_count: int, artifact_count: int
) -> None:
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        return
    with Path(summary_path).open("a", encoding="utf-8") as stream:
        stream.write(f"## {display_name} passed\n\n")
        stream.write(
            f"Verified {scenario_count} toolchain lane(s) and "
            f"{artifact_count} required artifact(s).\n"
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", required=True)
    parser.add_argument("--arch", required=True)
    parser.add_argument("--display-name", required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--jobs", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        catalog = load_json(args.catalog)
        expected = expected_artifacts(catalog, args.platform, args.arch)
        expected_jobs = expected_job_names(catalog, args.platform, args.arch)
        actual = read_artifact_names(args.artifacts)
        job_results = read_job_results(args.jobs)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    failed_jobs: list[str] = []
    for expected_job in expected_jobs:
        conclusions = matching_job_conclusions(expected_job, job_results)
        if not conclusions or set(conclusions) != {"success"}:
            rendered = ", ".join(conclusions) if conclusions else "missing"
            failed_jobs.append(f"{expected_job} ({rendered})")
    if failed_jobs:
        print(
            f"error: {args.display_name} has incomplete jobs:",
            file=sys.stderr,
        )
        for job in failed_jobs:
            print(f"  - {job}", file=sys.stderr)
        return 1

    missing = [name for name in expected if name not in actual]
    if missing:
        print(
            f"error: {args.display_name} is missing required artifacts:",
            file=sys.stderr,
        )
        for name in missing:
            print(f"  - {name}", file=sys.stderr)
        return 1

    scenario_count = len(expected_jobs)
    print(
        f"Verified {args.display_name}: {scenario_count} lane(s), "
        f"{len(expected)} artifact(s)."
    )
    append_step_summary(args.display_name, scenario_count, len(expected))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
