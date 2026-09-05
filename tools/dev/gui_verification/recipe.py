"""Validate recipe fields, policies, and coverage before execution."""

from __future__ import annotations

from typing import Mapping
import re

from .common import (
    ACTION_NAMES,
    KEYBOARD_ACTIONS,
    MAX_CHANNEL_THRESHOLD,
    MAX_DIFFERENT_RATIO,
    MAX_GEOMETRY_TOLERANCE,
    MAX_INSPECTOR_BUDGET,
    MAX_PIXEL_COORDINATE,
    MAX_SETTLE_MS,
    MAX_TIMEOUT_SECONDS,
    POINTER_ACTIONS,
    RECIPE_SCHEMA_VERSION,
    SAFE_ID,
    SUPPORTED_TAG_PREFIXES,
    TAG,
    effective_require_native_desktop,
    is_trimmed_nonempty,
    merged_dict,
)


def validate_fields(
    value: Mapping[str, object],
    allowed: set[str],
    context: str,
    errors: list[str],
    required: set[str] | None = None,
) -> None:
    unknown = sorted(set(value) - allowed)
    missing = sorted((required or set()) - set(value))
    if unknown:
        errors.append(f"{context} has unsupported fields: {', '.join(unknown)}")
    if missing:
        errors.append(f"{context} is missing fields: {', '.join(missing)}")


def validate_inspector_policy(
    value: object, context: str, errors: list[str]
) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    validate_fields(
        value,
        {"max_findings", "max_by_severity", "allowed_codes"},
        context,
        errors,
    )
    maximum = value.get("max_findings")
    if "max_findings" in value and (
        not isinstance(maximum, int)
        or isinstance(maximum, bool)
        or not 0 <= maximum <= MAX_INSPECTOR_BUDGET
    ):
        errors.append(
            f"{context}.max_findings must be from 0 to {MAX_INSPECTOR_BUDGET}"
        )
    severity = value.get("max_by_severity")
    if "max_by_severity" in value:
        if not isinstance(severity, dict):
            errors.append(f"{context}.max_by_severity must be an object")
        else:
            validate_fields(
                severity, {"info", "warning", "error"},
                f"{context}.max_by_severity", errors
            )
            for name, budget in severity.items():
                if (
                    not isinstance(budget, int)
                    or isinstance(budget, bool)
                    or not 0 <= budget <= MAX_INSPECTOR_BUDGET
                ):
                    errors.append(
                        f"{context}.max_by_severity.{name} must be from 0 to "
                        f"{MAX_INSPECTOR_BUDGET}"
                    )
    allowed_codes = value.get("allowed_codes")
    if "allowed_codes" in value and (
        not isinstance(allowed_codes, list)
        or not all(isinstance(code, str) and code for code in allowed_codes)
        or len(set(allowed_codes)) != len(allowed_codes)
    ):
        errors.append(f"{context}.allowed_codes must be a unique string array")


def validate_size(value: object, context: str, errors: list[str]) -> None:
    if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]*x[1-9][0-9]*", value):
        errors.append(f"{context} must be WIDTHxHEIGHT")
        return
    width, height = (int(piece) for piece in value.split("x", 1))
    if not 320 <= width <= 3840 or not 240 <= height <= 2160:
        errors.append(f"{context} must be within 320x240 and 3840x2160")


def validate_runtime_options(
    value: Mapping[str, object], context: str, errors: list[str]
) -> None:
    settle_ms = value.get("settle_ms")
    if "settle_ms" in value and (
        not isinstance(settle_ms, int)
        or isinstance(settle_ms, bool)
        or not 0 <= settle_ms <= MAX_SETTLE_MS
    ):
        errors.append(f"{context}.settle_ms must be from 0 to {MAX_SETTLE_MS}")
    timeout = value.get("timeout_seconds")
    if "timeout_seconds" in value and (
        not isinstance(timeout, int)
        or isinstance(timeout, bool)
        or not 1 <= timeout <= MAX_TIMEOUT_SECONDS
    ):
        errors.append(
            f"{context}.timeout_seconds must be from 1 to {MAX_TIMEOUT_SECONDS}"
        )
    native = value.get("require_native_desktop")
    if "require_native_desktop" in value and not isinstance(native, bool):
        errors.append(f"{context}.require_native_desktop must be a boolean")


def validate_environment(value: object, context: str, errors: list[str]) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    for name, setting in value.items():
        if not isinstance(name, str) or not name:
            errors.append(f"{context} keys must be non-empty strings")
        if not isinstance(setting, (str, int, float, bool)):
            errors.append(
                f"{context}.{name} must be a string, number, or boolean"
            )


def validate_geometry_policy(
    value: object, context: str, errors: list[str], require_probes: bool
) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    validate_fields(value, {"required", "tolerance"}, context, errors)
    required = value.get("required")
    if (require_probes or "required" in value) and (
        not isinstance(required, list) or not required
    ):
        errors.append(f"{context}.required must be a non-empty array")
    if isinstance(required, list):
        probe_names: list[str] = []
        for index, raw in enumerate(required):
            if is_trimmed_nonempty(raw):
                probe_names.append(raw)
                continue
            if not isinstance(raw, dict) or not is_trimmed_nonempty(
                raw.get("object_name")
            ):
                errors.append(
                    f"{context}.required[{index}] must name one object_name"
                )
                continue
            probe_names.append(str(raw["object_name"]))
            validate_fields(
                raw,
                {
                    "object_name",
                    "tolerance",
                    "not_clipped",
                    "min_width",
                    "min_height",
                    "max_width",
                    "max_height",
                    "rect",
                },
                f"{context}.required[{index}]",
                errors,
                {"object_name"},
            )
            probe_tolerance = raw.get("tolerance")
            if "tolerance" in raw and (
                not isinstance(probe_tolerance, int)
                or isinstance(probe_tolerance, bool)
                or not 0 <= probe_tolerance <= MAX_GEOMETRY_TOLERANCE
            ):
                errors.append(
                    f"{context}.required[{index}].tolerance must be from 0 to "
                    f"{MAX_GEOMETRY_TOLERANCE}"
                )
            rect = raw.get("rect")
            if "rect" in raw and (
                not isinstance(rect, dict)
                or set(rect) != {"x", "y", "width", "height"}
                or not all(
                    isinstance(rect.get(key), int)
                    and not isinstance(rect.get(key), bool)
                    for key in rect
                )
                or int(rect.get("width", 0)) <= 0
                or int(rect.get("height", 0)) <= 0
            ):
                errors.append(
                    f"{context}.required[{index}].rect must contain integer x, y, width, and height"
                )
            not_clipped = raw.get("not_clipped")
            if "not_clipped" in raw and not isinstance(not_clipped, bool):
                errors.append(
                    f"{context}.required[{index}].not_clipped must be a boolean"
                )
            for dimension in (
                "min_width",
                "min_height",
                "max_width",
                "max_height",
            ):
                candidate = raw.get(dimension)
                if dimension in raw and (
                    not isinstance(candidate, int)
                    or isinstance(candidate, bool)
                    or candidate < 1
                ):
                    errors.append(
                        f"{context}.required[{index}].{dimension} must be a "
                        "positive integer"
                    )
            for minimum, maximum in (
                ("min_width", "max_width"),
                ("min_height", "max_height"),
            ):
                lower = raw.get(minimum)
                upper = raw.get(maximum)
                if (
                    isinstance(lower, int)
                    and not isinstance(lower, bool)
                    and isinstance(upper, int)
                    and not isinstance(upper, bool)
                    and lower > upper
                ):
                    errors.append(
                        f"{context}.required[{index}].{minimum} must not exceed "
                        f"{maximum}"
                    )
        if len(set(probe_names)) != len(probe_names):
            errors.append(
                f"{context}.required contains duplicate object_name probes"
            )
    tolerance = value.get("tolerance")
    if "tolerance" in value and (
        not isinstance(tolerance, int)
        or isinstance(tolerance, bool)
        or not 0 <= tolerance <= MAX_GEOMETRY_TOLERANCE
    ):
        errors.append(
            f"{context}.tolerance must be from 0 to {MAX_GEOMETRY_TOLERANCE}"
        )


def validate_pixel_policy(
    value: object,
    context: str,
    errors: list[str],
    *,
    allow_regions: bool = True,
) -> None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be an object")
        return
    allowed = {
        "channel_threshold",
        "max_different_pixels",
        "max_different_ratio",
        "search_radius",
        "max_translation",
        "edge_threshold",
    }
    if allow_regions:
        allowed.add("regions")
    validate_fields(value, allowed, context, errors)
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
            or isinstance(candidate, bool)
            or candidate < minimum
            or (maximum is not None and candidate > maximum)
        ):
            errors.append(f"{context}.{name} is outside its valid range")
    channel_threshold = value.get("channel_threshold")
    if isinstance(channel_threshold, int) and channel_threshold > MAX_CHANNEL_THRESHOLD:
        errors.append(
            f"{context}.channel_threshold must not exceed {MAX_CHANNEL_THRESHOLD}"
        )
    ratio = value.get("max_different_ratio")
    if "max_different_ratio" in value and (
        not isinstance(ratio, (int, float))
        or isinstance(ratio, bool)
        or not 0 <= ratio <= MAX_DIFFERENT_RATIO
    ):
        errors.append(
            f"{context}.max_different_ratio must be from 0 to "
            f"{MAX_DIFFERENT_RATIO}"
        )
    if "regions" not in value:
        return
    regions = value.get("regions")
    if not allow_regions:
        errors.append(f"{context}.regions is not allowed in a region override")
        return
    if not isinstance(regions, list):
        errors.append(f"{context}.regions must be an array")
        return
    region_ids: set[str] = set()
    for index, raw in enumerate(regions):
        if not isinstance(raw, dict):
            errors.append(f"{context}.regions[{index}] must be an object")
            continue
        validate_fields(
            raw,
            {"id", "rect", "coordinate_space", "policy"},
            f"{context}.regions[{index}]",
            errors,
            {"id", "rect"},
        )
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
            or not all(
                isinstance(item, int) and not isinstance(item, bool)
                for item in rect
            )
            or (isinstance(rect, list) and len(rect) == 4 and (rect[0] < 0 or rect[1] < 0 or rect[2] <= 0 or rect[3] <= 0))
        ):
            errors.append(
                f"{context}.regions[{index}].rect must be non-negative x,y and positive width,height"
            )
        elif any(item > MAX_PIXEL_COORDINATE for item in rect):
            errors.append(
                f"{context}.regions[{index}].rect values must not exceed "
                f"{MAX_PIXEL_COORDINATE}"
            )
        if raw.get("coordinate_space", "logical") not in {"logical", "device"}:
            errors.append(
                f"{context}.regions[{index}].coordinate_space must be logical or device"
            )
        if "policy" in raw:
            validate_pixel_policy(
                raw["policy"],
                f"{context}.regions[{index}].policy",
                errors,
                allow_regions=False,
            )
            region_policy = raw.get("policy")
            max_pixels = (
                region_policy.get("max_different_pixels")
                if isinstance(region_policy, dict)
                else None
            )
            if (
                isinstance(rect, list)
                and len(rect) == 4
                and all(
                    isinstance(item, int) and not isinstance(item, bool)
                    for item in rect
                )
                and isinstance(max_pixels, int)
                and not isinstance(max_pixels, bool)
                and max_pixels > (rect[2] * rect[3]) // 10
            ):
                errors.append(
                    f"{context}.regions[{index}].policy.max_different_pixels "
                    f"must not exceed {MAX_DIFFERENT_RATIO:.0%} of the region pixels"
                )


def validate_action_script(
    value: object, context: str, errors: list[str]
) -> dict[str, object] | None:
    if not isinstance(value, dict):
        errors.append(f"{context} must be a JSON object")
        return None
    validate_fields(
        value,
        {"schema_version", "stop_on_failure", "steps"},
        context,
        errors,
        {"schema_version", "steps"},
    )
    if value.get("schema_version") != 1:
        errors.append(f"{context}.schema_version must be 1")
    if "stop_on_failure" in value and not isinstance(
        value.get("stop_on_failure"), bool
    ):
        errors.append(f"{context}.stop_on_failure must be a boolean")
    steps = value.get("steps")
    if not isinstance(steps, list) or not steps:
        errors.append(f"{context}.steps must be a non-empty array")
        return value
    ids: set[str] = set()
    allowed_step_fields = {
        "id",
        "action",
        "target",
        "descendant_class",
        "position",
        "button",
        "modifiers",
        "key",
        "text",
        "property",
        "value",
        "milliseconds",
        "after_ms",
        "observe",
        "expect",
    }
    for index, step in enumerate(steps):
        step_context = f"{context}.steps[{index}]"
        if not isinstance(step, dict):
            errors.append(f"{step_context} must be an object")
            continue
        validate_fields(
            step, allowed_step_fields, step_context, errors, {"action"}
        )
        action = step.get("action")
        if action not in ACTION_NAMES:
            errors.append(f"{step_context}.action is unsupported")
        step_id = step.get("id")
        if "id" in step:
            if not isinstance(step_id, str) or not step_id:
                errors.append(f"{step_context}.id must be a non-empty string")
            elif step_id in ids:
                errors.append(f"{step_context}.id is duplicated")
            else:
                ids.add(step_id)
        for name in ("target", "descendant_class", "key", "property"):
            candidate = step.get(name)
            if name in step and (
                not isinstance(candidate, str) or not candidate
            ):
                errors.append(f"{step_context}.{name} must be a non-empty string")
        if "text" in step and not isinstance(step.get("text"), str):
            errors.append(f"{step_context}.text must be a string")
        if action == "type_text" and (
            not isinstance(step.get("text"), str) or not step.get("text")
        ):
            errors.append(f"{step_context}.type_text requires non-empty text")
        position = step.get("position")
        if "position" in step and (
            not isinstance(position, dict)
            or set(position) != {"x", "y"}
            or not all(
                isinstance(position.get(name), int)
                and not isinstance(position.get(name), bool)
                for name in ("x", "y")
            )
        ):
            errors.append(f"{step_context}.position must contain integer x and y")
        if "button" in step and step.get("button") not in {
            "left",
            "right",
            "middle",
        }:
            errors.append(f"{step_context}.button is unsupported")
        modifiers = step.get("modifiers")
        if "modifiers" in step and (
            not isinstance(modifiers, list)
            or len(set(modifiers)) != len(modifiers)
            or not all(
                modifier in {"shift", "control", "ctrl", "alt", "meta", "shortcut"}
                for modifier in modifiers
            )
        ):
            errors.append(f"{step_context}.modifiers is invalid")
        for name in ("milliseconds", "after_ms"):
            candidate = step.get(name)
            if name in step and (
                not isinstance(candidate, int)
                or isinstance(candidate, bool)
                or not 0 <= candidate <= MAX_SETTLE_MS
            ):
                errors.append(
                    f"{step_context}.{name} must be from 0 to {MAX_SETTLE_MS}"
                )
        observe = step.get("observe")
        if "observe" in step and (
            not isinstance(observe, list)
            or len(set(observe)) != len(observe)
            or not all(isinstance(item, str) and item for item in observe)
        ):
            errors.append(f"{step_context}.observe must be a unique string array")
        expectations = step.get("expect")
        if "expect" in step and (
            not isinstance(expectations, dict) or not expectations
        ):
            errors.append(f"{step_context}.expect must be a non-empty object")
    return value


def validate_scenario_semantics(
    scenario: Mapping[str, object],
    action_script: Mapping[str, object] | None,
    context: str,
    errors: list[str],
    *,
    external_actions_pending: bool = False,
    require_native_desktop: bool = True,
) -> None:
    tags = scenario.get("tags")
    if not isinstance(tags, list) or not all(isinstance(tag, str) for tag in tags):
        return
    if len(set(tags)) != len(tags):
        errors.append(f"{context}.tags must not contain duplicates")
    malformed = [tag for tag in tags if not TAG.fullmatch(tag)]
    if malformed:
        errors.append(f"{context}.tags contain malformed values: {', '.join(malformed)}")
    unsupported_prefixes = sorted(
        {
            tag.split(":", 1)[0]
            for tag in tags
            if ":" in tag and tag.split(":", 1)[0] not in SUPPORTED_TAG_PREFIXES
        }
    )
    if unsupported_prefixes:
        errors.append(
            f"{context}.tags use unsupported coverage categories: "
            + ", ".join(unsupported_prefixes)
        )
    theme_tags = [tag for tag in tags if tag.startswith("theme:")]
    expected_theme = f"theme:{scenario.get('theme')}"
    if theme_tags != [expected_theme]:
        errors.append(f"{context}.tags must contain exactly {expected_theme}")
    direction_tags = [tag for tag in tags if tag.startswith("direction:")]
    if direction_tags and direction_tags != [
        f"direction:{scenario.get('direction', 'ltr')}"
    ]:
        errors.append(f"{context}.direction tag does not match the scenario")
    width_tags = [tag for tag in tags if tag.startswith("width:")]
    if len(width_tags) != 1:
        errors.append(f"{context}.tags must contain exactly one width tag")
    else:
        size = scenario.get("size")
        if isinstance(size, str) and "x" in size:
            try:
                width = int(size.split("x", 1)[0])
            except ValueError:
                width = 0
            expected_width = (
                "width:narrow"
                if width <= 640
                else "width:normal"
                if width <= 1007
                else "width:wide"
            )
            if width_tags[0] != expected_width:
                errors.append(
                    f"{context}.{width_tags[0]} does not match viewport width {width}"
                )
    platform_tags = [tag for tag in tags if tag.startswith("platform:")]
    unsupported_platform_tags = sorted(
        set(platform_tags) - {"platform:desktop-qpa"}
    )
    if unsupported_platform_tags:
        errors.append(
            f"{context} has unsupported platform tags: "
            + ", ".join(unsupported_platform_tags)
        )
    if "platform:desktop-qpa" in tags and not require_native_desktop:
        errors.append(
            f"{context} platform:desktop-qpa cannot disable desktop QPA capture"
        )
    native_state_tags = sorted(tag for tag in tags if tag.startswith("state:native-"))
    if native_state_tags:
        errors.append(
            f"{context} cannot claim native state coverage from the Gallery runner: "
            + ", ".join(native_state_tags)
        )
    if external_actions_pending:
        return
    input_tags = [tag for tag in tags if tag.startswith("input:")]
    unknown_input_tags = sorted(set(input_tags) - {"input:keyboard", "input:mouse"})
    if unknown_input_tags:
        errors.append(
            f"{context} has unsupported input tags: {', '.join(unknown_input_tags)}"
        )
    state_tags = [tag for tag in tags if tag.startswith("state:")]
    supported_state_tags = {
        "state:default",
        "state:focus",
        "state:hover",
        "state:pressed",
    }
    unknown_state_tags = sorted(set(state_tags) - supported_state_tags)
    if unknown_state_tags:
        errors.append(
            f"{context} has unsupported state tags: "
            + ", ".join(unknown_state_tags)
        )
    if len(state_tags) != 1:
        errors.append(f"{context} must declare exactly one state tag")
    steps = (
        action_script.get("steps")
        if isinstance(action_script, Mapping)
        and isinstance(action_script.get("steps"), list)
        else []
    )
    actions = {
        step.get("action")
        for step in steps
        if isinstance(step, dict) and isinstance(step.get("action"), str)
    }
    has_assertion = any(
        isinstance(step, dict)
        and isinstance(step.get("expect"), dict)
        and bool(step.get("expect"))
        for step in steps
    )
    if input_tags and action_script is None:
        errors.append(f"{context} input coverage requires an action script")
    if "input:keyboard" in input_tags and not (actions & KEYBOARD_ACTIONS):
        errors.append(f"{context} keyboard coverage requires a key or type_text action")
    if "input:mouse" in input_tags and not (actions & POINTER_ACTIONS):
        errors.append(f"{context} mouse coverage requires a pointer action")
    if input_tags and not has_assertion:
        errors.append(f"{context} input coverage requires a non-empty expectation")
    non_default_states = [tag for tag in state_tags if tag != "state:default"]
    if non_default_states and action_script is None:
        errors.append(f"{context} non-default state coverage requires an action script")
    if non_default_states and not has_assertion:
        errors.append(
            f"{context} non-default state coverage requires a non-empty expectation"
        )
    if "state:focus" in state_tags and not (
        actions & (KEYBOARD_ACTIONS | {"focus"})
    ):
        errors.append(f"{context} focus state requires focus or keyboard input")
    final_step = steps[-1] if steps and isinstance(steps[-1], dict) else {}
    final_action = final_step.get("action")
    final_expectation = (
        final_step.get("expect")
        if isinstance(final_step.get("expect"), dict)
        else {}
    )
    if "state:focus" in state_tags and final_expectation.get("has_focus") is not True:
        errors.append(
            f"{context} focus state requires a final has_focus=true assertion"
        )
    if "state:hover" in state_tags and "mouse_move" not in actions:
        errors.append(f"{context} hover state requires mouse_move input")
    if "state:hover" in state_tags and (
        final_action != "mouse_move" or not final_expectation
    ):
        errors.append(
            f"{context} hover state must end with an asserted mouse_move action"
        )
    if "state:pressed" in state_tags and "mouse_press" not in actions:
        errors.append(f"{context} pressed state requires mouse_press input")
    if "state:pressed" in state_tags and (
        final_action != "mouse_press" or not final_expectation
    ):
        errors.append(
            f"{context} pressed state must end with an asserted mouse_press action"
        )


def validate_recipe(recipe: Mapping[str, object]) -> list[str]:
    errors: list[str] = []
    validate_fields(
        recipe,
        {
            "schema_version",
            "id",
            "path_base",
            "author",
            "selection",
            "coverage",
            "environment",
            "defaults",
            "scenarios",
        },
        "recipe",
        errors,
        {
            "schema_version",
            "id",
            "path_base",
            "author",
            "selection",
            "coverage",
            "defaults",
            "scenarios",
        },
    )
    if recipe.get("schema_version") != RECIPE_SCHEMA_VERSION:
        errors.append("schema_version must be 1")
    recipe_id = recipe.get("id")
    if not isinstance(recipe_id, str) or not SAFE_ID.fullmatch(recipe_id):
        errors.append("id must use lowercase letters, numbers, dot, underscore, or dash")
    if recipe.get("path_base") not in {"repository", "recipe"}:
        errors.append("path_base must be repository or recipe")
    author = recipe.get("author")
    if not isinstance(author, dict) or not is_trimmed_nonempty(author.get("id")):
        errors.append("author.id is required")
    elif author.get("kind") not in {"ai", "human"}:
        errors.append("author.kind must be ai or human")
    if isinstance(author, dict):
        validate_fields(
            author, {"id", "kind"}, "author", errors, {"id", "kind"}
        )
    selection = recipe.get("selection")
    if not isinstance(selection, dict) or not isinstance(selection.get("route"), str) or not selection.get("route"):
        errors.append("selection.route is required")
    if isinstance(selection, dict):
        validate_fields(
            selection, {"route", "sample"}, "selection", errors, {"route"}
        )
        if "sample" in selection and not is_trimmed_nonempty(
            selection.get("sample")
        ):
            errors.append("selection.sample must be a non-empty string")

    coverage = recipe.get("coverage")
    required_tags = coverage.get("required_tags") if isinstance(coverage, dict) else None
    if not isinstance(required_tags, list) or not required_tags or not all(
        isinstance(tag, str) and tag for tag in required_tags
    ):
        errors.append("coverage.required_tags must be a non-empty string array")
    elif len(set(required_tags)) != len(required_tags):
        errors.append("coverage.required_tags must not contain duplicates")
    elif any(not TAG.fullmatch(tag) for tag in required_tags):
        errors.append("coverage.required_tags contain malformed values")
    if isinstance(coverage, dict):
        validate_fields(
            coverage,
            {"required_tags"},
            "coverage",
            errors,
            {"required_tags"},
        )

    if "environment" in recipe:
        validate_environment(recipe.get("environment"), "environment", errors)

    defaults = recipe.get("defaults")
    defaults = defaults if isinstance(defaults, dict) else {}
    validate_fields(
        defaults,
        {
            "settle_ms",
            "timeout_seconds",
            "require_native_desktop",
            "inspector",
            "geometry",
            "pixel",
        },
        "defaults",
        errors,
        {"inspector", "geometry", "pixel"},
    )
    validate_runtime_options(defaults, "defaults", errors)
    validate_inspector_policy(defaults.get("inspector"), "defaults.inspector", errors)
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
        validate_fields(
            raw,
            {
                "id",
                "selection",
                "theme",
                "direction",
                "size",
                "tags",
                "baseline",
                "review",
                "actions",
                "environment",
                "settle_ms",
                "timeout_seconds",
                "require_native_desktop",
                "inspector",
                "geometry",
                "pixel",
            },
            context,
            errors,
            {"id", "theme", "size", "tags", "baseline", "review"},
        )
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
        scenario_selection = raw.get("selection")
        if "selection" in raw:
            if not isinstance(scenario_selection, dict):
                errors.append(f"{context}.selection must be an object")
            else:
                validate_fields(
                    scenario_selection,
                    {"route", "sample"},
                    f"{context}.selection",
                    errors,
                    {"route"},
                )
                if not is_trimmed_nonempty(scenario_selection.get("route")):
                    errors.append(
                        f"{context}.selection.route must be a non-empty string"
                    )
                if "sample" in scenario_selection and not is_trimmed_nonempty(
                    scenario_selection.get("sample")
                ):
                    errors.append(
                        f"{context}.selection.sample must be a non-empty string"
                    )
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
            is_trimmed_nonempty(item) for item in review
        ):
            errors.append(f"{context}.review must contain visual review prompts")
        actions = raw.get("actions")
        if "actions" in raw and not isinstance(actions, (str, dict)):
            errors.append(f"{context}.actions must be a path or JSON object")
        inline_actions = None
        if isinstance(actions, dict):
            inline_actions = validate_action_script(
                actions, f"{context}.actions", errors
            )
        validate_scenario_semantics(
            raw,
            inline_actions,
            context,
            errors,
            external_actions_pending=isinstance(actions, str),
            require_native_desktop=effective_require_native_desktop(
                defaults, raw
            ),
        )
        if "environment" in raw:
            validate_environment(
                raw.get("environment"), f"{context}.environment", errors
            )
        validate_runtime_options(raw, context, errors)
        if "inspector" in raw:
            validate_inspector_policy(
                raw.get("inspector"), f"{context}.inspector", errors
            )
        if "geometry" in raw:
            validate_geometry_policy(raw.get("geometry"), f"{context}.geometry", errors, False)
        if "pixel" in raw:
            validate_pixel_policy(raw.get("pixel"), f"{context}.pixel", errors)
        size = raw.get("size")
        if isinstance(size, str) and re.fullmatch(r"[1-9][0-9]*x[1-9][0-9]*", size):
            width, height = (int(piece) for piece in size.split("x", 1))
            pixel_policy = merged_dict(defaults.get("pixel"), raw.get("pixel"))
            regions = pixel_policy.get("regions")
            if isinstance(regions, list):
                for region_index, region in enumerate(regions):
                    if not isinstance(region, dict):
                        continue
                    rect = region.get("rect")
                    if (
                        region.get("coordinate_space", "logical") == "logical"
                        and isinstance(rect, list)
                        and len(rect) == 4
                        and all(
                            isinstance(item, int) and not isinstance(item, bool)
                            for item in rect
                        )
                        and (
                            rect[0] + rect[2] > width
                            or rect[1] + rect[3] > height
                        )
                    ):
                        errors.append(
                            f"{context}.pixel.regions[{region_index}] exceeds "
                            "the logical viewport"
                        )
            max_pixels = pixel_policy.get("max_different_pixels")
            if (
                isinstance(max_pixels, int)
                and not isinstance(max_pixels, bool)
                and max_pixels > (width * height) // 10
            ):
                errors.append(
                    f"{context}.pixel.max_different_pixels must not exceed "
                    f"{MAX_DIFFERENT_RATIO:.0%} of the logical viewport pixels"
                )
    if isinstance(required_tags, list):
        missing = sorted(set(required_tags) - covered)
        if missing:
            errors.append("coverage is missing required tags: " + ", ".join(missing))
    return errors
