#!/usr/bin/env python3

"""Run evidence-first FluentQt Gallery GUI verification recipes.

The tool deliberately separates deterministic capture gates from visual review.
A run can prove that pixels, geometry, interactions, Inspector findings, and the
capture environment satisfy an approved contract.  It cannot self-approve a
new baseline or its own visual judgment.
"""

from __future__ import annotations

from collections import Counter
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence
import argparse
import copy
import locale
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile

# Resolve sibling modules for direct execution and file-based test loaders.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from gui_verification.artifacts import (
    canonical_json,
    path_is_within,
    paths_overlap,
    png_dimensions,
    read_json,
    resolved_path,
    sha256_bytes,
    sha256_file,
    write_json,
)
from gui_verification.common import (
    ACTION_NAMES,
    BASELINE_PROVENANCE_SCHEMA_VERSION,
    BASELINE_SCHEMA_VERSION,
    NATIVE_PLUGINS,
    REVIEW_SCHEMA_VERSION,
    SAFE_ID,
    STATUS_PRIORITY,
    TOOL_SCHEMA_VERSION,
    VerificationError,
    canonical_machine_name,
    canonical_system_name,
    check,
    combined_status,
    effective_require_native_desktop,
    is_json_integer,
    is_sha256,
    is_trimmed_nonempty,
    is_utc_timestamp,
    merged_dict,
    nested,
    qt_scale_positive,
    utc_now,
    validated_device_pixel_ratio,
)
from gui_verification.comparison import (
    comparator_policy_arguments,
    geometry_baseline_check,
    geometry_contract_check,
    pixel_comparisons,
    validate_comparator_report,
)
from gui_verification.presentation import (
    review_html,
)
from gui_verification.recipe import (
    validate_action_script,
    validate_fields,
    validate_geometry_policy,
    validate_recipe,
    validate_scenario_semantics,
)
from gui_verification.reports import (
    baseline_report_errors,
    capture_environment_check,
    capture_environment_errors,
    capture_report_check,
    capture_report_errors,
    inspector_report_errors,
)


PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOL = Path(__file__).with_name("fluent_qt_build.py")


def approved_baseline_root() -> Path:
    return PROJECT_ROOT / "tests" / "visual-baselines" / "gui"


def is_approved_baseline_bundle_path(path: Path) -> bool:
    """Return whether path is exactly gui/<component>/<scenario>."""
    try:
        relative = path.resolve().relative_to(approved_baseline_root().resolve())
    except ValueError:
        return False
    return len(relative.parts) == 2 and all(
        SAFE_ID.fullmatch(part) is not None for part in relative.parts
    )


def recipe_path_base(recipe: Mapping[str, object], recipe_path: Path) -> Path:
    """Return the explicitly declared base for relative recipe paths."""

    path_base = recipe.get("path_base")
    if path_base == "repository":
        return PROJECT_ROOT
    if path_base == "recipe":
        return recipe_path.parent.resolve()
    raise VerificationError("path_base must be repository or recipe")


def default_preset() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "darwin":
        return "vcpkg-osx-x64" if machine in {"x86_64", "amd64"} else "vcpkg-osx"
    if system == "windows":
        return "vcpkg-windows-arm64" if machine in {"arm64", "aarch64"} else "vcpkg-windows"
    if system == "linux":
        return "vcpkg-linux-arm64" if machine in {"arm64", "aarch64"} else "vcpkg-linux"
    raise VerificationError(f"Unsupported GUI verification host: {platform.system()}")


def configured_build_dir(args: argparse.Namespace) -> Path:
    if args.build_dir:
        return args.build_dir.expanduser().resolve()
    return PROJECT_ROOT / "build" / args.preset


def resolve_gallery_executable(build_dir: Path) -> Path:
    app_dir = build_dir / "app"
    candidates = [
        app_dir / "fluent_qt_gallery",
        app_dir / "Fluent-Qt Gallery.app" / "Contents" / "MacOS" / "Fluent-Qt Gallery",
        app_dir / "fluent_qt_gallery.exe",
        app_dir / "Debug" / "fluent_qt_gallery.exe",
        app_dir / "Release" / "fluent_qt_gallery.exe",
        app_dir / "RelWithDebInfo" / "fluent_qt_gallery.exe",
    ]
    existing = [candidate.resolve() for candidate in candidates if candidate.is_file()]
    if existing:
        return existing[0]
    raise VerificationError(f"Could not find fluent_qt_gallery under {app_dir}")


def resolve_comparator_executable(build_dir: Path) -> Path:
    tool_dir = build_dir / "tools" / "dev"
    candidates = [
        tool_dir / "fluent_qt_visual_compare",
        tool_dir / "fluent_qt_visual_compare.exe",
        tool_dir / "Debug" / "fluent_qt_visual_compare.exe",
        tool_dir / "Release" / "fluent_qt_visual_compare.exe",
        tool_dir / "RelWithDebInfo" / "fluent_qt_visual_compare.exe",
    ]
    existing = [candidate.resolve() for candidate in candidates if candidate.is_file()]
    if existing:
        return existing[0]
    raise VerificationError(f"Could not find fluent_qt_visual_compare under {tool_dir}")


def decode_captured_output(value: bytes | str | None) -> str:
    """Decode captured process output without assuming the Windows code page."""

    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if not isinstance(value, (bytes, bytearray)):
        return str(value)

    payload = bytes(value)
    try:
        return payload.decode("utf-8", errors="strict")
    except UnicodeDecodeError:
        preferred_encoding = locale.getpreferredencoding(False) or "utf-8"
        try:
            return payload.decode(preferred_encoding, errors="replace")
        except LookupError:
            return payload.decode("utf-8", errors="replace")


def normalize_completed_process_output(
    completed: subprocess.CompletedProcess[Any],
) -> subprocess.CompletedProcess[str]:
    completed.stdout = decode_captured_output(completed.stdout)
    completed.stderr = decode_captured_output(completed.stderr)
    return completed


def normalize_timeout_output(error: subprocess.TimeoutExpired) -> subprocess.TimeoutExpired:
    error.output = decode_captured_output(error.output)
    error.stderr = decode_captured_output(error.stderr)
    return error


def run_captured_command(
    command: Sequence[str], **kwargs: Any
) -> subprocess.CompletedProcess[str]:
    """Run a command with byte capture and normalize both success and timeout output."""

    try:
        completed = subprocess.run(
            command,
            text=False,
            capture_output=True,
            check=False,
            **kwargs,
        )
    except subprocess.TimeoutExpired as error:
        normalize_timeout_output(error)
        raise
    return normalize_completed_process_output(completed)


def command_record(command: Sequence[str], completed: subprocess.CompletedProcess[str]) -> dict[str, object]:
    return {
        "command": list(command),
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def build_dependencies(args: argparse.Namespace) -> dict[str, object]:
    if args.no_build:
        return {"requested": False, "status": "not-requested"}
    command = [sys.executable, str(BUILD_TOOL)]
    if args.build_dir:
        command.append(str(args.build_dir.expanduser().resolve()))
    else:
        command.extend(["--preset", args.preset])
    command.extend(["--target", "fluent_qt_gallery", "fluent_qt_visual_compare"])
    completed = run_captured_command(
        command,
        cwd=PROJECT_ROOT,
    )
    result = command_record(command, completed)
    result.update({"requested": True, "status": "pass" if completed.returncode == 0 else "fail"})
    return result


def git_state() -> dict[str, object]:
    def run(*arguments: str) -> subprocess.CompletedProcess[str]:
        return run_captured_command(
            ["git", *arguments],
            cwd=PROJECT_ROOT,
        )

    revision = run("rev-parse", "HEAD")
    status = run("status", "--porcelain")
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "dirty": bool(status.stdout.strip()) if status.returncode == 0 else None,
    }


def host_key() -> tuple[str, str]:
    system = canonical_system_name(platform.system())
    machine = canonical_machine_name(platform.machine())
    return system, f"{system}-{machine}"


def capture_host_key(report: Mapping[str, object]) -> str:
    system = canonical_system_name(
        nested(report, "environment", "system", "kernel_type")
    )
    machine = canonical_machine_name(
        nested(report, "environment", "system", "cpu_architecture")
    )
    return f"{system}-{machine}" if system and machine else ""


def select_baseline(raw: object, path_base: Path) -> Path:
    if isinstance(raw, str):
        return resolved_path(raw, path_base)
    if not isinstance(raw, dict):
        raise VerificationError("baseline must be a path or platform map")
    system, exact = host_key()
    selected = raw.get(exact, raw.get(system, raw.get("default")))
    if not isinstance(selected, str):
        raise VerificationError(
            f"baseline has no entry for {exact}, {system}, or default"
        )
    return resolved_path(selected, path_base)


def prepare_action_script(
    raw: object, path_base: Path, scenario_dir: Path
) -> tuple[Path | None, dict[str, object] | None]:
    if raw is None:
        return None, None
    if isinstance(raw, str):
        path = resolved_path(raw, path_base)
        if not path.is_file():
            raise VerificationError(f"Action script does not exist: {path}")
        script = read_json(path)
        errors: list[str] = []
        validate_action_script(script, "actions", errors)
        if errors:
            raise VerificationError("Invalid action script:\n- " + "\n- ".join(errors))
        return path, script
    if not isinstance(raw, dict):
        raise VerificationError("actions must be a path or JSON object")
    errors = []
    validate_action_script(raw, "actions", errors)
    if errors:
        raise VerificationError("Invalid action script:\n- " + "\n- ".join(errors))
    path = scenario_dir / "actions.json"
    write_json(path, raw)
    return path, dict(raw)


def relevant_environment(recipe: Mapping[str, object], scenario: Mapping[str, object]) -> tuple[dict[str, str], dict[str, str]]:
    configured = merged_dict(recipe.get("environment"), scenario.get("environment"))
    defaults = {
        "QT_SCALE_FACTOR": "1",
        "QT_FONT_DPI": "96",
        "QT_AUTO_SCREEN_SCALE_FACTOR": "0",
    }
    defaults.update({str(key): str(value) for key, value in configured.items()})
    environment = os.environ.copy()
    environment.update(defaults)
    return environment, defaults


def scenario_contract(
    recipe: Mapping[str, object],
    scenario: Mapping[str, object],
    action_path: Path | None,
) -> dict[str, object]:
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    _environment, environment_overrides = relevant_environment(recipe, scenario)
    return {
        "schema_version": 2,
        "recipe_id": recipe.get("id"),
        "scenario_id": scenario.get("id"),
        "path_base": recipe.get("path_base"),
        "selection": merged_dict(recipe.get("selection"), scenario.get("selection")),
        "coverage": {
            "required_tags": nested(recipe, "coverage", "required_tags"),
            "scenario_tags": scenario.get("tags"),
        },
        "scene": {
            "theme": scenario.get("theme"),
            "direction": scenario.get("direction", "ltr"),
            "size": scenario.get("size"),
            "settle_ms": scenario.get("settle_ms", defaults.get("settle_ms", 250)),
            "timeout_seconds": scenario.get(
                "timeout_seconds", defaults.get("timeout_seconds", 45)
            ),
            "require_native_desktop": effective_require_native_desktop(
                defaults, scenario
            ),
        },
        "acceptance": {
            "inspector": merged_dict(
                defaults.get("inspector"), scenario.get("inspector")
            ),
            "geometry": merged_dict(
                defaults.get("geometry"), scenario.get("geometry")
            ),
            "pixel": merged_dict(defaults.get("pixel"), scenario.get("pixel")),
            "review": scenario.get("review"),
        },
        "baseline": scenario.get("baseline"),
        "environment_overrides": environment_overrides,
        "actions_sha256": sha256_file(action_path) if action_path else None,
    }


def capture_command(
    gallery: Path,
    recipe: Mapping[str, object],
    scenario: Mapping[str, object],
    scenario_dir: Path,
    action_path: Path | None,
) -> list[str]:
    selection = merged_dict(recipe.get("selection"), scenario.get("selection"))
    scenario_dir = scenario_dir.resolve()
    command = [
        str(gallery.resolve()),
        "--preview",
        "--route",
        str(selection["route"]),
    ]
    sample = selection.get("sample")
    if sample:
        command.extend(["--sample", str(sample)])
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    settle_ms = scenario.get("settle_ms", defaults.get("settle_ms", 250))
    command.extend(
        [
            "--theme",
            str(scenario["theme"]),
            "--size",
            str(scenario["size"]),
            "--settle-ms",
            str(settle_ms),
            "--snapshot",
            str(scenario_dir / "actual.png"),
            "--report",
            str(scenario_dir / "capture.json"),
        ]
    )
    if scenario.get("direction", "ltr") == "rtl":
        command.append("--rtl")
    if action_path:
        command.extend(["--actions", str(action_path.resolve())])
    return command


def identity_checks(
    recipe: Mapping[str, object],
    scenario: Mapping[str, object],
    report: Mapping[str, object],
    action_path: Path | None,
    actual_path: Path,
) -> list[dict[str, object]]:
    checks: list[dict[str, object]] = []
    schema_matches = (
        report.get("schema_version") == 2
        and report.get("tool") == "FluentQt Gallery Preview"
    )
    checks.append(
        check(
            "capture.schema",
            "pass" if schema_matches else "incomplete",
            "Capture report schema and tool identity are supported."
            if schema_matches
            else "Capture report schema or tool identity is unsupported.",
            {
                "expected_schema": 2,
                "actual_schema": report.get("schema_version"),
                "expected_tool": "FluentQt Gallery Preview",
                "actual_tool": report.get("tool"),
            },
        )
    )
    selection = merged_dict(recipe.get("selection"), scenario.get("selection"))
    actual_selection = report.get("selection") if isinstance(report.get("selection"), dict) else {}
    selection_matches = actual_selection.get("route") == selection.get("route") and (
        not selection.get("sample") or actual_selection.get("sample") == selection.get("sample")
    )
    checks.append(
        check(
            "capture.identity",
            "pass" if selection_matches else "fail",
            "Captured route and sample match the recipe."
            if selection_matches
            else "Captured route or sample does not match the recipe.",
            {"expected": selection, "actual": actual_selection},
        )
    )
    scene = report.get("scene") if isinstance(report.get("scene"), dict) else {}
    width, height = (int(piece) for piece in str(scenario["size"]).split("x", 1))
    direction = scenario.get("direction", "ltr")
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    settle_ms = scenario.get("settle_ms", defaults.get("settle_ms", 250))
    scene_matches = (
        scene.get("requested_theme") == scenario.get("theme")
        and scene.get("theme") == scenario.get("theme")
        and scene.get("layout_direction") == direction
        and scene.get("settle_ms") == settle_ms
        and scene.get("requested_width") == width
        and scene.get("requested_height") == height
        and scene.get("actual_width") == width
        and scene.get("actual_height") == height
    )
    checks.append(
        check(
            "capture.scene",
            "pass" if scene_matches else "fail",
            "Theme, direction, and viewport match the scenario."
            if scene_matches
            else "Theme, direction, or viewport differs from the scenario.",
            {
                "expected": {
                    "theme": scenario.get("theme"),
                    "layout_direction": direction,
                    "settle_ms": settle_ms,
                    "requested_width": width,
                    "requested_height": height,
                    "width": width,
                    "height": height,
                },
                "actual": scene,
            },
        )
    )
    snapshot_record = nested(report, "artifacts", "snapshot")
    snapshot_record = snapshot_record if isinstance(snapshot_record, dict) else {}
    snapshot_dimensions = png_dimensions(actual_path) if actual_path.is_file() else None
    device_pixel_ratio = nested(report, "environment", "device_pixel_ratio")
    expected_snapshot_dimensions = None
    normalized_device_pixel_ratio = validated_device_pixel_ratio(
        device_pixel_ratio
    )
    if (
        normalized_device_pixel_ratio is not None
        and is_json_integer(scene.get("actual_width"), minimum=1)
        and is_json_integer(scene.get("actual_height"), minimum=1)
    ):
        expected_snapshot_dimensions = (
            qt_scale_positive(
                scene["actual_width"], normalized_device_pixel_ratio
            ),
            qt_scale_positive(
                scene["actual_height"], normalized_device_pixel_ratio
            ),
        )
    snapshot_written = (
        actual_path.is_file()
        and snapshot_dimensions is not None
        and expected_snapshot_dimensions is not None
        and snapshot_dimensions == expected_snapshot_dimensions
        and snapshot_record.get("written") is True
        and Path(str(snapshot_record.get("path", ""))).resolve()
        == actual_path.resolve()
        and snapshot_record.get("sha256") == sha256_file(actual_path)
    )
    checks.append(
        check(
            "capture.snapshot",
            "pass" if snapshot_written else "incomplete",
            "Native-resolution snapshot was written."
            if snapshot_written
            else (
                "Snapshot is missing, has the wrong physical dimensions, or "
                "the capture report did not confirm it."
            ),
            {
                "expected_dimensions": expected_snapshot_dimensions,
                "actual_dimensions": snapshot_dimensions,
                "device_pixel_ratio": device_pixel_ratio,
            },
        )
    )
    interaction = report.get("interaction_report")
    interaction = interaction if isinstance(interaction, dict) else {}
    if action_path:
        try:
            action_script = read_json(action_path)
        except VerificationError:
            action_script = {}
        expected_steps = action_script.get("steps")
        expected_count = len(expected_steps) if isinstance(expected_steps, list) else 0
        summary = (
            interaction.get("summary")
            if isinstance(interaction.get("summary"), dict)
            else {}
        )
        results = interaction.get("steps")
        action_passed = (
            expected_count > 0
            and interaction.get("schema_version") == 1
            and interaction.get("requested") is True
            and interaction.get("status") == "pass"
            and Path(str(interaction.get("source", ""))).resolve()
            == action_path.resolve()
            and summary.get("total") == expected_count
            and summary.get("executed") == expected_count
            and summary.get("passed") == expected_count
            and summary.get("failed") == 0
            and isinstance(results, list)
            and len(results) == expected_count
            and all(
                isinstance(result, dict)
                and result.get("index") == index
                and result.get("request") == expected_steps[index]
                and result.get("status") == "pass"
                for index, result in enumerate(results)
            )
        )
    else:
        action_passed = (
            interaction.get("schema_version") == 1
            and interaction.get("requested") is False
            and interaction.get("status") == "not-requested"
            and interaction.get("steps") == []
        )
    checks.append(
        check(
            "capture.interactions",
            "pass" if action_passed else "fail",
            "Interaction script and assertions passed."
            if action_passed and action_path
            else "No interaction script was requested."
            if action_passed
            else "Interaction evidence is missing or contains a failed step.",
            interaction,
        )
    )
    return checks


def inspector_check(report: Mapping[str, object], policy: Mapping[str, object]) -> dict[str, object]:
    quality = report.get("quality_report")
    report_errors = inspector_report_errors(quality, report)
    if report_errors:
        return check(
            "inspector",
            "incomplete",
            "Inspector report is malformed and cannot support a quality claim.",
            {"validation_errors": report_errors},
        )
    assert isinstance(quality, dict)
    findings = quality["findings"]
    assert isinstance(findings, list)
    allowed = set(policy.get("allowed_codes", [])) if isinstance(policy.get("allowed_codes", []), list) else set()
    relevant = [
        item
        for item in findings
        if isinstance(item, dict) and item.get("code") not in allowed
    ]
    by_severity = Counter(str(item.get("severity", "warning")) for item in relevant)
    max_findings = int(policy.get("max_findings", 0))
    maximums = policy.get("max_by_severity")
    maximums = maximums if isinstance(maximums, dict) else {}
    violations: list[str] = []
    if len(relevant) > max_findings:
        violations.append(f"findings {len(relevant)} > {max_findings}")
    for severity in ("info", "warning", "error"):
        limit = int(maximums.get(severity, max_findings))
        if by_severity[severity] > limit:
            violations.append(f"{severity} {by_severity[severity]} > {limit}")
    return check(
        "inspector",
        "fail" if violations else "pass",
        "; ".join(violations) if violations else "Inspector findings stay within the declared budget.",
        {
            "allowed_codes": sorted(allowed),
            "remaining_findings": relevant,
            "counts": dict(by_severity),
        },
    )


def baseline_bundle(
    baseline_dir: Path,
    recipe_id: str,
    scenario_id: str,
    scenario_contract_sha256: str,
    author_id: str,
) -> tuple[dict[str, object] | None, dict[str, Any] | None, list[dict[str, object]]]:
    image = baseline_dir / "baseline.png"
    report_path = baseline_dir / "baseline-report.json"
    source_evidence_path = baseline_dir / "source-evidence.json"
    metadata_path = baseline_dir / "baseline.json"
    paths = {
        "image": str(image),
        "report": str(report_path),
        "source_evidence": str(source_evidence_path),
        "metadata": str(metadata_path),
    }
    if (
        not image.is_file()
        or not report_path.is_file()
        or not source_evidence_path.is_file()
        or not metadata_path.is_file()
    ):
        return None, None, [
            check(
                "baseline.approval",
                "human-required",
                "Approved baseline bundle is missing.",
                paths,
            )
        ]
    try:
        metadata = read_json(metadata_path)
        report = read_json(report_path)
        source_evidence = read_json(source_evidence_path)
    except VerificationError as error:
        return None, None, [check("baseline.approval", "fail", str(error), paths)]
    if png_dimensions(image) is None:
        return metadata, report, [
            check(
                "baseline.approval",
                "fail",
                "Approved baseline image is not a valid PNG.",
                paths,
            )
        ]
    metadata_fields = {
        "schema_version",
        "status",
        "recipe_id",
        "scenario_id",
        "scenario_contract_sha256",
        "approved_by",
        "approver_kind",
        "approved_at",
        "approval_note",
        "source_evidence",
        "source_evidence_sha256",
        "image_sha256",
        "capture_report_sha256",
    }
    approval_valid = (
        set(metadata) == metadata_fields
        and metadata.get("schema_version") == BASELINE_SCHEMA_VERSION
        and metadata.get("status") == "approved"
        and metadata.get("recipe_id") == recipe_id
        and metadata.get("scenario_id") == scenario_id
        and metadata.get("scenario_contract_sha256")
        == scenario_contract_sha256
        and isinstance(metadata.get("approved_by"), str)
        and is_trimmed_nonempty(metadata.get("approved_by"))
        and metadata.get("approved_by") != author_id
        and metadata.get("approver_kind") in {"ai", "human"}
        and is_utc_timestamp(metadata.get("approved_at"))
        and is_trimmed_nonempty(metadata.get("approval_note"))
        and metadata.get("source_evidence") == "source-evidence.json"
        and isinstance(metadata.get("source_evidence_sha256"), str)
        and re.fullmatch(
            r"[0-9a-f]{64}", str(metadata.get("source_evidence_sha256"))
        )
    )
    if not approval_valid:
        return metadata, report, [
            check(
                "baseline.approval",
                "human-required",
                "Baseline metadata is unapproved, incomplete, or self-approved.",
                metadata,
            )
        ]
    actual_image_sha = sha256_file(image)
    actual_report_sha = sha256_file(report_path)
    actual_source_sha = sha256_file(source_evidence_path)
    source_recipe = (
        source_evidence.get("recipe")
        if isinstance(source_evidence.get("recipe"), dict)
        else {}
    )
    source_scenario = (
        source_evidence.get("scenario")
        if isinstance(source_evidence.get("scenario"), dict)
        else {}
    )
    source_artifacts = (
        source_scenario.get("artifacts")
        if isinstance(source_scenario.get("artifacts"), dict)
        else {}
    )
    source_author = (
        source_recipe.get("author")
        if isinstance(source_recipe.get("author"), dict)
        else {}
    )
    source_binaries = (
        source_evidence.get("binaries")
        if isinstance(source_evidence.get("binaries"), dict)
        else {}
    )
    source_git = (
        source_evidence.get("git")
        if isinstance(source_evidence.get("git"), dict)
        else {}
    )
    git_revision = source_git.get("revision")
    git_revision_valid = git_revision is None or (
        isinstance(git_revision, str)
        and re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", git_revision) is not None
    )
    source_valid = (
        set(source_evidence)
        == {
            "schema_version",
            "tool",
            "source_evidence_sha256",
            "recipe",
            "scenario",
            "binaries",
            "git",
        }
        and source_evidence.get("schema_version")
        == BASELINE_PROVENANCE_SCHEMA_VERSION
        and source_evidence.get("tool") == "FluentQt GUI Baseline Provenance"
        and is_sha256(source_evidence.get("source_evidence_sha256"))
        and set(source_recipe) == {"id", "sha256", "author"}
        and source_recipe.get("id") == recipe_id
        and is_sha256(source_recipe.get("sha256"))
        and set(source_author) == {"id", "kind"}
        and source_author.get("id") == author_id
        and source_author.get("kind") in {"ai", "human"}
        and set(source_scenario)
        == {"id", "pre_baseline_status", "contract_sha256", "artifacts"}
        and source_scenario.get("id") == scenario_id
        and source_scenario.get("pre_baseline_status") == "pass"
        and source_scenario.get("contract_sha256") == scenario_contract_sha256
        and set(source_artifacts)
        == {
            "actual_sha256",
            "capture_report_sha256",
            "baseline_report_sha256",
        }
        and source_artifacts.get("actual_sha256") == actual_image_sha
        and is_sha256(source_artifacts.get("capture_report_sha256"))
        and source_artifacts.get("baseline_report_sha256") == actual_report_sha
        and set(source_binaries) == {"gallery_sha256", "comparator_sha256"}
        and is_sha256(source_binaries.get("gallery_sha256"))
        and is_sha256(source_binaries.get("comparator_sha256"))
        and set(source_git) == {"revision", "dirty"}
        and git_revision_valid
        and (source_git.get("dirty") is None or isinstance(source_git.get("dirty"), bool))
    )
    baseline_validation_errors = baseline_report_errors(report)
    digest_valid = (
        metadata.get("image_sha256") == actual_image_sha
        and metadata.get("capture_report_sha256") == actual_report_sha
        and metadata.get("source_evidence_sha256") == actual_source_sha
        and source_valid
        and not baseline_validation_errors
    )
    return metadata, report, [
        check(
            "baseline.approval",
            "pass" if digest_valid else "fail",
            "Baseline approval, sanitized provenance, and content digests are valid."
            if digest_valid
            else "Baseline content or sanitized provenance changed after approval.",
            {
                **paths,
                "approved_by": metadata.get("approved_by"),
                "expected_image_sha256": metadata.get("image_sha256"),
                "actual_image_sha256": actual_image_sha,
                "expected_report_sha256": metadata.get("capture_report_sha256"),
                "actual_report_sha256": actual_report_sha,
                "expected_source_evidence_sha256": metadata.get(
                    "source_evidence_sha256"
                ),
                "actual_source_evidence_sha256": actual_source_sha,
                "source_evidence_consistent": source_valid,
                "report_validation_errors": baseline_validation_errors,
            },
        )
    ]


def fingerprint_check(
    actual_report: Mapping[str, object], baseline_report: Mapping[str, object] | None
) -> dict[str, object]:
    if baseline_report is None:
        return check(
            "environment.fingerprint",
            "human-required",
            "Approved capture fingerprint is not available.",
        )
    actual_errors = capture_environment_errors(actual_report.get("environment"))
    expected = baseline_report.get("environment_sha256")
    expected_errors = (
        []
        if is_sha256(expected)
        else ["baseline report environment_sha256 is malformed"]
    )
    actual = (
        capture_environment_sha256(actual_report) if not actual_errors else ""
    )
    matches = not actual_errors and not expected_errors and actual == expected
    return check(
        "environment.fingerprint",
        "pass" if matches else "human-required",
        "Capture environment exactly matches the approved baseline."
        if matches
        else "Capture environment differs; pixel evidence cannot be reused safely.",
        {
            "expected": expected,
            "actual": actual,
            "actual_validation_errors": actual_errors,
            "baseline_validation_errors": expected_errors,
        },
    )


def capture_environment_fingerprint(
    report: Mapping[str, object],
) -> dict[str, object]:
    environment = report.get("environment")
    environment = copy.deepcopy(environment) if isinstance(environment, dict) else {}
    screen = environment.get("screen")
    if isinstance(screen, dict):
        for field in ("name", "manufacturer", "model", "serial_number"):
            screen.pop(field, None)
    scale_environment = environment.get("scale_environment")
    if isinstance(scale_environment, dict):
        for name, value in scale_environment.items():
            if isinstance(value, str):
                scale_environment[name] = "sha256:" + sha256_bytes(
                    value.encode("utf-8")
                )
    return environment


def capture_environment_sha256(report: Mapping[str, object]) -> str:
    return sha256_bytes(canonical_json(capture_environment_fingerprint(report)))


def sanitized_baseline_report(
    report: Mapping[str, object], geometry_policy: Mapping[str, object]
) -> dict[str, object]:
    errors = capture_report_errors(report)
    if errors:
        raise VerificationError(
            "Capture report cannot be sanitized:\n- " + "\n- ".join(errors)
        )
    geometry = report["geometry_report"]
    interaction = report["interaction_report"]
    quality = report["quality_report"]
    assert isinstance(geometry, dict)
    assert isinstance(interaction, dict)
    assert isinstance(quality, dict)
    source_widgets = geometry.get("widgets")
    source_steps = interaction.get("steps")
    assert isinstance(source_widgets, list)
    assert isinstance(source_steps, list)
    geometry_policy_errors: list[str] = []
    validate_geometry_policy(
        geometry_policy,
        "baseline geometry policy",
        geometry_policy_errors,
        True,
    )
    if geometry_policy_errors:
        raise VerificationError(
            "Baseline geometry policy is invalid:\n- "
            + "\n- ".join(geometry_policy_errors)
        )
    required = geometry_policy.get("required")
    assert isinstance(required, list)
    required_names = [
        raw if isinstance(raw, str) else str(raw["object_name"])
        for raw in required
    ]
    if len(set(required_names)) != len(required_names):
        raise VerificationError(
            "Baseline geometry policy contains duplicate object_name probes"
        )
    widgets_by_name: dict[str, list[Mapping[str, object]]] = {}
    for widget in source_widgets:
        assert isinstance(widget, dict)
        name = widget.get("object_name")
        if widget.get("stable") is True and isinstance(name, str) and name:
            widgets_by_name.setdefault(name, []).append(widget)
    missing_or_ambiguous = [
        name for name in required_names if len(widgets_by_name.get(name, [])) != 1
    ]
    if missing_or_ambiguous:
        raise VerificationError(
            "Baseline geometry probes must have exactly one stable match: "
            + ", ".join(missing_or_ambiguous)
        )
    widgets = [
        {
            "object_name": name,
            "rect": copy.deepcopy(widgets_by_name[name][0]["rect"]),
        }
        for name in required_names
    ]
    steps: list[dict[str, object]] = []
    for step in source_steps:
        assert isinstance(step, dict)
        request = step.get("request")
        action = step.get("action")
        if action not in ACTION_NAMES and isinstance(request, dict):
            action = request.get("action")
        steps.append(
            {
                "index": step.get("index"),
                "action": action,
                "status": step.get("status"),
            }
        )
    sanitized = {
        "schema_version": BASELINE_SCHEMA_VERSION,
        "tool": "FluentQt GUI Baseline Report",
        "environment_sha256": capture_environment_sha256(report),
        "geometry_report": {
            "schema_version": geometry.get("schema_version"),
            "tool": geometry.get("tool"),
            "root_size": copy.deepcopy(geometry.get("root_size")),
            "widget_count": len(widgets),
            "widgets": widgets,
        },
        "interaction_report": {
            "schema_version": interaction.get("schema_version"),
            "requested": interaction.get("requested"),
            "status": interaction.get("status"),
            "summary": copy.deepcopy(interaction.get("summary")),
            "steps": steps,
        },
        "quality_report": {
            "schema_version": quality.get("schema_version"),
            "tool": quality.get("tool"),
            "summary": copy.deepcopy(quality.get("summary")),
        },
    }
    sanitized_errors = baseline_report_errors(sanitized)
    if sanitized_errors:
        raise VerificationError(
            "Sanitized capture report is invalid:\n- "
            + "\n- ".join(sanitized_errors)
        )
    return sanitized


def native_desktop_check(report: Mapping[str, object], required: bool) -> dict[str, object]:
    environment_errors = capture_environment_errors(report.get("environment"))
    plugin = nested(report, "environment", "platform_plugin")
    product_type = nested(report, "environment", "system", "product_type")
    kernel_type = nested(report, "environment", "system", "kernel_type")
    normalized_plugin = plugin.lower() if isinstance(plugin, str) else ""
    normalized_product = (
        product_type.lower() if isinstance(product_type, str) else ""
    )
    normalized_kernel = (
        kernel_type.lower() if isinstance(kernel_type, str) else ""
    )
    expected_host = host_key()[1]
    captured_host = capture_host_key(report)
    platform_matches = (
        normalized_plugin == "cocoa"
        and normalized_product in {"macos", "osx"}
        and normalized_kernel == "darwin"
    ) or (
        normalized_plugin == "windows"
        and normalized_product == "windows"
        and normalized_kernel in {"windows", "winnt"}
    ) or (
        normalized_plugin in {"xcb", "wayland", "wayland-egl"}
        and normalized_kernel == "linux"
        and bool(normalized_product)
        and normalized_product
        not in {"android", "ios", "macos", "osx", "tvos", "watchos", "windows"}
    )
    is_native = normalized_plugin in NATIVE_PLUGINS and platform_matches
    if environment_errors:
        return check(
            "environment.native-desktop",
            "incomplete",
            "Final evidence requires a complete capture environment fingerprint.",
            {
                "platform_plugin": plugin,
                "product_type": product_type,
                "kernel_type": kernel_type,
                "validation_errors": environment_errors,
            },
        )
    if captured_host != expected_host:
        return check(
            "environment.native-desktop",
            "incomplete",
            "Capture OS or architecture differs from the verification host; "
            "baseline routing would be unsafe.",
            {
                "expected_host": expected_host,
                "captured_host": captured_host,
                "platform_plugin": plugin,
                "product_type": product_type,
                "kernel_type": kernel_type,
            },
        )
    if not required:
        return check(
            "environment.native-desktop",
            "not-applicable",
            "Recipe explicitly allows a headless or non-desktop QPA plugin.",
            {
                "expected_host": expected_host,
                "captured_host": captured_host,
                "platform_plugin": plugin,
                "product_type": product_type,
                "kernel_type": kernel_type,
            },
        )
    return check(
        "environment.native-desktop",
        "pass" if is_native else "incomplete",
        "Capture used an OS-consistent desktop QPA plugin."
        if is_native
        else "Final evidence requires an OS-consistent desktop QPA plugin.",
        {
            "expected_host": expected_host,
            "captured_host": captured_host,
            "platform_plugin": plugin,
            "product_type": product_type,
            "kernel_type": kernel_type,
        },
    )


def reset_expected_output(path: Path, owner_directory: Path) -> None:
    """Remove only a declared producer output so stale evidence cannot be reused."""

    if not path_is_within(path, owner_directory):
        raise VerificationError(
            f"Refusing to reset output outside {owner_directory}: {path}"
        )
    if not path.exists():
        return
    if not path.is_file():
        raise VerificationError(f"Expected output is not a regular file: {path}")
    path.unlink()


def run_pixel_comparison(
    comparator: Path,
    baseline_dir: Path,
    actual_path: Path,
    scenario_dir: Path,
    policy: Mapping[str, object],
    device_pixel_ratio: float,
) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    baseline_path = baseline_dir / "baseline.png"
    if not baseline_path.is_file():
        return [
            check("pixels.full", "human-required", "Approved baseline pixels are unavailable.")
        ], []
    comparisons = pixel_comparisons(policy, device_pixel_ratio)

    checks: list[dict[str, object]] = []
    executions: list[dict[str, object]] = []
    for comparison_id, comparison_policy, region in comparisons:
        report_path = scenario_dir / f"pixel-{comparison_id}.json"
        diff_path = scenario_dir / f"diff-{comparison_id}.png"
        reset_expected_output(report_path, scenario_dir)
        reset_expected_output(diff_path, scenario_dir)
        command = [
            str(comparator),
            "--baseline",
            str(baseline_path),
            "--actual",
            str(actual_path),
            "--report",
            str(report_path),
            "--diff",
            str(diff_path),
            *comparator_policy_arguments(comparison_policy),
            "--quiet",
        ]
        if region is not None:
            command.extend(["--region", ",".join(str(value) for value in region)])
        completed = run_captured_command(
            command,
            cwd=PROJECT_ROOT,
        )
        execution = command_record(command, completed)
        execution.update(
            {
                "id": comparison_id,
                "report": str(report_path),
                "diff": str(diff_path) if diff_path.is_file() else None,
            }
        )
        executions.append(execution)
        comparison_report: dict[str, object] | None = None
        if report_path.is_file():
            try:
                comparison_report = read_json(report_path)
            except VerificationError:
                comparison_report = None
        validation_errors = (
            validate_comparator_report(
                comparison_report,
                completed.returncode,
                baseline_path,
                actual_path,
                region,
                comparison_policy,
            )
            if comparison_report is not None
            else ["comparator report is missing or invalid JSON"]
        )
        if comparison_report is not None:
            execution["report_sha256"] = sha256_file(report_path)
        if completed.returncode == 0 and not validation_errors:
            status = "pass"
            message = "Pixels match the approved policy."
        elif completed.returncode == 1 and not validation_errors:
            status = "fail"
            message = "Pixel comparison exceeded the approved policy."
        else:
            status = "incomplete"
            message = "Pixel comparator could not produce trustworthy evidence."
        checks.append(
            check(
                f"pixels.{comparison_id}",
                status,
                message,
                {
                    "region": region,
                    "report": comparison_report,
                    "report_sha256": execution.get("report_sha256"),
                    "validation_errors": validation_errors,
                    "execution": execution,
                },
            )
        )
    return checks, executions


def scenario_pre_baseline_checks(
    recipe: Mapping[str, object],
    scenario: Mapping[str, object],
    report: Mapping[str, object],
    action_path: Path | None,
    actual_path: Path,
) -> list[dict[str, object]]:
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    inspector = merged_dict(defaults.get("inspector"), scenario.get("inspector"))
    geometry = merged_dict(defaults.get("geometry"), scenario.get("geometry"))
    required_native = effective_require_native_desktop(defaults, scenario)
    return [
        capture_report_check(report),
        *identity_checks(recipe, scenario, report, action_path, actual_path),
        capture_environment_check(report),
        native_desktop_check(report, required_native),
        inspector_check(report, inspector),
        geometry_contract_check(report, geometry),
    ]


def run_scenario(
    recipe: Mapping[str, object],
    recipe_path: Path,
    scenario: Mapping[str, object],
    output_dir: Path,
    gallery: Path,
    comparator: Path,
) -> dict[str, object]:
    scenario_id = str(scenario["id"])
    scenario_dir = output_dir / "scenarios" / scenario_id
    scenario_dir.mkdir(parents=True, exist_ok=True)
    path_base = recipe_path_base(recipe, recipe_path)
    baseline_dir = select_baseline(scenario.get("baseline"), path_base)
    action_path, action_script = prepare_action_script(
        scenario.get("actions"), path_base, scenario_dir
    )
    semantic_errors: list[str] = []
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    validate_scenario_semantics(
        scenario,
        action_script,
        f"scenario {scenario_id}",
        semantic_errors,
        require_native_desktop=effective_require_native_desktop(
            defaults, scenario
        ),
    )
    if semantic_errors:
        raise VerificationError(
            "Invalid scenario semantics:\n- " + "\n- ".join(semantic_errors)
        )
    contract = scenario_contract(recipe, scenario, action_path)
    contract_sha256 = sha256_bytes(canonical_json(contract))
    actual_path = scenario_dir / "actual.png"
    report_path = scenario_dir / "capture.json"
    reset_expected_output(actual_path, scenario_dir)
    reset_expected_output(report_path, scenario_dir)
    command = capture_command(gallery, recipe, scenario, scenario_dir, action_path)
    environment, environment_overrides = relevant_environment(recipe, scenario)
    timeout = int(scenario.get("timeout_seconds", defaults.get("timeout_seconds", 45)))
    try:
        completed = run_captured_command(
            command,
            cwd=PROJECT_ROOT,
            env=environment,
            timeout=timeout,
        )
        execution = command_record(command, completed)
    except subprocess.TimeoutExpired as error:
        execution = {
            "command": command,
            "returncode": None,
            "stdout": error.stdout or "",
            "stderr": error.stderr or "",
            "timed_out": True,
        }
        return {
            "id": scenario_id,
            "tags": scenario.get("tags", []),
            "review": scenario.get("review", []),
            "status": "incomplete",
            "pre_baseline_status": "incomplete",
            "baseline_dir": str(baseline_dir),
            "contract": contract,
            "contract_sha256": contract_sha256,
            "checks": [check("capture.process", "incomplete", f"Capture timed out after {timeout}s.")],
            "capture": execution,
            "artifacts": {"directory": str(scenario_dir)},
        }

    process_status = "pass" if completed.returncode == 0 else "fail" if completed.returncode == 6 else "incomplete"
    process_check = check(
        "capture.process",
        process_status,
        "Gallery preview process completed successfully."
        if process_status == "pass"
        else "Gallery interaction assertions failed."
        if process_status == "fail"
        else f"Gallery preview exited with code {completed.returncode}.",
        execution,
    )
    if not report_path.is_file():
        return {
            "id": scenario_id,
            "tags": scenario.get("tags", []),
            "review": scenario.get("review", []),
            "status": "incomplete",
            "pre_baseline_status": "incomplete",
            "baseline_dir": str(baseline_dir),
            "contract": contract,
            "contract_sha256": contract_sha256,
            "checks": [process_check, check("capture.report", "incomplete", "Capture report is missing.")],
            "capture": execution,
            "artifacts": {"directory": str(scenario_dir), "actual": str(actual_path)},
        }
    try:
        report = read_json(report_path)
    except VerificationError as error:
        return {
            "id": scenario_id,
            "tags": scenario.get("tags", []),
            "review": scenario.get("review", []),
            "status": "incomplete",
            "pre_baseline_status": "incomplete",
            "baseline_dir": str(baseline_dir),
            "contract": contract,
            "contract_sha256": contract_sha256,
            "checks": [process_check, check("capture.report", "incomplete", str(error))],
            "capture": execution,
            "artifacts": {"directory": str(scenario_dir), "actual": str(actual_path), "report": str(report_path)},
        }

    report_status = report.get("status")
    if report_status == "ok":
        report_gate = check("capture.report", "pass", "Capture report status is ok.")
    elif report_status == "interaction-error":
        report_gate = check("capture.report", "fail", "Capture report contains interaction failures.")
    else:
        report_gate = check("capture.report", "incomplete", f"Capture report status is {report_status!r}.")
    pre_checks = [process_check, report_gate]
    pre_checks.extend(scenario_pre_baseline_checks(recipe, scenario, report, action_path, actual_path))
    pre_status = combined_status(pre_checks)

    author = recipe.get("author") if isinstance(recipe.get("author"), dict) else {}
    metadata, baseline_report, baseline_checks = baseline_bundle(
        baseline_dir,
        str(recipe.get("id", "")),
        scenario_id,
        contract_sha256,
        str(author.get("id", "")),
    )
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    geometry_policy = merged_dict(defaults.get("geometry"), scenario.get("geometry"))
    baseline_checks.append(geometry_baseline_check(report, baseline_report, geometry_policy))
    baseline_checks.append(fingerprint_check(report, baseline_report))
    pixel_policy = merged_dict(defaults.get("pixel"), scenario.get("pixel"))
    pixel_checks: list[dict[str, object]] = []
    pixel_executions: list[dict[str, object]] = []
    baseline_approval = next((item for item in baseline_checks if item["id"] == "baseline.approval"), None)
    fingerprint = next((item for item in baseline_checks if item["id"] == "environment.fingerprint"), None)
    device_pixel_ratio = validated_device_pixel_ratio(
        nested(report, "environment", "device_pixel_ratio")
    )
    if (
        baseline_approval
        and baseline_approval["status"] == "pass"
        and fingerprint
        and fingerprint["status"] == "pass"
        and device_pixel_ratio is not None
    ):
        pixel_checks, pixel_executions = run_pixel_comparison(
            comparator,
            baseline_dir,
            actual_path,
            scenario_dir,
            pixel_policy,
            device_pixel_ratio,
        )
    else:
        pixel_checks.append(
            check(
                "pixels.full",
                "human-required",
                "Pixel comparison is gated on an approved same-environment baseline.",
            )
        )

    checks = [*pre_checks, *baseline_checks, *pixel_checks]
    status = combined_status(checks)
    artifacts = {
        "directory": str(scenario_dir),
        "actual": str(actual_path),
        "actual_sha256": sha256_file(actual_path) if actual_path.is_file() else None,
        "report": str(report_path),
        "report_sha256": sha256_file(report_path),
        "baseline": str(baseline_dir / "baseline.png"),
        "baseline_report": str(baseline_dir / "baseline-report.json"),
        "diffs": [execution.get("diff") for execution in pixel_executions if execution.get("diff")],
    }
    return {
        "id": scenario_id,
        "tags": scenario.get("tags", []),
        "review": scenario.get("review", []),
        "conditions": {
            "theme": scenario.get("theme"),
            "direction": scenario.get("direction", "ltr"),
            "size": scenario.get("size"),
            "actions": str(action_path) if action_path else None,
            "actions_sha256": sha256_file(action_path) if action_path else None,
        },
        "status": status,
        "pre_baseline_status": pre_status,
        "baseline_dir": str(baseline_dir),
        "contract": contract,
        "contract_sha256": contract_sha256,
        "baseline_metadata": metadata,
        "checks": checks,
        "capture": execution,
        "environment_overrides": environment_overrides,
        "pixel_executions": pixel_executions,
        "artifacts": artifacts,
    }


def write_review_request(evidence_path: Path, evidence: Mapping[str, object], output: Path) -> dict[str, object]:
    evidence_sha = sha256_file(evidence_path)
    author = nested(evidence, "recipe", "author")
    scenarios = evidence.get("scenarios") if isinstance(evidence.get("scenarios"), list) else []
    scenario_ids = [str(item.get("id")) for item in scenarios if isinstance(item, dict)]
    interactions_required = any(
        isinstance(item, dict) and nested(item, "conditions", "actions") is not None
        for item in scenarios
    )
    request = {
        "schema_version": 1,
        "tool": "FluentQt GUI Independent Review Request",
        "evidence": str(evidence_path),
        "evidence_sha256": evidence_sha,
        "author": author,
        "deterministic_status": evidence.get("deterministic_status"),
        "required_scenarios": scenario_ids,
        "instructions": [
            "Open every actual, approved baseline, and available diff at native resolution.",
            "Use the scenario prompts; inspect hierarchy, typography, spacing, clipping, states, and Light/Dark behavior.",
            "Verify interaction evidence when the scenario declares actions.",
            "Cite scenario, region, artifact, and concrete observation for every finding.",
            "Do not approve if you authored the evidence or did not open the visual artifacts.",
        ],
        "review_template": {
            "schema_version": REVIEW_SCHEMA_VERSION,
            "reviewer": {"id": "", "kind": "ai-or-human", "tool": ""},
            "evidence_sha256": evidence_sha,
            "verdict": "pass-or-fail",
            "summary": "",
            "reviewed_scenarios": scenario_ids,
            "attestation": {
                "independent": True,
                "visual_artifacts_opened": True,
                "interaction_evidence_reviewed": interactions_required,
            },
            "findings": [],
        },
    }
    write_json(output, request)
    return request


def run_recipe(args: argparse.Namespace) -> int:
    recipe_path = args.recipe.expanduser().resolve()
    recipe = read_json(recipe_path)
    errors = validate_recipe(recipe)
    if errors:
        raise VerificationError("Invalid GUI verification recipe:\n- " + "\n- ".join(errors))
    if args.output_dir:
        output_dir = args.output_dir.expanduser().resolve()
        path_base = recipe_path_base(recipe, recipe_path)
        for raw in recipe.get("scenarios", []):
            if not isinstance(raw, dict):
                continue
            baseline_dir = select_baseline(raw.get("baseline"), path_base)
            if paths_overlap(output_dir, baseline_dir):
                raise VerificationError(
                    "GUI verification output must not overlap an approved "
                    f"baseline destination: {baseline_dir}"
                )
        if output_dir.exists() and any(output_dir.iterdir()) and not args.replace_output:
            raise VerificationError(
                f"Output directory is not empty: {output_dir}; use --replace-output or a new directory"
            )
    else:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        output_dir = PROJECT_ROOT / "build" / "gui-verification" / str(recipe["id"]) / stamp
    output_dir.mkdir(parents=True, exist_ok=True)
    started_at = utc_now()
    build = build_dependencies(args)
    evidence_path = output_dir / "evidence.json"
    review_request_path = output_dir / "review-request.json"
    review_html_path = output_dir / "review.html"
    if build.get("status") == "fail":
        evidence = {
            "schema_version": TOOL_SCHEMA_VERSION,
            "tool": "FluentQt GUI Verify",
            "status": "incomplete",
            "deterministic_status": "incomplete",
            "started_at": started_at,
            "finished_at": utc_now(),
            "recipe": {
                "id": recipe.get("id"),
                "path": str(recipe_path),
                "sha256": sha256_file(recipe_path),
                "author": recipe.get("author"),
                "path_base": recipe.get("path_base"),
                "resolved_path_base": str(recipe_path_base(recipe, recipe_path)),
            },
            "build": build,
            "git": git_state(),
            "scenarios": [],
            "summary": {"total": 0, "by_status": {}},
        }
        write_json(evidence_path, evidence)
        write_review_request(evidence_path, evidence, review_request_path)
        review_html_path.write_text(review_html(evidence), encoding="utf-8")
        print(f"GUI verification incomplete: {evidence_path}")
        return 1

    build_dir = configured_build_dir(args)
    gallery = args.gallery.expanduser().resolve() if args.gallery else resolve_gallery_executable(build_dir)
    comparator = args.comparator.expanduser().resolve() if args.comparator else resolve_comparator_executable(build_dir)
    if not gallery.is_file():
        raise VerificationError(f"Gallery executable does not exist: {gallery}")
    if not comparator.is_file():
        raise VerificationError(f"Visual comparator does not exist: {comparator}")
    if not path_is_within(gallery, build_dir / "app"):
        raise VerificationError(
            "Gallery executable must live under the configured build app directory"
        )
    if not path_is_within(comparator, build_dir / "tools" / "dev"):
        raise VerificationError(
            "Visual comparator must live under the configured build tools directory"
        )
    scenarios = [
        run_scenario(recipe, recipe_path, raw, output_dir, gallery, comparator)
        for raw in recipe["scenarios"]
        if isinstance(raw, dict)
    ]
    counts = Counter(str(item["status"]) for item in scenarios)
    deterministic_status = combined_status(
        [{"status": item["status"]} for item in scenarios]
    )
    status = "review-required" if deterministic_status == "pass" else deterministic_status
    evidence = {
        "schema_version": TOOL_SCHEMA_VERSION,
        "tool": "FluentQt GUI Verify",
        "status": status,
        "deterministic_status": deterministic_status,
        "started_at": started_at,
        "finished_at": utc_now(),
        "recipe": {
            "id": recipe.get("id"),
            "path": str(recipe_path),
            "sha256": sha256_file(recipe_path),
            "author": recipe.get("author"),
            "coverage": recipe.get("coverage"),
            "path_base": recipe.get("path_base"),
            "resolved_path_base": str(recipe_path_base(recipe, recipe_path)),
        },
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "git": git_state(),
        "build": build,
        "binaries": {
            "build_dir": str(build_dir),
            "gallery": {"path": str(gallery), "sha256": sha256_file(gallery)},
            "comparator": {"path": str(comparator), "sha256": sha256_file(comparator)},
        },
        "scenarios": scenarios,
        "summary": {"total": len(scenarios), "by_status": dict(counts)},
        "artifacts": {
            "evidence": str(evidence_path),
            "review_request": str(review_request_path),
            "review_html": str(review_html_path),
        },
    }
    write_json(evidence_path, evidence)
    write_review_request(evidence_path, evidence, review_request_path)
    review_html_path.write_text(review_html(evidence), encoding="utf-8")
    print(f"GUI verification {status}: {evidence_path}")
    # A deterministic pass is only ready for a separate visual review.  The
    # finalize command is the sole path that returns success for final acceptance.
    return 1


def load_evidence_recipe(
    evidence: Mapping[str, object], errors: list[str]
) -> tuple[dict[str, Any] | None, Path | None]:
    if evidence.get("schema_version") != TOOL_SCHEMA_VERSION:
        errors.append("evidence schema_version is missing or unsupported")
    if evidence.get("tool") != "FluentQt GUI Verify":
        errors.append("evidence tool identity is missing or unsupported")
    recipe_record = (
        evidence.get("recipe") if isinstance(evidence.get("recipe"), dict) else {}
    )
    path_value = recipe_record.get("path")
    if not isinstance(path_value, str) or not path_value:
        errors.append("evidence recipe.path is required")
        return None, None
    recipe_path = Path(path_value).expanduser().resolve()
    if not recipe_path.is_file():
        errors.append("evidence source recipe is missing")
        return None, recipe_path
    if recipe_record.get("sha256") != sha256_file(recipe_path):
        errors.append("evidence source recipe digest is stale")
        return None, recipe_path
    try:
        recipe = read_json(recipe_path)
    except VerificationError as error:
        errors.append(str(error))
        return None, recipe_path
    recipe_errors = validate_recipe(recipe)
    if recipe_errors:
        errors.extend(f"source recipe: {error}" for error in recipe_errors)
    for field in ("id", "author", "coverage", "path_base"):
        if recipe_record.get(field) != recipe.get(field):
            errors.append(f"evidence recipe.{field} does not match the source recipe")
    return recipe, recipe_path


def stored_check_statuses(
    scenario_record: Mapping[str, object], errors: list[str], context: str
) -> dict[str, str]:
    checks = scenario_record.get("checks")
    if not isinstance(checks, list) or not checks:
        errors.append(f"{context}.checks must be a non-empty array")
        return {}
    result: dict[str, str] = {}
    for index, item in enumerate(checks):
        if not isinstance(item, dict):
            errors.append(f"{context}.checks[{index}] must be an object")
            continue
        check_id = item.get("id")
        status = item.get("status")
        if not isinstance(check_id, str) or not check_id:
            errors.append(f"{context}.checks[{index}].id is required")
            continue
        if check_id in result:
            errors.append(f"{context}.checks contains duplicate id {check_id}")
            continue
        if status not in STATUS_PRIORITY:
            errors.append(f"{context}.checks[{index}].status is invalid")
            continue
        result[check_id] = str(status)
    return result


def scenario_capture_context(
    evidence: Mapping[str, object],
    recipe: Mapping[str, object],
    recipe_path: Path,
    scenario_record: Mapping[str, object],
    source_scenario: Mapping[str, object],
    errors: list[str],
) -> dict[str, object]:
    scenario_id = str(source_scenario.get("id"))
    context = f"scenario {scenario_id}"
    stored_statuses = stored_check_statuses(scenario_record, errors, context)
    artifacts = (
        scenario_record.get("artifacts")
        if isinstance(scenario_record.get("artifacts"), dict)
        else {}
    )
    conditions = (
        scenario_record.get("conditions")
        if isinstance(scenario_record.get("conditions"), dict)
        else {}
    )
    actual = Path(str(artifacts.get("actual", ""))).expanduser().resolve()
    report_path = Path(str(artifacts.get("report", ""))).expanduser().resolve()
    scenario_directory = Path(str(artifacts.get("directory", ""))).resolve()
    if actual != scenario_directory / "actual.png":
        errors.append(f"{context} actual artifact is outside its scenario directory")
    if report_path != scenario_directory / "capture.json":
        errors.append(f"{context} capture report is outside its scenario directory")
    if not actual.is_file() or png_dimensions(actual) is None:
        errors.append(f"{context} actual artifact is missing or not a valid PNG")
    elif artifacts.get("actual_sha256") != sha256_file(actual):
        errors.append(f"{context} actual artifact digest is stale")
    if not report_path.is_file():
        errors.append(f"{context} capture report is missing")
        report: dict[str, Any] = {}
    else:
        if artifacts.get("report_sha256") != sha256_file(report_path):
            errors.append(f"{context} capture report digest is stale")
        try:
            report = read_json(report_path)
        except VerificationError as error:
            errors.append(f"{context} {error}")
            report = {}

    source_actions = source_scenario.get("actions")
    action_path: Path | None = None
    action_script: dict[str, object] | None = None
    if source_actions is not None:
        recorded_action = conditions.get("actions")
        if not isinstance(recorded_action, str) or not recorded_action:
            errors.append(f"{context} action artifact path is missing")
        else:
            action_path = Path(recorded_action).expanduser().resolve()
            if isinstance(source_actions, str):
                expected_action = resolved_path(
                    source_actions, recipe_path_base(recipe, recipe_path)
                )
            else:
                directory = Path(str(artifacts.get("directory", ""))).resolve()
                expected_action = directory / "actions.json"
            if action_path != expected_action.resolve():
                errors.append(f"{context} action artifact path does not match the recipe")
            if not action_path.is_file():
                errors.append(f"{context} action artifact is missing")
            else:
                if conditions.get("actions_sha256") != sha256_file(action_path):
                    errors.append(f"{context} action artifact digest is stale")
                try:
                    action_script = read_json(action_path)
                except VerificationError as error:
                    errors.append(f"{context} {error}")
                if action_script is not None:
                    action_errors: list[str] = []
                    validate_action_script(action_script, f"{context}.actions", action_errors)
                    validate_scenario_semantics(
                        source_scenario,
                        action_script,
                        context,
                        action_errors,
                        require_native_desktop=effective_require_native_desktop(
                            recipe.get("defaults", {})
                            if isinstance(recipe.get("defaults"), dict)
                            else {},
                            source_scenario,
                        ),
                    )
                    errors.extend(action_errors)
    elif conditions.get("actions") is not None or conditions.get("actions_sha256") is not None:
        errors.append(f"{context} records actions that are absent from the recipe")

    contract = scenario_contract(recipe, source_scenario, action_path)
    contract_sha256 = sha256_bytes(canonical_json(contract))
    if scenario_record.get("contract") != contract:
        errors.append(f"{context} scenario contract does not match the source recipe")
    if scenario_record.get("contract_sha256") != contract_sha256:
        errors.append(f"{context} scenario contract digest is stale")

    capture = (
        scenario_record.get("capture")
        if isinstance(scenario_record.get("capture"), dict)
        else {}
    )
    gallery_path = Path(
        str(nested(evidence, "binaries", "gallery", "path") or "")
    ).resolve()
    capture_command_value = capture.get("command")
    expected_capture_command = capture_command(
        gallery_path,
        recipe,
        source_scenario,
        scenario_directory,
        action_path,
    )
    if capture_command_value != expected_capture_command:
        errors.append(
            f"{context} capture command does not match the source scenario"
        )
    process_check = check(
        "capture.process",
        "pass" if capture.get("returncode") == 0 else "incomplete",
        "Capture process integrity was recomputed.",
    )
    report_check = check(
        "capture.report",
        "pass" if report.get("status") == "ok" else "incomplete",
        "Capture report integrity was recomputed.",
    )
    pre_checks = [process_check, report_check]
    pre_checks.extend(
        scenario_pre_baseline_checks(
            recipe, source_scenario, report, action_path, actual
        )
    )
    for expected in pre_checks:
        if stored_statuses.get(str(expected["id"])) != expected["status"]:
            errors.append(
                f"{context} stored {expected['id']} status does not match recomputation"
            )
    pre_status = combined_status(pre_checks)
    if pre_status != "pass" or scenario_record.get("pre_baseline_status") != pre_status:
        errors.append(f"{context} pre-baseline evidence is not a recomputed pass")
    return {
        "record": scenario_record,
        "source": source_scenario,
        "actual": actual,
        "report_path": report_path,
        "report": report,
        "action_path": action_path,
        "contract_sha256": contract_sha256,
        "stored_statuses": stored_statuses,
        "comparator": Path(
            str(nested(evidence, "binaries", "comparator", "path") or "")
        ).resolve(),
    }


def validate_evidence_captures(
    evidence: Mapping[str, object], only_scenario: str | None = None
) -> tuple[dict[str, Any] | None, Path | None, list[dict[str, object]], list[str]]:
    errors: list[str] = []
    recipe, recipe_path = load_evidence_recipe(evidence, errors)
    records = evidence.get("scenarios")
    if not isinstance(records, list) or not records:
        errors.append("evidence scenarios must be a non-empty array")
        return recipe, recipe_path, [], errors
    record_by_id: dict[str, Mapping[str, object]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, dict) or not isinstance(record.get("id"), str):
            errors.append(f"evidence scenarios[{index}] is malformed")
            continue
        scenario_id = str(record["id"])
        if scenario_id in record_by_id:
            errors.append(f"evidence contains duplicate scenario {scenario_id}")
        record_by_id[scenario_id] = record
    if recipe is None or recipe_path is None:
        return recipe, recipe_path, [], errors
    source_scenarios = recipe.get("scenarios")
    source_scenarios = source_scenarios if isinstance(source_scenarios, list) else []
    source_by_id = {
        str(item.get("id")): item for item in source_scenarios if isinstance(item, dict)
    }
    if set(record_by_id) != set(source_by_id):
        errors.append("evidence scenarios do not exactly match the source recipe")
    requested = [only_scenario] if only_scenario is not None else list(source_by_id)
    contexts: list[dict[str, object]] = []
    for scenario_id in requested:
        record = record_by_id.get(scenario_id)
        source = source_by_id.get(scenario_id)
        if record is None or source is None:
            errors.append(f"scenario is not present in recipe-backed evidence: {scenario_id}")
            continue
        contexts.append(
            scenario_capture_context(
                evidence, recipe, recipe_path, record, source, errors
            )
        )
    return recipe, recipe_path, contexts, errors


def validate_final_scenario(
    recipe: Mapping[str, object],
    recipe_path: Path,
    context: Mapping[str, object],
    errors: list[str],
) -> None:
    record = context["record"]
    source = context["source"]
    assert isinstance(record, Mapping) and isinstance(source, Mapping)
    scenario_id = str(source.get("id"))
    prefix = f"scenario {scenario_id}"
    expected_baseline = select_baseline(
        source.get("baseline"), recipe_path_base(recipe, recipe_path)
    )
    baseline_dir = Path(str(record.get("baseline_dir", ""))).resolve()
    if baseline_dir != expected_baseline.resolve():
        errors.append(f"{prefix} baseline directory does not match the recipe")
    if not is_approved_baseline_bundle_path(baseline_dir):
        errors.append(
            f"{prefix} approved baseline must be exactly "
            "tests/visual-baselines/gui/<component>/<scenario>"
        )
    author = recipe.get("author") if isinstance(recipe.get("author"), dict) else {}
    metadata, baseline_report, baseline_checks = baseline_bundle(
        baseline_dir,
        str(recipe.get("id", "")),
        scenario_id,
        str(context["contract_sha256"]),
        str(author.get("id", "")),
    )
    report = context["report"]
    assert isinstance(report, Mapping)
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    geometry_policy = merged_dict(
        defaults.get("geometry"), source.get("geometry")
    )
    baseline_checks.append(
        geometry_baseline_check(report, baseline_report, geometry_policy)
    )
    baseline_checks.append(fingerprint_check(report, baseline_report))
    stored_statuses = context["stored_statuses"]
    assert isinstance(stored_statuses, Mapping)
    for expected in baseline_checks:
        if expected.get("status") != "pass":
            errors.append(f"{prefix} {expected.get('id')} is not a recomputed pass")
        if stored_statuses.get(str(expected.get("id"))) != expected.get("status"):
            errors.append(f"{prefix} stored {expected.get('id')} status is stale")
    if record.get("baseline_metadata") != metadata:
        errors.append(f"{prefix} stored baseline metadata is stale")

    pixel_policy = merged_dict(defaults.get("pixel"), source.get("pixel"))
    dpr = validated_device_pixel_ratio(
        nested(report, "environment", "device_pixel_ratio")
    )
    if dpr is None:
        errors.append(f"{prefix} capture device_pixel_ratio is invalid")
        comparisons = []
    else:
        comparisons = pixel_comparisons(pixel_policy, dpr)
    executions = record.get("pixel_executions")
    executions = executions if isinstance(executions, list) else []
    if len(executions) != len(comparisons):
        errors.append(f"{prefix} pixel execution count is incomplete")
    actual = context["actual"]
    assert isinstance(actual, Path)
    for index, (comparison_id, policy, region) in enumerate(comparisons):
        if index >= len(executions) or not isinstance(executions[index], dict):
            continue
        execution = executions[index]
        if execution.get("id") != comparison_id:
            errors.append(f"{prefix} pixel execution order is stale")
        command = execution.get("command")
        comparator = context.get("comparator")
        if (
            not isinstance(command, list)
            or not command
            or not isinstance(comparator, Path)
            or Path(str(command[0])).resolve() != comparator
        ):
            errors.append(
                f"{prefix} pixel execution {comparison_id} does not use the "
                "recorded comparator"
            )
        report_path = Path(str(execution.get("report", ""))).resolve()
        if not report_path.is_file():
            errors.append(f"{prefix} pixel report {comparison_id} is missing")
            continue
        if execution.get("report_sha256") != sha256_file(report_path):
            errors.append(f"{prefix} pixel report {comparison_id} digest is stale")
        try:
            pixel_report = read_json(report_path)
        except VerificationError as error:
            errors.append(f"{prefix} {error}")
            continue
        validation_errors = validate_comparator_report(
            pixel_report,
            int(execution.get("returncode", -1)),
            baseline_dir / "baseline.png",
            actual,
            region,
            policy,
        )
        if validation_errors or execution.get("returncode") != 0:
            errors.append(
                f"{prefix} pixel report {comparison_id} is not a trustworthy pass"
            )
        if stored_statuses.get(f"pixels.{comparison_id}") != "pass":
            errors.append(f"{prefix} stored pixels.{comparison_id} status is stale")
    if record.get("status") != "pass":
        errors.append(f"{prefix} deterministic scenario status is not pass")


def validate_final_evidence(
    evidence: Mapping[str, object]
) -> list[str]:
    recipe, recipe_path, contexts, errors = validate_evidence_captures(evidence)
    if recipe is None or recipe_path is None:
        return errors
    binaries = (
        evidence.get("binaries")
        if isinstance(evidence.get("binaries"), dict)
        else {}
    )
    build_dir_value = binaries.get("build_dir")
    if not isinstance(build_dir_value, str) or not build_dir_value:
        errors.append("evidence binaries.build_dir is required")
    else:
        build_dir = Path(build_dir_value).resolve()
        for name, relative_root in (
            ("gallery", Path("app")),
            ("comparator", Path("tools/dev")),
        ):
            record = binaries.get(name)
            record = record if isinstance(record, dict) else {}
            binary_path = Path(str(record.get("path", ""))).resolve()
            if not path_is_within(binary_path, build_dir / relative_root):
                errors.append(f"evidence {name} binary is outside its build directory")
            if not binary_path.is_file():
                errors.append(f"evidence {name} binary is missing")
            elif record.get("sha256") != sha256_file(binary_path):
                errors.append(f"evidence {name} binary digest is stale")
    for context in contexts:
        validate_final_scenario(recipe, recipe_path, context, errors)
    if evidence.get("deterministic_status") != "pass":
        errors.append("evidence deterministic_status is not pass")
    if evidence.get("status") != "review-required":
        errors.append("passing deterministic evidence must remain review-required")
    scenarios = evidence.get("scenarios")
    scenarios = scenarios if isinstance(scenarios, list) else []
    counts = Counter(
        str(item.get("status")) for item in scenarios if isinstance(item, dict)
    )
    expected_summary = {"total": len(scenarios), "by_status": dict(counts)}
    if evidence.get("summary") != expected_summary:
        errors.append("evidence summary does not match scenario statuses")
    return errors


def baseline_provenance_manifest(
    evidence: Mapping[str, object],
    evidence_path: Path,
    scenario: Mapping[str, object],
    contract_sha256: str,
    actual: Path,
    capture_report: Path,
    baseline_report: Path,
) -> dict[str, object]:
    recipe_record = (
        evidence.get("recipe") if isinstance(evidence.get("recipe"), dict) else {}
    )
    author = (
        recipe_record.get("author")
        if isinstance(recipe_record.get("author"), dict)
        else {}
    )
    binaries = (
        evidence.get("binaries")
        if isinstance(evidence.get("binaries"), dict)
        else {}
    )
    build_dir = Path(str(binaries.get("build_dir", ""))).resolve()
    binary_digests: dict[str, str] = {}
    for name, relative_root in (
        ("gallery", Path("app")),
        ("comparator", Path("tools/dev")),
    ):
        record = binaries.get(name)
        record = record if isinstance(record, dict) else {}
        binary_path = Path(str(record.get("path", ""))).resolve()
        digest = record.get("sha256")
        if (
            not path_is_within(binary_path, build_dir / relative_root)
            or not binary_path.is_file()
            or not is_sha256(digest)
            or digest != sha256_file(binary_path)
        ):
            raise VerificationError(
                f"Cannot approve a baseline with stale {name} binary provenance"
            )
        binary_digests[f"{name}_sha256"] = str(digest)
    git = evidence.get("git") if isinstance(evidence.get("git"), dict) else {}
    return {
        "schema_version": BASELINE_PROVENANCE_SCHEMA_VERSION,
        "tool": "FluentQt GUI Baseline Provenance",
        "source_evidence_sha256": sha256_file(evidence_path),
        "recipe": {
            "id": recipe_record.get("id"),
            "sha256": recipe_record.get("sha256"),
            "author": {"id": author.get("id"), "kind": author.get("kind")},
        },
        "scenario": {
            "id": scenario.get("id"),
            "pre_baseline_status": scenario.get("pre_baseline_status"),
            "contract_sha256": contract_sha256,
            "artifacts": {
                "actual_sha256": sha256_file(actual),
                "capture_report_sha256": sha256_file(capture_report),
                "baseline_report_sha256": sha256_file(baseline_report),
            },
        },
        "binaries": binary_digests,
        "git": {"revision": git.get("revision"), "dirty": git.get("dirty")},
    }


def baseline_approval_source_paths(
    evidence: Mapping[str, object],
    evidence_path: Path,
    recipe_path: Path,
    scenario: Mapping[str, object],
) -> set[Path]:
    protected = {evidence_path.resolve(), recipe_path.resolve()}

    def add(value: object) -> None:
        if isinstance(value, str) and value:
            protected.add(Path(value).expanduser().resolve())

    binaries = evidence.get("binaries")
    if isinstance(binaries, dict):
        for name in ("gallery", "comparator"):
            record = binaries.get(name)
            if isinstance(record, dict):
                add(record.get("path"))
    artifacts = evidence.get("artifacts")
    if isinstance(artifacts, dict):
        for value in artifacts.values():
            add(value)
    scenario_artifacts = scenario.get("artifacts")
    if isinstance(scenario_artifacts, dict):
        for name in ("directory", "actual", "report"):
            add(scenario_artifacts.get(name))
        diffs = scenario_artifacts.get("diffs")
        for value in diffs if isinstance(diffs, list) else []:
            add(value)
    conditions = scenario.get("conditions")
    if isinstance(conditions, dict):
        add(conditions.get("actions"))
    executions = scenario.get("pixel_executions")
    for execution in executions if isinstance(executions, list) else []:
        if isinstance(execution, dict):
            add(execution.get("report"))
            add(execution.get("diff"))
    return protected


def approve_baseline(args: argparse.Namespace) -> int:
    evidence_path = args.evidence.expanduser().resolve()
    evidence = read_json(evidence_path)
    recipe, recipe_path, contexts, integrity_errors = validate_evidence_captures(
        evidence, args.scenario
    )
    if integrity_errors or recipe is None or recipe_path is None or not contexts:
        raise VerificationError(
            "Evidence capture integrity failed:\n- "
            + "\n- ".join(integrity_errors or ["scenario context is missing"])
        )
    context = contexts[0]
    scenario = context["record"]
    source_scenario = context["source"]
    assert isinstance(scenario, Mapping) and isinstance(source_scenario, Mapping)
    contract_sha256 = context.get("contract_sha256")
    recipe_id = nested(evidence, "recipe", "id")
    if (
        not isinstance(contract_sha256, str)
        or not re.fullmatch(r"[0-9a-f]{64}", contract_sha256)
        or not isinstance(recipe_id, str)
        or not recipe_id
    ):
        raise VerificationError(
            "Evidence does not contain a valid recipe/scenario contract digest"
        )
    author_id = nested(evidence, "recipe", "author", "id")
    if (
        not is_trimmed_nonempty(args.approved_by)
        or not is_trimmed_nonempty(args.approval_note)
    ):
        raise VerificationError("Baseline approver id and approval note are required")
    if args.approved_by == author_id:
        raise VerificationError("Baseline approver must differ from the evidence author")
    actual = context["actual"]
    report = context["report_path"]
    assert isinstance(actual, Path) and isinstance(report, Path)
    expected_baseline_dir = select_baseline(
        source_scenario.get("baseline"), recipe_path_base(recipe, recipe_path)
    )
    baseline_dir = (
        args.baseline_dir.expanduser().resolve()
        if args.baseline_dir
        else Path(str(scenario.get("baseline_dir"))).resolve()
    )
    if baseline_dir != expected_baseline_dir.resolve():
        raise VerificationError(
            "Baseline destination must match the source recipe scenario"
        )
    if not is_approved_baseline_bundle_path(baseline_dir):
        raise VerificationError(
            "Approved baseline destinations must be exactly "
            "tests/visual-baselines/gui/<component>/<scenario>"
        )
    overlapping_sources = sorted(
        str(path)
        for path in baseline_approval_source_paths(
            evidence, evidence_path, recipe_path, scenario
        )
        if paths_overlap(path, baseline_dir)
    )
    if overlapping_sources:
        raise VerificationError(
            "Baseline destination overlaps immutable capture evidence:\n- "
            + "\n- ".join(overlapping_sources)
        )
    if baseline_dir.exists() and not baseline_dir.is_dir():
        raise VerificationError(f"Baseline destination is not a directory: {baseline_dir}")
    if baseline_dir.exists() and any(baseline_dir.iterdir()) and not args.replace:
        raise VerificationError(
            f"Baseline bundle already exists in {baseline_dir}; use --replace to supersede it"
        )
    baseline_dir.parent.mkdir(parents=True, exist_ok=True)
    staging_dir = Path(
        tempfile.mkdtemp(
            prefix=f".{baseline_dir.name}.staging-", dir=baseline_dir.parent
        )
    )
    staged_targets = [
        staging_dir / "baseline.png",
        staging_dir / "baseline-report.json",
        staging_dir / "source-evidence.json",
        staging_dir / "baseline.json",
    ]
    try:
        shutil.copy2(actual, staged_targets[0])
        geometry_policy = merged_dict(
            nested(recipe, "defaults", "geometry"),
            source_scenario.get("geometry"),
        )
        write_json(
            staged_targets[1],
            sanitized_baseline_report(read_json(report), geometry_policy),
        )
        provenance = baseline_provenance_manifest(
            evidence,
            evidence_path,
            scenario,
            contract_sha256,
            actual,
            report,
            staged_targets[1],
        )
        write_json(staged_targets[2], provenance)
        metadata = {
            "schema_version": BASELINE_SCHEMA_VERSION,
            "status": "approved",
            "recipe_id": recipe_id,
            "scenario_id": args.scenario,
            "scenario_contract_sha256": contract_sha256,
            "approved_by": args.approved_by,
            "approver_kind": args.approver_kind,
            "approved_at": utc_now(),
            "approval_note": args.approval_note,
            "source_evidence": "source-evidence.json",
            "source_evidence_sha256": sha256_file(staged_targets[2]),
            "image_sha256": sha256_file(staged_targets[0]),
            "capture_report_sha256": sha256_file(staged_targets[1]),
        }
        write_json(staged_targets[3], metadata)
        _metadata, _report, staged_checks = baseline_bundle(
            staging_dir,
            str(recipe_id),
            str(args.scenario),
            contract_sha256,
            str(author_id),
        )
        if combined_status(staged_checks) != "pass":
            raise VerificationError("Staged baseline bundle failed its integrity check")
    except Exception:
        shutil.rmtree(staging_dir, ignore_errors=True)
        raise

    backup_dir = staging_dir.with_name(
        staging_dir.name.replace(".staging-", ".backup-", 1)
    )
    old_bundle_moved = False
    try:
        if baseline_dir.exists():
            os.replace(baseline_dir, backup_dir)
            old_bundle_moved = True
        os.replace(staging_dir, baseline_dir)
    except OSError:
        if old_bundle_moved and backup_dir.exists() and not baseline_dir.exists():
            os.replace(backup_dir, baseline_dir)
        raise
    finally:
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
    if old_bundle_moved and backup_dir.exists():
        try:
            shutil.rmtree(backup_dir)
        except OSError as error:
            print(
                f"Warning: approved bundle installed but old backup remains at "
                f"{backup_dir}: {error}",
                file=sys.stderr,
            )
    print(f"Approved baseline bundle: {baseline_dir}")
    return 0


def validate_review(
    evidence: Mapping[str, object], evidence_path: Path, review: Mapping[str, object]
) -> list[str]:
    errors: list[str] = []
    validate_fields(
        review,
        {
            "schema_version",
            "reviewer",
            "evidence_sha256",
            "verdict",
            "summary",
            "reviewed_scenarios",
            "attestation",
            "findings",
        },
        "review",
        errors,
        {
            "schema_version",
            "reviewer",
            "evidence_sha256",
            "verdict",
            "summary",
            "reviewed_scenarios",
            "attestation",
            "findings",
        },
    )
    if review.get("schema_version") != REVIEW_SCHEMA_VERSION:
        errors.append("review schema_version must be 1")
    reviewer = review.get("reviewer")
    reviewer = reviewer if isinstance(reviewer, dict) else {}
    validate_fields(
        reviewer,
        {"id", "kind", "tool", "model"},
        "reviewer",
        errors,
        {"id", "kind"},
    )
    reviewer_id = reviewer.get("id")
    if not is_trimmed_nonempty(reviewer_id):
        errors.append("reviewer.id is required")
    if reviewer.get("kind") not in {"ai", "human"}:
        errors.append("reviewer.kind must be ai or human")
    author_id = nested(evidence, "recipe", "author", "id")
    if reviewer_id == author_id:
        errors.append("reviewer must differ from the evidence author")
    if review.get("evidence_sha256") != sha256_file(evidence_path):
        errors.append("review evidence_sha256 does not match the evidence file")
    if review.get("verdict") not in {"pass", "fail"}:
        errors.append("review verdict must be pass or fail")
    if not is_trimmed_nonempty(review.get("summary")):
        errors.append("review summary is required")
    required = {
        str(item.get("id"))
        for item in evidence.get("scenarios", [])
        if isinstance(item, dict)
    }
    reviewed = review.get("reviewed_scenarios")
    reviewed_set = set(reviewed) if isinstance(reviewed, list) else set()
    if (
        not isinstance(reviewed, list)
        or not reviewed
        or not all(is_trimmed_nonempty(item) for item in reviewed)
    ):
        errors.append("reviewed_scenarios must be a non-empty string array")
    elif len(reviewed_set) != len(reviewed):
        errors.append("reviewed_scenarios must not contain duplicates")
    if reviewed_set != required:
        missing = sorted(required - reviewed_set)
        unknown = sorted(reviewed_set - required)
        if missing:
            errors.append("review omitted scenarios: " + ", ".join(missing))
        if unknown:
            errors.append("review contains unknown scenarios: " + ", ".join(unknown))
    attestation = review.get("attestation")
    attestation = attestation if isinstance(attestation, dict) else {}
    validate_fields(
        attestation,
        {
            "independent",
            "visual_artifacts_opened",
            "interaction_evidence_reviewed",
        },
        "attestation",
        errors,
        {
            "independent",
            "visual_artifacts_opened",
            "interaction_evidence_reviewed",
        },
    )
    if attestation.get("independent") is not True:
        errors.append("review must attest independence")
    if attestation.get("visual_artifacts_opened") is not True:
        errors.append("review must attest that visual artifacts were opened")
    interactions_required = any(
        isinstance(item, dict) and nested(item, "conditions", "actions") is not None
        for item in evidence.get("scenarios", [])
    )
    if interactions_required and attestation.get("interaction_evidence_reviewed") is not True:
        errors.append("review must cover interaction evidence")
    if not isinstance(attestation.get("interaction_evidence_reviewed"), bool):
        errors.append("interaction_evidence_reviewed must be a boolean")
    findings = review.get("findings")
    if not isinstance(findings, list):
        errors.append("review findings must be an array")
        findings = []
    blocking = [
        item
        for item in findings
        if isinstance(item, dict) and item.get("severity") in {"blocker", "major"}
    ]
    if review.get("verdict") == "pass" and blocking:
        errors.append("pass verdict cannot include blocker or major findings")
    for index, item in enumerate(findings):
        if not isinstance(item, dict):
            errors.append(f"findings[{index}] must be an object")
            continue
        validate_fields(
            item,
            {"severity", "scenario_id", "region", "artifact", "message"},
            f"findings[{index}]",
            errors,
            {"severity", "scenario_id", "region", "artifact", "message"},
        )
        if item.get("severity") not in {"blocker", "major", "minor", "note"}:
            errors.append(f"findings[{index}].severity is invalid")
        if item.get("scenario_id") not in required:
            errors.append(f"findings[{index}].scenario_id is not in the evidence")
        for key in ("severity", "scenario_id", "region", "artifact", "message"):
            if not is_trimmed_nonempty(item.get(key)):
                errors.append(f"findings[{index}].{key} is required")
    return errors


def final_evidence_input_paths(evidence: Mapping[str, object]) -> set[Path]:
    protected: set[Path] = set()

    def add(value: object) -> None:
        if isinstance(value, str) and value:
            protected.add(Path(value).expanduser().resolve())

    recipe = evidence.get("recipe")
    if isinstance(recipe, dict):
        add(recipe.get("path"))
    binaries = evidence.get("binaries")
    if isinstance(binaries, dict):
        for name in ("gallery", "comparator"):
            record = binaries.get(name)
            if isinstance(record, dict):
                add(record.get("path"))
    artifacts = evidence.get("artifacts")
    if isinstance(artifacts, dict):
        for value in artifacts.values():
            add(value)
    scenarios = evidence.get("scenarios")
    for scenario in scenarios if isinstance(scenarios, list) else []:
        if not isinstance(scenario, dict):
            continue
        baseline_value = scenario.get("baseline_dir")
        if isinstance(baseline_value, str) and baseline_value:
            baseline_dir = Path(baseline_value).expanduser().resolve()
            protected.update(
                baseline_dir / name
                for name in (
                    "baseline.png",
                    "baseline-report.json",
                    "source-evidence.json",
                    "baseline.json",
                )
            )
        scenario_artifacts = scenario.get("artifacts")
        if isinstance(scenario_artifacts, dict):
            for value in scenario_artifacts.values():
                if isinstance(value, list):
                    for item in value:
                        add(item)
                else:
                    add(value)
        conditions = scenario.get("conditions")
        if isinstance(conditions, dict):
            add(conditions.get("actions"))
        executions = scenario.get("pixel_executions")
        for execution in executions if isinstance(executions, list) else []:
            if isinstance(execution, dict):
                add(execution.get("report"))
                add(execution.get("diff"))
    return protected


def finalize_review(args: argparse.Namespace) -> int:
    evidence_path = args.evidence.expanduser().resolve()
    review_path = args.review.expanduser().resolve()
    output = (
        args.output.expanduser().resolve()
        if args.output
        else evidence_path.with_name("verification.json")
    )
    if output in {evidence_path, review_path}:
        raise VerificationError(
            "Verification output must not overwrite the evidence or review input"
        )
    evidence = read_json(evidence_path)
    if path_is_within(output, approved_baseline_root()):
        raise VerificationError(
            "Verification output must not be written inside the approved baseline root"
        )
    if output in final_evidence_input_paths(evidence):
        raise VerificationError(
            "Verification output must not overwrite an evidence-referenced input"
        )
    review = read_json(review_path)
    integrity_errors = validate_final_evidence(evidence)
    review_errors = validate_review(evidence, evidence_path, review)
    errors = [
        *(f"evidence integrity: {error}" for error in integrity_errors),
        *review_errors,
    ]
    deterministic_status = str(evidence.get("deterministic_status", "incomplete"))
    if integrity_errors:
        status = "incomplete"
    elif deterministic_status != "pass":
        status = deterministic_status
    elif errors:
        status = "review-required"
    elif review.get("verdict") == "fail":
        status = "fail"
    else:
        status = "pass"
    result = {
        "schema_version": 1,
        "tool": "FluentQt GUI Verification Decision",
        "status": status,
        "evidence": str(evidence_path),
        "evidence_sha256": sha256_file(evidence_path),
        "review": str(review_path),
        "review_sha256": sha256_file(review_path),
        "deterministic_status": deterministic_status,
        "review_verdict": review.get("verdict"),
        "validation_errors": errors,
        "decided_at": utc_now(),
    }
    write_json(output, result)
    print(f"GUI verification decision {status}: {output}")
    return 0 if status == "pass" else 1


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="Capture and evaluate a GUI recipe.")
    run.add_argument("--recipe", type=Path, required=True)
    run.add_argument("--output-dir", type=Path)
    run.add_argument("--replace-output", action="store_true")
    run.add_argument("--preset", default=default_preset())
    run.add_argument("--build-dir", type=Path)
    run.add_argument("--gallery", type=Path)
    run.add_argument("--comparator", type=Path)
    run.add_argument("--no-build", action="store_true")

    approve = subparsers.add_parser(
        "approve", help="Create an immutable-by-default approved baseline bundle."
    )
    approve.add_argument("--evidence", type=Path, required=True)
    approve.add_argument("--scenario", required=True)
    approve.add_argument("--baseline-dir", type=Path)
    approve.add_argument("--approved-by", required=True)
    approve.add_argument("--approver-kind", choices=("ai", "human"), required=True)
    approve.add_argument("--approval-note", required=True)
    approve.add_argument("--replace", action="store_true")

    finalize = subparsers.add_parser(
        "finalize", help="Validate an independent review against immutable evidence."
    )
    finalize.add_argument("--evidence", type=Path, required=True)
    finalize.add_argument("--review", type=Path, required=True)
    finalize.add_argument("--output", type=Path)
    return parser.parse_args(list(argv) if argv is not None else None)


def main(argv: Iterable[str] | None = None) -> int:
    try:
        args = parse_args(argv)
        if args.command == "run":
            return run_recipe(args)
        if args.command == "approve":
            return approve_baseline(args)
        if args.command == "finalize":
            return finalize_review(args)
        raise VerificationError(f"Unknown command: {args.command}")
    except VerificationError as error:
        print(f"fluent_qt_gui_verify: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
