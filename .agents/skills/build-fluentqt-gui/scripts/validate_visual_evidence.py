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

from validate_design_brief import load_brief, validate_brief


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
CURRENT_CONTRACT_VERSION = 4
ALLOWED_CONTRACT_VERSIONS = {1, 2, 3, CURRENT_CONTRACT_VERSION}
ALLOWED_BACKDROPS = {"mica", "acrylic", "solid", "host-owned"}
ALLOWED_FILL_POLICIES = {"reveal-material", "opaque-hosts", "inherit-host"}
ALLOWED_SIGNATURE_FINISH = {"product", "wireframe"}
ALLOWED_CHROME_ON_MATERIAL = {"quiet", "filled-stickers"}
ALLOWED_SPARSE_CANVAS = {"composed", "dead-space"}
ALLOWED_PRIMARY_INPUT = {"integrated-dock", "independent-card", "none"}
ALLOWED_COPY_REGISTER = {"user-facing", "developer-labeled"}
ALLOWED_REVIEWER_KINDS = {"human", "independent-agent"}
LEGACY_V3_REVIEW_SCORES = {
    "workflow_fit",
    "product_signature",
    "visual_hierarchy",
    "density_and_typography",
    "theme_and_material",
    "responsive_quality",
    "state_and_interaction_polish",
}
CURRENT_REVIEW_SCORES = LEGACY_V3_REVIEW_SCORES | {
    "iconography",
    "surface_composition",
}


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


def validate_design_brief_reference(
    data: dict[str, Any],
    manifest_path: Path,
    contract_version: int,
    errors: list[str],
) -> None:
    raw_path = data.get("design_brief")
    if not isinstance(raw_path, str) or not raw_path.strip():
        errors.append("design_brief must be a non-empty local path for contract v3+")
        return
    brief_path = resolve_local_path(raw_path, manifest_path)
    if brief_path is None or not brief_path.is_file():
        errors.append(f"design_brief file does not exist: {raw_path}")
        return
    try:
        brief = load_brief(brief_path)
    except ValueError as exc:
        errors.append(f"design_brief is invalid: {exc}")
        return
    for brief_error in validate_brief(
        brief,
        brief_path,
        require_current=contract_version >= CURRENT_CONTRACT_VERSION,
    ):
        errors.append(f"design_brief: {brief_error}")
    for field in ("application", "profile", "author_id"):
        if brief.get(field) != data.get(field):
            errors.append(f"design_brief {field} must match visual evidence")


def _validate_review_evidence(
    evidence: object,
    manifest_path: Path,
    label: str,
    errors: list[str],
) -> None:
    if (
        not isinstance(evidence, list)
        or not evidence
        or not all(isinstance(item, str) and item for item in evidence)
    ):
        errors.append(f"{label} must be a non-empty evidence-path array")
        return
    for index, raw_path in enumerate(evidence):
        validate_existing_path(
            raw_path,
            manifest_path,
            f"{label}[{index}]",
            errors,
            require_file=True,
        )


def validate_independent_review(
    data: dict[str, Any],
    manifest_path: Path,
    contract_version: int,
    errors: list[str],
) -> None:
    author_id = data.get("author_id")
    if not isinstance(author_id, str) or not author_id.strip():
        errors.append("author_id must be a non-empty string for contract v3+")
        author_id = ""
    review = data.get("review")
    if not isinstance(review, dict):
        errors.append("review must be an object for contract v3+")
        return
    reviewer_kind = review.get("reviewer_kind")
    reviewer_id = review.get("reviewer_id")
    if reviewer_kind not in ALLOWED_REVIEWER_KINDS:
        errors.append(
            "review.reviewer_kind must be one of "
            + ", ".join(sorted(ALLOWED_REVIEWER_KINDS))
        )
    if not isinstance(reviewer_id, str) or not reviewer_id.strip():
        errors.append("review.reviewer_id must be a non-empty string")
    elif author_id and reviewer_id.strip().casefold() == author_id.strip().casefold():
        errors.append("reviewer_id must differ from author_id")
    reviewed_at = review.get("reviewed_at")
    if not isinstance(reviewed_at, str) or not reviewed_at.strip():
        errors.append("review.reviewed_at must be a non-empty string")
    if review.get("verdict") != "pass":
        errors.append("review.verdict must be pass")

    review_board = review.get("review_board")
    if not isinstance(review_board, str) or not review_board.strip():
        errors.append("review.review_board must be a non-empty local path")
    else:
        validate_existing_path(
            review_board,
            manifest_path,
            "review.review_board",
            errors,
            require_file=True,
        )

    reference_images = review.get("reference_images")
    if data.get("profile") == "full":
        _validate_review_evidence(
            reference_images,
            manifest_path,
            "review.reference_images",
            errors,
        )
    elif reference_images:
        _validate_review_evidence(
            reference_images,
            manifest_path,
            "review.reference_images",
            errors,
        )

    scores = review.get("scores")
    if not isinstance(scores, dict):
        errors.append("review.scores must be an object")
    else:
        required_scores = (
            CURRENT_REVIEW_SCORES
            if contract_version >= CURRENT_CONTRACT_VERSION
            else LEGACY_V3_REVIEW_SCORES
        )
        missing_scores = required_scores - set(scores)
        if missing_scores:
            errors.append(
                "review.scores missing: " + ", ".join(sorted(missing_scores))
            )
        for score_id in sorted(required_scores):
            entry = scores.get(score_id)
            prefix = f"review.scores.{score_id}"
            if not isinstance(entry, dict):
                continue
            score = entry.get("score")
            if isinstance(score, bool) or not isinstance(score, int) or not 1 <= score <= 5:
                errors.append(f"{prefix}.score must be an integer 1..5")
            elif score < 4:
                errors.append(f"{prefix}.score must be at least 4 to pass")
            note = entry.get("note")
            if not isinstance(note, str) or not note.strip():
                errors.append(f"{prefix}.note must be a non-empty string")
            _validate_review_evidence(
                entry.get("evidence"),
                manifest_path,
                f"{prefix}.evidence",
                errors,
            )

    findings = review.get("findings")
    if not isinstance(findings, list):
        errors.append("review.findings must be an array")
    else:
        for index, finding in enumerate(findings):
            if not isinstance(finding, dict):
                errors.append(f"review.findings[{index}] must be an object")
                continue
            if (
                finding.get("status") == "open"
                and finding.get("severity") in {"blocker", "major"}
            ):
                errors.append(
                    "independent review has an open "
                    f"{finding.get('severity')} finding: "
                    f"{finding.get('id', index)}"
                )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument(
        "--require-current",
        action="store_true",
        help="reject legacy evidence that does not use the current contract",
    )
    args = parser.parse_args()

    try:
        data = load_manifest(args.manifest)
    except ValueError as exc:
        print(f"visual evidence: FAIL\n- {exc}", file=sys.stderr)
        return 1

    errors: list[str] = []
    contract_version = validate_contract_version(data, errors)
    if args.require_current and contract_version != CURRENT_CONTRACT_VERSION:
        errors.append(
            f"contract_version {CURRENT_CONTRACT_VERSION} is required; "
            f"received {contract_version}"
        )
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
    if contract_version >= 3:
        validate_design_brief_reference(
            data, args.manifest, contract_version, errors
        )
        validate_independent_review(
            data, args.manifest, contract_version, errors
        )

    if errors:
        print("visual evidence: FAIL", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    if contract_version < CURRENT_CONTRACT_VERSION:
        print(
            f"visual evidence: WARNING legacy contract v{contract_version}; "
            f"new evidence uses contract_version {CURRENT_CONTRACT_VERSION} "
            "with a validated design brief and nine-dimension independent review",
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
