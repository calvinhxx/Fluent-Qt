#!/usr/bin/env python3
"""Validate visual-review coverage for build-fluentqt-gui."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse
from urllib.request import url2pathname


FULL_REQUIRED_STATES = {
    "normal-light",
    "normal-dark",
    "narrow",
    "minimum",
    "empty",
    "collection-density",
    "long-localized-content",
    "input-single-line",
    "input-max-lines",
    "scroll-end",
    "async-settled",
    "selected-focus-disabled",
    "transient-surface",
    "ime-preedit",
}

LITE_REQUIRED_STATES = {
    "normal-light",
    "normal-dark",
    "narrow",
    "minimum",
    "long-localized-content",
    "selected-focus-disabled",
}

FULL_REQUIRED_REGIONS = {
    "titlebar",
    "navigation-or-list",
    "primary-viewport",
    "pane-header-or-status",
    "footer-or-primary-input",
    "scroll-viewport",
    "transient-surface",
}

LITE_REQUIRED_REGIONS = {
    "titlebar",
    "primary-viewport",
    "footer-or-primary-input",
}

FULL_REQUIRED_DYNAMIC_CHECKS = {
    "wrapped-text-height",
    "multiline-input",
    "async-scroll-end",
}

LITE_REQUIRED_DYNAMIC_CHECKS: set[str] = set()

ALLOWED_STATUS = {"pass", "fail", "unverified", "not-applicable"}
ALLOWED_PROFILES = {"lite", "full"}


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read JSON manifest: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError("manifest root must be an object")
    return data


def resolve_local_path(raw_path: str, manifest_path: Path) -> Path | None:
    direct = Path(raw_path).expanduser()
    if direct.is_absolute():
        return direct.resolve()
    parsed = urlparse(raw_path)
    if parsed.scheme and parsed.scheme != "file":
        return None
    value = (
        url2pathname(unquote(parsed.path))
        if parsed.scheme == "file"
        else raw_path
    )
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = manifest_path.parent / path
    return path.resolve()


def validate_existing_path(
    raw_path: str,
    manifest_path: Path,
    label: str,
    errors: list[str],
    *,
    require_file: bool,
) -> None:
    path = resolve_local_path(raw_path, manifest_path)
    if path is None:
        errors.append(f"{label} must be a local path or file:// URL: {raw_path}")
        return
    exists = path.is_file() if require_file else path.exists()
    if not exists:
        kind = "file" if require_file else "path"
        errors.append(f"{label} {kind} does not exist: {raw_path}")


def validate_entries(
    data: dict[str, Any],
    field: str,
    required_ids: set[str],
    allowed_ids: set[str],
    manifest_path: Path,
    errors: list[str],
) -> None:
    entries = data.get(field)
    if not isinstance(entries, list):
        errors.append(f"{field} must be an array")
        return

    seen: set[str] = set()
    for index, entry in enumerate(entries):
        prefix = f"{field}[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{prefix} must be an object")
            continue
        entry_id = entry.get("id")
        status = entry.get("status")
        if not isinstance(entry_id, str) or not entry_id:
            errors.append(f"{prefix}.id must be a non-empty string")
            continue
        if entry_id in seen:
            errors.append(f"duplicate {field} id: {entry_id}")
        seen.add(entry_id)
        if entry_id not in allowed_ids:
            errors.append(f"unknown {field} id: {entry_id}")
        if status not in ALLOWED_STATUS:
            errors.append(f"{prefix}.status must be one of {sorted(ALLOWED_STATUS)}")
            continue
        if status in {"fail", "unverified"}:
            errors.append(f"{field} entry {entry_id} blocks acceptance: {status}")
        if status == "not-applicable":
            reason = entry.get("reason")
            if not isinstance(reason, str) or not reason.strip():
                errors.append(f"{field} entry {entry_id} needs a not-applicable reason")
        elif status == "pass":
            evidence = entry.get("evidence")
            if not isinstance(evidence, list) or not evidence or not all(
                isinstance(item, str) and item for item in evidence
            ):
                errors.append(f"{field} entry {entry_id} needs non-empty evidence paths")
            else:
                for evidence_index, evidence_path in enumerate(evidence):
                    validate_existing_path(
                        evidence_path,
                        manifest_path,
                        f"{field} entry {entry_id} evidence[{evidence_index}]",
                        errors,
                        require_file=True,
                    )

    missing = sorted(required_ids - seen)
    if missing:
        errors.append(f"{field} missing required ids: {', '.join(missing)}")


def validate_measurements(
    data: dict[str, Any], manifest_path: Path, errors: list[str]
) -> None:
    entries = data.get("measurements")
    if not isinstance(entries, list) or not entries:
        errors.append("measurements must be a non-empty array")
        return
    for index, entry in enumerate(entries):
        prefix = f"measurements[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{prefix} must be an object")
            continue
        for key in ("id", "expected", "actual", "evidence"):
            if not isinstance(entry.get(key), str) or not entry[key]:
                errors.append(f"{prefix}.{key} must be a non-empty string")
        evidence = entry.get("evidence")
        if isinstance(evidence, str) and evidence:
            validate_existing_path(
                evidence,
                manifest_path,
                f"{prefix}.evidence",
                errors,
                require_file=True,
            )
        if entry.get("status") != "pass":
            errors.append(f"{prefix} blocks acceptance: status must be pass")


def validate_issues(data: dict[str, Any], errors: list[str]) -> None:
    entries = data.get("issues")
    if not isinstance(entries, list):
        errors.append("issues must be an array")
        return
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"issues[{index}] must be an object")
            continue
        if entry.get("status") == "open" and entry.get("severity") in {"blocker", "major"}:
            errors.append(
                f"open {entry.get('severity')} issue blocks acceptance: "
                f"{entry.get('id', index)}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()

    try:
        data = load_manifest(args.manifest)
    except ValueError as exc:
        print(f"visual evidence: FAIL\n- {exc}", file=sys.stderr)
        return 1

    errors: list[str] = []
    for key in ("application", "reviewed_build", "platform", "profile"):
        if not isinstance(data.get(key), str) or not data[key].strip():
            errors.append(f"{key} must be a non-empty string")
    profile = data.get("profile")
    if profile not in ALLOWED_PROFILES:
        errors.append(f"profile must be one of {sorted(ALLOWED_PROFILES)}")
        profile = "full"

    reviewed_build = data.get("reviewed_build")
    if isinstance(reviewed_build, str) and reviewed_build:
        validate_existing_path(
            reviewed_build,
            args.manifest,
            "reviewed_build",
            errors,
            require_file=False,
        )

    required_states = (
        LITE_REQUIRED_STATES if profile == "lite" else FULL_REQUIRED_STATES
    )
    required_regions = (
        LITE_REQUIRED_REGIONS if profile == "lite" else FULL_REQUIRED_REGIONS
    )
    required_dynamic_checks = (
        LITE_REQUIRED_DYNAMIC_CHECKS
        if profile == "lite"
        else FULL_REQUIRED_DYNAMIC_CHECKS
    )
    validate_entries(
        data,
        "states",
        required_states,
        FULL_REQUIRED_STATES,
        args.manifest,
        errors,
    )
    validate_entries(
        data,
        "regions",
        required_regions,
        FULL_REQUIRED_REGIONS,
        args.manifest,
        errors,
    )
    validate_entries(
        data,
        "dynamic_checks",
        required_dynamic_checks,
        FULL_REQUIRED_DYNAMIC_CHECKS,
        args.manifest,
        errors,
    )
    validate_measurements(data, args.manifest, errors)
    validate_issues(data, errors)

    if errors:
        print("visual evidence: FAIL", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        "visual evidence: PASS "
        f"({profile}, {len(data['states'])} states, {len(data['regions'])} regions, "
        f"{len(data['measurements'])} measurements)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
