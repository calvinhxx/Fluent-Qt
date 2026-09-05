"""Validate Gallery capture, Inspector, interaction, and baseline reports."""

from __future__ import annotations

from collections import Counter
from typing import Mapping
import re

from .common import (
    ACTION_NAMES,
    BASELINE_SCHEMA_VERSION,
    MAX_DEVICE_PIXEL_RATIO,
    check,
    is_json_integer,
    is_json_number,
    is_sha256,
    is_trimmed_nonempty,
    nested,
    object_fields_are,
    validated_device_pixel_ratio,
)


def rect_report_errors(value: object, context: str) -> list[str]:
    fields = {"x", "y", "width", "height"}
    if not object_fields_are(value, fields, fields):
        return [f"{context} must contain exactly x, y, width, and height"]
    assert isinstance(value, dict)
    errors: list[str] = []
    for name in ("x", "y"):
        if not is_json_integer(value.get(name)):
            errors.append(f"{context}.{name} must be an integer")
    for name in ("width", "height"):
        if not is_json_integer(value.get(name), minimum=0):
            errors.append(f"{context}.{name} must be a non-negative integer")
    return errors


def size_report_errors(
    value: object, context: str, *, minimum: int | None = 0
) -> list[str]:
    fields = {"width", "height"}
    if not object_fields_are(value, fields, fields):
        return [f"{context} must contain exactly width and height"]
    assert isinstance(value, dict)
    return [
        f"{context}.{name} must be an integer"
        + ("" if minimum is None else f" >= {minimum}")
        for name in ("width", "height")
        if not is_json_integer(value.get(name), minimum=minimum)
    ]


def capture_environment_errors(value: object) -> list[str]:
    fields = {
        "fingerprint_schema_version",
        "qt_version",
        "platform_plugin",
        "style",
        "device_pixel_ratio",
        "logical_dpi_x",
        "logical_dpi_y",
        "locale",
        "font",
        "screen",
        "system",
        "scale_environment",
    }
    if not object_fields_are(value, fields, fields):
        return ["capture environment has missing or unsupported top-level fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("fingerprint_schema_version") != 1:
        errors.append("capture environment fingerprint_schema_version must be 1")
    for name in ("qt_version", "platform_plugin", "style", "locale"):
        if not isinstance(value.get(name), str) or not value.get(name):
            errors.append(f"capture environment {name} must be non-empty")
    if validated_device_pixel_ratio(value.get("device_pixel_ratio")) is None:
        errors.append(
            "capture environment device_pixel_ratio must be positive and <= "
            f"{MAX_DEVICE_PIXEL_RATIO:g}"
        )
    for name in ("logical_dpi_x", "logical_dpi_y"):
        if not is_json_number(value.get(name), minimum=0.000001):
            errors.append(f"capture environment {name} must be positive")

    font_fields = {
        "family",
        "style_name",
        "point_size",
        "pixel_size",
        "weight",
        "italic",
    }
    font = value.get("font")
    if not object_fields_are(font, font_fields, font_fields):
        errors.append("capture environment font has missing or unsupported fields")
    else:
        assert isinstance(font, dict)
        if not isinstance(font.get("family"), str) or not font.get("family"):
            errors.append("capture environment font.family must be non-empty")
        if not isinstance(font.get("style_name"), str):
            errors.append("capture environment font.style_name must be a string")
        if not is_json_number(font.get("point_size")):
            errors.append("capture environment font.point_size must be numeric")
        for name in ("pixel_size", "weight"):
            if not is_json_integer(font.get(name)):
                errors.append(f"capture environment font.{name} must be an integer")
        if not isinstance(font.get("italic"), bool):
            errors.append("capture environment font.italic must be a boolean")

    screen_fields = {
        "depth",
        "geometry",
        "available_geometry",
        "physical_dpi_x",
        "physical_dpi_y",
    }
    screen_fields |= {"name", "manufacturer", "model", "serial_number"}
    screen = value.get("screen")
    if not object_fields_are(screen, screen_fields, screen_fields):
        errors.append("capture environment screen has missing or unsupported fields")
    else:
        assert isinstance(screen, dict)
        for name in ("name", "manufacturer", "model", "serial_number"):
            if not isinstance(screen.get(name), str):
                errors.append(f"capture environment screen.{name} must be a string")
        if not is_json_integer(screen.get("depth"), minimum=1):
            errors.append("capture environment screen.depth must be positive")
        for name in ("geometry", "available_geometry"):
            errors.extend(
                rect_report_errors(
                    screen.get(name), f"capture environment screen.{name}"
                )
            )
            rect = screen.get(name)
            if isinstance(rect, dict) and any(
                not is_json_integer(rect.get(dimension), minimum=1)
                for dimension in ("width", "height")
            ):
                errors.append(
                    f"capture environment screen.{name} must have positive area"
                )
        for name in ("physical_dpi_x", "physical_dpi_y"):
            if not is_json_number(screen.get(name), minimum=0.000001):
                errors.append(f"capture environment screen.{name} must be positive")

    system_fields = {
        "product_type",
        "product_version",
        "kernel_type",
        "kernel_version",
        "cpu_architecture",
    }
    system = value.get("system")
    if not object_fields_are(system, system_fields, system_fields):
        errors.append("capture environment system has missing or unsupported fields")
    else:
        assert isinstance(system, dict)
        for name in system_fields:
            if not isinstance(system.get(name), str):
                errors.append(f"capture environment system.{name} must be a string")
        for name in ("product_type", "kernel_type", "cpu_architecture"):
            if isinstance(system.get(name), str) and not system.get(name):
                errors.append(f"capture environment system.{name} must be non-empty")

    scale_fields = {
        "QT_SCALE_FACTOR",
        "QT_SCREEN_SCALE_FACTORS",
        "QT_FONT_DPI",
        "QT_AUTO_SCREEN_SCALE_FACTOR",
        "QT_ENABLE_HIGHDPI_SCALING",
    }
    scale_environment = value.get("scale_environment")
    if not object_fields_are(scale_environment, scale_fields, scale_fields):
        errors.append(
            "capture environment scale_environment has missing or unsupported fields"
        )
    elif not all(
        isinstance(scale_environment.get(name), str) for name in scale_fields
    ):
        errors.append("capture environment scale_environment values must be strings")
    return errors


def capture_environment_check(report: Mapping[str, object]) -> dict[str, object]:
    errors = capture_environment_errors(report.get("environment"))
    return check(
        "capture.environment",
        "incomplete" if errors else "pass",
        "Capture environment fingerprint is complete and well formed."
        if not errors
        else "Capture environment fingerprint is incomplete or malformed.",
        {"validation_errors": errors},
    )


def interaction_report_errors(value: object) -> list[str]:
    required = {"schema_version", "requested", "status", "summary", "steps"}
    allowed = required | {"source", "error"}
    if not object_fields_are(value, required, allowed):
        return ["interaction_report has missing or unsupported fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("interaction_report.schema_version must be 1")
    if not isinstance(value.get("requested"), bool):
        errors.append("interaction_report.requested must be a boolean")
    if value.get("status") not in {"pass", "fail", "not-requested"}:
        errors.append("interaction_report.status is unsupported")
    if "source" in value and not isinstance(value.get("source"), str):
        errors.append("interaction_report.source must be a string")
    if "error" in value and not isinstance(value.get("error"), str):
        errors.append("interaction_report.error must be a string")
    summary_fields = {"total", "executed", "passed", "failed"}
    summary = value.get("summary")
    if not object_fields_are(summary, summary_fields, summary_fields):
        errors.append("interaction_report.summary is malformed")
    else:
        assert isinstance(summary, dict)
        if any(
            not is_json_integer(summary.get(name), minimum=0)
            for name in summary_fields
        ):
            errors.append("interaction_report.summary values must be integers")
    steps = value.get("steps")
    if not isinstance(steps, list):
        return [*errors, "interaction_report.steps must be an array"]
    requested = value.get("requested")
    if requested is True:
        if not is_trimmed_nonempty(value.get("source")):
            errors.append(
                "requested interaction_report.source must be non-empty"
            )
        if value.get("status") not in {"pass", "fail"}:
            errors.append("requested interaction_report status is invalid")
    elif requested is False:
        if "source" in value:
            errors.append(
                "not-requested interaction_report must not contain source"
            )
        if value.get("status") != "not-requested" or steps:
            errors.append("not-requested interaction_report is inconsistent")
    if isinstance(summary, dict) and all(
        is_json_integer(summary.get(name), minimum=0)
        for name in summary_fields
    ):
        if summary.get("executed") != len(steps):
            errors.append(
                "interaction_report.summary.executed does not match steps"
            )
        if summary.get("passed") + summary.get("failed") != len(steps):
            errors.append(
                "interaction_report.summary outcomes do not match steps"
            )
        if summary.get("total") < summary.get("executed"):
            errors.append(
                "interaction_report.summary.total is smaller than executed"
            )
    step_fields = {
        "index",
        "request",
        "id",
        "action",
        "target",
        "mechanism",
        "expect",
        "observation",
        "status",
        "message",
    }
    for index, step in enumerate(steps):
        context = f"interaction_report.steps[{index}]"
        if not isinstance(step, dict) or not {"index", "status"} <= set(step):
            errors.append(f"{context} is malformed")
            continue
        if not set(step) <= step_fields:
            errors.append(f"{context} has unsupported fields")
        if not is_json_integer(step.get("index"), minimum=0):
            errors.append(f"{context}.index must be a non-negative integer")
        if step.get("status") not in {"pass", "fail"}:
            errors.append(f"{context}.status is unsupported")
        for name in ("request", "expect", "observation"):
            if name in step and not isinstance(step.get(name), dict):
                errors.append(f"{context}.{name} must be an object")
        for name in ("id", "action", "target", "mechanism", "message"):
            if name in step and not isinstance(step.get(name), str):
                errors.append(f"{context}.{name} must be a string")
    return errors


def capture_report_errors(value: object) -> list[str]:
    fields = {
        "schema_version",
        "tool",
        "status",
        "selection",
        "scene",
        "environment",
        "artifacts",
        "interaction_report",
        "geometry_report",
        "quality_report",
    }
    if not object_fields_are(value, fields, fields):
        return ["capture report has missing or unsupported top-level fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 2:
        errors.append("capture report schema_version must be 2")
    if value.get("tool") != "FluentQt Gallery Preview":
        errors.append("capture report tool is unsupported")
    if value.get("status") not in {"ok", "artifact-error", "interaction-error"}:
        errors.append("capture report status is unsupported")

    selection_fields = {"route", "sample"}
    selection = value.get("selection")
    if not object_fields_are(selection, selection_fields, selection_fields):
        errors.append("capture report selection is malformed")
    else:
        assert isinstance(selection, dict)
        if not isinstance(selection.get("route"), str) or not selection.get("route"):
            errors.append("capture report selection.route must be non-empty")
        if not isinstance(selection.get("sample"), str):
            errors.append("capture report selection.sample must be a string")

    scene_fields = {
        "requested_theme",
        "theme",
        "layout_direction",
        "settle_ms",
        "requested_width",
        "requested_height",
        "actual_width",
        "actual_height",
    }
    scene = value.get("scene")
    if not object_fields_are(scene, scene_fields, scene_fields):
        errors.append("capture report scene is malformed")
    else:
        assert isinstance(scene, dict)
        for name in ("requested_theme", "theme"):
            if scene.get(name) not in {"light", "dark"}:
                errors.append(f"capture report scene.{name} is unsupported")
        if scene.get("layout_direction") not in {"ltr", "rtl"}:
            errors.append("capture report scene.layout_direction is unsupported")
        if not is_json_integer(scene.get("settle_ms"), minimum=0):
            errors.append("capture report scene.settle_ms must be non-negative")
        for name in ("requested_width", "actual_width"):
            if not is_json_integer(scene.get(name), minimum=1, maximum=3840):
                errors.append(
                    f"capture report scene.{name} must be from 1 to 3840"
                )
        for name in ("requested_height", "actual_height"):
            if not is_json_integer(scene.get(name), minimum=1, maximum=2160):
                errors.append(
                    f"capture report scene.{name} must be from 1 to 2160"
                )

    artifacts = value.get("artifacts")
    if not object_fields_are(artifacts, {"snapshot"}, {"snapshot"}):
        errors.append("capture report artifacts is malformed")
    else:
        assert isinstance(artifacts, dict)
        snapshot_fields = {"requested", "written", "path", "sha256", "error"}
        snapshot = artifacts.get("snapshot")
        if not object_fields_are(snapshot, snapshot_fields, snapshot_fields):
            errors.append("capture report artifacts.snapshot is malformed")
        else:
            assert isinstance(snapshot, dict)
            for name in ("requested", "written"):
                if not isinstance(snapshot.get(name), bool):
                    errors.append(
                        f"capture report artifacts.snapshot.{name} must be a boolean"
                    )
            for name in ("path", "error"):
                if not isinstance(snapshot.get(name), str):
                    errors.append(
                        f"capture report artifacts.snapshot.{name} must be a string"
                    )
            if not isinstance(snapshot.get("sha256"), str):
                errors.append(
                    "capture report artifacts.snapshot.sha256 must be a string"
                )
            if snapshot.get("written") is True and not is_sha256(
                snapshot.get("sha256")
            ):
                errors.append(
                    "capture report artifacts.snapshot.sha256 must be a digest"
                )
            if snapshot.get("written") is True and (
                snapshot.get("requested") is not True
                or not snapshot.get("path")
                or bool(snapshot.get("error"))
            ):
                errors.append(
                    "capture report written snapshot state is inconsistent"
                )
    errors.extend(capture_environment_errors(value.get("environment")))
    errors.extend(interaction_report_errors(value.get("interaction_report")))
    errors.extend(geometry_report_errors(value.get("geometry_report")))
    errors.extend(inspector_report_errors(value.get("quality_report"), value))
    return errors


def capture_report_check(report: Mapping[str, object]) -> dict[str, object]:
    errors = capture_report_errors(report)
    return check(
        "capture.contract",
        "incomplete" if errors else "pass",
        "Capture report is closed-schema and internally valid."
        if not errors
        else "Capture report is malformed or contains unsupported fields.",
        {"validation_errors": errors},
    )


def inspector_report_errors(
    value: object, capture_report: Mapping[str, object] | None = None
) -> list[str]:
    fields = {"schema_version", "tool", "root", "summary", "findings"}
    if not object_fields_are(value, fields, fields):
        return ["quality_report has missing or unsupported top-level fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("quality_report.schema_version must be 1")
    if value.get("tool") != "FluentQt Inspector":
        errors.append("quality_report.tool is unsupported")
    root = value.get("root")
    root_fields = {"class", "object_name", "width", "height"}
    if not object_fields_are(root, root_fields, root_fields):
        errors.append("quality_report.root is malformed")
    else:
        assert isinstance(root, dict)
        if not isinstance(root.get("class"), str) or not root.get("class"):
            errors.append("quality_report.root.class must be non-empty")
        if not isinstance(root.get("object_name"), str):
            errors.append("quality_report.root.object_name must be a string")
        for name in ("width", "height"):
            if not is_json_integer(root.get(name), minimum=0):
                errors.append(
                    f"quality_report.root.{name} must be a non-negative integer"
                )
        if capture_report is not None:
            expected_sizes = [
                (
                    nested(capture_report, "scene", "actual_width"),
                    nested(capture_report, "scene", "actual_height"),
                    "scene",
                ),
                (
                    nested(capture_report, "geometry_report", "root_size", "width"),
                    nested(capture_report, "geometry_report", "root_size", "height"),
                    "geometry_report",
                ),
            ]
            for width, height, source in expected_sizes:
                if (
                    is_json_integer(width, minimum=0)
                    and is_json_integer(height, minimum=0)
                    and (root.get("width"), root.get("height")) != (width, height)
                ):
                    errors.append(
                        f"quality_report.root size does not match {source}"
                    )
    findings = value.get("findings")
    if not isinstance(findings, list):
        return [*errors, "quality_report.findings must be an array"]
    valid_severities = {"info", "warning", "error"}
    valid_categories = {
        "text",
        "accessibility",
        "input",
        "focus",
        "layout",
        "actions",
        "scrolling",
    }
    finding_fields = {
        "code", "category", "severity", "path", "rect", "message", "details"
    }
    severity_counts: Counter[str] = Counter()
    category_counts: Counter[str] = Counter()
    for index, finding in enumerate(findings):
        context = f"quality_report.findings[{index}]"
        if not object_fields_are(finding, finding_fields, finding_fields):
            errors.append(f"{context} has missing or unsupported fields")
            continue
        assert isinstance(finding, dict)
        code = finding.get("code")
        if not isinstance(code, str) or re.fullmatch(
            r"[a-z]+(?:[.-][a-z]+)*", code
        ) is None:
            errors.append(f"{context}.code is malformed")
        category = finding.get("category")
        if category not in valid_categories:
            errors.append(f"{context}.category is unsupported")
        else:
            category_counts[str(category)] += 1
        severity = finding.get("severity")
        if severity not in valid_severities:
            errors.append(f"{context}.severity is unsupported")
        else:
            severity_counts[str(severity)] += 1
        if not isinstance(finding.get("path"), str) or not finding.get("path"):
            errors.append(f"{context}.path must be non-empty")
        errors.extend(rect_report_errors(finding.get("rect"), f"{context}.rect"))
        if not isinstance(finding.get("message"), str) or not finding.get("message"):
            errors.append(f"{context}.message must be non-empty")
        if not isinstance(finding.get("details"), dict):
            errors.append(f"{context}.details must be an object")
    summary = value.get("summary")
    summary_fields = {"findings", "by_severity", "by_category"}
    if not object_fields_are(summary, summary_fields, summary_fields):
        errors.append("quality_report.summary is malformed")
        return errors
    assert isinstance(summary, dict)
    if summary.get("findings") != len(findings):
        errors.append("quality_report.summary.findings does not match findings")
    by_severity = summary.get("by_severity")
    if not object_fields_are(by_severity, valid_severities, valid_severities):
        errors.append("quality_report.summary.by_severity is malformed")
    else:
        assert isinstance(by_severity, dict)
        for severity in sorted(valid_severities):
            if not is_json_integer(by_severity.get(severity), minimum=0):
                errors.append(
                    f"quality_report.summary.by_severity.{severity} is invalid"
                )
            elif by_severity.get(severity) != severity_counts[severity]:
                errors.append(
                    f"quality_report.summary.by_severity.{severity} does not match findings"
                )
    by_category = summary.get("by_category")
    if not isinstance(by_category, dict) or not all(
        category in valid_categories and is_json_integer(count, minimum=0)
        for category, count in by_category.items()
    ):
        errors.append("quality_report.summary.by_category is malformed")
    elif dict(category_counts) != by_category:
        errors.append("quality_report.summary.by_category does not match findings")
    return errors


def geometry_report_errors(value: object) -> list[str]:
    fields = {"schema_version", "tool", "root_size", "widget_count", "widgets"}
    if not object_fields_are(value, fields, fields):
        return ["geometry_report has missing or unsupported top-level fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("geometry_report.schema_version must be 1")
    if value.get("tool") != "FluentQt Named Widget Geometry":
        errors.append("geometry_report.tool is unsupported")
    errors.extend(size_report_errors(value.get("root_size"), "geometry_report.root_size"))
    widgets = value.get("widgets")
    if not isinstance(widgets, list):
        return [*errors, "geometry_report.widgets must be an array"]
    if value.get("widget_count") != len(widgets):
        errors.append("geometry_report.widget_count does not match widgets")
    widget_fields = {
        "path", "class", "object_name", "stable", "rect", "visible_rect",
        "minimum_size", "maximum_size", "size_hint", "enabled", "has_focus",
        "clipped", "layout_direction", "accessible_name",
    }
    for index, widget in enumerate(widgets):
        context = f"geometry_report.widgets[{index}]"
        if not object_fields_are(widget, widget_fields, widget_fields):
            errors.append(f"{context} has missing or unsupported fields")
            continue
        assert isinstance(widget, dict)
        for name in ("path", "class"):
            if not isinstance(widget.get(name), str) or not widget.get(name):
                errors.append(f"{context}.{name} must be non-empty")
        for name in ("object_name", "accessible_name"):
            if not isinstance(widget.get(name), str):
                errors.append(f"{context}.{name} must be a string")
        if widget.get("stable") is not bool(widget.get("object_name")):
            errors.append(f"{context}.stable does not match object_name")
        for name in ("enabled", "has_focus", "clipped"):
            if not isinstance(widget.get(name), bool):
                errors.append(f"{context}.{name} must be a boolean")
        if widget.get("layout_direction") not in {"ltr", "rtl"}:
            errors.append(f"{context}.layout_direction is unsupported")
        for name in ("rect", "visible_rect"):
            errors.extend(rect_report_errors(widget.get(name), f"{context}.{name}"))
        rect = widget.get("rect")
        visible_rect = widget.get("visible_rect")
        if isinstance(rect, dict) and isinstance(visible_rect, dict):
            derived_clipped = visible_rect != rect
            if widget.get("clipped") is not derived_clipped:
                errors.append(f"{context}.clipped does not match visible_rect")
            if all(
                is_json_integer(rect.get(name))
                and is_json_integer(visible_rect.get(name))
                for name in ("x", "y", "width", "height")
            ) and visible_rect.get("width", 0) > 0 and visible_rect.get("height", 0) > 0:
                rect_right = int(rect["x"]) + int(rect["width"])
                rect_bottom = int(rect["y"]) + int(rect["height"])
                visible_right = int(visible_rect["x"]) + int(visible_rect["width"])
                visible_bottom = int(visible_rect["y"]) + int(visible_rect["height"])
                root_size = value.get("root_size")
                root_width = int(root_size.get("width", 0)) if isinstance(root_size, dict) else 0
                root_height = int(root_size.get("height", 0)) if isinstance(root_size, dict) else 0
                if not (
                    int(rect["x"]) <= int(visible_rect["x"]) <= visible_right <= rect_right
                    and int(rect["y"]) <= int(visible_rect["y"]) <= visible_bottom <= rect_bottom
                    and 0 <= int(visible_rect["x"]) <= visible_right <= root_width
                    and 0 <= int(visible_rect["y"]) <= visible_bottom <= root_height
                ):
                    errors.append(
                        f"{context}.visible_rect is outside rect or root_size"
                    )
        for name in ("minimum_size", "maximum_size"):
            errors.extend(size_report_errors(widget.get(name), f"{context}.{name}"))
        errors.extend(
            size_report_errors(
                widget.get("size_hint"), f"{context}.size_hint", minimum=None
            )
        )
    return errors


def baseline_geometry_report_errors(value: object) -> list[str]:
    fields = {"schema_version", "tool", "root_size", "widget_count", "widgets"}
    if not object_fields_are(value, fields, fields):
        return ["baseline geometry_report has missing or unsupported fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("baseline geometry_report.schema_version must be 1")
    if value.get("tool") != "FluentQt Named Widget Geometry":
        errors.append("baseline geometry_report.tool is unsupported")
    errors.extend(
        size_report_errors(
            value.get("root_size"), "baseline geometry_report.root_size"
        )
    )
    widgets = value.get("widgets")
    if not isinstance(widgets, list):
        return [*errors, "baseline geometry_report.widgets must be an array"]
    if not is_json_integer(value.get("widget_count"), minimum=0):
        errors.append(
            "baseline geometry_report.widget_count must be a non-negative integer"
        )
    elif value.get("widget_count") != len(widgets):
        errors.append(
            "baseline geometry_report.widget_count does not match widgets"
        )
    widget_fields = {"object_name", "rect"}
    for index, widget in enumerate(widgets):
        context = f"baseline geometry_report.widgets[{index}]"
        if not object_fields_are(widget, widget_fields, widget_fields):
            errors.append(f"{context} has missing or unsupported fields")
            continue
        assert isinstance(widget, dict)
        if not is_trimmed_nonempty(widget.get("object_name")):
            errors.append(f"{context}.object_name must be non-empty")
        errors.extend(rect_report_errors(widget.get("rect"), f"{context}.rect"))
    return errors


def baseline_interaction_report_errors(value: object) -> list[str]:
    fields = {"schema_version", "requested", "status", "summary", "steps"}
    if not object_fields_are(value, fields, fields):
        return ["baseline interaction_report has missing or unsupported fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("baseline interaction_report.schema_version must be 1")
    if not isinstance(value.get("requested"), bool):
        errors.append("baseline interaction_report.requested must be a boolean")
    if value.get("status") not in {"pass", "not-requested"}:
        errors.append("baseline interaction_report.status is unsupported")
    summary_fields = {"total", "executed", "passed", "failed"}
    summary = value.get("summary")
    if not object_fields_are(summary, summary_fields, summary_fields):
        errors.append("baseline interaction_report.summary is malformed")
    elif any(
        not is_json_integer(summary.get(name), minimum=0)
        for name in summary_fields
    ):
        errors.append("baseline interaction_report.summary values are invalid")
    steps = value.get("steps")
    if not isinstance(steps, list):
        return [*errors, "baseline interaction_report.steps must be an array"]
    step_fields = {"index", "action", "status"}
    for index, step in enumerate(steps):
        context = f"baseline interaction_report.steps[{index}]"
        if not object_fields_are(step, step_fields, step_fields):
            errors.append(f"{context} has missing or unsupported fields")
            continue
        assert isinstance(step, dict)
        if not is_json_integer(step.get("index"), minimum=0):
            errors.append(f"{context}.index must be non-negative")
        elif step.get("index") != index:
            errors.append(f"{context}.index does not match its position")
        if step.get("action") not in ACTION_NAMES:
            errors.append(f"{context}.action is unsupported")
        if step.get("status") != "pass":
            errors.append(f"{context}.status must be pass")
    if isinstance(summary, dict) and all(
        is_json_integer(summary.get(name), minimum=0)
        for name in summary_fields
    ):
        if summary.get("total") != len(steps):
            errors.append(
                "baseline interaction_report.summary.total does not match steps"
            )
        if summary.get("executed") != len(steps):
            errors.append(
                "baseline interaction_report.summary.executed does not match steps"
            )
        if summary.get("passed") != len(steps) or summary.get("failed") != 0:
            errors.append(
                "baseline interaction_report.summary outcomes do not match steps"
            )
    if value.get("requested") is True and value.get("status") != "pass":
        errors.append("requested baseline interaction_report must pass")
    if value.get("requested") is False and (
        value.get("status") != "not-requested" or steps
    ):
        errors.append("not-requested baseline interaction_report is inconsistent")
    return errors


def baseline_quality_report_errors(value: object) -> list[str]:
    fields = {"schema_version", "tool", "summary"}
    if not object_fields_are(value, fields, fields):
        return ["baseline quality_report has missing or unsupported fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != 1:
        errors.append("baseline quality_report.schema_version must be 1")
    if value.get("tool") != "FluentQt Inspector":
        errors.append("baseline quality_report.tool is unsupported")
    summary = value.get("summary")
    summary_fields = {"findings", "by_severity", "by_category"}
    if not object_fields_are(summary, summary_fields, summary_fields):
        return [*errors, "baseline quality_report.summary is malformed"]
    assert isinstance(summary, dict)
    if not is_json_integer(summary.get("findings"), minimum=0):
        errors.append("baseline quality_report.summary.findings is invalid")
    severities = {"info", "warning", "error"}
    by_severity = summary.get("by_severity")
    if not object_fields_are(by_severity, severities, severities) or any(
        not is_json_integer(by_severity.get(name), minimum=0)
        for name in severities
    ):
        errors.append("baseline quality_report.summary.by_severity is malformed")
    valid_categories = {
        "text",
        "accessibility",
        "input",
        "focus",
        "layout",
        "actions",
        "scrolling",
    }
    by_category = summary.get("by_category")
    if not isinstance(by_category, dict) or not all(
        name in valid_categories and is_json_integer(count, minimum=0)
        for name, count in by_category.items()
    ):
        errors.append("baseline quality_report.summary.by_category is malformed")
    findings = summary.get("findings")
    if (
        is_json_integer(findings, minimum=0)
        and isinstance(by_severity, dict)
        and all(
            is_json_integer(by_severity.get(name), minimum=0)
            for name in severities
        )
        and sum(int(by_severity[name]) for name in severities) != findings
    ):
        errors.append(
            "baseline quality_report.summary.by_severity does not match findings"
        )
    if (
        is_json_integer(findings, minimum=0)
        and isinstance(by_category, dict)
        and all(
            name in valid_categories and is_json_integer(count, minimum=0)
            for name, count in by_category.items()
        )
        and sum(int(count) for count in by_category.values()) != findings
    ):
        errors.append(
            "baseline quality_report.summary.by_category does not match findings"
        )
    return errors


def baseline_report_errors(value: object) -> list[str]:
    fields = {
        "schema_version",
        "tool",
        "environment_sha256",
        "geometry_report",
        "interaction_report",
        "quality_report",
    }
    if not object_fields_are(value, fields, fields):
        return ["baseline report has missing or unsupported top-level fields"]
    assert isinstance(value, dict)
    errors: list[str] = []
    if value.get("schema_version") != BASELINE_SCHEMA_VERSION:
        errors.append("baseline report schema_version is unsupported")
    if value.get("tool") != "FluentQt GUI Baseline Report":
        errors.append("baseline report tool is unsupported")
    if not is_sha256(value.get("environment_sha256")):
        errors.append("baseline report environment_sha256 is malformed")
    errors.extend(
        baseline_geometry_report_errors(value.get("geometry_report"))
    )
    errors.extend(
        baseline_interaction_report_errors(value.get("interaction_report"))
    )
    errors.extend(baseline_quality_report_errors(value.get("quality_report")))
    return errors
