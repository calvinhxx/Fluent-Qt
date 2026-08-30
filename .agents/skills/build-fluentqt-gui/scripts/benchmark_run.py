#!/usr/bin/env python3

"""Initialize, validate, and summarize portable FluentQt cross-agent runs."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
from typing import Iterable


SKILL_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SPEC = SKILL_ROOT / "assets/benchmarks/agent-run-workspace.json"
AGENTS = ("codex", "cursor")
ARTIFACTS = (
    "design_brief",
    "project_structure",
    "visual_evidence",
    "inspector_report",
    "built_application",
)
CHECKS = (
    "project_structure",
    "build",
    "tests",
    "workflow",
    "inspector",
    "visual_evidence",
)
RUN_ID_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
INTEGRITY_SCHEMA_VERSION = 1


def _timestamp() -> str:
    return (
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    if path.is_file():
        digest.update(path.read_bytes())
        return digest.hexdigest()
    if not path.is_dir():
        raise ValueError(f"Cannot hash missing path: {path}")
    for child in sorted(item for item in path.rglob("*") if item.is_file()):
        if "__pycache__" in child.parts or child.suffix in {".pyc", ".pyo"}:
            continue
        relative = child.relative_to(path).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        digest.update(child.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _portable_path(path: Path, base: Path) -> str:
    resolved = path.resolve()
    try:
        return Path(os.path.relpath(resolved, start=base.resolve())).as_posix()
    except ValueError:
        # Windows cannot express a relative path across drive letters.  Keep
        # the absolute path in that case; _recorded_path already supports it.
        return resolved.as_posix()


def _recorded_path(value: str, manifest_path: Path) -> Path:
    candidate = Path(value)
    if candidate.is_absolute():
        return candidate
    return (manifest_path.parent / candidate).resolve()


def _read_json(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"Could not read JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value


def _write_json(path: Path, value: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def _git_commit(workspace: Path) -> str | None:
    result = subprocess.run(
        ["git", "-C", str(workspace), "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
    )
    commit = result.stdout.strip()
    return commit if result.returncode == 0 and commit else None


def _exact_keys(
    value: object, expected: Iterable[str], label: str, errors: list[str]
) -> dict[str, object]:
    if not isinstance(value, dict):
        errors.append(f"{label} must be an object")
        return {}
    expected_set = set(expected)
    actual_set = set(value)
    missing = expected_set - actual_set
    unknown = actual_set - expected_set
    if missing:
        errors.append(f"{label} is missing: {', '.join(sorted(missing))}")
    if unknown:
        errors.append(f"{label} has unknown fields: {', '.join(sorted(unknown))}")
    return value


def _valid_timestamp(value: object) -> bool:
    if not isinstance(value, str) or not value:
        return False
    try:
        datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return False
    return True


def _evidence_paths_exist(
    values: object, manifest_path: Path, label: str, errors: list[str]
) -> None:
    if not isinstance(values, list) or not values:
        errors.append(f"{label} must contain evidence paths")
        return
    for value in values:
        if not isinstance(value, str) or not value:
            errors.append(f"{label} contains an empty evidence path")
        elif not _recorded_path(value, manifest_path).exists():
            errors.append(f"{label} does not exist: {value}")


def _default_integrity_path(manifest_path: Path) -> Path:
    return manifest_path.with_name(f"{manifest_path.stem}.integrity.json")


def _recorded_evidence_paths(
    manifest_path: Path, manifest: dict[str, object]
) -> list[Path]:
    values: list[str] = []
    artifacts = manifest.get("artifacts")
    if isinstance(artifacts, dict):
        values.extend(value for value in artifacts.values() if isinstance(value, str))
    commands = manifest.get("commands")
    if isinstance(commands, list):
        for command in commands:
            if not isinstance(command, dict):
                continue
            evidence = command.get("evidence")
            if isinstance(evidence, list):
                values.extend(value for value in evidence if isinstance(value, str))
    review = manifest.get("review")
    if isinstance(review, dict):
        dimensions = review.get("dimensions")
        if isinstance(dimensions, dict):
            for dimension in dimensions.values():
                if not isinstance(dimension, dict):
                    continue
                evidence = dimension.get("evidence")
                if isinstance(evidence, list):
                    values.extend(
                        value for value in evidence if isinstance(value, str)
                    )
    return sorted({_recorded_path(value, manifest_path) for value in values})


def _integrity_entries(
    manifest_path: Path, manifest: dict[str, object]
) -> list[dict[str, object]]:
    entries: list[dict[str, object]] = []
    for path in _recorded_evidence_paths(manifest_path, manifest):
        entries.append(
            {
                "path": _portable_path(path, manifest_path.parent),
                "kind": "directory" if path.is_dir() else "file",
                "sha256": _digest(path),
            }
        )
    return entries


def _validate_integrity(
    manifest_path: Path,
    manifest: dict[str, object],
    integrity_path: Path,
) -> list[str]:
    errors: list[str] = []
    if not integrity_path.is_file():
        return [f"integrity sidecar does not exist: {integrity_path}"]
    try:
        integrity = _read_json(integrity_path)
    except ValueError as error:
        return [str(error)]
    record = _exact_keys(
        integrity,
        ("schema_version", "manifest", "manifest_sha256", "sealed_at", "files"),
        "integrity",
        errors,
    )
    if record.get("schema_version") != INTEGRITY_SCHEMA_VERSION:
        errors.append(
            f"integrity.schema_version must be {INTEGRITY_SCHEMA_VERSION}"
        )
    expected_manifest = _portable_path(manifest_path, integrity_path.parent)
    if record.get("manifest") != expected_manifest:
        errors.append("integrity.manifest does not point to the validated run")
    if record.get("manifest_sha256") != _digest(manifest_path):
        errors.append("integrity.manifest_sha256 does not match the run manifest")
    if not _valid_timestamp(record.get("sealed_at")):
        errors.append("integrity.sealed_at must be an ISO-8601 timestamp")

    files = record.get("files")
    if not isinstance(files, list):
        errors.append("integrity.files must be an array")
        return errors
    if not files and manifest.get("status") == "completed":
        errors.append("completed run integrity must contain sealed evidence")
    recorded_entries: dict[str, dict[str, object]] = {}
    for index, value in enumerate(files):
        entry = _exact_keys(
            value, ("path", "kind", "sha256"), f"integrity.files[{index}]", errors
        )
        path_value = entry.get("path")
        if not isinstance(path_value, str) or not path_value:
            errors.append(f"integrity.files[{index}].path must be non-empty")
            continue
        if path_value in recorded_entries:
            errors.append(f"integrity.files contains duplicate path: {path_value}")
        recorded_entries[path_value] = entry
        path = _recorded_path(path_value, manifest_path)
        if not path.exists():
            errors.append(f"sealed evidence does not exist: {path_value}")
            continue
        actual_kind = "directory" if path.is_dir() else "file"
        if entry.get("kind") != actual_kind:
            errors.append(f"sealed evidence kind changed: {path_value}")
        if entry.get("sha256") != _digest(path):
            errors.append(f"sealed evidence digest changed: {path_value}")

    expected_paths = {
        _portable_path(path, manifest_path.parent)
        for path in _recorded_evidence_paths(manifest_path, manifest)
    }
    if set(recorded_entries) != expected_paths:
        missing = expected_paths - set(recorded_entries)
        extra = set(recorded_entries) - expected_paths
        if missing:
            errors.append(
                "integrity.files is missing recorded evidence: "
                + ", ".join(sorted(missing))
            )
        if extra:
            errors.append(
                "integrity.files contains unrecorded evidence: "
                + ", ".join(sorted(extra))
            )
    return errors


def validate_manifest(
    manifest_path: Path,
    manifest: dict[str, object],
    *,
    require_complete: bool = False,
    require_pass: bool = False,
) -> list[str]:
    errors: list[str] = []
    top_level = _exact_keys(
        manifest,
        (
            "schema_version",
            "benchmark_id",
            "run_id",
            "agent",
            "status",
            "author_id",
            "created_at",
            "started_at",
            "completed_at",
            "spec",
            "skill_package",
            "workspace",
            "prompt",
            "blind_protocol",
            "commands",
            "artifacts",
            "checks",
            "review",
        ),
        "run",
        errors,
    )
    if top_level.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if top_level.get("agent") not in AGENTS:
        errors.append(f"agent must be one of: {', '.join(AGENTS)}")
    run_id = top_level.get("run_id")
    if not isinstance(run_id, str) or RUN_ID_PATTERN.fullmatch(run_id) is None:
        errors.append("run_id must use letters, digits, dots, dashes, or underscores")
    author_id = top_level.get("author_id")
    if not isinstance(author_id, str) or not author_id.strip():
        errors.append("author_id must be non-empty")
    if not _valid_timestamp(top_level.get("created_at")):
        errors.append("created_at must be an ISO-8601 timestamp")

    status = top_level.get("status")
    if status not in {"planned", "running", "completed", "blocked"}:
        errors.append("status must be planned, running, completed, or blocked")
    terminal = status in {"completed", "blocked"}
    if require_complete and not terminal:
        errors.append("run must be completed or blocked")
    if status == "planned" and top_level.get("started_at") is not None:
        errors.append("planned run must not have started_at")
    if status in {"running", "completed", "blocked"} and not _valid_timestamp(
        top_level.get("started_at")
    ):
        errors.append(f"{status} run must have started_at")
    if terminal and not _valid_timestamp(top_level.get("completed_at")):
        errors.append(f"{status} run must have completed_at")

    spec_record = _exact_keys(top_level.get("spec"), ("path", "sha256"), "spec", errors)
    skill_record = _exact_keys(
        top_level.get("skill_package"), ("path", "sha256"), "skill_package", errors
    )
    spec: dict[str, object] = {}
    for label, record in (("spec", spec_record), ("skill_package", skill_record)):
        recorded = record.get("path")
        expected_hash = record.get("sha256")
        if not isinstance(recorded, str) or not recorded:
            errors.append(f"{label}.path must be non-empty")
            continue
        resolved = _recorded_path(recorded, manifest_path)
        if not resolved.exists():
            errors.append(f"{label}.path does not exist: {recorded}")
            continue
        actual_hash = _digest(resolved)
        if expected_hash != actual_hash:
            errors.append(f"{label}.sha256 does not match {recorded}")
        if label == "spec":
            try:
                spec = _read_json(resolved)
            except ValueError as error:
                errors.append(str(error))

    benchmark_id = top_level.get("benchmark_id")
    if spec:
        if benchmark_id != spec.get("id"):
            errors.append("benchmark_id does not match the benchmark specification")
        if top_level.get("prompt") != spec.get("prompt"):
            errors.append("prompt must match the benchmark specification exactly")

    workspace = _exact_keys(
        top_level.get("workspace"), ("path", "source_commit"), "workspace", errors
    )
    workspace_path = workspace.get("path")
    if not isinstance(workspace_path, str) or not workspace_path:
        errors.append("workspace.path must be non-empty")
    elif not _recorded_path(workspace_path, manifest_path).is_dir():
        errors.append(f"workspace.path is not a directory: {workspace_path}")
    source_commit = workspace.get("source_commit")
    if terminal and (
        not isinstance(source_commit, str)
        or re.fullmatch(r"[0-9a-fA-F]{7,64}", source_commit) is None
    ):
        errors.append("terminal run must record a Git source_commit")

    blind_protocol = _exact_keys(
        top_level.get("blind_protocol"),
        ("prompt_only_context", "no_intended_layout", "no_prior_diagnosis"),
        "blind_protocol",
        errors,
    )
    if not all(blind_protocol.get(field) is True for field in blind_protocol):
        errors.append("blind_protocol declarations must all be true")

    artifacts = _exact_keys(top_level.get("artifacts"), ARTIFACTS, "artifacts", errors)
    for name in ARTIFACTS:
        value = artifacts.get(name)
        if value is not None and (not isinstance(value, str) or not value):
            errors.append(f"artifacts.{name} must be a path or null")
        elif isinstance(value, str) and not _recorded_path(value, manifest_path).exists():
            errors.append(f"artifacts.{name} does not exist: {value}")
        if status == "completed" and value is None:
            errors.append(f"completed run is missing artifacts.{name}")

    checks = _exact_keys(top_level.get("checks"), CHECKS, "checks", errors)
    for name in CHECKS:
        value = checks.get(name)
        if value not in {True, False, None}:
            errors.append(f"checks.{name} must be true, false, or null")
        if status == "completed" and value is None:
            errors.append(f"completed run is missing checks.{name}")

    commands = top_level.get("commands")
    if not isinstance(commands, list):
        errors.append("commands must be an array")
        commands = []
    if status == "completed" and not commands:
        errors.append("completed run must record commands")
    for index, command_value in enumerate(commands):
        command = _exact_keys(
            command_value,
            ("command", "exit_code", "duration_ms", "evidence"),
            f"commands[{index}]",
            errors,
        )
        if not isinstance(command.get("command"), str) or not command.get("command"):
            errors.append(f"commands[{index}].command must be non-empty")
        exit_code = command.get("exit_code")
        duration_ms = command.get("duration_ms")
        if exit_code is not None and not isinstance(exit_code, int):
            errors.append(f"commands[{index}].exit_code must be an integer or null")
        if duration_ms is not None and (
            not isinstance(duration_ms, int) or duration_ms < 0
        ):
            errors.append(f"commands[{index}].duration_ms must be non-negative or null")
        if status == "completed":
            if exit_code is None or duration_ms is None:
                errors.append(f"commands[{index}] is incomplete")
            _evidence_paths_exist(
                command.get("evidence"),
                manifest_path,
                f"commands[{index}].evidence",
                errors,
            )

    review = _exact_keys(
        top_level.get("review"),
        ("reviewer_id", "dimensions", "verdict", "blockers", "notes"),
        "review",
        errors,
    )
    dimensions = review.get("dimensions")
    expected_dimensions = spec.get("blind_review_dimensions", []) if spec else []
    if isinstance(expected_dimensions, list):
        dimensions = _exact_keys(
            dimensions, expected_dimensions, "review.dimensions", errors
        )
    else:
        errors.append("benchmark blind_review_dimensions must be an array")
        dimensions = {}
    reviewer_id = review.get("reviewer_id")
    if status == "completed":
        if not isinstance(reviewer_id, str) or not reviewer_id.strip():
            errors.append("completed run must record reviewer_id")
        elif reviewer_id == author_id:
            errors.append("reviewer_id must differ from author_id")
        if review.get("verdict") not in {"pass", "fail"}:
            errors.append("completed run review.verdict must be pass or fail")
    elif review.get("verdict") not in {None, "pass", "fail"}:
        errors.append("review.verdict must be pass, fail, or null")

    for name, dimension_value in dimensions.items():
        dimension = _exact_keys(
            dimension_value, ("score", "evidence"), f"review.dimensions.{name}", errors
        )
        score = dimension.get("score")
        if score is not None and (
            not isinstance(score, int) or isinstance(score, bool) or not 1 <= score <= 5
        ):
            errors.append(f"review.dimensions.{name}.score must be 1 through 5 or null")
        if status == "completed":
            if score is None:
                errors.append(f"review.dimensions.{name}.score is missing")
            _evidence_paths_exist(
                dimension.get("evidence"),
                manifest_path,
                f"review.dimensions.{name}.evidence",
                errors,
            )

    blockers = review.get("blockers")
    notes = review.get("notes")
    for label, values in (("review.blockers", blockers), ("review.notes", notes)):
        if not isinstance(values, list) or not all(
            isinstance(item, str) and item.strip() for item in values
        ):
            errors.append(f"{label} must be an array of non-empty strings")
    if status == "blocked" and (not isinstance(blockers, list) or not blockers):
        errors.append("blocked run must record at least one blocker")

    if require_pass:
        threshold = spec.get("pass_threshold", {}) if spec else {}
        minimum_score = threshold.get("minimum_dimension_score", 4)
        if status != "completed":
            errors.append("passing run must have status completed")
        if any(checks.get(name) is not True for name in CHECKS):
            errors.append("passing run requires every check to pass")
        if any(
            not isinstance(value, dict)
            or not isinstance(value.get("score"), int)
            or value["score"] < minimum_score
            for value in dimensions.values()
        ):
            errors.append(f"passing run requires every dimension score >= {minimum_score}")
        if review.get("verdict") != "pass":
            errors.append("passing run requires review.verdict=pass")
        if blockers:
            errors.append("passing run must have no blockers")
        if any(
            isinstance(command, dict) and command.get("exit_code") != 0
            for command in commands
        ):
            errors.append("passing run requires zero exit codes")

    return errors


def init_run(args: argparse.Namespace) -> int:
    output = args.output.resolve()
    if output.exists():
        print(f"Refusing to overwrite existing run: {output}", file=sys.stderr)
        return 2
    spec_path = args.spec.resolve()
    skill_path = args.skill_package.resolve()
    workspace = args.workspace.resolve()
    if not spec_path.is_file():
        print(f"Benchmark specification does not exist: {spec_path}", file=sys.stderr)
        return 2
    if not skill_path.exists():
        print(f"Skill package does not exist: {skill_path}", file=sys.stderr)
        return 2
    if not workspace.is_dir():
        print(f"Workspace is not a directory: {workspace}", file=sys.stderr)
        return 2
    if not args.author_id.strip():
        print("author-id must be non-empty", file=sys.stderr)
        return 2
    spec = _read_json(spec_path)
    benchmark_id = spec.get("id")
    prompt = spec.get("prompt")
    dimensions = spec.get("blind_review_dimensions")
    if not isinstance(benchmark_id, str) or not isinstance(prompt, str) or not isinstance(
        dimensions, list
    ):
        print("Benchmark specification is incomplete", file=sys.stderr)
        return 2
    run_id = args.run_id or (
        f"{benchmark_id}-{args.agent}-"
        + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    )
    if RUN_ID_PATTERN.fullmatch(run_id) is None:
        print("run-id contains unsupported characters", file=sys.stderr)
        return 2
    base = output.parent
    manifest: dict[str, object] = {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "run_id": run_id,
        "agent": args.agent,
        "status": "planned",
        "author_id": args.author_id,
        "created_at": _timestamp(),
        "started_at": None,
        "completed_at": None,
        "spec": {
            "path": _portable_path(spec_path, base),
            "sha256": _digest(spec_path),
        },
        "skill_package": {
            "path": _portable_path(skill_path, base),
            "sha256": _digest(skill_path),
        },
        "workspace": {
            "path": _portable_path(workspace, base),
            "source_commit": _git_commit(workspace),
        },
        "prompt": prompt,
        "blind_protocol": {
            "prompt_only_context": True,
            "no_intended_layout": True,
            "no_prior_diagnosis": True,
        },
        "commands": [],
        "artifacts": {name: None for name in ARTIFACTS},
        "checks": {name: None for name in CHECKS},
        "review": {
            "reviewer_id": None,
            "dimensions": {
                name: {"score": None, "evidence": []} for name in dimensions
            },
            "verdict": None,
            "blockers": [],
            "notes": [],
        },
    }
    _write_json(output, manifest)
    print(output)
    return 0


def validate_run(args: argparse.Namespace) -> int:
    path = args.manifest.resolve()
    try:
        manifest = _read_json(path)
        errors = validate_manifest(
            path,
            manifest,
            require_complete=args.require_complete or args.require_pass,
            require_pass=args.require_pass,
        )
        if args.require_current:
            errors.extend(
                _validate_integrity(
                    path,
                    manifest,
                    _default_integrity_path(path),
                )
            )
    except ValueError as error:
        errors = [str(error)]
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    status = manifest.get("status")
    result = "PASS" if args.require_pass else "VALID"
    print(f"benchmark run: {result} ({manifest.get('agent')}, {status})")
    return 0


def seal_run(args: argparse.Namespace) -> int:
    manifest_path = args.manifest.resolve()
    output = _default_integrity_path(manifest_path)
    if output.exists():
        print(
            f"Refusing to overwrite existing integrity sidecar: {output}",
            file=sys.stderr,
        )
        return 2
    try:
        manifest = _read_json(manifest_path)
        errors = validate_manifest(manifest_path, manifest, require_complete=True)
        if errors:
            for error in errors:
                print(f"ERROR: {error}", file=sys.stderr)
            return 1
        entries = _integrity_entries(manifest_path, manifest)
    except ValueError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    sidecar: dict[str, object] = {
        "schema_version": INTEGRITY_SCHEMA_VERSION,
        "manifest": _portable_path(manifest_path, output.parent),
        "manifest_sha256": _digest(manifest_path),
        "sealed_at": _timestamp(),
        "files": entries,
    }
    _write_json(output, sidecar)
    print(output)
    return 0


def summarize_runs(args: argparse.Namespace) -> int:
    output = args.output.resolve()
    manifests: list[tuple[Path, dict[str, object]]] = []
    errors: list[str] = []
    for input_path in args.manifests:
        path = input_path.resolve()
        try:
            manifest = _read_json(path)
            run_errors = validate_manifest(path, manifest, require_complete=True)
            if args.require_current:
                run_errors.extend(
                    _validate_integrity(
                        path,
                        manifest,
                        _default_integrity_path(path),
                    )
                )
        except ValueError as error:
            run_errors = [str(error)]
            manifest = {}
        if run_errors:
            errors.extend(f"{path.name}: {error}" for error in run_errors)
        manifests.append((path, manifest))
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    agents = [str(manifest["agent"]) for _, manifest in manifests]
    if sorted(agents) != sorted(AGENTS):
        print(
            "ERROR: summary requires exactly one completed or blocked run for "
            + ", ".join(AGENTS),
            file=sys.stderr,
        )
        return 1
    benchmark_ids = {manifest["benchmark_id"] for _, manifest in manifests}
    spec_hashes = {
        manifest["spec"]["sha256"]  # type: ignore[index]
        for _, manifest in manifests
    }
    skill_hashes = {
        manifest["skill_package"]["sha256"]  # type: ignore[index]
        for _, manifest in manifests
    }
    prompts = {manifest["prompt"] for _, manifest in manifests}
    if any(len(values) != 1 for values in (benchmark_ids, spec_hashes, skill_hashes, prompts)):
        print("ERROR: runs do not share one benchmark, prompt, and Skill package", file=sys.stderr)
        return 1

    first_path, first = manifests[0]
    spec_path = _recorded_path(first["spec"]["path"], first_path)  # type: ignore[index]
    spec = _read_json(spec_path)
    dimensions = spec["blind_review_dimensions"]
    threshold = spec["pass_threshold"]
    reviewed = [
        manifest
        for _, manifest in manifests
        if manifest["status"] == "completed"
    ]
    dimension_scores: dict[str, list[int]] = {name: [] for name in dimensions}
    for manifest in reviewed:
        review_dimensions = manifest["review"]["dimensions"]  # type: ignore[index]
        for name in dimensions:
            score = review_dimensions[name]["score"]
            if isinstance(score, int):
                dimension_scores[name].append(score)
    minimum_score = min(
        (score for scores in dimension_scores.values() for score in scores),
        default=0,
    )
    mean_scores = {
        name: round(statistics.mean(scores), 2) if scores else None
        for name, scores in dimension_scores.items()
    }
    total = len(manifests)
    checks = [manifest["checks"] for _, manifest in manifests]
    build_success = sum(check["build"] is True for check in checks)
    workflow_success = sum(check["workflow"] is True for check in checks)
    open_blockers = sum(
        len(manifest["review"]["blockers"])  # type: ignore[index]
        for _, manifest in manifests
    )
    build_percent = round(build_success * 100 / total, 2)
    workflow_percent = round(workflow_success * 100 / total, 2)
    minimum_build = threshold.get("minimum_build_success_percent", 85)
    minimum_workflow = threshold.get("minimum_workflow_completion_percent", 85)
    minimum_dimension = threshold["minimum_dimension_score"]
    minimum_preference = threshold["minimum_pairwise_preference_percent"]
    maximum_blockers = threshold["open_blocker_or_major_findings"]
    all_checks_pass = all(
        all(check[name] is True for name in CHECKS) for check in checks
    )
    non_preference_pass = (
        build_percent >= minimum_build
        and workflow_percent >= minimum_workflow
        and minimum_score >= minimum_dimension
        and open_blockers <= maximum_blockers
        and len(reviewed) == total
        and all_checks_pass
    )
    preference = args.pairwise_preference_percent
    if preference is not None and not 0 <= preference <= 100:
        print("ERROR: pairwise preference percent must be between 0 and 100", file=sys.stderr)
        return 1
    if preference is not None and not args.preference_note:
        print("ERROR: --preference-note is required with a preference percent", file=sys.stderr)
        return 1
    preference_pass = preference is not None and preference >= minimum_preference
    status = (
        "pass"
        if non_preference_pass and preference_pass
        else "awaiting-preference"
        if non_preference_pass and preference is None
        else "fail"
    )
    run_rows = []
    for path, manifest in sorted(manifests, key=lambda item: str(item[1]["agent"])):
        scores = manifest["review"]["dimensions"]  # type: ignore[index]
        recorded_scores = [
            value["score"] for value in scores.values() if isinstance(value["score"], int)
        ]
        run_rows.append(
            {
                "run_id": manifest["run_id"],
                "agent": manifest["agent"],
                "status": manifest["status"],
                "manifest": _portable_path(path, output.parent),
                "manifest_sha256": _digest(path),
                "mean_dimension_score": (
                    round(statistics.mean(recorded_scores), 2)
                    if recorded_scores
                    else None
                ),
                "minimum_dimension_score": min(recorded_scores, default=None),
            }
        )
    summary: dict[str, object] = {
        "schema_version": 1,
        "benchmark_id": next(iter(benchmark_ids)),
        "generated_at": _timestamp(),
        "status": status,
        "agents": sorted(agents),
        "skill_package_sha256": next(iter(skill_hashes)),
        "spec_sha256": next(iter(spec_hashes)),
        "runs": run_rows,
        "scores": {
            "mean_by_dimension": mean_scores,
            "minimum_dimension_score": minimum_score,
        },
        "thresholds": threshold,
        "gates": {
            "required_agents_present": True,
            "completed_runs": len(reviewed),
            "all_checks_pass": all_checks_pass,
            "build_success_percent": build_percent,
            "workflow_completion_percent": workflow_percent,
            "open_blockers": open_blockers,
            "pairwise_preference_percent": preference,
            "preference_gate_pass": (
                preference_pass if preference is not None else None
            ),
            "preference_note": args.preference_note,
        },
    }
    _write_json(output, summary)
    print(f"benchmark summary: {status} ({output})")
    return 1 if status == "fail" else 0


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    initialize = subparsers.add_parser("init", help="Create one blind run manifest")
    initialize.add_argument("--agent", choices=AGENTS, required=True)
    initialize.add_argument("--author-id", required=True)
    initialize.add_argument("--skill-package", type=Path, required=True)
    initialize.add_argument("--workspace", type=Path, default=Path.cwd())
    initialize.add_argument("--spec", type=Path, default=DEFAULT_SPEC)
    initialize.add_argument("--run-id")
    initialize.add_argument("--output", type=Path, required=True)
    initialize.set_defaults(handler=init_run)

    validate = subparsers.add_parser("validate", help="Validate one run manifest")
    validate.add_argument("manifest", type=Path)
    validate.add_argument("--require-complete", action="store_true")
    validate.add_argument("--require-pass", action="store_true")
    validate.add_argument(
        "--require-current",
        action="store_true",
        help="Require a matching run.integrity.json sidecar",
    )
    validate.set_defaults(handler=validate_run)

    seal = subparsers.add_parser(
        "seal", help="Hash the completed run manifest and all recorded evidence"
    )
    seal.add_argument("manifest", type=Path)
    seal.set_defaults(handler=seal_run)

    summarize = subparsers.add_parser(
        "summarize", help="Combine Codex and Cursor runs"
    )
    summarize.add_argument("manifests", type=Path, nargs="+")
    summarize.add_argument("--output", type=Path, required=True)
    summarize.add_argument("--pairwise-preference-percent", type=float)
    summarize.add_argument("--preference-note")
    summarize.add_argument(
        "--require-current",
        action="store_true",
        help="Require matching integrity sidecars for every run",
    )
    summarize.set_defaults(handler=summarize_runs)

    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
