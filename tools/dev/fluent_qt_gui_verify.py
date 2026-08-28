#!/usr/bin/env python3

"""Run evidence-first FluentQt Gallery GUI verification recipes.

The tool deliberately separates deterministic capture gates from visual review.
A run can prove that pixels, geometry, interactions, Inspector findings, and the
capture environment satisfy an approved contract.  It cannot self-approve a
new baseline or its own visual judgment.
"""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
from html import escape
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
from typing import Any, Iterable, Mapping, Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOL = Path(__file__).with_name("fluent_qt_build.py")
TOOL_SCHEMA_VERSION = 1
RECIPE_SCHEMA_VERSION = 1
BASELINE_SCHEMA_VERSION = 1
REVIEW_SCHEMA_VERSION = 1
SAFE_ID = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
HEADLESS_PLUGINS = {"offscreen", "minimal", "minimalegl", "vnc"}
STATUS_PRIORITY = {
    "pass": 0,
    "not-applicable": 0,
    "human-required": 1,
    "review-required": 1,
    "incomplete": 2,
    "fail": 3,
}


class VerificationError(RuntimeError):
    """Raised for invalid inputs or unusable verification infrastructure."""


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_json(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise VerificationError(f"Could not read JSON {path}: {error}") from error
    except json.JSONDecodeError as error:
        raise VerificationError(f"Could not parse JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise VerificationError(f"JSON root must be an object: {path}")
    return value


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def resolved_path(raw: str | os.PathLike[str], base: Path) -> Path:
    path = Path(raw).expanduser()
    return (base / path).resolve() if not path.is_absolute() else path.resolve()


def recipe_path_base(recipe: Mapping[str, object], recipe_path: Path) -> Path:
    """Return the explicitly declared base for relative recipe paths."""

    path_base = recipe.get("path_base")
    if path_base == "repository":
        return PROJECT_ROOT
    if path_base == "recipe":
        return recipe_path.parent.resolve()
    raise VerificationError("path_base must be repository or recipe")


def merged_dict(*values: object) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for value in values:
        if isinstance(value, dict):
            result.update(value)
    return result


def nested(value: object, *keys: str) -> object:
    current = value
    for key in keys:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def check(
    check_id: str,
    status: str,
    message: str,
    details: object | None = None,
) -> dict[str, object]:
    result: dict[str, object] = {
        "id": check_id,
        "status": status,
        "message": message,
    }
    if details is not None:
        result["details"] = details
    return result


def combined_status(checks: Sequence[Mapping[str, object]]) -> str:
    status = "pass"
    for item in checks:
        candidate = str(item.get("status", "incomplete"))
        if STATUS_PRIORITY.get(candidate, STATUS_PRIORITY["incomplete"]) > STATUS_PRIORITY[
            status
        ]:
            status = candidate
    return status


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


def validate_size(value: object, context: str, errors: list[str]) -> None:
    if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*x[1-9][0-9]*", value):
        errors.append(f"{context} must be WIDTHxHEIGHT")
        return
    width, height = (int(piece) for piece in value.split("x", 1))
    if not 320 <= width <= 3840 or not 240 <= height <= 2160:
        errors.append(f"{context} must be within 320x240 and 3840x2160")


def validate_geometry_policy(
    value: object, context: str, errors: list[str], require_probes: bool
) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    required = value.get("required")
    if require_probes and (not isinstance(required, list) or not required):
        errors.append(f"{context}.required must be a non-empty array")
    if required is not None:
        if not isinstance(required, list):
            errors.append(f"{context}.required must be an array")
        else:
            for index, raw in enumerate(required):
                if isinstance(raw, str) and raw:
                    continue
                if not isinstance(raw, dict) or not isinstance(raw.get("object_name"), str) or not raw.get("object_name"):
                    errors.append(
                        f"{context}.required[{index}] must name one object_name"
                    )
                    continue
                rect = raw.get("rect")
                if rect is not None and (
                    not isinstance(rect, dict)
                    or set(rect) != {"x", "y", "width", "height"}
                    or not all(isinstance(rect.get(key), int) for key in rect)
                    or int(rect.get("width", 0)) <= 0
                    or int(rect.get("height", 0)) <= 0
                ):
                    errors.append(
                        f"{context}.required[{index}].rect must contain integer x, y, width, and height"
                    )
    tolerance = value.get("tolerance")
    if tolerance is not None and (not isinstance(tolerance, int) or tolerance < 0):
        errors.append(f"{context}.tolerance must be a non-negative integer")


def validate_pixel_policy(value: object, context: str, errors: list[str]) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    integer_ranges = {
        "channel_threshold": (0, 255),
        "max_different_pixels": (0, None),
        "search_radius": (0, 32),
        "max_translation": (0, 32),
        "edge_threshold": (1, 255),
    }
    for name, (minimum, maximum) in integer_ranges.items():
        if name not in value:
            continue
        candidate = value[name]
        if (
            not isinstance(candidate, int)
            or candidate < minimum
            or (maximum is not None and candidate > maximum)
        ):
            errors.append(f"{context}.{name} is outside its valid range")
    ratio = value.get("max_different_ratio")
    if ratio is not None and (
        not isinstance(ratio, (int, float)) or not 0 <= ratio <= 1
    ):
        errors.append(f"{context}.max_different_ratio must be from 0 to 1")
    regions = value.get("regions")
    if regions is None:
        return
    if not isinstance(regions, list):
        errors.append(f"{context}.regions must be an array")
        return
    region_ids: set[str] = set()
    for index, raw in enumerate(regions):
        if not isinstance(raw, dict):
            errors.append(f"{context}.regions[{index}] must be an object")
            continue
        region_id = raw.get("id")
        if not isinstance(region_id, str) or not SAFE_ID.fullmatch(region_id):
            errors.append(f"{context}.regions[{index}].id is invalid")
        elif region_id in region_ids:
            errors.append(f"{context}.regions[{index}].id is duplicated")
        else:
            region_ids.add(region_id)
        rect = raw.get("rect")
        if (
            not isinstance(rect, list)
            or len(rect) != 4
            or not all(isinstance(item, int) for item in rect)
            or (isinstance(rect, list) and len(rect) == 4 and (rect[0] < 0 or rect[1] < 0 or rect[2] <= 0 or rect[3] <= 0))
        ):
            errors.append(
                f"{context}.regions[{index}].rect must be non-negative x,y and positive width,height"
            )
        if raw.get("coordinate_space", "logical") not in {"logical", "device"}:
            errors.append(
                f"{context}.regions[{index}].coordinate_space must be logical or device"
            )
        if "policy" in raw:
            validate_pixel_policy(
                raw["policy"], f"{context}.regions[{index}].policy", errors
            )


def validate_recipe(recipe: Mapping[str, object]) -> list[str]:
    errors: list[str] = []
    if recipe.get("schema_version") != RECIPE_SCHEMA_VERSION:
        errors.append("schema_version must be 1")
    recipe_id = recipe.get("id")
    if not isinstance(recipe_id, str) or not SAFE_ID.fullmatch(recipe_id):
        errors.append("id must use lowercase letters, numbers, dot, underscore, or dash")
    if recipe.get("path_base") not in {"repository", "recipe"}:
        errors.append("path_base must be repository or recipe")
    author = recipe.get("author")
    if not isinstance(author, dict) or not isinstance(author.get("id"), str) or not author.get("id"):
        errors.append("author.id is required")
    elif author.get("kind") not in {"ai", "human"}:
        errors.append("author.kind must be ai or human")
    selection = recipe.get("selection")
    if not isinstance(selection, dict) or not isinstance(selection.get("route"), str) or not selection.get("route"):
        errors.append("selection.route is required")

    coverage = recipe.get("coverage")
    required_tags = coverage.get("required_tags") if isinstance(coverage, dict) else None
    if not isinstance(required_tags, list) or not required_tags or not all(
        isinstance(tag, str) and tag for tag in required_tags
    ):
        errors.append("coverage.required_tags must be a non-empty string array")

    defaults = recipe.get("defaults")
    defaults = defaults if isinstance(defaults, dict) else {}
    if not isinstance(defaults.get("inspector"), dict):
        errors.append("defaults.inspector is required")
    validate_geometry_policy(
        defaults.get("geometry"), "defaults.geometry", errors, True
    )
    validate_pixel_policy(defaults.get("pixel"), "defaults.pixel", errors)

    scenarios = recipe.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        errors.append("scenarios must be a non-empty array")
        return errors
    seen: set[str] = set()
    covered: set[str] = set()
    for index, raw in enumerate(scenarios):
        context = f"scenarios[{index}]"
        if not isinstance(raw, dict):
            errors.append(f"{context} must be an object")
            continue
        scenario_id = raw.get("id")
        if not isinstance(scenario_id, str) or not SAFE_ID.fullmatch(scenario_id):
            errors.append(f"{context}.id is invalid")
        elif scenario_id in seen:
            errors.append(f"{context}.id is duplicated: {scenario_id}")
        else:
            seen.add(scenario_id)
        if raw.get("theme") not in {"light", "dark"}:
            errors.append(f"{context}.theme must be light or dark")
        if raw.get("direction", "ltr") not in {"ltr", "rtl"}:
            errors.append(f"{context}.direction must be ltr or rtl")
        validate_size(raw.get("size"), f"{context}.size", errors)
        tags = raw.get("tags")
        if not isinstance(tags, list) or not tags or not all(
            isinstance(tag, str) and tag for tag in tags
        ):
            errors.append(f"{context}.tags must be a non-empty string array")
        else:
            covered.update(tags)
        baseline = raw.get("baseline")
        if not isinstance(baseline, (str, dict)):
            errors.append(f"{context}.baseline must be a path or platform map")
        elif isinstance(baseline, str) and not baseline.strip():
            errors.append(f"{context}.baseline must not be empty")
        elif isinstance(baseline, dict) and (
            not baseline
            or not all(
                isinstance(key, str)
                and key
                and isinstance(path, str)
                and path
                for key, path in baseline.items()
            )
        ):
            errors.append(
                f"{context}.baseline platform map must contain non-empty paths"
            )
        review = raw.get("review")
        if not isinstance(review, list) or not review or not all(
            isinstance(item, str) and item for item in review
        ):
            errors.append(f"{context}.review must contain visual review prompts")
        actions = raw.get("actions")
        if actions is not None and not isinstance(actions, (str, dict)):
            errors.append(f"{context}.actions must be a path or JSON object")
        elif isinstance(actions, dict) and (
            actions.get("schema_version") != 1
            or not isinstance(actions.get("steps"), list)
        ):
            errors.append(
                f"{context}.actions must use schema_version 1 with a steps array"
            )
        if "geometry" in raw:
            validate_geometry_policy(raw.get("geometry"), f"{context}.geometry", errors, False)
        if "pixel" in raw:
            validate_pixel_policy(raw.get("pixel"), f"{context}.pixel", errors)
    if isinstance(required_tags, list):
        missing = sorted(set(required_tags) - covered)
        if missing:
            errors.append("coverage is missing required tags: " + ", ".join(missing))
    return errors


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
    completed = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    result = command_record(command, completed)
    result.update({"requested": True, "status": "pass" if completed.returncode == 0 else "fail"})
    return result


def git_state() -> dict[str, object]:
    def run(*arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", *arguments],
            cwd=PROJECT_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    revision = run("rev-parse", "HEAD")
    status = run("status", "--porcelain")
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "dirty": bool(status.stdout.strip()) if status.returncode == 0 else None,
    }


def host_key() -> tuple[str, str]:
    system = platform.system().lower()
    machine = platform.machine().lower().replace("aarch64", "arm64").replace("x86_64", "x64")
    return system, f"{system}-{machine}"


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


def prepare_action_script(raw: object, path_base: Path, scenario_dir: Path) -> Path | None:
    if raw is None:
        return None
    if isinstance(raw, str):
        path = resolved_path(raw, path_base)
        if not path.is_file():
            raise VerificationError(f"Action script does not exist: {path}")
        return path
    if not isinstance(raw, dict):
        raise VerificationError("actions must be a path or JSON object")
    path = scenario_dir / "actions.json"
    write_json(path, raw)
    return path


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
        "schema_version": 1,
        "recipe_id": recipe.get("id"),
        "scenario_id": scenario.get("id"),
        "selection": merged_dict(recipe.get("selection"), scenario.get("selection")),
        "scene": {
            "theme": scenario.get("theme"),
            "direction": scenario.get("direction", "ltr"),
            "size": scenario.get("size"),
            "settle_ms": scenario.get("settle_ms", defaults.get("settle_ms", 250)),
            "require_native_desktop": scenario.get(
                "require_native_desktop",
                defaults.get("require_native_desktop", True),
            ),
        },
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
    command = [
        str(gallery),
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
        command.extend(["--actions", str(action_path)])
    return command


def identity_checks(
    recipe: Mapping[str, object],
    scenario: Mapping[str, object],
    report: Mapping[str, object],
    action_path: Path | None,
    actual_path: Path,
) -> list[dict[str, object]]:
    checks: list[dict[str, object]] = []
    schema_matches = report.get("schema_version") == 2
    checks.append(
        check(
            "capture.schema",
            "pass" if schema_matches else "incomplete",
            "Capture report schema is supported."
            if schema_matches
            else "Capture report schema is missing or unsupported.",
            {"expected": 2, "actual": report.get("schema_version")},
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
    scene_matches = (
        scene.get("theme") == scenario.get("theme")
        and scene.get("layout_direction") == direction
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
                    "width": width,
                    "height": height,
                },
                "actual": scene,
            },
        )
    )
    snapshot_written = (
        actual_path.is_file()
        and nested(report, "artifacts", "snapshot", "written") is True
    )
    checks.append(
        check(
            "capture.snapshot",
            "pass" if snapshot_written else "incomplete",
            "Native-resolution snapshot was written."
            if snapshot_written
            else "Snapshot is missing or the capture report did not confirm it.",
        )
    )
    interaction = report.get("interaction_report")
    interaction = interaction if isinstance(interaction, dict) else {}
    if action_path:
        action_passed = interaction.get("requested") is True and interaction.get("status") == "pass"
    else:
        action_passed = interaction.get("requested") is False and interaction.get("status") == "not-requested"
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
    if not isinstance(quality, dict) or quality.get("schema_version") != 1:
        return check("inspector", "incomplete", "Inspector report is missing or unsupported.")
    findings = quality.get("findings")
    if not isinstance(findings, list):
        return check("inspector", "incomplete", "Inspector findings are missing.")
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


def widget_index(report: Mapping[str, object]) -> dict[str, list[dict[str, object]]]:
    geometry = report.get("geometry_report")
    widgets = geometry.get("widgets") if isinstance(geometry, dict) else None
    index: dict[str, list[dict[str, object]]] = {}
    if not isinstance(widgets, list):
        return index
    for item in widgets:
        if not isinstance(item, dict):
            continue
        name = item.get("object_name")
        if isinstance(name, str) and name:
            index.setdefault(name, []).append(item)
    return index


def normalized_geometry_entry(raw: object, default_tolerance: int) -> dict[str, object]:
    if isinstance(raw, str):
        return {"object_name": raw, "tolerance": default_tolerance, "not_clipped": True}
    if not isinstance(raw, dict):
        raise VerificationError("geometry.required entries must be strings or objects")
    result = dict(raw)
    result.setdefault("tolerance", default_tolerance)
    result.setdefault("not_clipped", True)
    return result


def rect_deltas(actual: Mapping[str, object], expected: Mapping[str, object]) -> dict[str, int]:
    return {
        key: int(actual.get(key, 0)) - int(expected.get(key, 0))
        for key in ("x", "y", "width", "height")
    }


def geometry_contract_check(report: Mapping[str, object], policy: Mapping[str, object]) -> dict[str, object]:
    index = widget_index(report)
    required = policy.get("required")
    if not isinstance(required, list) or not required:
        return check("geometry.contract", "incomplete", "No required geometry probes were declared.")
    default_tolerance = int(policy.get("tolerance", 0))
    probes: list[dict[str, object]] = []
    failures: list[str] = []
    for raw in required:
        entry = normalized_geometry_entry(raw, default_tolerance)
        name = entry.get("object_name")
        if not isinstance(name, str) or not name:
            failures.append("geometry probe has no object_name")
            continue
        matches = index.get(name, [])
        probe: dict[str, object] = {"object_name": name, "matches": len(matches)}
        if len(matches) != 1:
            failures.append(f"{name} has {len(matches)} matches")
            probes.append(probe)
            continue
        widget = matches[0]
        probe["actual"] = widget
        if entry.get("not_clipped", True) and widget.get("clipped") is True:
            failures.append(f"{name} is clipped")
        rect = widget.get("rect") if isinstance(widget.get("rect"), dict) else {}
        for key, comparison in (("min_width", ">="), ("min_height", ">="), ("max_width", "<="), ("max_height", "<=")):
            if key not in entry:
                continue
            dimension = "width" if "width" in key else "height"
            actual = int(rect.get(dimension, 0))
            expected = int(entry[key])
            bad = actual < expected if comparison == ">=" else actual > expected
            if bad:
                failures.append(f"{name}.{dimension} {actual} violates {key}={expected}")
        expected_rect = entry.get("rect")
        if isinstance(expected_rect, dict):
            tolerance = int(entry.get("tolerance", default_tolerance))
            deltas = rect_deltas(rect, expected_rect)
            probe["explicit_rect_delta"] = deltas
            if any(abs(value) > tolerance for value in deltas.values()):
                failures.append(f"{name} differs from its explicit rect by more than {tolerance}px")
        probes.append(probe)
    return check(
        "geometry.contract",
        "fail" if failures else "pass",
        "; ".join(failures) if failures else "All named geometry probes are unique, visible, and within contract.",
        {"probes": probes},
    )


def geometry_baseline_check(
    actual_report: Mapping[str, object],
    baseline_report: Mapping[str, object] | None,
    policy: Mapping[str, object],
) -> dict[str, object]:
    if baseline_report is None:
        return check(
            "geometry.baseline",
            "human-required",
            "Approved baseline geometry is not available.",
        )
    actual_index = widget_index(actual_report)
    baseline_index = widget_index(baseline_report)
    required = policy.get("required") if isinstance(policy.get("required"), list) else []
    default_tolerance = int(policy.get("tolerance", 0))
    comparisons: list[dict[str, object]] = []
    failures: list[str] = []
    for raw in required:
        entry = normalized_geometry_entry(raw, default_tolerance)
        name = str(entry.get("object_name", ""))
        actual = actual_index.get(name, [])
        baseline = baseline_index.get(name, [])
        if len(actual) != 1 or len(baseline) != 1:
            failures.append(
                f"{name} must be unique in actual and baseline ({len(actual)}/{len(baseline)})"
            )
            continue
        actual_rect = actual[0].get("rect") if isinstance(actual[0].get("rect"), dict) else {}
        baseline_rect = baseline[0].get("rect") if isinstance(baseline[0].get("rect"), dict) else {}
        deltas = rect_deltas(actual_rect, baseline_rect)
        tolerance = int(entry.get("tolerance", default_tolerance))
        comparisons.append(
            {
                "object_name": name,
                "actual": actual_rect,
                "baseline": baseline_rect,
                "delta": deltas,
                "tolerance": tolerance,
            }
        )
        if any(abs(value) > tolerance for value in deltas.values()):
            failures.append(f"{name} moved or resized by more than {tolerance}px")
    return check(
        "geometry.baseline",
        "fail" if failures else "pass",
        "; ".join(failures) if failures else "Named geometry matches the approved baseline.",
        {"comparisons": comparisons},
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
    metadata_path = baseline_dir / "baseline.json"
    paths = {"image": str(image), "report": str(report_path), "metadata": str(metadata_path)}
    if not image.is_file() or not report_path.is_file() or not metadata_path.is_file():
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
    except VerificationError as error:
        return None, None, [check("baseline.approval", "fail", str(error), paths)]
    approval_valid = (
        metadata.get("schema_version") == BASELINE_SCHEMA_VERSION
        and metadata.get("status") == "approved"
        and metadata.get("recipe_id") == recipe_id
        and metadata.get("scenario_id") == scenario_id
        and metadata.get("scenario_contract_sha256")
        == scenario_contract_sha256
        and isinstance(metadata.get("approved_by"), str)
        and bool(metadata.get("approved_by"))
        and metadata.get("approved_by") != author_id
        and isinstance(metadata.get("approved_at"), str)
        and bool(metadata.get("approval_note"))
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
    digest_valid = (
        metadata.get("image_sha256") == actual_image_sha
        and metadata.get("capture_report_sha256") == actual_report_sha
    )
    return metadata, report, [
        check(
            "baseline.approval",
            "pass" if digest_valid else "fail",
            "Baseline approval and content digests are valid."
            if digest_valid
            else "Baseline content changed after approval.",
            {
                **paths,
                "approved_by": metadata.get("approved_by"),
                "expected_image_sha256": metadata.get("image_sha256"),
                "actual_image_sha256": actual_image_sha,
                "expected_report_sha256": metadata.get("capture_report_sha256"),
                "actual_report_sha256": actual_report_sha,
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
    actual = actual_report.get("environment")
    expected = baseline_report.get("environment")
    matches = isinstance(actual, dict) and isinstance(expected, dict) and canonical_json(actual) == canonical_json(expected)
    return check(
        "environment.fingerprint",
        "pass" if matches else "human-required",
        "Capture environment exactly matches the approved baseline."
        if matches
        else "Capture environment differs; pixel evidence cannot be reused safely.",
        {"expected": expected, "actual": actual},
    )


def native_desktop_check(report: Mapping[str, object], required: bool) -> dict[str, object]:
    plugin = nested(report, "environment", "platform_plugin")
    is_native = isinstance(plugin, str) and plugin.lower() not in HEADLESS_PLUGINS
    if not required:
        return check(
            "environment.native-desktop",
            "not-applicable",
            "Recipe explicitly allows a headless platform plugin.",
            {"platform_plugin": plugin},
        )
    return check(
        "environment.native-desktop",
        "pass" if is_native else "incomplete",
        "Capture used a native desktop platform plugin."
        if is_native
        else "Final evidence requires a native desktop platform plugin.",
        {"platform_plugin": plugin},
    )


def comparator_policy_arguments(policy: Mapping[str, object]) -> list[str]:
    mapping = (
        ("channel_threshold", "--channel-threshold"),
        ("max_different_pixels", "--max-different-pixels"),
        ("max_different_ratio", "--max-different-ratio"),
        ("search_radius", "--search-radius"),
        ("max_translation", "--max-translation"),
        ("edge_threshold", "--edge-threshold"),
    )
    arguments: list[str] = []
    for key, flag in mapping:
        if key in policy:
            arguments.extend([flag, str(policy[key])])
    return arguments


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
    comparisons: list[tuple[str, Mapping[str, object], list[int] | None]] = [
        ("full", policy, None)
    ]
    regions = policy.get("regions")
    if isinstance(regions, list):
        for index, raw in enumerate(regions):
            if not isinstance(raw, dict):
                continue
            region_id = str(raw.get("id", f"region-{index + 1}"))
            if not SAFE_ID.fullmatch(region_id):
                region_id = f"region-{index + 1}"
            rect = raw.get("rect")
            if not isinstance(rect, list) or len(rect) != 4:
                continue
            physical_rect = [int(value) for value in rect]
            if raw.get("coordinate_space", "logical") == "logical":
                physical_rect = [
                    round(value * device_pixel_ratio) for value in physical_rect
                ]
            comparisons.append(
                (region_id, merged_dict(policy, raw.get("policy")), physical_rect)
            )

    checks: list[dict[str, object]] = []
    executions: list[dict[str, object]] = []
    for comparison_id, comparison_policy, region in comparisons:
        report_path = scenario_dir / f"pixel-{comparison_id}.json"
        diff_path = scenario_dir / f"diff-{comparison_id}.png"
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
        completed = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            text=True,
            capture_output=True,
            check=False,
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
        if completed.returncode == 0 and comparison_report is not None:
            status = "pass"
            message = "Pixels match the approved policy."
        elif completed.returncode == 1 and comparison_report is not None:
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
                {"region": region, "report": comparison_report, "execution": execution},
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
    required_native = bool(scenario.get("require_native_desktop", defaults.get("require_native_desktop", True)))
    return [
        *identity_checks(recipe, scenario, report, action_path, actual_path),
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
    action_path = prepare_action_script(scenario.get("actions"), path_base, scenario_dir)
    contract = scenario_contract(recipe, scenario, action_path)
    contract_sha256 = sha256_bytes(canonical_json(contract))
    actual_path = scenario_dir / "actual.png"
    report_path = scenario_dir / "capture.json"
    command = capture_command(gallery, recipe, scenario, scenario_dir, action_path)
    environment, environment_overrides = relevant_environment(recipe, scenario)
    defaults = recipe.get("defaults") if isinstance(recipe.get("defaults"), dict) else {}
    timeout = int(scenario.get("timeout_seconds", defaults.get("timeout_seconds", 45)))
    try:
        completed = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
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
    if baseline_approval and baseline_approval["status"] == "pass" and fingerprint and fingerprint["status"] == "pass":
        pixel_checks, pixel_executions = run_pixel_comparison(
            comparator,
            baseline_dir,
            actual_path,
            scenario_dir,
            pixel_policy,
            float(nested(report, "environment", "device_pixel_ratio") or 1.0),
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


def html_uri(path: object) -> str:
    if not isinstance(path, str) or not path:
        return ""
    candidate = Path(path)
    return candidate.resolve().as_uri() if candidate.exists() else ""


def review_html(evidence: Mapping[str, object]) -> str:
    cards: list[str] = []
    scenarios = evidence.get("scenarios") if isinstance(evidence.get("scenarios"), list) else []
    for raw in scenarios:
        if not isinstance(raw, dict):
            continue
        artifacts = raw.get("artifacts") if isinstance(raw.get("artifacts"), dict) else {}
        images: list[str] = []
        for label, key in (("Actual", "actual"), ("Approved baseline", "baseline")):
            uri = html_uri(artifacts.get(key))
            if uri:
                images.append(
                    f'<figure><figcaption>{escape(label)}</figcaption><img src="{escape(uri)}" alt="{escape(label)}"></figure>'
                )
        diffs = artifacts.get("diffs") if isinstance(artifacts.get("diffs"), list) else []
        for index, diff in enumerate(diffs):
            uri = html_uri(diff)
            if uri:
                images.append(
                    f'<figure><figcaption>Diff {index + 1}</figcaption><img src="{escape(uri)}" alt="Diff"></figure>'
                )
        failed_checks = [
            item
            for item in raw.get("checks", [])
            if isinstance(item, dict) and item.get("status") not in {"pass", "not-applicable"}
        ]
        check_items = "".join(
            f'<li><code>{escape(str(item.get("id")))}</code> — {escape(str(item.get("status")))}: {escape(str(item.get("message")))}</li>'
            for item in failed_checks
        ) or "<li>All deterministic checks passed.</li>"
        prompts = "".join(f"<li>{escape(str(prompt))}</li>" for prompt in raw.get("review", []))
        cards.append(
            f'''<section>
<h2>{escape(str(raw.get("id")))} <span class="status {escape(str(raw.get("status")))}">{escape(str(raw.get("status")))}</span></h2>
<div class="images">{"".join(images)}</div>
<h3>Deterministic gates</h3><ul>{check_items}</ul>
<h3>Independent review prompts</h3><ul>{prompts}</ul>
</section>'''
        )
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FluentQt GUI verification</title>
<style>
body{{font:14px system-ui;margin:24px;background:#111318;color:#f4f6fa}}h1{{font-size:22px}}section{{margin:20px 0;padding:16px;border:1px solid #3a3f49;border-radius:10px;background:#191c22}}h2{{margin-top:0}}h3{{font-size:14px}}.images{{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px}}figure{{margin:0;background:#fff;border-radius:6px;overflow:hidden}}figcaption{{padding:8px;background:#2a2f38;color:#fff}}img{{display:block;width:100%;height:auto;image-rendering:auto}}.status{{font-size:12px;padding:3px 7px;border-radius:999px;background:#404754}}.pass{{background:#176b42}}.fail{{background:#a73333}}.human-required,.review-required,.incomplete{{background:#8a641d}}code{{color:#9fd2ff}}li{{margin:5px 0}}
</style></head><body><h1>FluentQt GUI verification</h1><p>Deterministic status: <strong>{escape(str(evidence.get("deterministic_status")))}</strong>. Final visual acceptance requires a separate reviewer whose identity differs from the author.</p>{"".join(cards)}</body></html>'''


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
            "summary": {"total": 0, "by_status": {"incomplete": 1}},
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


def approve_baseline(args: argparse.Namespace) -> int:
    evidence_path = args.evidence.expanduser().resolve()
    evidence = read_json(evidence_path)
    scenarios = evidence.get("scenarios")
    scenarios = scenarios if isinstance(scenarios, list) else []
    scenario = next(
        (item for item in scenarios if isinstance(item, dict) and item.get("id") == args.scenario),
        None,
    )
    if scenario is None:
        raise VerificationError(f"Scenario is not present in evidence: {args.scenario}")
    if scenario.get("pre_baseline_status") != "pass":
        raise VerificationError(
            f"Scenario pre-baseline gates are {scenario.get('pre_baseline_status')}; refusing approval"
        )
    contract_sha256 = scenario.get("contract_sha256")
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
    if args.approved_by == author_id:
        raise VerificationError("Baseline approver must differ from the evidence author")
    artifacts = scenario.get("artifacts") if isinstance(scenario.get("artifacts"), dict) else {}
    actual = Path(str(artifacts.get("actual", "")))
    report = Path(str(artifacts.get("report", "")))
    if not actual.is_file() or not report.is_file():
        raise VerificationError("Scenario capture artifacts are missing")
    baseline_dir = (
        args.baseline_dir.expanduser().resolve()
        if args.baseline_dir
        else Path(str(scenario.get("baseline_dir"))).resolve()
    )
    targets = [
        baseline_dir / "baseline.png",
        baseline_dir / "baseline-report.json",
        baseline_dir / "baseline.json",
    ]
    if any(path.exists() for path in targets) and not args.replace:
        raise VerificationError(
            f"Baseline bundle already exists in {baseline_dir}; use --replace to supersede it"
        )
    baseline_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(actual, targets[0])
    shutil.copy2(report, targets[1])
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
        "source_evidence": str(evidence_path),
        "source_evidence_sha256": sha256_file(evidence_path),
        "image_sha256": sha256_file(targets[0]),
        "capture_report_sha256": sha256_file(targets[1]),
    }
    write_json(targets[2], metadata)
    print(f"Approved baseline bundle: {baseline_dir}")
    return 0


def validate_review(
    evidence: Mapping[str, object], evidence_path: Path, review: Mapping[str, object]
) -> list[str]:
    errors: list[str] = []
    if review.get("schema_version") != REVIEW_SCHEMA_VERSION:
        errors.append("review schema_version must be 1")
    reviewer = review.get("reviewer")
    reviewer = reviewer if isinstance(reviewer, dict) else {}
    reviewer_id = reviewer.get("id")
    if not isinstance(reviewer_id, str) or not reviewer_id:
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
    if not isinstance(review.get("summary"), str) or not review.get("summary"):
        errors.append("review summary is required")
    required = {
        str(item.get("id"))
        for item in evidence.get("scenarios", [])
        if isinstance(item, dict)
    }
    reviewed = review.get("reviewed_scenarios")
    reviewed_set = set(reviewed) if isinstance(reviewed, list) else set()
    missing = sorted(required - reviewed_set)
    if missing:
        errors.append("review omitted scenarios: " + ", ".join(missing))
    attestation = review.get("attestation")
    attestation = attestation if isinstance(attestation, dict) else {}
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
        if item.get("severity") not in {"blocker", "major", "minor", "note"}:
            errors.append(f"findings[{index}].severity is invalid")
        if item.get("scenario_id") not in required:
            errors.append(f"findings[{index}].scenario_id is not in the evidence")
        for key in ("severity", "scenario_id", "region", "artifact", "message"):
            if not isinstance(item.get(key), str) or not item.get(key):
                errors.append(f"findings[{index}].{key} is required")
    return errors


def finalize_review(args: argparse.Namespace) -> int:
    evidence_path = args.evidence.expanduser().resolve()
    review_path = args.review.expanduser().resolve()
    evidence = read_json(evidence_path)
    review = read_json(review_path)
    errors = validate_review(evidence, evidence_path, review)
    deterministic_status = str(evidence.get("deterministic_status", "incomplete"))
    if deterministic_status != "pass":
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
    output = (
        args.output.expanduser().resolve()
        if args.output
        else evidence_path.with_name("verification.json")
    )
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
