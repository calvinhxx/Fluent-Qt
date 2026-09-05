"""Evaluate geometry contracts and verify pixel-comparator results."""

from __future__ import annotations

from pathlib import Path
from typing import Mapping

from .artifacts import png_dimensions, sha256_file
from .common import (
    SAFE_ID,
    VerificationError,
    check,
    is_json_integer,
    merged_dict,
    qt_scale_positive,
)
from .reports import baseline_geometry_report_errors, geometry_report_errors


def validated_widget_index(
    report: Mapping[str, object], *, sanitized: bool = False
) -> tuple[dict[str, list[dict[str, object]]], list[str]]:
    geometry = report.get("geometry_report")
    errors = (
        baseline_geometry_report_errors(geometry)
        if sanitized
        else geometry_report_errors(geometry)
    )
    if errors:
        return {}, errors
    assert isinstance(geometry, dict)
    widgets = geometry.get("widgets")
    index: dict[str, list[dict[str, object]]] = {}
    assert isinstance(widgets, list)
    for item in widgets:
        assert isinstance(item, dict)
        name = item.get("object_name")
        if isinstance(name, str) and name:
            index.setdefault(name, []).append(item)
    return index, []


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
    index, report_errors = validated_widget_index(report)
    if report_errors:
        return check(
            "geometry.contract",
            "incomplete",
            "Geometry report is malformed and cannot support a contract claim.",
            {"validation_errors": report_errors},
        )
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
        rect = widget.get("rect") if isinstance(widget.get("rect"), dict) else {}
        visible_rect = (
            widget.get("visible_rect")
            if isinstance(widget.get("visible_rect"), dict)
            else {}
        )
        if any(int(rect.get(name, 0)) <= 0 for name in ("width", "height")):
            failures.append(f"{name} has a zero-size rect")
        if any(
            int(visible_rect.get(name, 0)) <= 0
            for name in ("width", "height")
        ):
            failures.append(f"{name} has no visible area")
        if entry.get("not_clipped", True) and widget.get("clipped") is True:
            failures.append(f"{name} is clipped")
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
    actual_index, actual_errors = validated_widget_index(actual_report)
    baseline_index, baseline_errors = validated_widget_index(
        baseline_report, sanitized=True
    )
    if actual_errors or baseline_errors:
        return check(
            "geometry.baseline",
            "incomplete",
            "Actual or approved geometry report is malformed.",
            {
                "actual_validation_errors": actual_errors,
                "baseline_validation_errors": baseline_errors,
            },
        )
    required = policy.get("required") if isinstance(policy.get("required"), list) else []
    default_tolerance = int(policy.get("tolerance", 0))
    comparisons: list[dict[str, object]] = []
    failures: list[str] = []
    required_names = [
        str(normalized_geometry_entry(raw, default_tolerance)["object_name"])
        for raw in required
    ]
    baseline_geometry = baseline_report.get("geometry_report")
    assert isinstance(baseline_geometry, dict)
    baseline_widgets = baseline_geometry.get("widgets")
    assert isinstance(baseline_widgets, list)
    baseline_names = [
        str(widget.get("object_name", "")) for widget in baseline_widgets
    ]
    if baseline_names != required_names:
        failures.append(
            "approved baseline geometry must contain exactly the recipe-required "
            "probes in declared order"
        )
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


def expected_comparator_policy(policy: Mapping[str, object]) -> dict[str, object]:
    max_pixels = policy.get("max_different_pixels")
    max_ratio = policy.get("max_different_ratio")
    if max_pixels is None and max_ratio is None:
        max_pixels = 0
    return {
        "channel_threshold": policy.get("channel_threshold", 0),
        "max_different_pixels": max_pixels,
        "max_different_ratio": max_ratio,
        "translation_search_radius": policy.get("search_radius", 4),
        "max_translation": policy.get("max_translation", 0),
        "edge_threshold": policy.get("edge_threshold", 12),
    }


def expected_region(region: list[int] | None) -> dict[str, int] | None:
    if region is None:
        return None
    return dict(zip(("x", "y", "width", "height"), region))


def validate_comparator_report(
    report: Mapping[str, object],
    returncode: int,
    baseline_path: Path,
    actual_path: Path,
    region: list[int] | None,
    policy: Mapping[str, object],
) -> list[str]:
    errors: list[str] = []
    baseline_dimensions = png_dimensions(baseline_path)
    actual_dimensions = png_dimensions(actual_path)
    if baseline_dimensions is None:
        errors.append("comparator baseline is not a valid PNG")
    if actual_dimensions is None:
        errors.append("comparator actual is not a valid PNG")
    expected_dimensions: tuple[int, int] | None = None
    if baseline_dimensions is not None and actual_dimensions is not None:
        if region is None:
            if baseline_dimensions != actual_dimensions:
                errors.append("comparator input PNG dimensions do not match")
            else:
                expected_dimensions = baseline_dimensions
        else:
            x, y, width, height = region
            expected_dimensions = (width, height)
            for label, dimensions in (
                ("baseline", baseline_dimensions),
                ("actual", actual_dimensions),
            ):
                if (
                    x < 0
                    or y < 0
                    or x + width > dimensions[0]
                    or y + height > dimensions[1]
                ):
                    errors.append(
                        f"comparator region is outside the {label} PNG bounds"
                    )
    if report.get("schema_version") != 1:
        errors.append("unsupported comparator report schema")
    if report.get("tool") != "FluentQt Visual Compare":
        errors.append("unexpected comparator tool identity")
    expected_status = "pass" if returncode == 0 else "fail" if returncode == 1 else None
    if expected_status is None or report.get("status") != expected_status:
        errors.append("comparator status does not match its process return code")
    inputs = report.get("inputs") if isinstance(report.get("inputs"), dict) else {}
    if Path(str(inputs.get("baseline", ""))).resolve() != baseline_path.resolve():
        errors.append("comparator baseline input does not match the requested file")
    if Path(str(inputs.get("actual", ""))).resolve() != actual_path.resolve():
        errors.append("comparator actual input does not match the requested file")
    if inputs.get("baseline_sha256") != sha256_file(baseline_path):
        errors.append("comparator baseline digest is missing or stale")
    if inputs.get("actual_sha256") != sha256_file(actual_path):
        errors.append("comparator actual digest is missing or stale")
    if inputs.get("region") != expected_region(region):
        errors.append("comparator region does not match the requested crop")
    expected_size = (
        {"width": expected_dimensions[0], "height": expected_dimensions[1]}
        if expected_dimensions is not None
        else None
    )
    if report.get("baseline_size") != expected_size:
        errors.append("comparator baseline size does not match the input PNG")
    if report.get("actual_size") != expected_size:
        errors.append("comparator actual size does not match the input PNG")
    if report.get("policy") != expected_comparator_policy(policy):
        errors.append("comparator report policy does not match the requested policy")
    checks = report.get("checks") if isinstance(report.get("checks"), dict) else {}
    required_checks = {
        "size_matches",
        "pixel_limits_pass",
        "translation_limit_pass",
    }
    if set(checks) != required_checks or not all(
        isinstance(checks.get(name), bool) for name in required_checks
    ):
        errors.append("comparator checks are missing or malformed")
    elif expected_status == "pass" and not all(checks.values()):
        errors.append("passing comparator report contains a failed check")
    elif expected_status == "fail" and all(checks.values()):
        errors.append("failing comparator report contains no failed check")
    metrics = report.get("metrics") if isinstance(report.get("metrics"), dict) else {}
    total = metrics.get("total_pixels")
    different = metrics.get("different_pixels")
    ratio = metrics.get("different_ratio")
    numeric = lambda value: isinstance(value, (int, float)) and not isinstance(value, bool)
    if not is_json_integer(total, minimum=1):
        errors.append("comparator total_pixels must be a positive integer")
    elif expected_dimensions is not None and total != (
        expected_dimensions[0] * expected_dimensions[1]
    ):
        errors.append("comparator total_pixels does not match the compared image size")
    if not is_json_integer(different, minimum=0) or (
        is_json_integer(total, minimum=1) and different > total
    ):
        errors.append("comparator different_pixels is outside the image bounds")
    if not numeric(ratio) or not 0 <= ratio <= 1:
        errors.append("comparator different_ratio is outside 0..1")
    elif is_json_integer(total, minimum=1) and is_json_integer(different, minimum=0):
        expected_ratio = different / total
        if abs(ratio - expected_ratio) > 1e-9:
            errors.append("comparator different_ratio does not match pixel counts")
    expected_policy = expected_comparator_policy(policy)
    if is_json_integer(different, minimum=0) and numeric(ratio):
        max_pixels = expected_policy["max_different_pixels"]
        max_ratio = expected_policy["max_different_ratio"]
        expected_pixel_limit = (
            (max_pixels is None or different <= max_pixels)
            and (max_ratio is None or ratio <= max_ratio)
        )
        if checks.get("pixel_limits_pass") != expected_pixel_limit:
            errors.append(
                "comparator pixel limit check does not match its metrics"
            )
    translation = (
        metrics.get("estimated_translation")
        if isinstance(metrics.get("estimated_translation"), dict)
        else {}
    )
    translation_fields = {
        "dx",
        "dy",
        "confident",
        "baseline_edge_pixels",
        "actual_edge_pixels",
        "zero_offset_score",
        "best_score",
        "improvement",
    }
    if set(translation) != translation_fields:
        errors.append("comparator estimated_translation is missing or malformed")
    else:
        for name in ("dx", "dy"):
            if not is_json_integer(translation.get(name)):
                errors.append(
                    f"comparator estimated_translation.{name} must be an integer"
                )
        if not isinstance(translation.get("confident"), bool):
            errors.append(
                "comparator estimated_translation.confident must be a boolean"
            )
        for name in ("baseline_edge_pixels", "actual_edge_pixels"):
            if not is_json_integer(translation.get(name), minimum=0):
                errors.append(
                    f"comparator estimated_translation.{name} must be a non-negative integer"
                )
        for name in ("zero_offset_score", "best_score", "improvement"):
            if not numeric(translation.get(name)):
                errors.append(
                    f"comparator estimated_translation.{name} must be numeric"
                )
    max_translation = expected_policy["max_translation"]
    if max_translation is not None and is_json_integer(different, minimum=0):
        if different == 0 or translation.get("confident") is not True:
            expected_translation_limit = True
        else:
            dx = translation.get("dx")
            dy = translation.get("dy")
            expected_translation_limit = (
                translation.get("confident") is True
                and is_json_integer(dx)
                and is_json_integer(dy)
                and abs(dx) <= max_translation
                and abs(dy) <= max_translation
            )
        if checks.get("translation_limit_pass") != expected_translation_limit:
            errors.append(
                "comparator translation check does not match its metrics"
            )
    return errors


def pixel_comparisons(
    policy: Mapping[str, object], device_pixel_ratio: float
) -> list[tuple[str, Mapping[str, object], list[int] | None]]:
    comparisons: list[tuple[str, Mapping[str, object], list[int] | None]] = [
        ("full", policy, None)
    ]
    regions = policy.get("regions")
    if not isinstance(regions, list):
        return comparisons
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
            scaled_rect = [
                qt_scale_positive(value, device_pixel_ratio)
                for value in physical_rect
            ]
            if any(value is None for value in scaled_rect):
                continue
            physical_rect = [int(value) for value in scaled_rect]
        comparisons.append(
            (region_id, merged_dict(policy, raw.get("policy")), physical_rect)
        )
    return comparisons
