"""Shared limits, scalar checks, and result records for GUI verification."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Any, Mapping, Sequence
import math
import re


TOOL_SCHEMA_VERSION = 1
RECIPE_SCHEMA_VERSION = 1
BASELINE_SCHEMA_VERSION = 1
BASELINE_PROVENANCE_SCHEMA_VERSION = 1
REVIEW_SCHEMA_VERSION = 1
SAFE_ID = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
TAG = re.compile(r"^[a-z][a-z0-9_-]*:[a-z0-9][a-z0-9._-]*$")
SUPPORTED_TAG_PREFIXES = {
    "theme",
    "width",
    "state",
    "input",
    "direction",
    "platform",
}
NATIVE_PLUGINS = {"cocoa", "windows", "xcb", "wayland", "wayland-egl"}
MAX_GEOMETRY_TOLERANCE = 32
MAX_INSPECTOR_BUDGET = 100
MAX_CHANNEL_THRESHOLD = 64
MAX_DIFFERENT_RATIO = 0.10
MAX_DEVICE_PIXEL_RATIO = 16.0
MAX_PIXEL_COORDINATE = 32768
MAX_SETTLE_MS = 10_000
MAX_TIMEOUT_SECONDS = 300
MAX_JSON_INTEGER_DIGITS = 4096
MAX_JSON_NESTING_DEPTH = 128
ACTION_NAMES = {
    "focus",
    "click",
    "mouse_move",
    "mouse_press",
    "mouse_release",
    "mouse_leave",
    "key",
    "type_text",
    "set_property",
    "wait",
}
KEYBOARD_ACTIONS = {"key", "type_text"}
POINTER_ACTIONS = {
    "click",
    "mouse_move",
    "mouse_press",
    "mouse_release",
    "mouse_leave",
}
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


def is_sha256(value: object) -> bool:
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def is_trimmed_nonempty(value: object) -> bool:
    return isinstance(value, str) and bool(value) and value == value.strip()


def is_utc_timestamp(value: object) -> bool:
    if not is_trimmed_nonempty(value):
        return False
    try:
        parsed = datetime.fromisoformat(value)
    except ValueError:
        return False
    return parsed.tzinfo is not None and parsed.utcoffset() == timezone.utc.utcoffset(
        parsed
    )


def qt_round_positive(value: float) -> int | None:
    """Match qRound() for the non-negative geometry values used here."""

    try:
        numeric = float(value)
    except (OverflowError, TypeError, ValueError):
        return None
    if not math.isfinite(numeric) or numeric < 0:
        return None
    return math.floor(numeric + 0.5)


def qt_scale_positive(value: object, scale: object) -> int | None:
    try:
        scaled = float(value) * float(scale)
    except (OverflowError, TypeError, ValueError):
        return None
    return qt_round_positive(scaled)


def merged_dict(*values: object) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for value in values:
        if isinstance(value, dict):
            result.update(value)
    return result


def effective_require_native_desktop(
    defaults: Mapping[str, object], scenario: Mapping[str, object]
) -> bool:
    if "require_native_desktop" in scenario:
        value = scenario["require_native_desktop"]
    elif "require_native_desktop" in defaults:
        value = defaults["require_native_desktop"]
    else:
        value = True
    return value if isinstance(value, bool) else True


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


def canonical_system_name(value: object) -> str:
    raw = value.lower() if isinstance(value, str) else ""
    return {
        "darwin": "macos",
        "macos": "macos",
        "osx": "macos",
        "windows": "windows",
        "winnt": "windows",
        "linux": "linux",
    }.get(raw, raw)


def canonical_machine_name(value: object) -> str:
    raw = value.lower() if isinstance(value, str) else ""
    return {
        "aarch64": "arm64",
        "arm64": "arm64",
        "amd64": "x64",
        "x86_64": "x64",
        "x64": "x64",
    }.get(raw, raw)


def is_json_integer(
    value: object,
    *,
    minimum: int | None = None,
    maximum: int | None = None,
) -> bool:
    return (
        isinstance(value, int)
        and not isinstance(value, bool)
        and (minimum is None or value >= minimum)
        and (maximum is None or value <= maximum)
    )


def is_json_number(
    value: object,
    *,
    minimum: float | None = None,
    maximum: float | None = None,
) -> bool:
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        return False
    try:
        numeric = float(value)
    except (OverflowError, TypeError, ValueError):
        return False
    return (
        math.isfinite(numeric)
        and (minimum is None or numeric >= minimum)
        and (maximum is None or numeric <= maximum)
    )


def validated_device_pixel_ratio(value: object) -> float | None:
    if not is_json_number(
        value,
        minimum=0.000001,
        maximum=MAX_DEVICE_PIXEL_RATIO,
    ):
        return None
    try:
        return float(value)
    except (OverflowError, TypeError, ValueError):
        return None


def object_fields_are(
    value: object,
    required: set[str],
    allowed: set[str] | None = None,
) -> bool:
    return (
        isinstance(value, dict)
        and required <= set(value)
        and (allowed is None or set(value) <= allowed)
    )
