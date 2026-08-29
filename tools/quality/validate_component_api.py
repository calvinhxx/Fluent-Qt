#!/usr/bin/env python3

"""Validate installed component API structure and legacy exception drift."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Iterable


PROPERTY_KEYWORDS = {
    "READ",
    "WRITE",
    "RESET",
    "NOTIFY",
    "MEMBER",
    "CONSTANT",
    "FINAL",
    "REQUIRED",
    "REVISION",
    "DESIGNABLE",
    "SCRIPTABLE",
    "STORED",
    "USER",
    "BINDABLE",
}
PROPERTY_VALUE_KEYWORDS = {
    "READ",
    "WRITE",
    "RESET",
    "NOTIFY",
    "MEMBER",
    "REVISION",
    "DESIGNABLE",
    "SCRIPTABLE",
    "STORED",
    "USER",
    "BINDABLE",
}
EXCEPTION_GROUPS = {
    "write_without_notify": {"animation-channel", "legacy-compatibility"},
    "noun_boolean_reader": {"legacy-compatibility"},
}
EXCEPTION_CATEGORIES = set().union(*EXCEPTION_GROUPS.values())
COMMENT_OR_LITERAL_RE = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)
CLASS_RE = re.compile(
    r"\b(?:class|struct)\s+"
    r"(?:(?:FLUENT_QT_EXPORT|[A-Za-z_]\w*_EXPORT)\s+)?"
    r"([A-Za-z_]\w*)\b([^;{]*)\{"
)
PROPERTY_RE = re.compile(r"\bQ_PROPERTY\s*\(")
CALLABLE_RE_TEMPLATE = r"\b{identifier}\s*\("


@dataclass(frozen=True)
class ClassRegion:
    name: str
    bases: tuple[str, ...]
    body_start: int
    body_end: int


@dataclass(frozen=True)
class ApiProperty:
    relative_path: str
    class_name: str
    name: str
    type_name: str
    read: str
    write: str
    reset: str
    notify: str
    line: int
    class_body: str
    class_bases: tuple[str, ...]

    @property
    def key(self) -> str:
        return f"{self.relative_path}#{self.class_name}.{self.name}"


@dataclass(frozen=True)
class ValidationSummary:
    components: int
    installed_headers: int
    component_headers: int
    properties: int
    write_without_notify: int
    noun_boolean_readers: int


def load_json(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise AssertionError(f"{path} must contain a JSON object")
    return value


def mask_comments_and_literals(text: str) -> str:
    def replace(match: re.Match[str]) -> str:
        value = match.group(0)
        return "".join("\n" if character == "\n" else " " for character in value)

    return COMMENT_OR_LITERAL_RE.sub(replace, text)


def matching_delimiter(text: str, start: int, opening: str, closing: str) -> int:
    depth = 0
    for index in range(start, len(text)):
        character = text[index]
        if character == opening:
            depth += 1
        elif character == closing:
            depth -= 1
            if depth == 0:
                return index
    return -1


def class_regions(masked_text: str) -> list[ClassRegion]:
    regions: list[ClassRegion] = []
    for match in CLASS_RE.finditer(masked_text):
        opening = masked_text.find("{", match.start(), match.end())
        closing = matching_delimiter(masked_text, opening, "{", "}")
        if closing >= 0:
            tail = match.group(2)
            bases: list[str] = []
            if ":" in tail:
                for raw_base in tail.split(":", 1)[1].split(","):
                    normalized = re.sub(
                        r"\b(?:public|protected|private|virtual)\b", " ", raw_base
                    ).strip()
                    if normalized:
                        bases.append(normalized.rsplit("::", 1)[-1].split("<", 1)[0])
            regions.append(
                ClassRegion(match.group(1), tuple(bases), opening + 1, closing)
            )
    return regions


def enclosing_class(regions: Iterable[ClassRegion], position: int) -> ClassRegion | None:
    matches = [
        region
        for region in regions
        if region.body_start <= position < region.body_end
    ]
    if not matches:
        return None
    return min(matches, key=lambda region: region.body_end - region.body_start)


def property_attributes(payload: str) -> tuple[str, str, dict[str, str]]:
    tokens = payload.split()
    keyword_indexes = [
        index for index, token in enumerate(tokens) if token in PROPERTY_KEYWORDS
    ]
    if not keyword_indexes or keyword_indexes[0] < 2:
        raise ValueError("expected a type, property name, and READ attribute")
    first_keyword = keyword_indexes[0]
    type_name = " ".join(tokens[: first_keyword - 1])
    property_name = tokens[first_keyword - 1]
    attributes: dict[str, str] = {}
    index = first_keyword
    while index < len(tokens):
        token = tokens[index]
        if token not in PROPERTY_KEYWORDS:
            raise ValueError(f"unexpected token {token!r}")
        if token in PROPERTY_VALUE_KEYWORDS:
            if index + 1 >= len(tokens) or tokens[index + 1] in PROPERTY_KEYWORDS:
                raise ValueError(f"{token} requires a value")
            attributes[token] = tokens[index + 1]
            index += 2
        else:
            attributes[token] = "true"
            index += 1
    return type_name, property_name, attributes


def parse_properties(path: Path, project_root: Path) -> tuple[list[ApiProperty], list[str]]:
    text = path.read_text(encoding="utf-8")
    masked = mask_comments_and_literals(text)
    regions = class_regions(masked)
    properties: list[ApiProperty] = []
    errors: list[str] = []
    relative_path = path.relative_to(project_root).as_posix()
    for match in PROPERTY_RE.finditer(masked):
        opening = masked.find("(", match.start(), match.end())
        closing = matching_delimiter(masked, opening, "(", ")")
        line = text.count("\n", 0, match.start()) + 1
        if closing < 0:
            errors.append(f"{relative_path}:{line}: unterminated Q_PROPERTY")
            continue
        region = enclosing_class(regions, match.start())
        if region is None:
            errors.append(f"{relative_path}:{line}: Q_PROPERTY is not inside a class")
            continue
        payload = " ".join(masked[opening + 1 : closing].split())
        try:
            type_name, property_name, attributes = property_attributes(payload)
        except ValueError as error:
            errors.append(f"{relative_path}:{line}: malformed Q_PROPERTY: {error}")
            continue
        read = attributes.get("READ", "")
        if not read:
            errors.append(f"{relative_path}:{line}: {property_name} must declare READ")
        class_body = masked[region.body_start : region.body_end]
        properties.append(
            ApiProperty(
                relative_path=relative_path,
                class_name=region.name,
                name=property_name,
                type_name=type_name,
                read=read,
                write=attributes.get("WRITE", ""),
                reset=attributes.get("RESET", ""),
                notify=attributes.get("NOTIFY", ""),
                line=line,
                class_body=class_body,
                class_bases=region.bases,
            )
        )
    return properties, errors


def cmake_install_headers(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(
        r"set\s*\(\s*FLUENT_QT_INSTALL_HEADERS\b(.*?)\n\s*\)",
        text,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"{path} is missing FLUENT_QT_INSTALL_HEADERS")
    headers: list[str] = []
    for raw_line in match.group(1).splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        headers.extend(token.strip('"') for token in line.split())
    return headers


def source_path_for_installed_header(value: object) -> str:
    if not isinstance(value, str):
        return ""
    match = re.fullmatch(r"<FluentQt/(components/.+\.h)>", value)
    return f"src/{match.group(1)}" if match else ""


def source_path_for_umbrella_header(value: object) -> str:
    if not isinstance(value, str):
        return ""
    match = re.fullmatch(r"<FluentQt/([^/]+\.h)>", value)
    return f"include/FluentQt/{match.group(1)}" if match else ""


def test_source_path(value: object) -> str:
    if not isinstance(value, str) or "/blob/main/" not in value:
        return ""
    return value.split("/blob/main/", 1)[1]


def policy_exception_keys(
    policy: dict[str, object], errors: list[str]
) -> dict[str, set[str]]:
    category_descriptions = policy.get("exception_categories")
    if not isinstance(category_descriptions, dict):
        errors.append("component API policy exception_categories must be an object")
    else:
        unknown_categories = sorted(
            set(category_descriptions) - EXCEPTION_CATEGORIES
        )
        missing_categories = sorted(
            EXCEPTION_CATEGORIES - set(category_descriptions)
        )
        if unknown_categories:
            errors.append(
                "component API policy has unknown exception categories: "
                + ", ".join(unknown_categories)
            )
        if missing_categories:
            errors.append(
                "component API policy is missing exception categories: "
                + ", ".join(missing_categories)
            )
        for category in sorted(EXCEPTION_CATEGORIES & set(category_descriptions)):
            description = category_descriptions[category]
            if not isinstance(description, str) or not description.strip():
                errors.append(
                    f"component API policy exception category {category} "
                    "requires a description"
                )

    exception_value = policy.get("exceptions")
    if not isinstance(exception_value, dict):
        errors.append("component API policy exceptions must be an object")
        return {name: set() for name in EXCEPTION_GROUPS}
    unknown_groups = sorted(set(exception_value) - set(EXCEPTION_GROUPS))
    missing_groups = sorted(set(EXCEPTION_GROUPS) - set(exception_value))
    if unknown_groups:
        errors.append(
            "component API policy has unknown groups: " + ", ".join(unknown_groups)
        )
    if missing_groups:
        errors.append(
            "component API policy is missing groups: " + ", ".join(missing_groups)
        )
    result: dict[str, set[str]] = {}
    for group_name, allowed_categories in EXCEPTION_GROUPS.items():
        group_value = exception_value.get(group_name, {})
        if not isinstance(group_value, dict):
            errors.append(f"component API policy {group_name} must be an object")
            result[group_name] = set()
            continue
        unknown_categories = sorted(set(group_value) - allowed_categories)
        missing_categories = sorted(allowed_categories - set(group_value))
        if unknown_categories:
            errors.append(
                f"component API policy {group_name} has unknown categories: "
                + ", ".join(unknown_categories)
            )
        if missing_categories:
            errors.append(
                f"component API policy {group_name} is missing categories: "
                + ", ".join(missing_categories)
            )
        keys: list[str] = []
        for category in sorted(allowed_categories):
            category_value = group_value.get(category, [])
            if not isinstance(category_value, list) or not all(
                isinstance(key, str) and key for key in category_value
            ):
                errors.append(
                    f"component API policy {group_name}.{category} must be a string array"
                )
                continue
            if category_value != sorted(category_value):
                errors.append(
                    f"component API policy {group_name}.{category} must be sorted"
                )
            keys.extend(category_value)
        duplicates = sorted({key for key in keys if keys.count(key) > 1})
        if duplicates:
            errors.append(
                f"component API policy {group_name} has duplicate keys: "
                + ", ".join(duplicates)
            )
        result[group_name] = set(keys)
    return result


def is_standard_boolean_reader(name: str) -> bool:
    return bool(re.match(r"^(?:is|has|are|can)[A-Z_]", name))


def callable_declared(
    property_value: ApiProperty,
    identifier: str,
    class_contracts: dict[str, list[tuple[str, tuple[str, ...]]]],
) -> bool:
    if not identifier:
        return True
    pattern = CALLABLE_RE_TEMPLATE.format(identifier=re.escape(identifier))
    if re.search(pattern, property_value.class_body):
        return True

    def inherited(base_name: str, visited: set[str]) -> bool:
        if base_name in visited:
            return False
        visited.add(base_name)
        for body, bases in class_contracts.get(base_name, []):
            if re.search(pattern, body):
                return True
            if any(inherited(base, visited) for base in bases):
                return True
        return False

    return any(inherited(base, set()) for base in property_value.class_bases)


def validate(project_root: Path) -> tuple[ValidationSummary, list[str]]:
    root = project_root.resolve()
    policy_path = root / "docs/development/component-api-policy.json"
    install_path = root / "cmake/FluentQtInstallHeaders.cmake"
    policy = load_json(policy_path)
    errors: list[str] = []
    if policy.get("schema_version") != 1:
        errors.append("component API policy schema_version must be 1")
    catalog_value = policy.get("source_catalog")
    if not isinstance(catalog_value, str) or not catalog_value:
        errors.append("component API policy source_catalog is required")
        catalog_value = "site/api/catalog.json"
    if policy.get("installed_headers") != "cmake/FluentQtInstallHeaders.cmake":
        errors.append(
            "component API policy installed_headers must name cmake/FluentQtInstallHeaders.cmake"
        )

    install_headers = cmake_install_headers(install_path)
    install_header_set = set(install_headers)
    duplicate_install_headers = sorted(
        {header for header in install_headers if install_headers.count(header) > 1}
    )
    if duplicate_install_headers:
        errors.append(
            "duplicate installed headers: " + ", ".join(duplicate_install_headers)
        )
    for relative_path in install_headers:
        if not (root / relative_path).is_file():
            errors.append(f"installed header does not exist: {relative_path}")
        if relative_path.startswith("src/components/") and (
            "/private/" in relative_path or relative_path.endswith("_p.h")
        ):
            errors.append(f"private component header is installed: {relative_path}")

    component_headers = sorted(
        relative_path
        for relative_path in install_headers
        if relative_path.startswith("src/components/")
    )
    properties: list[ApiProperty] = []
    class_names_by_header: dict[str, set[str]] = {}
    class_contracts: dict[str, list[tuple[str, tuple[str, ...]]]] = {}
    for relative_path in component_headers:
        path = root / relative_path
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        masked = mask_comments_and_literals(text)
        regions = class_regions(masked)
        class_names_by_header[relative_path] = {region.name for region in regions}
        for region in regions:
            class_contracts.setdefault(region.name, []).append(
                (masked[region.body_start : region.body_end], region.bases)
            )
        parsed, parse_errors = parse_properties(path, root)
        properties.extend(parsed)
        errors.extend(parse_errors)

    catalog = load_json(root / str(catalog_value))
    components = catalog.get("components")
    if not isinstance(components, list):
        errors.append(f"{catalog_value} components must be an array")
        components = []
    component_ids: list[str] = []
    qualified_types: list[str] = []
    for index, component in enumerate(components):
        prefix = f"components[{index}]"
        if not isinstance(component, dict):
            errors.append(f"{prefix} must be an object")
            continue
        component_id = component.get("id")
        if not isinstance(component_id, str) or not component_id:
            errors.append(f"{prefix}.id must be a non-empty string")
            component_id = prefix
        component_ids.append(component_id)
        cpp = component.get("cpp")
        if not isinstance(cpp, dict):
            errors.append(f"{component_id}.cpp must be an object")
            continue
        declaration_header = source_path_for_installed_header(
            cpp.get("installed_declaration_header")
        )
        if not declaration_header:
            errors.append(
                f"{component_id} has an invalid installed declaration header"
            )
        elif declaration_header not in install_header_set:
            errors.append(
                f"{component_id} declaration header is not installed: {declaration_header}"
            )
        umbrella_header = source_path_for_umbrella_header(cpp.get("public_header"))
        if not umbrella_header:
            errors.append(f"{component_id} has an invalid public umbrella header")
        elif umbrella_header not in install_header_set:
            errors.append(
                f"{component_id} umbrella header is not installed: {umbrella_header}"
            )
        qualified_type = cpp.get("qualified_type")
        if not isinstance(qualified_type, str) or not qualified_type:
            errors.append(f"{component_id} has an invalid qualified type")
        else:
            qualified_types.append(qualified_type)
            leaf_type = qualified_type.rsplit("::", 1)[-1]
            if declaration_header and leaf_type not in class_names_by_header.get(
                declaration_header, set()
            ):
                errors.append(
                    f"{component_id} declaration header does not declare {leaf_type}: "
                    f"{declaration_header}"
                )
        tests = component.get("tests")
        if not isinstance(tests, list) or not tests:
            errors.append(f"{component_id} must list a focused test source")
        else:
            for test_index, test in enumerate(tests):
                if not isinstance(test, dict):
                    errors.append(f"{component_id}.tests[{test_index}] must be an object")
                    continue
                target = test.get("target")
                if not isinstance(target, str) or not target.startswith("test_"):
                    errors.append(
                        f"{component_id}.tests[{test_index}] has an invalid target"
                    )
                ctest_label = test.get("ctest_label")
                if not isinstance(ctest_label, str) or ctest_label != target:
                    errors.append(
                        f"{component_id}.tests[{test_index}] ctest_label must match "
                        "its target"
                    )
                source_path = test_source_path(test.get("source_url"))
                if not source_path:
                    errors.append(
                        f"{component_id}.tests[{test_index}] has an invalid source URL"
                    )
                elif not (root / source_path).is_file():
                    errors.append(
                        f"{component_id} focused test source does not exist: {source_path}"
                    )

    duplicate_ids = sorted(
        {value for value in component_ids if component_ids.count(value) > 1}
    )
    if duplicate_ids:
        errors.append("duplicate component ids: " + ", ".join(duplicate_ids))
    duplicate_types = sorted(
        {value for value in qualified_types if qualified_types.count(value) > 1}
    )
    if duplicate_types:
        errors.append("duplicate qualified component types: " + ", ".join(duplicate_types))

    for property_value in properties:
        for role, identifier in (
            ("READ", property_value.read),
            ("WRITE", property_value.write),
            ("RESET", property_value.reset),
            ("NOTIFY", property_value.notify),
        ):
            if identifier and not callable_declared(
                property_value, identifier, class_contracts
            ):
                errors.append(
                    f"{property_value.relative_path}:{property_value.line}: "
                    f"{property_value.key} {role} callable is not declared: {identifier}"
                )

    exception_keys = policy_exception_keys(policy, errors)
    write_without_notify = {
        property_value.key
        for property_value in properties
        if property_value.write and not property_value.notify
    }
    noun_boolean_readers = {
        property_value.key
        for property_value in properties
        if property_value.type_name == "bool"
        and property_value.read
        and not is_standard_boolean_reader(property_value.read)
    }
    current_violations = {
        "write_without_notify": write_without_notify,
        "noun_boolean_reader": noun_boolean_readers,
    }
    for group_name, current_keys in current_violations.items():
        known_keys = exception_keys.get(group_name, set())
        new_keys = sorted(current_keys - known_keys)
        stale_keys = sorted(known_keys - current_keys)
        if new_keys:
            errors.append(
                f"unclassified {group_name} properties: " + ", ".join(new_keys)
            )
        if stale_keys:
            errors.append(
                f"stale {group_name} policy entries: " + ", ".join(stale_keys)
            )

    summary = ValidationSummary(
        components=len(components),
        installed_headers=len(install_headers),
        component_headers=len(component_headers),
        properties=len(properties),
        write_without_notify=len(write_without_notify),
        noun_boolean_readers=len(noun_boolean_readers),
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
        test_result = subprocess.run(
            (sys.executable, str(Path(__file__).with_name("test_validate_component_api.py"))),
            check=False,
        )
        if test_result.returncode:
            return test_result.returncode
    try:
        summary, errors = validate(args.project_root)
    except (AssertionError, OSError, json.JSONDecodeError) as error:
        print(f"Component API validation failed: {error}")
        return 1
    if errors:
        for error in errors:
            print(f"error: {error}")
        return 1
    print(
        "Component API validation passed: "
        f"{summary.components} components, "
        f"{summary.component_headers}/{summary.installed_headers} component/install headers, "
        f"{summary.properties} properties, "
        f"{summary.write_without_notify} classified write-without-notify surfaces, "
        f"{summary.noun_boolean_readers} classified noun boolean readers."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
