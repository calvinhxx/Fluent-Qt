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
import copy
from datetime import datetime, timezone
from html import escape
import hashlib
import json
import locale
import math
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from typing import Any, Iterable, Mapping, Sequence
import zlib


PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOL = Path(__file__).with_name("fluent_qt_build.py")
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


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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


def png_dimensions(path: Path) -> tuple[int, int] | None:
    try:
        payload = path.read_bytes()
    except OSError:
        return None
    if len(payload) < 8 or payload[:8] != b"\x89PNG\r\n\x1a\n":
        return None

    offset = 8
    ihdr: bytes | None = None
    idat_chunks: list[bytes] = []
    seen_plte = False
    seen_idat = False
    idat_closed = False
    seen_iend = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            return None
        length = struct.unpack(">I", payload[offset : offset + 4])[0]
        chunk_end = offset + 12 + length
        if chunk_end > len(payload):
            return None
        chunk_type = payload[offset + 4 : offset + 8]
        if (
            re.fullmatch(rb"[A-Za-z]{4}", chunk_type) is None
            or not 65 <= chunk_type[2] <= 90
        ):
            return None
        chunk_payload = payload[offset + 8 : offset + 8 + length]
        recorded_crc = struct.unpack(">I", payload[offset + 8 + length : chunk_end])[0]
        computed_crc = zlib.crc32(chunk_type)
        computed_crc = zlib.crc32(chunk_payload, computed_crc) & 0xFFFFFFFF
        if recorded_crc != computed_crc:
            return None
        if ihdr is None and chunk_type != b"IHDR":
            return None
        if chunk_type == b"IHDR":
            if ihdr is not None or length != 13 or offset != 8:
                return None
            ihdr = chunk_payload
        elif chunk_type == b"PLTE":
            if seen_idat or seen_plte or not length or length % 3 or length > 768:
                return None
            seen_plte = True
        elif chunk_type == b"IDAT":
            if ihdr is None or idat_closed or seen_iend:
                return None
            seen_idat = True
            idat_chunks.append(chunk_payload)
        elif chunk_type == b"IEND":
            if length or not seen_idat or seen_iend:
                return None
            seen_iend = True
            offset = chunk_end
            break
        else:
            if seen_idat:
                idat_closed = True
            if chunk_type and 65 <= chunk_type[0] <= 90:
                return None
        offset = chunk_end

    if ihdr is None or not seen_iend or offset != len(payload):
        return None
    width, height, bit_depth, color_type, compression, filtering, interlace = (
        struct.unpack(">IIBBBBB", ihdr)
    )
    valid_depths = {
        0: {1, 2, 4, 8, 16},
        2: {8, 16},
        3: {1, 2, 4, 8},
        4: {8, 16},
        6: {8, 16},
    }
    if (
        width <= 0
        or height <= 0
        or width > 32768
        or height > 32768
        or color_type not in valid_depths
        or bit_depth not in valid_depths[color_type]
        or compression != 0
        or filtering != 0
        or interlace not in {0, 1}
        or (color_type == 3 and not seen_plte)
        or (color_type in {0, 4} and seen_plte)
    ):
        return None

    samples = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]

    def row_size(row_width: int) -> int:
        return 1 + ((row_width * samples * bit_depth + 7) // 8)

    scanline_sizes: list[int] = []
    if interlace == 0:
        scanline_sizes = [row_size(width)] * height
    else:
        for start_x, start_y, step_x, step_y in (
            (0, 0, 8, 8),
            (4, 0, 8, 8),
            (0, 4, 4, 8),
            (2, 0, 4, 4),
            (0, 2, 2, 4),
            (1, 0, 2, 2),
            (0, 1, 1, 2),
        ):
            pass_width = max(0, (width - start_x + step_x - 1) // step_x)
            pass_height = max(0, (height - start_y + step_y - 1) // step_y)
            if pass_width:
                scanline_sizes.extend([row_size(pass_width)] * pass_height)
    expected_bytes = sum(scanline_sizes)
    if expected_bytes <= 0 or expected_bytes > 256 * 1024 * 1024:
        return None

    filter_offsets: list[int] = []
    scanline_offset = 0
    for size in scanline_sizes:
        filter_offsets.append(scanline_offset)
        scanline_offset += size
    filter_index = 0
    decoded_bytes = 0

    def consume(decoded: bytes) -> bool:
        nonlocal decoded_bytes, filter_index
        if decoded_bytes + len(decoded) > expected_bytes:
            return False
        end = decoded_bytes + len(decoded)
        while filter_index < len(filter_offsets) and filter_offsets[filter_index] < end:
            position = filter_offsets[filter_index] - decoded_bytes
            if position >= 0 and decoded[position] > 4:
                return False
            filter_index += 1
        decoded_bytes = end
        return True

    try:
        decoder = zlib.decompressobj()
        for compressed in idat_chunks:
            remaining = compressed
            while remaining:
                decoded = decoder.decompress(remaining, 1024 * 1024)
                remaining = decoder.unconsumed_tail
                if not consume(decoded):
                    return None
                if not decoded and not remaining:
                    break
        if not consume(decoder.flush()):
            return None
    except zlib.error:
        return None
    if (
        not decoder.eof
        or decoder.unused_data
        or decoded_bytes != expected_bytes
        or filter_index != len(filter_offsets)
    ):
        return None
    return width, height


def canonical_json(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise VerificationError(f"Could not read JSON {path}: {error}") from error
    except (ValueError, RecursionError) as error:
        raise VerificationError(f"Could not parse JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise VerificationError(f"JSON root must be an object: {path}")
    return value


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            temporary = Path(stream.name)
        temporary.replace(path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def resolved_path(raw: str | os.PathLike[str], base: Path) -> Path:
    path = Path(raw).expanduser()
    return (base / path).resolve() if not path.is_absolute() else path.resolve()


def path_is_within(path: Path, directory: Path) -> bool:
    try:
        path.resolve().relative_to(directory.resolve())
    except ValueError:
        return False
    return True


def paths_overlap(first: Path, second: Path) -> bool:
    return path_is_within(first, second) or path_is_within(second, first)


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
