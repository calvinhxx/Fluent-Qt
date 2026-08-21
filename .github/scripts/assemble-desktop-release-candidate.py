#!/usr/bin/env python3
"""Assemble and verify the immutable desktop release candidate."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = ROOT / ".github" / "package-matrix.json"
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
REPOSITORY_PATTERN = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
PACKAGE_TARGETS = {
    ("windows", "x64"): ("Windows", "x64", ".exe"),
    ("windows", "arm64"): ("Windows", "arm64", ".exe"),
    ("macos", "x64"): ("Darwin", "x86_64", ".dmg"),
    ("macos", "arm64"): ("Darwin", "arm64", ".dmg"),
    ("linux", "x64"): ("Linux", "x86_64", ".deb"),
    ("linux", "arm64"): ("Linux", "arm64", ".deb"),
}


class CandidateError(RuntimeError):
    """Raised when desktop packages violate the promotion contract."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CandidateError(f"cannot read valid JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise CandidateError(f"{path} must contain a JSON object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_identity(
    repository: str,
    commit: str,
    ci_run_id: str,
    ci_run_attempt: str,
) -> None:
    if REPOSITORY_PATTERN.fullmatch(repository) is None:
        raise CandidateError(f"invalid GitHub repository identity: {repository!r}")
    if COMMIT_PATTERN.fullmatch(commit) is None:
        raise CandidateError("commit must be a lowercase 40-character SHA")
    if not ci_run_id.isdigit() or int(ci_run_id) <= 0:
        raise CandidateError("CI run id must be a positive integer")
    if not ci_run_attempt.isdigit() or int(ci_run_attempt) <= 0:
        raise CandidateError("CI run attempt must be a positive integer")


def selected_scenarios(
    catalog: dict[str, Any], package_set: str
) -> list[dict[str, Any]]:
    if package_set not in {"smoke", "standard"}:
        raise CandidateError(f"unsupported package set: {package_set!r}")
    scenarios = catalog.get("scenarios")
    if not isinstance(scenarios, list):
        raise CandidateError("package catalog must contain a scenarios array")
    selected = [
        scenario
        for scenario in scenarios
        if isinstance(scenario, dict) and scenario.get(package_set) is True
    ]
    expected_count = 2 if package_set == "smoke" else 9
    if len(selected) != expected_count:
        raise CandidateError(
            f"{package_set} package set must contain {expected_count} scenarios, "
            f"found {len(selected)}"
        )
    return sorted(selected, key=lambda scenario: str(scenario.get("id", "")))


def expected_artifact_name(scenario: dict[str, Any]) -> str:
    return f"fluentqt-desktop-package-{scenario['id']}"


def expected_package_filename(scenario: dict[str, Any], version: str) -> str:
    try:
        system, architecture, extension = PACKAGE_TARGETS[
            (str(scenario["platform"]), str(scenario["arch"]))
        ]
    except KeyError as error:
        raise CandidateError(
            f"scenario {scenario.get('id')!r} has an unsupported platform target"
        ) from error
    suffix = str(scenario.get("asset_suffix", ""))
    suffix_text = f"-{suffix}" if suffix else ""
    return (
        f"Fluent-Qt-Gallery-{version}-{system}-{architecture}"
        f"{suffix_text}{extension}"
    )


def expected_checksum_text(records: list[dict[str, Any]]) -> str:
    return "".join(
        f"{record['sha256']}  {record['filename']}\n"
        for record in sorted(records, key=lambda item: str(item["filename"]))
    )


def assemble_candidate(
    *,
    input_dir: Path,
    output_dir: Path,
    catalog_path: Path,
    version: str,
    package_set: str,
    repository: str,
    commit: str,
    ci_run_id: str,
    ci_run_attempt: str,
) -> dict[str, Any]:
    if VERSION_PATTERN.fullmatch(version) is None:
        raise CandidateError(f"invalid release version: {version!r}")
    validate_identity(repository, commit, ci_run_id, ci_run_attempt)
    if not input_dir.is_dir():
        raise CandidateError(f"input artifact directory does not exist: {input_dir}")
    if output_dir.exists():
        raise CandidateError(f"output candidate directory already exists: {output_dir}")

    scenarios = selected_scenarios(load_json(catalog_path), package_set)
    expected_artifacts = {
        expected_artifact_name(scenario): scenario for scenario in scenarios
    }
    actual_entries = sorted(input_dir.iterdir(), key=lambda path: path.name)
    root_files = [path.name for path in actual_entries if not path.is_dir()]
    if root_files:
        raise CandidateError(
            "desktop artifact root must contain directories only: "
            + ", ".join(root_files)
        )
    actual_artifacts = {path.name: path for path in actual_entries}
    missing = sorted(set(expected_artifacts) - set(actual_artifacts))
    extra = sorted(set(actual_artifacts) - set(expected_artifacts))
    if missing or extra:
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing))
        if extra:
            details.append("unexpected: " + ", ".join(extra))
        raise CandidateError("desktop artifact set mismatch (" + "; ".join(details) + ")")

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_dir.parent) as temporary:
        staging = Path(temporary) / output_dir.name
        dist = staging / "dist"
        dist.mkdir(parents=True)
        records: list[dict[str, Any]] = []
        for artifact_name, scenario in sorted(expected_artifacts.items()):
            artifact_dir = actual_artifacts[artifact_name]
            files = sorted(
                path
                for path in artifact_dir.rglob("*")
                if path.is_file() and not path.is_symlink()
            )
            expected_filename = expected_package_filename(scenario, version)
            if len(files) != 1 or files[0].name != expected_filename:
                raise CandidateError(
                    f"artifact {artifact_name} must contain only "
                    f"{expected_filename}, found {[path.name for path in files]}"
                )
            destination = dist / expected_filename
            shutil.copy2(files[0], destination)
            records.append(
                {
                    "scenario_id": scenario["id"],
                    "filename": expected_filename,
                    "sha256": sha256_file(destination),
                    "size": destination.stat().st_size,
                }
            )

        manifest = {
            "schema_version": 1,
            "version": version,
            "package_set": package_set,
            "source": {
                "repository": repository,
                "commit": commit,
                "ci_run_id": int(ci_run_id),
                "ci_run_attempt": int(ci_run_attempt),
            },
            "packages": records,
        }
        (staging / "desktop-release-manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        (staging / "DESKTOP_SHA256SUMS.txt").write_text(
            expected_checksum_text(records), encoding="utf-8"
        )
        staging.replace(output_dir)
    return manifest


def verify_candidate(
    *,
    candidate_dir: Path,
    catalog_path: Path,
    version: str,
    package_set: str,
    repository: str,
    commit: str,
    ci_run_id: str,
    ci_run_attempt: str,
) -> dict[str, Any]:
    validate_identity(repository, commit, ci_run_id, ci_run_attempt)
    manifest_path = candidate_dir / "desktop-release-manifest.json"
    manifest = load_json(manifest_path)
    expected_source = {
        "repository": repository,
        "commit": commit,
        "ci_run_id": int(ci_run_id),
        "ci_run_attempt": int(ci_run_attempt),
    }
    if manifest.get("schema_version") != 1:
        raise CandidateError("desktop candidate schema_version must be 1")
    if manifest.get("version") != version:
        raise CandidateError("desktop candidate version does not match the release")
    if manifest.get("package_set") != package_set:
        raise CandidateError("desktop candidate package set does not match the release")
    if manifest.get("source") != expected_source:
        raise CandidateError("desktop candidate source identity does not match the run")

    scenarios = selected_scenarios(load_json(catalog_path), package_set)
    expected_names = {
        str(scenario["id"]): expected_package_filename(scenario, version)
        for scenario in scenarios
    }
    packages = manifest.get("packages")
    if not isinstance(packages, list):
        raise CandidateError("desktop candidate packages must be an array")
    records = {
        str(record.get("scenario_id")): record
        for record in packages
        if isinstance(record, dict)
    }
    if set(records) != set(expected_names) or len(records) != len(packages):
        raise CandidateError("desktop candidate manifest has an unexpected scenario set")

    dist = candidate_dir / "dist"
    if not dist.is_dir():
        raise CandidateError("desktop candidate has no dist directory")
    actual_files = sorted(path.name for path in dist.iterdir() if path.is_file())
    if actual_files != sorted(expected_names.values()):
        raise CandidateError("desktop candidate dist file set does not match the catalog")
    for scenario_id, filename in expected_names.items():
        record = records[scenario_id]
        package = dist / filename
        if record.get("filename") != filename:
            raise CandidateError(f"desktop candidate filename mismatch for {scenario_id}")
        if record.get("size") != package.stat().st_size:
            raise CandidateError(f"desktop candidate size mismatch for {filename}")
        if record.get("sha256") != sha256_file(package):
            raise CandidateError(f"desktop candidate checksum mismatch for {filename}")

    checksum_path = candidate_dir / "DESKTOP_SHA256SUMS.txt"
    try:
        checksum_text = checksum_path.read_text(encoding="utf-8")
    except OSError as error:
        raise CandidateError(f"cannot read {checksum_path}: {error}") from error
    if checksum_text != expected_checksum_text(packages):
        raise CandidateError("DESKTOP_SHA256SUMS.txt does not match the manifest")
    return manifest


def add_identity_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--version", required=True)
    parser.add_argument("--package-set", choices=("smoke", "standard"), required=True)
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--commit", default=os.environ.get("GITHUB_SHA", ""))
    parser.add_argument("--ci-run-id", default=os.environ.get("GITHUB_RUN_ID", ""))
    parser.add_argument(
        "--ci-run-attempt", default=os.environ.get("GITHUB_RUN_ATTEMPT", "")
    )
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    assemble = subparsers.add_parser("assemble")
    assemble.add_argument("--input", type=Path, required=True)
    assemble.add_argument("--output", type=Path, required=True)
    add_identity_arguments(assemble)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--candidate", type=Path, required=True)
    add_identity_arguments(verify)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "assemble":
            assemble_candidate(
                input_dir=args.input,
                output_dir=args.output,
                catalog_path=args.catalog,
                version=args.version,
                package_set=args.package_set,
                repository=args.repository,
                commit=args.commit,
                ci_run_id=args.ci_run_id,
                ci_run_attempt=args.ci_run_attempt,
            )
        else:
            verify_candidate(
                candidate_dir=args.candidate,
                catalog_path=args.catalog,
                version=args.version,
                package_set=args.package_set,
                repository=args.repository,
                commit=args.commit,
                ci_run_id=args.ci_run_id,
                ci_run_attempt=args.ci_run_attempt,
            )
    except (CandidateError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"Desktop release candidate {args.command} validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
