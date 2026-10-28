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
    CategorySource("charts", "Charts", "ChartsSamples.cpp"),
    CategorySource("basic-input", "Basic input", "BasicInputSamples.cpp"),
    CategorySource("collections", "Collections", "CollectionsSamples.cpp"),
    CategorySource("date-time", "Date & time", "DateTimeSamples.cpp"),
    CategorySource(
        "dialogs-flyouts", "Dialogs & flyouts", "DialogsFlyoutsSamples.cpp"
    ),
    CategorySource("layout", "Layout", "LayoutSamples.cpp"),
    CategorySource("particles", "Particles", "ParticlesSamples.cpp"),
    CategorySource("spatial", "Spatial", "SpatialSamples.cpp"),
    CategorySource("qml-bridge", "QmlBridge", "QmlBridgeSamples.cpp"),
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

QML_VIEW_SOURCES = (
    ("qml-view-load", "loadSource", "Basic.qml"),
    ("qml-view-theme", "themeSource", "Theme.qml"),
    ("qml-view-context", "contextSource", "Context.qml"),
    ("qml-view-action", "actionSource", "Action.qml"),
    ("qml-view-lifecycle", "lifecycleSource", "Basic.qml"),
    ("qml-view-error", "errorSource", "Basic.qml"),
)


_CPP_RAW_STRING = re.compile(r'(?<![A-Za-z0-9_])(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')


def _raw_string_end(text: str, opening: int) -> tuple[int, int]:
    match = _CPP_RAW_STRING.match(text, opening)
    if match is None:
        raise ValueError("invalid C++ raw string at offset {0}".format(opening))
    closing_text = ")" + match.group(1) + '"'
    closing = text.find(closing_text, match.end())
    if closing < 0:
        raise ValueError("unterminated C++ raw string at offset {0}".format(opening))
    return closing, closing + len(closing_text)


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
        raw = _CPP_RAW_STRING.match(text, index)
        if raw is not None:
            _, index = _raw_string_end(text, index)
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
        raw = _CPP_RAW_STRING.match(text, index)
        if raw is not None:
            _, index = _raw_string_end(text, index)
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
    fragments = []
    cursor = 0
    while cursor < len(expression):
        raw = _CPP_RAW_STRING.search(expression, cursor)
        literal = _CPP_STRING.search(expression, cursor)
        if raw is not None and (literal is None or raw.start() <= literal.start()):
            closing, cursor = _raw_string_end(expression, raw.start())
            fragments.append(expression[raw.end():closing])
        elif literal is not None:
            try:
                fragments.append(ast.literal_eval(literal.group(1)))
            except (SyntaxError, ValueError) as error:
                raise ValueError("invalid C++ string expression: {0}".format(expression)) from error
            cursor = literal.end()
        else:
            break
    if not fragments:
        raise ValueError("expected a C++ string expression: {0}".format(expression))
    return "".join(fragments)


def _qml_bridge_template(source: str) -> str:
    helpers = list(re.finditer(r"\bQString\s+withQml\s*\([^{};]*\)\s*\{", source))
    if len(helpers) != 1:
        raise ValueError("QmlBridge requires one withQml template helper")
    opening = source.find("{", helpers[0].start())
    signature = source[helpers[0].start():opening]
    if re.fullmatch(
        r"QString\s+withQml\s*\(\s*const\s+QString\s*&\s*cpp\s*,"
        r"\s*const\s+QString\s*&\s*name\s*\)\s*", signature
    ) is None:
        raise ValueError("QmlBridge withQml parameters must be cpp and name")
    closing = _matching_delimiter(source, opening, "{", "}")
    body = source[opening + 1:closing]
    prefix = re.match(
        r"\s*const\s+auto\s+example\s*=\s*QString::fromUtf8\s*\(", body
    )
    if prefix is None:
        raise ValueError("QmlBridge helper must read one host template literal")
    literal_opening = prefix.end() - 1
    literal_closing = _matching_delimiter(body, literal_opening, "(", ")")
    literal = body[literal_opening + 1:literal_closing].strip()
    if not literal.startswith('R"example('):
        raise ValueError("QmlBridge template must be one R\"example raw literal")
    _, literal_end = _raw_string_end(literal, 0)
    if literal[literal_end:].strip():
        raise ValueError("QmlBridge template must be one exact literal")
    replacement_chain = (
        r"\s*;\s*return\s+QString\s*\(\s*example\s*\)"
        r'\s*\.replace\s*\(\s*QStringLiteral\s*\(\s*"@NAME@"\s*\)\s*,\s*name\s*\)'
        r'\s*\.replace\s*\(\s*QStringLiteral\s*\(\s*"@BODY@"\s*\)\s*,\s*cpp\s*\)'
        r"\s*;\s*"
    )
    if re.fullmatch(replacement_chain, body[literal_closing + 1:]) is None:
        raise ValueError("QmlBridge replacements must be exactly NAME and BODY in order")
    template = _cpp_string_expression(literal)
    placeholders = ("@NAME@", "@BODY@")
    if any(template.count(placeholder) != 1 for placeholder in placeholders):
        raise ValueError("QmlBridge template requires each placeholder exactly once")
    if set(re.findall(r"@[A-Z_]+@", template)) != set(placeholders):
        raise ValueError("QmlBridge template contains unknown placeholders")
    if "import QtQuick" in template or 'R"qml(' in template:
        raise ValueError("QmlBridge C++ host template must not embed QML")
    return template


def _qml_bridge_source_expressions(source: str, samples_root: Path) -> dict[str, str]:
    template = _qml_bridge_template(source)
    expressions = {}
    for _, function_name, expected_asset in QML_VIEW_SOURCES:
        pattern = re.compile(r"\bQString\s+" + function_name + r"\s*\(\s*\)\s*\{")
        matches = list(pattern.finditer(source))
        if len(matches) != 1:
            raise ValueError("QmlBridge requires one source function: " + function_name)
        opening = source.find("{", matches[0].start())
        closing = _matching_delimiter(source, opening, "{", "}")
        body = source[opening + 1:closing]
        returned = re.search(r"\breturn\s+withQml\s*\(", body)
        if returned is None or body[:returned.start()].strip():
            raise ValueError("QmlBridge source must return withQml: " + function_name)
        call_opening = returned.end() - 1
        call_closing = _matching_delimiter(body, call_opening, "(", ")")
        if body[call_closing + 1:].strip() != ";":
            raise ValueError("QmlBridge source must contain one return expression: " + function_name)
        arguments = _split_arguments(body[call_opening + 1:call_closing])
        if len(arguments) != 2:
            raise ValueError("QmlBridge source requires a raw C++ snippet and QML file: " + function_name)
        snippet = re.fullmatch(r"QString::fromUtf8\s*\(\s*(.*?)\s*\)", arguments[0], re.S)
        if snippet is None or re.match(r'(?:u8)?R"', snippet.group(1)) is None:
            raise ValueError("QmlBridge C++ snippet must be one raw literal: " + function_name)
        _, raw_end = _raw_string_end(snippet.group(1), 0)
        if snippet.group(1)[raw_end:].strip():
            raise ValueError("QmlBridge C++ snippet must be one raw literal: " + function_name)
        if re.fullmatch(r'QStringLiteral\s*\(\s*"[^"\\]*"\s*\)', arguments[1]) is None:
            raise ValueError("QmlBridge QML file must be a literal: " + function_name)
        asset = _cpp_string_expression(arguments[1])
        if asset != expected_asset:
            raise ValueError("QmlBridge source uses the wrong QML file: " + function_name)
        cpp = _cpp_string_expression(arguments[0])
        if re.search(r"@[A-Z_]+@", cpp):
            raise ValueError("QmlBridge inserted source contains a template placeholder: " + function_name)
        expressions[function_name + "()"] = (
            template.replace("@NAME@", asset).replace("@BODY@", cpp)
        )
    return expressions


def _qml_helper_body(source: str, name: str, parameters: str) -> str:
    matches = list(re.finditer(r"\bQString\s+" + name + r"\s*\([^{};]*\)\s*\{", source))
    if len(matches) != 1:
        raise ValueError("QmlBridge requires exactly one " + name + " helper")
    opening = source.find("{", matches[0].start())
    signature = source[matches[0].start():opening]
    if re.fullmatch(r"QString\s+" + name + r"\s*\(" + parameters + r"\)\s*", signature) is None:
        raise ValueError("Unsupported QmlBridge helper signature: " + name)
    closing = _matching_delimiter(source, opening, "{", "}")
    return source[opening + 1:closing]


def _single_utf8_raw(expression: str, *, delimiter: str | None = None) -> str:
    expression = expression.strip()
    prefix = re.match(r"QString::fromUtf8\s*\(", expression)
    if prefix is None:
        raise ValueError("QmlBridge source requires QString::fromUtf8 of one raw literal")
    opening = prefix.end() - 1
    closing = _matching_delimiter(expression, opening, "(", ")")
    if expression[closing + 1:].strip():
        raise ValueError("QmlBridge source requires one exact UTF-8 expression")
    literal = expression[opening + 1:closing].strip()
    if re.match(r'(?:u8)?R"', literal) is None:
        raise ValueError("QmlBridge source must use a UTF-8 raw literal")
    if delimiter is not None and not literal.startswith('R"' + delimiter + '('):
        raise ValueError("QmlBridge template requires the " + delimiter + " raw delimiter")
    _, raw_end = _raw_string_end(literal, 0)
    if literal[raw_end:].strip():
        raise ValueError("QmlBridge source must contain one raw literal")
    return _cpp_string_expression(literal)


def _qml_bridge_key_usages(source: str) -> dict[str, str]:
    body = _qml_helper_body(source, "keyUsage", r"\s*const\s+QString\s*&\s*id\s*")
    usages = {}
    for sample_id, _, _ in QML_VIEW_SOURCES:
        branch = re.match(
            r'\s*if\s*\(\s*id\s*==\s*QStringLiteral\s*\(\s*"' + sample_id
            + r'"\s*\)\s*\)\s*\{', body
        )
        if branch is None:
            raise ValueError("QmlBridge Key usage must keep one branch per card in order")
        opening = branch.end() - 1
        closing = _matching_delimiter(body, opening, "{", "}")
        returned = re.fullmatch(r"\s*return\s+([\s\S]*);\s*", body[opening + 1:closing])
        if returned is None:
            raise ValueError("QmlBridge Key usage branch must contain one return expression")
        key = _single_utf8_raw(returned.group(1))
        if re.search(r"@[A-Z_]+@", key) or "import QtQuick" in key or "Rectangle {" in key:
            raise ValueError("QmlBridge host Key usage must contain only host-language code")
        usages[sample_id] = key
        body = body[closing + 1:]
    if re.fullmatch(r"\s*return\s*\{\s*\}\s*;\s*", body) is None:
        raise ValueError("QmlBridge Key usage may have no unparsed branches or statements")
    return usages


def _qml_bridge_file_templates(source: str) -> tuple[str, str]:
    body = _qml_helper_body(source, "qmlResourceSource", r"\s*const\s+QString\s*&\s*name\s*")
    prefix = re.match(r"\s*const\s+auto\s+resource\s*=\s*", body)
    if prefix is None:
        raise ValueError("QmlBridge resource helper must contain one resource template")
    start = prefix.end()
    expression = re.match(r"QString::fromUtf8\s*\(", body[start:])
    if expression is None:
        raise ValueError("QmlBridge resource template must be UTF-8")
    opening = start + expression.end() - 1
    closing = _matching_delimiter(body, opening, "(", ")")
    resource = _single_utf8_raw(body[start:closing + 1], delimiter="resource")
    suffix = (
        r"\s*;\s*return\s+QString\s*\(\s*resource\s*\)"
        r'\s*\.replace\s*\(\s*QStringLiteral\s*\(\s*"@NAME@"\s*\)\s*,\s*name\s*\)'
        r"\s*;\s*"
    )
    if re.fullmatch(suffix, body[closing + 1:]) is None:
        raise ValueError("QmlBridge resource template must replace only NAME")
    if resource.count("@NAME@") != 1 or set(re.findall(r"@[A-Z_]+@", resource)) != {"@NAME@"}:
        raise ValueError("QmlBridge resource template requires one NAME placeholder")
    body = _qml_helper_body(source, "qmlCmakeSource", r"\s*")
    returned = re.fullmatch(r"\s*return\s+([\s\S]*);\s*", body)
    if returned is None:
        raise ValueError("QmlBridge CMake helper must contain one return expression")
    cmake = _single_utf8_raw(returned.group(1), delimiter="cmake")
    if re.search(r"@[A-Z_]+@", cmake):
        raise ValueError("QmlBridge CMake template has an unknown placeholder")
    return resource, cmake


def _qml_bridge_source_files(source: str, samples_root: Path) -> dict[str, list[dict[str, str]]]:
    expressions = _qml_bridge_source_expressions(source, samples_root)
    usages = _qml_bridge_key_usages(source)
    resource, cmake = _qml_bridge_file_templates(source)
    filename_body = _qml_helper_body(source, "qmlFileName", r"\s*const\s+QString\s*&\s*id\s*")
    filename_expected = "".join(
        'if(id==QStringLiteral("' + sample_id + '"))returnQStringLiteral("' + asset + '");'
        for sample_id, _, asset in QML_VIEW_SOURCES if asset != "Basic.qml"
    ) + 'returnQStringLiteral("Basic.qml");'
    if re.sub(r"\s+", "", filename_body) != filename_expected:
        raise ValueError("QmlBridge file names must match the source functions' QML assets")
    body = _function_bodies(source).get("qmlViewSamples")
    if body is None:
        raise ValueError("QmlBridge requires its sample declaration to expose source files")
    assignments = re.findall(r"for\s*\(\s*auto\s*&\s*sample\s*:\s*result\s*\)\s*\{", body)
    expected = (
        'for(auto&sample:result){constautoname=qmlFileName(sample.id);'
        'sample.usageSnippet=keyUsage(sample.id);sample.sourceFiles={'
        '{QStringLiteral("main.cpp"),QStringLiteral("cpp"),sample.codeSnippet,sample.usageSnippet},'
        '{name,QStringLiteral("qml"),qmlAsset(name),{}},'
        '{QStringLiteral("qml.qrc"),QStringLiteral("xml"),qmlResourceSource(name),{}},'
        '{QStringLiteral("CMakeLists.txt"),QStringLiteral("cmake"),qmlCmakeSource(),{}}};}returnresult;'
    )
    if len(assignments) != 1 or not re.sub(r"\s+", "", body).endswith(expected):
        raise ValueError("QmlBridge samples must expose their four canonical source files")
    files = {}
    for sample_id, function_name, asset in QML_VIEW_SOURCES:
        try:
            qml = (samples_root / "qmlbridge" / asset).read_bytes().decode("utf-8")
        except (OSError, UnicodeError) as error:
            raise ValueError("QmlBridge canonical QML asset is unavailable: " + asset) from error
        files[sample_id] = [
            {"name": "main.cpp", "language": "cpp", "code": expressions[function_name + "()"],
             "usage_snippet": usages[sample_id]},
            {"name": asset, "language": "qml", "code": qml, "usage_snippet": ""},
            {"name": "qml.qrc", "language": "xml", "code": resource.replace("@NAME@", asset),
             "usage_snippet": ""},
            {"name": "CMakeLists.txt", "language": "cmake", "code": cmake, "usage_snippet": ""},
        ]
    return files


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


def _samples_in_body(
    body: str, *, source_expressions: dict[str, str] | None = None,
    source_files: dict[str, list[dict[str, str]]] | None = None,
) -> list[dict[str, object]]:
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
        snippet = (source_expressions or {}).get(arguments[3].strip())
        sample: dict[str, object] = {
            "id": _cpp_string_expression(arguments[0]),
            "title": _cpp_string_expression(arguments[1]),
            "description": _cpp_string_expression(arguments[2]),
            "cpp_snippet": snippet if snippet is not None else _cpp_string_expression(arguments[3]),
        }
        if source_files is not None:
            sample_id = str(sample["id"])
            if sample_id not in source_files:
                raise ValueError("QmlBridge sample has no parsed source files: " + sample_id)
            sample["source_files"] = source_files[sample_id]
        if len(arguments) > 5:
            if len(arguments) != 6 or arguments[5].strip() not in {"true", "false"}:
                raise ValueError("makeSample fillAvailableWidth must be a boolean literal")
            if arguments[5].strip() == "true":
                sample["fill_available_width"] = True
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


def _category_metadata(component_catalog_source: str) -> dict[str, dict[str, object]]:
    marker = "static const QVector<GalleryComponentCategory> catalog{"
    marker_index = component_catalog_source.find(marker)
    if marker_index < 0:
        raise ValueError("could not find native Gallery category catalog")
    opening = component_catalog_source.find("{", marker_index)
    closing = _matching_delimiter(component_catalog_source, opening, "{", "}")
    body = component_catalog_source[opening + 1:closing]
    categories = {}
    cursor = 0
    while cursor < len(body):
        opening = body.find("{", cursor)
        if opening < 0:
            break
        closing = _matching_delimiter(body, opening, "{", "}")
        arguments = _split_arguments(body[opening + 1:closing])
        if len(arguments) not in (5, 6):
            raise ValueError("unsupported native Gallery category initializer")
        category_id = _cpp_string_expression(arguments[0])
        if category_id in categories:
            raise ValueError("duplicate native Gallery category: " + category_id)
        plugin = arguments[5].strip() if len(arguments) == 6 else "false"
        if plugin not in ("true", "false"):
            raise ValueError("native Gallery category plugin flag must be a bool literal")
        categories[category_id] = {
            "title": _cpp_string_expression(arguments[1]),
            "plugin": plugin == "true",
            "components": tuple(_component_titles(arguments[4])),
        }
        cursor = closing + 1
    return categories


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


def _component_route(component: dict[str, object], category: dict[str, object]) -> dict[str, object]:
    return {
        "id": component["id"],
        "title": component["title"],
        "kind": "component",
        "parent_id": component["category_id"],
        "group": category["title"],
        "depth": 2 if category["plugin"] else 1,
        "description": component["description"],
    }


def generate_contract(project_root: Path, *, include_cpp_only: bool = True) -> dict[str, object]:
    app_root = project_root / "app"
    samples_root = app_root / "view" / "widgets" / "samples"
    component_source = (app_root / "model" / "GalleryComponentCatalog.cpp").read_text(
        encoding="utf-8"
    )
    content_source = (app_root / "model" / "GalleryContentCatalog.cpp").read_text(
        encoding="utf-8"
    )
    component_titles = _component_titles(component_source)
    category_metadata = _category_metadata(component_source)
    category_sources = {category.id: category for category in CATEGORY_SOURCES}
    if set(category_metadata) != set(category_sources):
        raise ValueError("native Gallery categories differ from sample source mappings")
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
    for category_id, metadata in category_metadata.items():
        category = category_sources[category_id]
        source = (samples_root / category.source_file).read_text(encoding="utf-8")
        route_functions = _route_sample_functions(source)
        if metadata["plugin"] and not include_cpp_only:
            # Explicit base-only contract for clients without optional modules.
            seen_routes.update(route_functions)
            continue
        category_components = []
        if set(route_functions) != set(metadata["components"]):
            raise ValueError("native Gallery category samples differ from its component catalog: " + category.id)
        source_expressions = {}
        source_files = None
        if category_id == "qml-bridge":
            source_expressions.update(_qml_bridge_source_expressions(source, samples_root))
            source_files = _qml_bridge_source_files(source, samples_root)
        for route_id in metadata["components"]:
            function_name = route_functions[route_id]
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
            # QFile locals belong to their sample function. Two functions may
            # both call the variable "source" without sharing teaching code.
            local_expressions = dict(source_expressions)
            for variable, filename in re.findall(
                r'QFile\s+(\w+)\(QStringLiteral\(":/app/samples/([\w.]+)"\)\)', bodies[function_name]
            ):
                local_expressions[f"QString::fromUtf8({variable}.readAll())"] = (
                    samples_root / filename
                ).read_text(encoding="utf-8")
            samples = _samples_in_body(
                bodies[function_name], source_expressions=local_expressions, source_files=source_files,
            )
            if not samples:
                raise ValueError("native Gallery route has no samples: {0}".format(route_id))
            if route_id == "qml-view" and [sample["id"] for sample in samples] != [
                sample_id for sample_id, _, _ in QML_VIEW_SOURCES
            ]:
                raise ValueError("QmlView must keep its six focused examples in canonical order")
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
                "title": metadata["title"],
                "plugin": metadata["plugin"],
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
            "parent_id": "plugins" if category["plugin"] else "controls",
            "group": "Plugins" if category["plugin"] else "Controls",
            "depth": 1 if category["plugin"] else 0,
            "description": descriptions.get(category["id"], ""),
        }
        for category in categories
        if category["id"] != "foundation"
    ]
    routes = []
    non_components_by_id = {route["id"]: dict(route) for route in NON_COMPONENT_ROUTES}
    for route in non_components_by_id.values():
        route["description"] = descriptions.get(route["id"], "")
        route["group"] = ("Root" if route["id"] == "home" else
                          "Footer" if route["id"] == "settings" else
                          "Controls" if route["id"] == "all-controls" else "Foundation")
        route["depth"] = 1 if route["parent_id"] == "foundation" else 0
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
            next(component for component in components if component["id"] == "font-icon"),
            category_metadata["foundation"],
        )
    )
    routes.append(non_components_by_id["foundation-geometry"])
    routes.append(non_components_by_id["foundation-spacing"])
    plugin_pages = [page for page in category_pages if page["parent_id"] == "plugins"]
    if plugin_pages:
        routes.append({
            "id": "plugins", "title": "Plugins", "kind": "category", "parent_id": "",
            "group": "Plugins", "depth": 0, "description": descriptions.get("plugins", ""),
        })
        for category_page in plugin_pages:
            routes.append(category_page)
            routes.extend(
                _component_route(component, category_metadata[category_page["id"]])
                for component in components
                if component["category_id"] == category_page["id"]
            )
    routes.append(non_components_by_id["all-controls"])
    for category_page in category_pages:
        if category_page["parent_id"] == "plugins":
            continue
        routes.append(category_page)
        routes.extend(
            _component_route(component, category_metadata[category_page["id"]])
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
    optional_classes = {
        name for module in api_manifest.get("optional_modules", {}).values()
        for name in module["classes"]
    }
    missing_bindings = sorted(routed_types - manifest_classes - optional_classes)
    if missing_bindings:
        raise ValueError(
            "native Gallery component types are absent from the binding manifest: {0}"
            .format(", ".join(missing_bindings))
        )
    support_types = sorted(manifest_classes - routed_types)
    expected_counts = (96, 277, 122) if include_cpp_only else (92, 257, 114)
    for name, actual, expected in zip(
        ("component", "sample", "route"),
        (component_count, sample_count, len(routes)),
        expected_counts,
    ):
        if actual != expected:
            raise ValueError(
                f"Gallery {name} count changed from {expected} to {actual}; review the contract"
            )

    return {
        "schema_version": 1,
        "canonical_source": "app/view/widgets/samples",
        "categories": categories,
        "components": components,
        "foundation_samples": {
            "foundation-qmlplus": _samples_in_body(
                (project_root / "app/view/widgets/samples/QmlPlusSamples.cpp").read_text(
                    encoding="utf-8"
                )
            ),
        },
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
