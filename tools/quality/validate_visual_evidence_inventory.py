#!/usr/bin/env python3

"""Validate high-risk visual evidence coverage without granting visual approval."""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys


@dataclass(frozen=True)
class RiskFamilyRule:
    capabilities: frozenset[str]
    additions: frozenset[str]
    exclusions: frozenset[str]
    states: tuple[str, ...]
    owner: str
    rationale: str
    manual_reason: str


@dataclass(frozen=True)
class ValidationSummary:
    high_risk_components: int
    risk_families: int
    automated_evidence: int
    manual_visual_surfaces: int
    gallery_fallbacks: int
    representative_pixel_gates: int


RISK_FAMILY_RULES = {
    "transient-overlay": RiskFamilyRule(
        capabilities=frozenset({"transient-surfaces"}),
        additions=frozenset(
            {
                "auto-suggest-box",
                "calendar-date-picker",
                "color-picker",
                "combobox",
                "date-picker",
                "drawer-view",
                "dropdown-button",
                "multi-select-combobox",
                "split-button",
                "time-picker",
                "toast",
                "toggle-split-button",
                "tooltip",
            }
        ),
        exclusions=frozenset(),
        states=(
            "normal-light",
            "normal-dark",
            "open",
            "placement",
            "keyboard-focus",
            "dismissed",
            "narrow",
            "native-animation",
        ),
        owner="fluentqt-maintainers:transient-surfaces",
        rationale=(
            "Transient surfaces combine placement, focus, dismissal, stacking, "
            "and animation state that is easy to regress without changing an API."
        ),
        manual_reason=(
            "Native popup stacking, focus transfer, light-dismiss behavior, and "
            "animation polish require a real desktop review."
        ),
    ),
    "calendar-layout": RiskFamilyRule(
        capabilities=frozenset(),
        additions=frozenset({"calendar-view"}),
        exclusions=frozenset(),
        states=(
            "normal-light",
            "normal-dark",
            "selection",
            "keyboard-focus",
            "narrow",
            "rtl",
            "native-animation",
        ),
        owner="fluentqt-maintainers:date-time",
        rationale=(
            "Calendar geometry changes across month data, selection, locale, "
            "direction, and responsive width."
        ),
        manual_reason=(
            "Locale typography, focus visibility, and native animation timing "
            "remain human-reviewed desktop behavior."
        ),
    ),
    "command-layering": RiskFamilyRule(
        capabilities=frozenset({"command-surfaces"}),
        additions=frozenset(),
        exclusions=frozenset(),
        states=(
            "normal-light",
            "normal-dark",
            "open-layering",
            "keyboard-focus",
            "narrow",
            "rtl",
            "native-popup",
        ),
        owner="fluentqt-maintainers:command-surfaces",
        rationale=(
            "Commands redistribute into overflow layers whose order, clipping, "
            "focus, and popup z-order must remain stable."
        ),
        manual_reason=(
            "Native menu activation, popup stacking, access keys, and animation "
            "cannot be certified by injected Qt events alone."
        ),
    ),
    "collection-scroll": RiskFamilyRule(
        capabilities=frozenset({"collections", "scrolling-and-paging"}),
        additions=frozenset(),
        exclusions=frozenset(),
        states=(
            "normal-light",
            "normal-dark",
            "selection",
            "scroll-end",
            "narrow",
            "rtl",
            "native-input",
        ),
        owner="fluentqt-maintainers:collections-and-scrolling",
        rationale=(
            "Model/view controls combine viewport geometry, selection, scrolling, "
            "delegates, and direction-sensitive hit testing."
        ),
        manual_reason=(
            "Physical wheel, trackpad, touch, and drag behavior plus raster polish "
            "require human review on native desktops."
        ),
    ),
    "responsive-navigation": RiskFamilyRule(
        capabilities=frozenset({"navigation"}),
        additions=frozenset(),
        exclusions=frozenset(),
        states=(
            "normal-light",
            "normal-dark",
            "selection",
            "keyboard-focus",
            "narrow",
            "rtl",
        ),
        owner="fluentqt-maintainers:navigation",
        rationale=(
            "Navigation controls change structure and overflow behavior at "
            "responsive thresholds and under RTL layout."
        ),
        manual_reason=(
            "Focus traversal, transition polish, and platform input behavior "
            "remain explicit human-review evidence."
        ),
    ),
    "window-material": RiskFamilyRule(
        capabilities=frozenset({"window-and-icons"}),
        additions=frozenset(),
        exclusions=frozenset({"font-icon"}),
        states=(
            "normal-light",
            "normal-dark",
            "narrow",
            "native-window-management",
        ),
        owner="fluentqt-maintainers:windowing",
        rationale=(
            "Window chrome and materials depend on OS composition, activation, "
            "resizing, display configuration, and theme state."
        ),
        manual_reason=(
            "Mica or Acrylic composition, native title-bar behavior, multi-display "
            "movement, and activation must be reviewed on each target OS."
        ),
    ),
}

MANUAL_PLATFORMS = ["macos", "windows", "linux"]
MANUAL_PROCEDURE = "docs/development/visual-review.md"

EVIDENCE_STATUS_MODEL = [
    "surface-registered",
    "execution-observed",
    "artifacts-verified",
    "comparison-passed",
    "independent-review-required",
]

OPEN_GAPS = [
    {
        "id": "automated-evidence-ci-execution",
        "owner": "fluentqt-maintainers:test-governance",
        "status": "open",
        "summary": (
            "Some high-risk automated evidence is registered only for local test "
            "targets and is not selected by a reusable CI lane."
        ),
        "next_gate": (
            "Promote high-value registered-only evidence into measured CI lanes "
            "without hiding local-only execution behind an automated label."
        ),
    },
    {
        "id": "new-approved-high-risk-bundles",
        "owner": "fluentqt-maintainers:visual-governance",
        "status": "open",
        "summary": (
            "No tests/visual-baselines/gui scenario currently has a complete, "
            "digest-bound approval bundle."
        ),
        "next_gate": (
            "Add and independently review at least one approved high-risk GUI "
            "bundle before TD-3 can become Complete."
        ),
    },
    {
        "id": "legacy-root-baseline-metadata",
        "owner": "fluentqt-maintainers:visual-governance",
        "status": "open",
        "summary": (
            "The three root PNG baselines predate Qt-version, OS-version, display, "
            "and independent approval metadata."
        ),
        "next_gate": (
            "Keep them legacy-representative or migrate them into verified GUI "
            "bundles without silently upgrading their trust level."
        ),
    },
    {
        "id": "source-to-binary-provenance",
        "owner": "fluentqt-maintainers:visual-tooling",
        "status": "open",
        "summary": (
            "Evidence binds Gallery and comparator paths and digests, but a "
            "--no-build run has no reproducible source-to-binary attestation."
        ),
        "next_gate": (
            "Bind formal evidence to a build manifest and source revision or require "
            "a verified build step for release acceptance."
        ),
    },
    {
        "id": "runtime-specific-baseline-routing",
        "owner": "fluentqt-maintainers:visual-tooling",
        "status": "open",
        "summary": (
            "Approved GUI baseline lookup currently separates OS and architecture "
            "but not every Qt, DPR, font, compositor, or display variant."
        ),
        "next_gate": (
            "Introduce measured runtime partitions only when real platform evidence "
            "shows that one approved host profile is insufficient."
        ),
    },
    {
        "id": "external-review-identity-provenance",
        "owner": "fluentqt-maintainers:visual-governance",
        "status": "open",
        "summary": (
            "Approver and final reviewer identities are digest-bound declarations, "
            "not externally authenticated identities."
        ),
        "next_gate": (
            "Use a repository review, signed record, or trusted service identity when "
            "release policy requires stronger provenance than local evidence."
        ),
    },
]

APPROVAL_HOST = {
    "id": "macos-arm64-cocoa-fusion-1x-96dpi",
    "operating_system": "macos",
    "architecture": "arm64",
    "platform_plugin": "cocoa",
    "style": "Fusion",
    "qt_scale_factor": "1",
    "qt_font_dpi": "96",
    "font_source": "bundled",
    "execution": "local-desktop-only",
}

FUTURE_BUNDLE_POLICY = {
    "runner": "tools/dev/fluent_qt_gui_verify.py",
    "missing_baseline_status": "human-required",
    "passing_pixels_status": "review-required",
    "final_acceptance": "independent-review",
    "automatic_baseline_updates": False,
}

PLATFORM_BOUNDARY = {
    "automated_event_scope": "qt-injected-events",
    "native_state_prefix": "native-",
    "python_gallery_role": "authoring-and-parity-smoke-only",
    "wasm_gallery_role": "browser-delivery-smoke-only",
    "cross_platform_pixel_claim": "not-established",
}

LEGACY_PIXEL_GATES = {
    "button-dark-ltr": {
        "component_ids": ["button"],
        "test_case": "VisualGateTest.ButtonStatesDarkLtr",
        "baseline": (
            "tests/visual-baselines/"
            "test_visual_gate__VisualGateTest__ButtonStatesDarkLtr__"
            "button-states-dark-ltr.png"
        ),
        "sha256": "a706884ce67f07bb748069d1058c228441f454dd05950d48d3e307cfeaf4c463",
        "state_ids": [
            "normal-dark",
            "hover",
            "pressed",
            "keyboard-focus",
            "disabled",
        ],
    },
    "button-light-ltr": {
        "component_ids": ["button"],
        "test_case": "VisualGateTest.ButtonStatesLightLtr",
        "baseline": (
            "tests/visual-baselines/"
            "test_visual_gate__VisualGateTest__ButtonStatesLightLtr__"
            "button-states-light-ltr.png"
        ),
        "sha256": "858fd70ee79000fdf1eabdd103a326d381ba4dbdfe281517173047a8379911d8",
        "state_ids": [
            "normal-light",
            "hover",
            "pressed",
            "keyboard-focus",
            "disabled",
        ],
    },
    "tree-view-light-rtl": {
        "component_ids": ["tree-view"],
        "test_case": "VisualGateTest.TreeViewRtl",
        "baseline": (
            "tests/visual-baselines/"
            "test_visual_gate__VisualGateTest__TreeViewRtl__tree-view-rtl.png"
        ),
        "sha256": "6d21caca861c7bb99f60e85655dfe803f3599db5df9d766de69b35dbe963c1c6",
        "state_ids": ["normal-light", "rtl"],
    },
}

LEGACY_PIXEL_KNOWN_GAP = (
    "The representative root PNGs predate digest-bound independent approval "
    "metadata and do not bind a Qt version; new coverage must use approved GUI "
    "verification bundles."
)

AUTOMATED_KINDS = {"geometry", "interaction"}
GEOMETRY_ONLY_STATES = {"placement", "open-layering"}
TOP_LEVEL_FIELDS = {
    "schema_version",
    "phase_status",
    "source_catalog",
    "evidence_status_model",
    "risk_families",
    "standard_risk_components",
    "components",
    "approval_host",
    "future_bundle_policy",
    "platform_boundary",
    "representative_pixel_gates",
    "open_gaps",
}
TEST_RE = re.compile(
    r"\bTEST(?:_F|_P)?\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*\)"
)


def strip_cmake_comments(text: str) -> str:
    stripped: list[str] = []
    index = 0
    quoted = False
    while index < len(text):
        character = text[index]
        if quoted:
            stripped.append(character)
            if character == "\\" and index + 1 < len(text):
                stripped.append(text[index + 1])
                index += 2
                continue
            if character == '"':
                quoted = False
            index += 1
            continue
        if character == '"':
            quoted = True
            stripped.append(character)
            index += 1
            continue
        bracket = re.match(r"\[(=*)\[", text[index:])
        if bracket:
            closing = "]" + bracket.group(1) + "]"
            end = text.find(closing, index + len(bracket.group(0)))
            if end < 0:
                stripped.append(text[index:])
                break
            end += len(closing)
            stripped.append(text[index:end])
            index = end
            continue
        if character == "#":
            bracket_comment = re.match(r"#\[(=*)\[", text[index:])
            if bracket_comment:
                closing = "]" + bracket_comment.group(1) + "]"
                end = text.find(
                    closing, index + len(bracket_comment.group(0))
                )
                end = len(text) if end < 0 else end + len(closing)
            else:
                end = text.find("\n", index)
                end = len(text) if end < 0 else end
            stripped.append("\n" * text[index:end].count("\n"))
            index = end
            continue
        stripped.append(character)
        index += 1
    return "".join(stripped)


def cpp_contract_tokens(text: str) -> str:
    output: list[str] = []
    index = 0
    while index < len(text):
        if text.startswith("//", index):
            end = text.find("\n", index + 2)
            index = len(text) if end < 0 else end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
            continue
        if text.startswith('R"', index):
            opening = text.find("(", index + 2, index + 20)
            if opening >= 0:
                delimiter = text[index + 2 : opening]
                terminator = ")" + delimiter + '"'
                end = text.find(terminator, opening + 1)
                if end >= 0:
                    content = text[opening + 1 : end]
                    output.append(
                        "__SKIP_VISUAL_TEST_LITERAL__"
                        if content == "SKIP_VISUAL_TEST"
                        else "__STRING_LITERAL__"
                    )
                    index = end + len(terminator)
                    continue
        if text[index] in {'"', "'"}:
            quote = text[index]
            cursor = index + 1
            content: list[str] = []
            while cursor < len(text):
                if text[cursor] == "\\" and cursor + 1 < len(text):
                    content.extend(text[cursor : cursor + 2])
                    cursor += 2
                    continue
                if text[cursor] == quote:
                    break
                content.append(text[cursor])
                cursor += 1
            if quote == '"':
                output.append(
                    "__SKIP_VISUAL_TEST_LITERAL__"
                    if "".join(content) == "SKIP_VISUAL_TEST"
                    else "__STRING_LITERAL__"
                )
            else:
                output.append("__CHAR_LITERAL__")
            index = min(cursor + 1, len(text))
            continue
        output.append(text[index])
        index += 1
    return "".join(output)


def load_json(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise AssertionError(f"{path} must contain a JSON object")
    return value


def source_path_from_url(value: object) -> str:
    if not isinstance(value, str) or "/blob/main/" not in value:
        return ""
    return value.split("/blob/main/", 1)[1]


def resolve_repo_file(project_root: Path, relative_path: str) -> Path | None:
    relative = Path(relative_path)
    if not relative_path or relative.is_absolute() or ".." in relative.parts:
        return None
    root = project_root.resolve()
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError:
        return None
    return candidate if candidate.is_file() else None


def parse_test_cases(path: Path) -> dict[str, str]:
    text = cpp_contract_tokens(path.read_text(encoding="utf-8"))
    matches = list(TEST_RE.finditer(text))
    cases: dict[str, str] = {}
    for match in matches:
        name = f"{match.group(1)}.{match.group(2)}"
        if name in cases:
            raise AssertionError(f"duplicate test case in {path}: {name}")
        cases[name] = cpp_braced_body(text, match.end(), path, name)
    return cases


def cpp_braced_body(text: str, start: int, path: Path, name: str) -> str:
    opening = -1
    depth = 0
    index = start
    while index < len(text):
        if text.startswith("//", index):
            end = text.find("\n", index + 2)
            index = len(text) if end < 0 else end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
            continue
        if text.startswith('R"', index):
            raw_open = text.find("(", index + 2, index + 20)
            if raw_open >= 0:
                delimiter = text[index + 2 : raw_open]
                terminator = ")" + delimiter + '"'
                end = text.find(terminator, raw_open + 1)
                if end >= 0:
                    index = end + len(terminator)
                    continue
        if text[index] in {'"', "'"}:
            quote = text[index]
            index += 1
            while index < len(text):
                if text[index] == "\\" and index + 1 < len(text):
                    index += 2
                    continue
                if text[index] == quote:
                    index += 1
                    break
                index += 1
            continue
        if text[index] == "{":
            if opening < 0:
                opening = index
            depth += 1
        elif text[index] == "}" and opening >= 0:
            depth -= 1
            if depth == 0:
                return text[opening + 1 : index]
        index += 1
    raise AssertionError(f"could not parse test body {name} in {path}")


def cmake_list(path: Path, variable: str) -> list[str]:
    variables = cmake_variable_values(path)
    if variable not in variables:
        raise AssertionError(f"{path} is missing {variable}")
    return variables[variable]


def cmake_command_spans(text: str):
    """Yield simple CMake command spans with balanced parentheses."""
    cursor = 0
    pattern = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    while match := pattern.search(text, cursor):
        depth = 1
        quote = ""
        index = match.end()
        while index < len(text) and depth:
            character = text[index]
            if quote:
                if character == "\\" and index + 1 < len(text):
                    index += 2
                    continue
                if character == quote:
                    quote = ""
            elif character in {'"', "'"}:
                quote = character
            elif bracket := re.match(r"\[(=*)\[", text[index:]):
                closing = "]" + bracket.group(1) + "]"
                end = text.find(closing, index + len(bracket.group(0)))
                if end < 0:
                    index = len(text)
                    break
                index = end + len(closing)
                continue
            elif character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
            index += 1
        if depth:
            break
        yield (
            match.group(1).lower(),
            text[match.end() : index - 1],
            match.start(),
            index,
        )
        cursor = index


def literal_cmake_condition(arguments: str) -> bool | None:
    tokens = re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+', arguments)
    position = 0

    numeric_comparators = {
        "EQUAL",
        "LESS",
        "LESS_EQUAL",
        "GREATER",
        "GREATER_EQUAL",
    }
    string_comparators = {
        "STREQUAL",
        "STRLESS",
        "STRLESS_EQUAL",
        "STRGREATER",
        "STRGREATER_EQUAL",
    }
    version_comparators = {
        "VERSION_EQUAL",
        "VERSION_LESS",
        "VERSION_LESS_EQUAL",
        "VERSION_GREATER",
        "VERSION_GREATER_EQUAL",
    }
    comparators = numeric_comparators | string_comparators | version_comparators | {
        "MATCHES"
    }

    def literal_operand(token: str) -> str | None:
        if len(token) >= 2 and token[0] == token[-1] == '"':
            return token[1:-1]
        normalized = token.upper()
        if normalized in {
            "0",
            "1",
            "FALSE",
            "OFF",
            "NO",
            "N",
            "TRUE",
            "ON",
            "YES",
            "Y",
        } or re.fullmatch(r"[+-]?\d+(?:\.\d+)*", token):
            return token
        return None

    def comparison_value(left_token: str, operator: str, right_token: str) -> bool | None:
        left = literal_operand(left_token)
        right = literal_operand(right_token)
        if left is None or right is None:
            return None
        if operator in numeric_comparators:
            try:
                left_number = float(left)
                right_number = float(right)
            except (OverflowError, ValueError):
                return None
            if not math.isfinite(left_number) or not math.isfinite(right_number):
                return None
            return {
                "EQUAL": left_number == right_number,
                "LESS": left_number < right_number,
                "LESS_EQUAL": left_number <= right_number,
                "GREATER": left_number > right_number,
                "GREATER_EQUAL": left_number >= right_number,
            }[operator]
        if operator in string_comparators:
            return {
                "STREQUAL": left == right,
                "STRLESS": left < right,
                "STRLESS_EQUAL": left <= right,
                "STRGREATER": left > right,
                "STRGREATER_EQUAL": left >= right,
            }[operator]
        if operator in version_comparators:
            if not re.fullmatch(r"\d+(?:\.\d+)*", left) or not re.fullmatch(
                r"\d+(?:\.\d+)*", right
            ):
                return None
            left_version = tuple(int(part) for part in left.split("."))
            right_version = tuple(int(part) for part in right.split("."))
            width = max(len(left_version), len(right_version))
            left_version += (0,) * (width - len(left_version))
            right_version += (0,) * (width - len(right_version))
            return {
                "VERSION_EQUAL": left_version == right_version,
                "VERSION_LESS": left_version < right_version,
                "VERSION_LESS_EQUAL": left_version <= right_version,
                "VERSION_GREATER": left_version > right_version,
                "VERSION_GREATER_EQUAL": left_version >= right_version,
            }[operator]
        try:
            return re.search(right, left) is not None
        except re.error:
            return None

    def atom_value(token: str) -> bool | None:
        token = token.strip('"').upper()
        if token in {
            "",
            "0",
            "FALSE",
            "OFF",
            "NO",
            "N",
            "IGNORE",
            "NOTFOUND",
        } or token.endswith("-NOTFOUND"):
            return False
        if token in {"1", "TRUE", "ON", "YES", "Y"}:
            return True
        try:
            return float(token) != 0
        except ValueError:
            return None

    def parse_atom() -> bool | None:
        nonlocal position
        if position >= len(tokens):
            return None
        if tokens[position] == "(":
            position += 1
            value = parse_or()
            if position >= len(tokens) or tokens[position] != ")":
                return None
            position += 1
            return value
        token = tokens[position]
        position += 1
        if position + 1 <= len(tokens) - 1:
            operator = tokens[position].upper()
            if operator in comparators:
                right = tokens[position + 1]
                position += 2
                return comparison_value(token, operator, right)
        return atom_value(token)

    def parse_not() -> bool | None:
        nonlocal position
        if position < len(tokens) and tokens[position].upper() == "NOT":
            position += 1
            value = parse_not()
            return None if value is None else not value
        return parse_atom()

    def parse_and() -> bool | None:
        nonlocal position
        value = parse_not()
        while position < len(tokens) and tokens[position].upper() == "AND":
            position += 1
            right = parse_not()
            if value is False or right is False:
                value = False
            elif value is True and right is True:
                value = True
            else:
                value = None
        return value

    def parse_or() -> bool | None:
        nonlocal position
        value = parse_and()
        while position < len(tokens) and tokens[position].upper() == "OR":
            position += 1
            right = parse_and()
            if value is True or right is True:
                value = True
            elif value is False and right is False:
                value = False
            else:
                value = None
        return value

    result = parse_or()
    return result if position == len(tokens) else None


def cmake_reachable_text(text: str) -> str:
    """Keep commands reachable under literal CMake control flow.

    Unknown configuration conditions are treated as potentially true. Commands
    inside function or macro definitions are excluded because defining a helper
    does not execute its body.
    """
    output = ["\n" if character == "\n" else " " for character in text]
    condition_stack: list[dict[str, bool]] = []
    definition_depth = 0
    active = True
    for name, arguments, start, end in cmake_command_spans(text):
        if name in {"function", "macro"}:
            definition_depth += 1
            continue
        if name in {"endfunction", "endmacro"}:
            definition_depth = max(0, definition_depth - 1)
            continue
        if definition_depth:
            continue
        if name == "if":
            condition = literal_cmake_condition(arguments)
            condition_stack.append(
                {
                    "parent": active,
                    "remaining": active and condition is not True,
                    "closed": False,
                }
            )
            active = active and condition is not False
            continue
        if name == "elseif" and condition_stack:
            frame = condition_stack[-1]
            condition = literal_cmake_condition(arguments)
            if frame["closed"]:
                active = False
            else:
                remaining = frame["remaining"]
                active = remaining and condition is not False
                frame["remaining"] = remaining and condition is not True
            continue
        if name == "else" and condition_stack:
            frame = condition_stack[-1]
            active = frame["remaining"] and not frame["closed"]
            frame["remaining"] = False
            frame["closed"] = True
            continue
        if name == "endif" and condition_stack:
            frame = condition_stack.pop()
            active = frame["parent"]
            continue
        if active:
            output[start:end] = text[start:end]
    return "".join(output)


def cmake_tokens(arguments: str) -> list[str]:
    return [
        token[1:-1] if len(token) >= 2 and token[0] == token[-1] == '"' else token
        for token in re.findall(r'"(?:\\.|[^"\\])*"|[^\s]+', arguments)
    ]


def expanded_cmake_values(
    tokens: list[str], variables: dict[str, list[str]]
) -> list[str]:
    values: list[str] = []
    for token in tokens:
        reference = re.fullmatch(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", token)
        expanded = variables.get(reference.group(1), []) if reference else [token]
        for value in expanded:
            values.extend(piece for piece in value.split(";") if piece)
    return values


def cmake_variable_values(path: Path) -> dict[str, list[str]]:
    """Interpret the list operations used by top-level test governance."""
    text = cmake_reachable_text(
        strip_cmake_comments(path.read_text(encoding="utf-8"))
    )
    variables: dict[str, list[str]] = {}
    unreliable: set[str] = set()
    for name, arguments, _start, _end in cmake_command_spans(text):
        tokens = cmake_tokens(arguments)
        if name == "set" and tokens:
            variable = tokens[0]
            variables[variable] = expanded_cmake_values(tokens[1:], variables)
            unreliable.discard(variable)
        elif name == "unset" and tokens:
            variables[tokens[0]] = []
            unreliable.discard(tokens[0])
        elif name == "list" and len(tokens) >= 2:
            operation = tokens[0].upper()
            variable = tokens[1]
            values = expanded_cmake_values(tokens[2:], variables)
            current = variables.setdefault(variable, [])
            if operation == "APPEND":
                current.extend(values)
            elif operation == "PREPEND":
                variables[variable] = values + current
            elif operation == "REMOVE_ITEM":
                variables[variable] = [
                    value for value in current if value not in set(values)
                ]
            elif operation == "REMOVE_DUPLICATES":
                variables[variable] = list(dict.fromkeys(current))
            else:
                unreliable.add(variable)
    for variable in unreliable:
        variables[variable] = []
    return variables


def cmake_target_list(
    path: Path, variable: str, seen: set[str] | None = None
) -> list[str]:
    del seen
    return cmake_list(path, variable)


def reachable_test_cmake_files(
    project_root: Path, errors: list[str]
) -> list[Path]:
    root = (project_root / "tests/CMakeLists.txt").resolve()
    pending = [root]
    seen: set[Path] = set()
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        if not path.is_file():
            errors.append(f"reachable test CMake file is missing: {path}")
            continue
        seen.add(path)
        text = cmake_reachable_text(
            strip_cmake_comments(path.read_text(encoding="utf-8"))
        )
        for name, arguments, _start, _end in cmake_command_spans(text):
            if name != "add_subdirectory":
                continue
            tokens = cmake_tokens(arguments)
            if not tokens or "$" in tokens[0]:
                errors.append(
                    f"test add_subdirectory path must be static in {path}"
                )
                continue
            child = (path.parent / tokens[0] / "CMakeLists.txt").resolve()
            pending.append(child)
    return sorted(seen)


def registered_test_targets(
    project_root: Path, errors: list[str]
) -> dict[str, str]:
    result: dict[str, str] = {}
    tests_root = (project_root / "tests").resolve()
    for cmake_path in reachable_test_cmake_files(project_root, errors):
        text = cmake_reachable_text(
            strip_cmake_comments(cmake_path.read_text(encoding="utf-8"))
        )
        for name, arguments, _start, _end in cmake_command_spans(text):
            if name != "add_qt_test_module":
                continue
            tokens = cmake_tokens(arguments)
            if not tokens:
                continue
            target = tokens[0]
            for source in tokens[1:]:
                if not source.endswith((".cpp", ".cc", ".cxx")) or "$" in source:
                    continue
                path = (cmake_path.parent / source).resolve()
                try:
                    relative = path.relative_to(project_root.resolve()).as_posix()
                    path.relative_to(tests_root)
                except ValueError:
                    continue
                if not path.is_file():
                    continue
                previous = result.get(relative)
                if previous is not None and previous != target:
                    errors.append(
                        f"test source {relative} belongs to multiple targets: "
                        f"{previous}, {target}"
                    )
                result[relative] = target
    return result


def is_manual_visual_body(body: str) -> bool:
    tokens = cpp_contract_tokens(body)
    exits_when_skipped = re.search(
        r"if\s*\(\s*qEnvironmentVariableIsSet\s*\(\s*"
        r"__SKIP_VISUAL_TEST_LITERAL__\s*\)\s*\)\s*"
        r"(?:"
        r"\{[^{}]*(?:GTEST_SKIP\s*\(\s*\)|return(?:\s+[^;]*)?;)[^{}]*\}"
        r"|(?:GTEST_SKIP\s*\(\s*\)|return(?:\s+[^;]*)?;)"
        r")",
        tokens,
        re.DOTALL,
    )
    blocks_for_manual_close = re.search(
        r"(?:qApp\s*->\s*|QApplication\s*::\s*)exec\s*\(\s*\)", tokens
    )
    return (
        exits_when_skipped is not None
        and blocks_for_manual_close is not None
        and brace_depth_at(tokens, exits_when_skipped.start()) == 0
        and brace_depth_at(tokens, blocks_for_manual_close.start()) == 0
        and not has_top_level_termination(
            tokens, 0, exits_when_skipped.start()
        )
        and not has_top_level_termination(
            tokens,
            exits_when_skipped.end(),
            blocks_for_manual_close.start(),
            allow_headless_skip=True,
        )
        and not re.match(
            r"\s*else\b",
            tokens[exits_when_skipped.end() : blocks_for_manual_close.start()],
        )
        and not has_unconditional_braced_termination(
            tokens, exits_when_skipped.end(), blocks_for_manual_close.start()
        )
        and exits_when_skipped.end() < blocks_for_manual_close.start()
    )


def brace_depth_at(text: str, position: int) -> int:
    depth = 0
    for character in text[:position]:
        if character == "{":
            depth += 1
        elif character == "}":
            depth = max(0, depth - 1)
    return depth


def has_top_level_termination(
    text: str,
    start: int,
    end: int,
    *,
    allow_headless_skip: bool = False,
) -> bool:
    for match in re.finditer(
        r"\b(?:return|throw)\b|GTEST_SKIP\s*\(\s*\)", text[start:end]
    ):
        position = start + match.start()
        if brace_depth_at(text, position) != 0:
            continue
        if allow_headless_skip and match.group(0).startswith("GTEST_SKIP"):
            prefix = text[start:position]
            if re.search(
                r"if\s*\(\s*tests::support::isHeadlessPlatform\s*"
                r"\(\s*\)\s*\)\s*$",
                prefix,
            ):
                continue
        if brace_depth_at(text, position) == 0:
            return True
    return False


def matching_brace(text: str, opening: int, end: int) -> int | None:
    depth = 0
    for index in range(opening, end):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def has_unconditional_braced_termination(
    text: str, start: int, end: int
) -> bool:
    controls = re.compile(
        r"(?:\b(?:if|while)\s*\(\s*(?:true|1)\s*\)|\bdo)\s*\{"
    )
    for match in controls.finditer(text, start, end):
        if brace_depth_at(text, match.start()) != 0:
            continue
        opening = text.find("{", match.start(), match.end())
        closing = matching_brace(text, opening, end)
        if closing is None:
            return True
        for termination in re.finditer(
            r"\b(?:return|throw)\b|GTEST_SKIP\s*\(\s*\)",
            text[opening + 1 : closing],
        ):
            position = opening + 1 + termination.start()
            if brace_depth_at(text, position) == 1:
                return True
    return False


def all_test_cases(
    project_root: Path, errors: list[str]
) -> dict[str, tuple[str, str]]:
    cases: dict[str, tuple[str, str]] = {}
    for path in sorted((project_root / "tests").rglob("*.cpp")):
        relative_path = path.relative_to(project_root).as_posix()
        tokens = cpp_contract_tokens(path.read_text(encoding="utf-8"))
        for match in TEST_RE.finditer(tokens):
            if "\n" in match.group(0):
                errors.append(
                    "CTest gtest_add_tests cannot discover a multiline TEST "
                    f"declaration: {relative_path}:{match.group(1)}.{match.group(2)}"
                )
        for name, body in parse_test_cases(path).items():
            if name in cases:
                other_path = cases[name][0]
                raise AssertionError(
                    f"duplicate repository test case {name}: "
                    f"{other_path}, {relative_path}"
                )
            cases[name] = (relative_path, body)
    return cases


def validate_manual_visual_labels(
    project_root: Path,
    repository_cases: dict[str, tuple[str, str]],
    errors: list[str],
) -> set[str]:
    allowlist = cmake_list(
        project_root / "tests/CMakeLists.txt", "FLUENT_QT_MANUAL_VISUAL_TESTS"
    )
    duplicate_entries = sorted(
        name for name, count in Counter(allowlist).items() if count > 1
    )
    if duplicate_entries:
        errors.append(
            "duplicate FLUENT_QT_MANUAL_VISUAL_TESTS entries: "
            + ", ".join(duplicate_entries)
        )
    allowlist_set = set(allowlist)
    for name in sorted(allowlist_set):
        case = repository_cases.get(name)
        if case is None:
            errors.append(f"stale manual visual CMake entry: {name}")
        elif not is_manual_visual_body(case[1]):
            errors.append(f"manual visual CMake entry has no event-loop contract: {name}")

    manual_cases = {
        name
        for name, (_, body) in repository_cases.items()
        if is_manual_visual_body(body)
    }
    named_visual_cases = {
        name
        for name in repository_cases
        if "VisualCheck" in name.rsplit(".", 1)[-1]
    }
    for name in sorted(named_visual_cases - manual_cases):
        errors.append(
            f"VisualCheck-named test has no guarded event-loop contract: {name}"
        )
    for name in sorted(manual_cases):
        leaf_name = name.rsplit(".", 1)[-1]
        if "VisualCheck" not in leaf_name and name not in allowlist_set:
            errors.append(f"manual visual test is not labeled local-only: {name}")
    return {
        name
        for name in manual_cases
        if "VisualCheck" in name.rsplit(".", 1)[-1] or name in allowlist_set
    }


def catalog_components(
    catalog: dict[str, object], errors: list[str]
) -> dict[str, dict[str, object]]:
    components = catalog.get("components")
    if not isinstance(components, list):
        errors.append("visual evidence source catalog components must be an array")
        return {}
    result: dict[str, dict[str, object]] = {}
    for index, component in enumerate(components):
        if not isinstance(component, dict):
            errors.append(f"catalog components[{index}] must be an object")
            continue
        component_id = component.get("id")
        if not isinstance(component_id, str) or not component_id:
            errors.append(f"catalog components[{index}].id must be a string")
            continue
        if component_id in result:
            errors.append(f"duplicate source catalog component: {component_id}")
            continue
        result[component_id] = component
    return result


def expected_family_members(
    components: dict[str, dict[str, object]],
    rules: dict[str, RiskFamilyRule],
    errors: list[str],
) -> dict[str, set[str]]:
    result: dict[str, set[str]] = {}
    owner_by_component: dict[str, str] = {}
    for family_id, rule in rules.items():
        missing_additions = sorted(rule.additions - set(components))
        missing_exclusions = sorted(rule.exclusions - set(components))
        if missing_additions or missing_exclusions:
            missing = missing_additions + missing_exclusions
            errors.append(
                f"{family_id} risk rule references unknown components: "
                + ", ".join(missing)
            )
        members = set(rule.additions)
        for component_id, component in components.items():
            capabilities = component.get("capabilities", [])
            if not isinstance(capabilities, list) or not all(
                isinstance(capability, str) for capability in capabilities
            ):
                errors.append(f"{component_id} has invalid catalog capabilities")
                continue
            if rule.capabilities & set(capabilities):
                members.add(component_id)
        members -= set(rule.exclusions)
        for component_id in sorted(members):
            previous = owner_by_component.get(component_id)
            if previous is not None:
                errors.append(
                    f"high-risk component {component_id} belongs to multiple families: "
                    f"{previous}, {family_id}"
                )
            owner_by_component[component_id] = family_id
        result[family_id] = members
    return result


def component_test_cases(
    project_root: Path,
    component_id: str,
    component: dict[str, object],
    errors: list[str],
) -> dict[str, str]:
    tests = component.get("tests")
    if not isinstance(tests, list) or not tests:
        errors.append(f"{component_id} has no focused tests in the source catalog")
        return {}
    result: dict[str, str] = {}
    for index, test in enumerate(tests):
        if not isinstance(test, dict):
            errors.append(f"{component_id}.tests[{index}] must be an object")
            continue
        relative_path = source_path_from_url(test.get("source_url"))
        path = resolve_repo_file(project_root, relative_path)
        if path is None:
            errors.append(
                f"{component_id} focused test source does not exist: {relative_path}"
            )
            continue
        for test_case in parse_test_cases(path):
            previous = result.get(test_case)
            if previous is not None and previous != relative_path:
                errors.append(
                    f"{component_id} test case {test_case} is ambiguous: "
                    f"{previous}, {relative_path}"
                )
            result[test_case] = relative_path
    return result


def validate_family_inventory(
    inventory: dict[str, object],
    expected_members: dict[str, set[str]],
    rules: dict[str, RiskFamilyRule],
    errors: list[str],
) -> None:
    entries = inventory.get("risk_families")
    if not isinstance(entries, list):
        errors.append("visual evidence risk_families must be an array")
        return
    expected_ids = list(rules)
    actual_ids = [entry.get("id") if isinstance(entry, dict) else None for entry in entries]
    if actual_ids != expected_ids:
        errors.append(
            "visual evidence risk family order/content must be: "
            + ", ".join(expected_ids)
        )
        return
    for entry in entries:
        assert isinstance(entry, dict)
        family_id = str(entry["id"])
        unknown = set(entry) - {
            "id",
            "severity",
            "owner",
            "rationale",
            "components",
            "required_states",
            "manual_contract",
        }
        if unknown:
            errors.append(
                f"{family_id} has unsupported fields: {', '.join(sorted(unknown))}"
            )
        rule = rules[family_id]
        if entry.get("severity") != "high":
            errors.append(f"{family_id} severity must remain high")
        if entry.get("owner") != rule.owner:
            errors.append(f"{family_id} owner does not match its risk rule")
        if entry.get("rationale") != rule.rationale:
            errors.append(f"{family_id} rationale does not match its risk rule")
        components = entry.get("components")
        expected_components = sorted(expected_members[family_id])
        if components != expected_components:
            errors.append(f"{family_id} component coverage does not match its risk rule")
        required_states = entry.get("required_states")
        if required_states != list(rule.states):
            errors.append(f"{family_id} required state profile is incomplete or reordered")
        manual_contract = entry.get("manual_contract")
        expected_manual_contract = {
            "status": "human-required",
            "platforms": MANUAL_PLATFORMS,
            "procedure": MANUAL_PROCEDURE,
            "reason": rule.manual_reason,
        }
        if manual_contract != expected_manual_contract:
            errors.append(f"{family_id} manual review contract has drifted")


def validate_catalog_partition(
    inventory: dict[str, object],
    catalog: dict[str, dict[str, object]],
    expected_members: dict[str, set[str]],
    errors: list[str],
) -> None:
    high_risk = set().union(*expected_members.values()) if expected_members else set()
    expected_standard = sorted(set(catalog) - high_risk)
    entry = inventory.get("standard_risk_components")
    expected_entry = {
        "status": "catalog-tracked",
        "reason": (
            "No TD-3 high-risk family rule matched; these components remain under "
            "focused test, accessibility, API, and manual VisualCheck policy."
        ),
        "component_ids": expected_standard,
    }
    if entry != expected_entry:
        errors.append(
            "standard_risk_components must be the exact canonical catalog "
            "complement of the TD-3 high-risk families"
        )
    covered = high_risk | set(expected_standard)
    if covered != set(catalog):
        errors.append("visual evidence risk partition does not cover the catalog")


def validate_governance_contract(
    project_root: Path, inventory: dict[str, object], errors: list[str]
) -> None:
    if inventory.get("phase_status") != "active":
        errors.append("TD-3 phase_status must remain active while open gaps exist")
    if inventory.get("evidence_status_model") != EVIDENCE_STATUS_MODEL:
        errors.append("visual evidence status model has drifted")
    if inventory.get("open_gaps") != OPEN_GAPS:
        errors.append("visual evidence open gaps are incomplete, reordered, or altered")
    roadmap_path = project_root / "docs/development/technical-debt-roadmap.md"
    try:
        roadmap_text = roadmap_path.read_text(encoding="utf-8")
    except OSError as error:
        errors.append(f"unable to read technical debt roadmap: {error}")
        return
    rows: list[list[str]] = []
    for line in roadmap_text.splitlines():
        if re.match(r"^\|\s*TD-3\s+—", line):
            rows.append(
                [cell.strip() for cell in line.strip().strip("|").split("|")]
            )
    if len(rows) != 1 or len(rows[0]) < 2:
        errors.append("technical debt roadmap must contain exactly one TD-3 row")
        return
    expected_state = {
        "active": "Active",
        "complete": "Complete",
    }.get(str(inventory.get("phase_status")))
    if rows[0][1] != expected_state:
        errors.append(
            "technical debt roadmap TD-3 state does not match visual inventory "
            f"phase_status ({rows[0][1]} != {expected_state})"
        )


def validate_component_inventory(
    project_root: Path,
    inventory: dict[str, object],
    catalog: dict[str, dict[str, object]],
    expected_members: dict[str, set[str]],
    rules: dict[str, RiskFamilyRule],
    manual_cases: set[str],
    target_by_source: dict[str, str],
    ci_targets: set[str],
    local_desktop_cases: set[str],
    errors: list[str],
) -> tuple[int, int, int]:
    entries = inventory.get("components")
    if not isinstance(entries, list):
        errors.append("visual evidence components must be an array")
        return 0, 0, 0
    expected_owner = {
        component_id: family_id
        for family_id, members in expected_members.items()
        for component_id in members
    }
    ids = [entry.get("id") if isinstance(entry, dict) else None for entry in entries]
    if ids != sorted(expected_owner):
        errors.append("visual evidence component inventory must match sorted high-risk ids")

    automated_count = 0
    visual_surface_count = 0
    gallery_count = 0
    seen: set[str] = set()
    for index, entry in enumerate(entries):
        prefix = f"components[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{prefix} must be an object")
            continue
        unknown = set(entry) - {
            "id",
            "family",
            "automated_evidence",
            "manual_evidence",
        }
        missing = {
            "id",
            "family",
            "automated_evidence",
            "manual_evidence",
        } - set(entry)
        if unknown:
            errors.append(f"{prefix} has unsupported fields: {', '.join(sorted(unknown))}")
        if missing:
            errors.append(f"{prefix} is missing fields: {', '.join(sorted(missing))}")
            continue
        component_id = entry["id"]
        if not isinstance(component_id, str) or component_id not in expected_owner:
            errors.append(f"{prefix}.id is not a high-risk component")
            continue
        if component_id in seen:
            errors.append(f"duplicate visual evidence component: {component_id}")
            continue
        seen.add(component_id)
        family_id = entry["family"]
        if family_id != expected_owner[component_id]:
            errors.append(f"{component_id} is assigned to the wrong risk family")
            continue
        required_states = rules[str(family_id)].states
        required_state_set = set(required_states)
        test_cases = component_test_cases(
            project_root, component_id, catalog[component_id], errors
        )

        automated = entry["automated_evidence"]
        if not isinstance(automated, list) or not automated:
            errors.append(f"{component_id} must list automated evidence")
        else:
            for evidence_index, evidence in enumerate(automated):
                evidence_prefix = (
                    f"{component_id}.automated_evidence[{evidence_index}]"
                )
                if not isinstance(evidence, dict):
                    errors.append(f"{evidence_prefix} must be an object")
                    continue
                if set(evidence) != {
                    "kind",
                    "test_case",
                    "states",
                    "execution",
                }:
                    errors.append(f"{evidence_prefix} has an invalid field set")
                    continue
                kind = evidence["kind"]
                test_case = evidence["test_case"]
                states = evidence["states"]
                if kind not in AUTOMATED_KINDS:
                    errors.append(f"{evidence_prefix} has an invalid evidence kind")
                if not isinstance(test_case, str) or test_case not in test_cases:
                    errors.append(f"{evidence_prefix} references a missing test case")
                elif test_case in manual_cases:
                    errors.append(f"{evidence_prefix} cannot use a manual visual test")
                else:
                    source_path = test_cases[test_case]
                    target = target_by_source.get(source_path)
                    if target is None:
                        errors.append(
                            f"{evidence_prefix} source is not registered in a test target"
                        )
                    test_name = test_case.rsplit(".", 1)[-1]
                    contract_lane = (
                        re.search(r"(^|[._/:-])Contract_", test_case) is not None
                        and not test_name.startswith("DISABLED_")
                        and not test_case.startswith("DISABLED_")
                    )
                    excluded_from_ci = (
                        test_case in local_desktop_cases
                        or re.search(r"(^|[._/:-])VisualGate", test_case)
                        is not None
                        or test_name.startswith("DISABLED_")
                        or test_case.startswith("DISABLED_")
                    )
                    expected_execution = (
                        "ci"
                        if not excluded_from_ci
                        and (target in ci_targets or contract_lane)
                        else "registered-only"
                    )
                    if evidence.get("execution") != expected_execution:
                        errors.append(
                            f"{evidence_prefix}.execution must be "
                            f"{expected_execution} for target {target}"
                        )
                if not isinstance(states, list) or not states or not all(
                    isinstance(state, str) for state in states
                ):
                    errors.append(f"{evidence_prefix}.states must be a string array")
                else:
                    state_set = set(states)
                    if len(state_set) != len(states):
                        errors.append(f"{evidence_prefix}.states contains duplicates")
                    if state_set - required_state_set:
                        errors.append(f"{evidence_prefix}.states contains unknown states")
                    expected_order = [
                        state for state in required_states if state in state_set
                    ]
                    if states != expected_order:
                        errors.append(f"{evidence_prefix}.states must follow profile order")
                    if any(state.startswith("native-") for state in states):
                        errors.append(
                            f"{evidence_prefix} cannot claim native-platform states"
                        )
                    geometry_only = sorted(state_set & GEOMETRY_ONLY_STATES)
                    if geometry_only and kind != "geometry":
                        errors.append(
                            f"{evidence_prefix} states require geometry evidence: "
                            + ", ".join(geometry_only)
                        )
                automated_count += 1

        manual = entry["manual_evidence"]
        if not isinstance(manual, dict):
            errors.append(f"{component_id}.manual_evidence must be an object")
            continue
        if manual.get("status") != "manual-required":
            errors.append(f"{component_id} manual evidence must remain manual-required")
        if manual.get("coverage") != "all-required-states":
            errors.append(f"{component_id} manual evidence must cover all required states")
        available_manual = sorted(set(test_cases) & manual_cases)
        surface = manual.get("surface")
        if surface == "visual-check":
            if set(manual) != {"status", "coverage", "surface", "test_case"}:
                errors.append(f"{component_id} visual-check evidence has invalid fields")
            test_case = manual.get("test_case")
            if test_case not in available_manual:
                errors.append(f"{component_id} manual visual test case is missing")
            elif target_by_source.get(test_cases[str(test_case)]) is None:
                errors.append(
                    f"{component_id} manual visual test source is not registered "
                    "in a test target"
                )
            visual_surface_count += 1
        elif surface == "gallery":
            if set(manual) != {"status", "coverage", "surface"}:
                errors.append(f"{component_id} gallery evidence has invalid fields")
            if available_manual:
                errors.append(
                    f"{component_id} cannot downgrade an existing VisualCheck to Gallery"
                )
            gallery = catalog[component_id].get("gallery")
            if not isinstance(gallery, dict) or gallery.get("route_id") != component_id:
                errors.append(f"{component_id} has no matching Gallery route")
            else:
                sample_source = source_path_from_url(gallery.get("sample_source_url"))
                if resolve_repo_file(project_root, sample_source) is None:
                    errors.append(
                        f"{component_id} Gallery sample source does not exist: "
                        f"{sample_source}"
                    )
            gallery_count += 1
        else:
            errors.append(f"{component_id} has an invalid manual evidence surface")

    return automated_count, visual_surface_count, gallery_count


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int] | None:
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    if header[12:16] != b"IHDR" or struct.unpack(">I", header[8:12])[0] != 13:
        return None
    width, height = struct.unpack(">II", header[16:24])
    if width <= 0 or height <= 0:
        return None
    return width, height


def validate_representative_pixel_gates(
    project_root: Path,
    inventory: dict[str, object],
    catalog: dict[str, dict[str, object]],
    repository_cases: dict[str, tuple[str, str]],
    expected_gates: dict[str, dict[str, object]],
    errors: list[str],
) -> int:
    if inventory.get("approval_host") != APPROVAL_HOST:
        errors.append("visual evidence approval_host contract has drifted")
    if inventory.get("future_bundle_policy") != FUTURE_BUNDLE_POLICY:
        errors.append("visual evidence future_bundle_policy has drifted")
    if inventory.get("platform_boundary") != PLATFORM_BOUNDARY:
        errors.append("visual evidence platform_boundary has drifted")

    entries = inventory.get("representative_pixel_gates")
    if not isinstance(entries, list):
        errors.append("representative_pixel_gates must be an array")
        return 0
    ids = [entry.get("id") if isinstance(entry, dict) else None for entry in entries]
    if ids != sorted(expected_gates):
        errors.append("representative pixel gate inventory is incomplete or reordered")
        return len(entries)
    for entry in entries:
        assert isinstance(entry, dict)
        gate_id = str(entry["id"])
        expected = expected_gates[gate_id]
        required_fields = {
            "id",
            "kind",
            "component_ids",
            "test_case",
            "baseline",
            "sha256",
            "state_ids",
            "execution",
            "known_gap",
        }
        if set(entry) != required_fields:
            errors.append(f"{gate_id} representative pixel gate has invalid fields")
            continue
        if entry.get("kind") != "legacy-representative":
            errors.append(f"{gate_id} must remain classified as legacy-representative")
        if entry.get("execution") != "approval-host-only":
            errors.append(f"{gate_id} must remain approval-host-only")
        if entry.get("known_gap") != LEGACY_PIXEL_KNOWN_GAP:
            errors.append(f"{gate_id} must retain its legacy approval limitation")
        for field in (
            "component_ids",
            "test_case",
            "baseline",
            "sha256",
            "state_ids",
        ):
            if entry.get(field) != expected[field]:
                errors.append(f"{gate_id} {field} does not match checked-in evidence")
        component_ids = entry.get("component_ids", [])
        if isinstance(component_ids, list):
            unknown_components = sorted(set(component_ids) - set(catalog))
            if unknown_components:
                errors.append(
                    f"{gate_id} references unknown components: "
                    + ", ".join(unknown_components)
                )
        test_case = entry.get("test_case")
        source = repository_cases.get(str(test_case))
        if source is None or source[0] != "tests/components/TestVisualGate.cpp":
            errors.append(f"{gate_id} test case is missing from TestVisualGate.cpp")
        baseline = resolve_repo_file(project_root, str(entry.get("baseline", "")))
        if baseline is None or baseline.stat().st_size <= 0:
            errors.append(f"{gate_id} baseline is missing or empty")
        else:
            if png_dimensions(baseline) is None:
                errors.append(f"{gate_id} baseline is not a decodable PNG header")
            if sha256_file(baseline) != entry.get("sha256"):
                errors.append(f"{gate_id} baseline hash does not match the inventory")
    return len(entries)


def validate(
    project_root: Path,
    *,
    rules: dict[str, RiskFamilyRule] = RISK_FAMILY_RULES,
    expected_pixel_gates: dict[str, dict[str, object]] = LEGACY_PIXEL_GATES,
) -> tuple[ValidationSummary, list[str]]:
    root = project_root.resolve()
    inventory_path = root / "docs/development/visual-evidence-inventory.json"
    inventory = load_json(inventory_path)
    errors: list[str] = []
    if set(inventory) != TOP_LEVEL_FIELDS:
        missing = sorted(TOP_LEVEL_FIELDS - set(inventory))
        unknown = sorted(set(inventory) - TOP_LEVEL_FIELDS)
        if missing:
            errors.append("visual evidence inventory is missing fields: " + ", ".join(missing))
        if unknown:
            errors.append(
                "visual evidence inventory has unsupported fields: "
                + ", ".join(unknown)
            )
    if inventory.get("schema_version") != 1:
        errors.append("visual evidence inventory schema_version must be 1")
    source_catalog = inventory.get("source_catalog")
    if source_catalog != "site/api/catalog.json":
        errors.append("visual evidence source_catalog must be site/api/catalog.json")
        source_catalog = "site/api/catalog.json"
    catalog = catalog_components(load_json(root / str(source_catalog)), errors)
    repository_cases = all_test_cases(root, errors)
    manual_cases = validate_manual_visual_labels(root, repository_cases, errors)
    target_by_source = registered_test_targets(root, errors)
    root_cmake = root / "tests/CMakeLists.txt"
    ci_targets = set(cmake_target_list(root_cmake, "FLUENT_QT_CI_FAST_TARGETS"))
    ci_targets.update(cmake_target_list(root_cmake, "FLUENT_QT_CI_FULL_TARGETS"))
    local_desktop_cases = set(
        cmake_list(root_cmake, "FLUENT_QT_LOCAL_DESKTOP_TESTS")
    )
    expected_members = expected_family_members(catalog, rules, errors)
    validate_governance_contract(root, inventory, errors)
    validate_family_inventory(inventory, expected_members, rules, errors)
    validate_catalog_partition(inventory, catalog, expected_members, errors)
    automated, visual_surfaces, gallery_fallbacks = validate_component_inventory(
        root,
        inventory,
        catalog,
        expected_members,
        rules,
        manual_cases,
        target_by_source,
        ci_targets,
        local_desktop_cases,
        errors,
    )
    pixel_count = validate_representative_pixel_gates(
        root,
        inventory,
        catalog,
        repository_cases,
        expected_pixel_gates,
        errors,
    )
    summary = ValidationSummary(
        high_risk_components=len(
            set().union(*expected_members.values()) if expected_members else set()
        ),
        risk_families=len(rules),
        automated_evidence=automated,
        manual_visual_surfaces=visual_surfaces,
        gallery_fallbacks=gallery_fallbacks,
        representative_pixel_gates=pixel_count,
    )
    return summary, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run validator regression tests before repository validation",
    )
    args = parser.parse_args()
    if args.self_test:
        result = subprocess.run(
            (
                sys.executable,
                str(Path(__file__).with_name("test_validate_visual_evidence_inventory.py")),
            ),
            check=False,
        )
        if result.returncode:
            return result.returncode
    try:
        summary, errors = validate(args.project_root)
    except (AssertionError, OSError, json.JSONDecodeError) as error:
        print(f"Visual evidence inventory validation failed: {error}")
        return 1
    if errors:
        for error in errors:
            print(f"error: {error}")
        return 1
    print(
        "Visual evidence inventory passed: "
        f"{summary.high_risk_components} high-risk components, "
        f"{summary.risk_families} risk families, "
        f"{summary.automated_evidence} automated evidence records, "
        f"{summary.manual_visual_surfaces} VisualCheck surfaces, "
        f"{summary.gallery_fallbacks} Gallery fallbacks, "
        f"{summary.representative_pixel_gates} legacy representative pixel gates."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
