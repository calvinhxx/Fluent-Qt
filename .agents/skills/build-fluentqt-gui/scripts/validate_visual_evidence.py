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
CURRENT_CONTRACT_VERSION = 2
ALLOWED_CONTRACT_VERSIONS = {1, CURRENT_CONTRACT_VERSION}
ALLOWED_BACKDROPS = {"mica", "acrylic", "solid", "host-owned"}
ALLOWED_FILL_POLICIES = {"reveal-material", "opaque-hosts", "inherit-host"}
ALLOWED_SIGNATURE_FINISH = {"product", "wireframe"}
ALLOWED_CHROME_ON_MATERIAL = {"quiet", "filled-stickers"}
ALLOWED_SPARSE_CANVAS = {"composed", "dead-space"}
ALLOWED_PRIMARY_INPUT = {"integrated-dock", "independent-card", "none"}
ALLOWED_COPY_REGISTER = {"user-facing", "developer-labeled"}


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


def validate_contract_version(data: dict[str, Any], errors: list[str]) -> int:
    version = data.get("contract_version")
    if version is None:
        return 1
    if isinstance(version, bool) or not isinstance(version, int):
        errors.append("contract_version must be an integer")
        return CURRENT_CONTRACT_VERSION
    if version not in ALLOWED_CONTRACT_VERSIONS:
        errors.append(
            "contract_version must be one of "
            + ", ".join(str(item) for item in sorted(ALLOWED_CONTRACT_VERSIONS))
        )
        return CURRENT_CONTRACT_VERSION
    return version


def validate_window_material(data: dict[str, Any], errors: list[str]) -> None:
    backdrop = data.get("window_backdrop")
    fill_policy = data.get("surface_fill_policy")
    if backdrop not in ALLOWED_BACKDROPS:
        errors.append(
            "window_backdrop must be one of " + ", ".join(sorted(ALLOWED_BACKDROPS))
        )
    elif backdrop in {"solid", "host-owned"}:
        reason = data.get("window_backdrop_reason")
        if not isinstance(reason, str) or not reason.strip():
            errors.append(
                f"window_backdrop {backdrop} requires a non-empty "
                "window_backdrop_reason"
            )
    if fill_policy not in ALLOWED_FILL_POLICIES:
        errors.append(
            "surface_fill_policy must be one of "
            + ", ".join(sorted(ALLOWED_FILL_POLICIES))
        )
    elif fill_policy in {"opaque-hosts", "inherit-host"}:
        reason = data.get("surface_fill_reason")
        if not isinstance(reason, str) or not reason.strip():
            errors.append(
                f"surface_fill_policy {fill_policy} requires a non-empty "
                "surface_fill_reason"
            )
    if backdrop == "host-owned" and fill_policy not in {
        "inherit-host",
        "opaque-hosts",
    }:
        errors.append(
            "window_backdrop host-owned requires surface_fill_policy inherit-host "
            "or opaque-hosts"
        )
    if fill_policy == "inherit-host" and backdrop != "host-owned":
        errors.append(
            "surface_fill_policy inherit-host requires window_backdrop host-owned"
        )


def validate_signature_surface(data: dict[str, Any], errors: list[str]) -> None:
    finish = data.get("signature_finish")
    chrome = data.get("chrome_on_material")
    sparse = data.get("sparse_canvas_treatment")
    primary_input = data.get("primary_input_treatment")
    copy_register = data.get("visible_copy_register")

    if finish not in ALLOWED_SIGNATURE_FINISH:
        errors.append(
            "signature_finish must be one of "
            + ", ".join(sorted(ALLOWED_SIGNATURE_FINISH))
        )
    elif finish == "wireframe":
        errors.append("signature_finish wireframe never passes")

    if chrome not in ALLOWED_CHROME_ON_MATERIAL:
        errors.append(
            "chrome_on_material must be one of "
            + ", ".join(sorted(ALLOWED_CHROME_ON_MATERIAL))
        )
    elif chrome == "filled-stickers":
        errors.append("chrome_on_material filled-stickers never passes")

    if sparse not in ALLOWED_SPARSE_CANVAS:
        errors.append(
            "sparse_canvas_treatment must be one of "
            + ", ".join(sorted(ALLOWED_SPARSE_CANVAS))
        )
    elif sparse == "dead-space":
        errors.append("sparse_canvas_treatment dead-space never passes")

    if primary_input not in ALLOWED_PRIMARY_INPUT:
        errors.append(
            "primary_input_treatment must be one of "
            + ", ".join(sorted(ALLOWED_PRIMARY_INPUT))
        )
    elif primary_input == "independent-card":
        reason = data.get("primary_input_reason")
        if not isinstance(reason, str) or not reason.strip():
            errors.append(
                "primary_input_treatment independent-card requires a "
                "non-empty primary_input_reason"
            )

    if copy_register not in ALLOWED_COPY_REGISTER:
        errors.append(
            "visible_copy_register must be one of "
            + ", ".join(sorted(ALLOWED_COPY_REGISTER))
        )
    elif copy_register == "developer-labeled":
        errors.append("visible_copy_register developer-labeled never passes")


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
    contract_version = validate_contract_version(data, errors)
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
    if contract_version >= 2:
        validate_window_material(data, errors)
        validate_signature_surface(data, errors)

    if errors:
        print("visual evidence: FAIL", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    if contract_version == 1:
        print(
            "visual evidence: WARNING legacy contract v1; add contract_version 2 "
            "and material/signature fields for new evidence",
            file=sys.stderr,
        )

    print(
        "visual evidence: PASS "
        f"({profile}, contract v{contract_version}, {len(data['states'])} states, "
        f"{len(data['regions'])} regions, "
        f"{len(data['measurements'])} measurements)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
