"""Generate the PySide6 Gallery contract from the native C++ Gallery.

The C++ Gallery remains the canonical authoring surface.  This generator reads
its component catalog and ``GallerySample`` declarations and emits the exact
route/card metadata packaged by the Python Gallery.  It deliberately parses a
small, validated subset of the project's C++ initializer style instead of
maintaining a second hand-written catalog that can silently drift.
"""

from __future__ import annotations

import argparse
import ast
from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Iterable


@dataclass(frozen=True)
class CategorySource:
    id: str
    title: str
    source_file: str


CATEGORY_SOURCES = (
    CategorySource("basic-input", "Basic input", "BasicInputSamples.cpp"),
    CategorySource("collections", "Collections", "CollectionsSamples.cpp"),
    CategorySource("date-time", "Date & time", "DateTimeSamples.cpp"),
    CategorySource(
        "dialogs-flyouts", "Dialogs & flyouts", "DialogsFlyoutsSamples.cpp"
    ),
    CategorySource("layout", "Layout", "LayoutSamples.cpp"),
    CategorySource(
        "menus-toolbars", "Menus & toolbars", "MenusToolbarsSamples.cpp"
    ),
    CategorySource("navigation", "Navigation", "NavigationSamples.cpp"),
    CategorySource("scrolling", "Scrolling", "ScrollingSamples.cpp"),
    CategorySource("status-info", "Status & info", "StatusInfoSamples.cpp"),
    CategorySource("text-fields", "Text fields", "TextFieldsSamples.cpp"),
    CategorySource("windowing", "Windowing", "WindowingSamples.cpp"),
    CategorySource("foundation", "Foundation", "FoundationSamples.cpp"),
)


NON_COMPONENT_ROUTES = (
    {"id": "home", "title": "Home", "kind": "home", "parent_id": ""},
    {
        "id": "foundation",
        "title": "Foundation",
        "kind": "foundation",
        "parent_id": "",
    },
    {
        "id": "foundation-qmlplus",
        "title": "QML+",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "foundation-typography",
        "title": "Typography",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "foundation-color",
        "title": "Color",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "foundation-iconography",
        "title": "Iconography",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "foundation-geometry",
        "title": "Geometry",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "foundation-spacing",
        "title": "Spacing",
        "kind": "foundation-topic",
        "parent_id": "foundation",
    },
    {
        "id": "all-controls",
        "title": "All",
        "kind": "all-controls",
        "parent_id": "controls",
    },
    {
        "id": "settings",
        "title": "Settings",
        "kind": "settings",
        "parent_id": "",
    },
)


def _matching_delimiter(text: str, start: int, opening: str, closing: str) -> int:
    if start >= len(text) or text[start] != opening:
        raise ValueError("delimiter scan did not start at {0!r}".format(opening))
    depth = 0
    index = start
    in_string = False
    escaped = False
    line_comment = False
    block_comment = False
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == "*" and following == "/":
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == "/" and following == "/":
            line_comment = True
            index += 2
            continue
        if char == "/" and following == "*":
            block_comment = True
            index += 2
            continue
        if char == '"':
            in_string = True
        elif char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError("unmatched {0!r} at offset {1}".format(opening, start))


def _split_arguments(text: str) -> list[str]:
    arguments = []
    start = 0
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    in_string = False
    escaped = False
    line_comment = False
    block_comment = False
    index = 0
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue
        if block_comment:
            if char == "*" and following == "/":
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == "/" and following == "/":
            line_comment = True
            index += 2
            continue
        if char == "/" and following == "*":
            block_comment = True
            index += 2
            continue
        if char == '"':
            in_string = True
        elif char in "([{":
            stack.append(char)
        elif char in ")]}" and stack:
            if stack[-1] != pairs[char]:
                raise ValueError("mismatched delimiter in argument list")
            stack.pop()
        elif char == "," and not stack:
            arguments.append(text[start:index].strip())
            start = index + 1
        index += 1
    arguments.append(text[start:].strip())
    return arguments


_CPP_STRING = re.compile(r'(?<![A-Za-z0-9_])(?:u8|u|U|L)?("(?:\\.|[^"\\])*")')


def _cpp_string_expression(expression: str) -> str:
    literals = _CPP_STRING.findall(expression)
    if not literals:
        raise ValueError("expected a C++ string expression: {0}".format(expression))
    try:
        return "".join(ast.literal_eval(literal) for literal in literals)
    except (SyntaxError, ValueError) as error:
        raise ValueError("invalid C++ string expression: {0}".format(expression)) from error


def _function_bodies(source: str) -> dict[str, str]:
    pattern = re.compile(
        r"QVector\s*<\s*GallerySample\s*>\s+([A-Za-z0-9_]+Samples)\s*\(\s*\)\s*\{"
    )
    bodies = {}
    for match in pattern.finditer(source):
        opening = source.find("{", match.start())
        closing = _matching_delimiter(source, opening, "{", "}")
        bodies[match.group(1)] = source[opening + 1 : closing]
    return bodies


_ROOT_GROUP = re.compile(
    r"\b(horizontalGroup|verticalGroup)\s*\(\s*parent\s*"
    r"(?:,\s*([0-9]+)\s*)?\)"
)


def _preview_layout(create_preview: str) -> dict[str, object] | None:
    """Extract the native root sample-group contract from a preview lambda."""

    matches = _ROOT_GROUP.findall(create_preview)
    if not matches:
        return None
    if len(matches) != 1:
        raise ValueError(
            "preview lambda has multiple sample groups parented directly to parent"
        )
    helper, spacing = matches[0]
    return {
        "orientation": "horizontal" if helper == "horizontalGroup" else "vertical",
        "spacing": int(spacing) if spacing else 12,
    }


def _samples_in_body(body: str) -> list[dict[str, object]]:
    samples = []
    cursor = 0
    while True:
        match = re.search(r"\bmakeSample\s*\(", body[cursor:])
        if match is None:
            break
        opening = cursor + match.end() - 1
        closing = _matching_delimiter(body, opening, "(", ")")
        arguments = _split_arguments(body[opening + 1 : closing])
        if len(arguments) < 5:
            raise ValueError("makeSample requires at least five arguments")
        sample: dict[str, object] = {
            "id": _cpp_string_expression(arguments[0]),
            "title": _cpp_string_expression(arguments[1]),
            "description": _cpp_string_expression(arguments[2]),
            "cpp_snippet": _cpp_string_expression(arguments[3]),
        }
        preview_layout = _preview_layout(arguments[4])
        if preview_layout is not None:
            sample["preview_layout"] = preview_layout
        samples.append(sample)
        cursor = closing + 1
    return samples


def _route_sample_functions(source: str) -> dict[str, str]:
    pattern = re.compile(
        r"if\s*\(\s*routeId\s*==\s*QStringLiteral\(\s*\"([^\"]+)\"\s*\)\s*\)"
        r"\s*return\s+(?:[A-Za-z_][A-Za-z0-9_]*::)*"
        r"([A-Za-z0-9_]+Samples)\s*\(\s*\)\s*;",
        re.S,
    )
    return {route_id: function_name for route_id, function_name in pattern.findall(source)}


def _component_titles(component_catalog_source: str) -> dict[str, str]:
    pattern = re.compile(
        r"\{\s*QStringLiteral\(\s*\"([^\"]+)\"\s*\)\s*,\s*"
        r"QStringLiteral\(\s*\"([^\"]+)\"\s*\)\s*,\s*"
        r"Typography::Icons::[A-Za-z0-9_]+",
        re.S,
    )
    titles = {}
    for route_id, title in pattern.findall(component_catalog_source):
        if route_id in titles:
            raise ValueError("duplicate component route: {0}".format(route_id))
        titles[route_id] = title
    return titles


def _route_descriptions(content_catalog_source: str) -> dict[str, str]:
    marker = "static const QHash<QString, QString> descriptions{"
    marker_index = content_catalog_source.find(marker)
    if marker_index < 0:
        raise ValueError("could not find native routeDescriptions initializer")
    opening = content_catalog_source.find("{", marker_index + len(marker) - 1)
    closing = _matching_delimiter(content_catalog_source, opening, "{", "}")
    body = content_catalog_source[opening + 1 : closing]
    descriptions = {}
    cursor = 0
    while cursor < len(body):
        opening = body.find("{", cursor)
        if opening < 0:
            break
        closing = _matching_delimiter(body, opening, "{", "}")
        arguments = _split_arguments(body[opening + 1 : closing])
        if len(arguments) == 2 and "QStringLiteral" in arguments[0]:
            route_id = _cpp_string_expression(arguments[0])
            descriptions[route_id] = _cpp_string_expression(arguments[1])
        cursor = closing + 1
    return descriptions


def _api_type(route_id: str, title: str) -> str:
    return {
        "menu": "FluentMenu",
        "menu-bar": "FluentMenuBar",
    }.get(route_id, title)


def _component_route(component: dict[str, object]) -> dict[str, object]:
    return {
        "id": component["id"],
        "title": component["title"],
        "kind": "component",
        "parent_id": component["category_id"],
        "description": component["description"],
    }


def generate_contract(project_root: Path) -> dict[str, object]:
    app_root = project_root / "app"
    samples_root = app_root / "view" / "widgets" / "samples"
    component_source = (app_root / "model" / "GalleryComponentCatalog.cpp").read_text(
        encoding="utf-8"
    )
    content_source = (app_root / "model" / "GalleryContentCatalog.cpp").read_text(
        encoding="utf-8"
    )
    component_titles = _component_titles(component_source)
    descriptions = _route_descriptions(content_source)

    # Dispatch functions remain grouped by category, while large sample
    # implementations may be split into companion translation units. Resolve
    # function bodies across the whole sample directory so modularization does
    # not silently remove routes from the generated Python contract.
    bodies: dict[str, str] = {}
    for source_path in sorted(samples_root.glob("*Samples.cpp")):
        source = source_path.read_text(encoding="utf-8")
        for function_name, body in _function_bodies(source).items():
            if function_name in bodies:
                raise ValueError(
                    "duplicate native Gallery sample function {0}: {1}".format(
                        function_name, source_path.name
                    )
                )
            bodies[function_name] = body

    categories = []
    components = []
    seen_routes: set[str] = set()
    seen_samples: set[tuple[str, str]] = set()
    for category in CATEGORY_SOURCES:
        source = (samples_root / category.source_file).read_text(encoding="utf-8")
        route_functions = _route_sample_functions(source)
        category_components = []
        for route_id, function_name in route_functions.items():
            if route_id in seen_routes:
                raise ValueError("duplicate native Gallery route: {0}".format(route_id))
            if route_id not in component_titles:
                raise ValueError("sample route is absent from component catalog: {0}".format(route_id))
            if function_name not in bodies:
                raise ValueError(
                    "route {0} references missing sample function {1}".format(
                        route_id, function_name
                    )
                )
            samples = _samples_in_body(bodies[function_name])
            if not samples:
                raise ValueError("native Gallery route has no samples: {0}".format(route_id))
            for sample in samples:
                key = (route_id, sample["id"])
                if key in seen_samples:
                    raise ValueError(
                        "duplicate sample id on route {0}: {1}".format(*key)
                    )
                seen_samples.add(key)
            title = component_titles[route_id]
            component = {
                "id": route_id,
                "title": title,
                "api_type": _api_type(route_id, title),
                "category_id": category.id,
                "description": descriptions.get(route_id, ""),
                "samples": samples,
            }
            components.append(component)
            category_components.append(route_id)
            seen_routes.add(route_id)
        categories.append(
            {
                "id": category.id,
                "title": category.title,
                "components": category_components,
            }
        )

    missing_component_routes = sorted(set(component_titles) - seen_routes)
    if missing_component_routes:
        raise ValueError(
            "component catalog routes have no native samples: {0}".format(
                ", ".join(missing_component_routes)
            )
        )

    category_pages = [
        {
            "id": category["id"],
            "title": category["title"],
            "kind": "category",
            "parent_id": "controls",
            "description": descriptions.get(category["id"], ""),
        }
        for category in categories
        if category["id"] != "foundation"
    ]
    routes = []
    non_components_by_id = {route["id"]: dict(route) for route in NON_COMPONENT_ROUTES}
    for route in non_components_by_id.values():
        route["description"] = descriptions.get(route["id"], "")
    routes.append(non_components_by_id["home"])
    routes.append(non_components_by_id["foundation"])
    for route_id in (
        "foundation-qmlplus",
        "foundation-typography",
        "foundation-color",
        "foundation-iconography",
    ):
        routes.append(non_components_by_id[route_id])
    # FontIcon is a real component nested between Iconography and Geometry.
    routes.append(
        _component_route(
            next(component for component in components if component["id"] == "font-icon")
        )
    )
    routes.append(non_components_by_id["foundation-geometry"])
    routes.append(non_components_by_id["foundation-spacing"])
    routes.append(non_components_by_id["all-controls"])
    for category_page in category_pages:
        routes.append(category_page)
        routes.extend(
            _component_route(component)
            for component in components
            if component["category_id"] == category_page["id"]
        )
    routes.append(non_components_by_id["settings"])

    component_count = len(components)
    sample_count = sum(len(component["samples"]) for component in components)
    api_manifest = json.loads(
        (project_root / "bindings" / "pyside6" / "api-manifest.json").read_text(
            encoding="utf-8"
        )
    )
    manifest_classes = set(api_manifest["classes"])
    routed_types = {component["api_type"] for component in components}
    missing_bindings = sorted(routed_types - manifest_classes)
    if missing_bindings:
        raise ValueError(
            "native Gallery component types are absent from the binding manifest: {0}"
            .format(", ".join(missing_bindings))
        )
    support_types = sorted(manifest_classes - routed_types)
    if component_count != 70:
        raise ValueError(
            "native component route count changed from 70 to {0}; review the contract"
            .format(component_count)
        )
    if sample_count != 208:
        raise ValueError(
            "native sample count changed from 208 to {0}; review the contract".format(
                sample_count
            )
        )
    if len(routes) != 91:
        raise ValueError(
            "native navigation route count changed from 91 to {0}; review the contract"
            .format(len(routes))
        )

    return {
        "schema_version": 1,
        "canonical_source": "app/view/widgets/samples",
        "categories": categories,
        "components": components,
        "binding_support_types": support_types,
        "routes": routes,
        "summary": {
            "route_count": len(routes),
            "component_count": component_count,
            "sample_count": sample_count,
        },
    }


def write_contract(contract: dict[str, object], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    serialized = json.dumps(contract, ensure_ascii=False, indent=2) + "\n"
    if output.is_file() and output.read_text(encoding="utf-8") == serialized:
        return
    output.write_text(serialized, encoding="utf-8", newline="\n")


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    write_contract(generate_contract(args.project_root.resolve()), args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
