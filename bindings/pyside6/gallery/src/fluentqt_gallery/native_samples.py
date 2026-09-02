"""Standalone Gallery ports of canonical C++ SampleCards.

Every registered builder owns one or more exact ``(route_id, sample_id)`` keys.
Returning a ``PreviewResult`` with ``native-equivalent`` means the preview and
the displayed Python source exercise the same public capability as the native
card.  Unregistered cards intentionally fall back to ``component-smoke`` and
are rejected by the parity acceptance test.
"""

from __future__ import annotations

import ast
import copy
from collections.abc import Callable, Mapping, Sequence
import json
import keyword
from pathlib import Path
import re
from textwrap import dedent

import fluentqt
from PySide6.QtCore import QDate, QItemSelectionModel, QRect, QRectF, QSize, Qt, QTime
from PySide6.QtGui import (
    QColor,
    QFont,
    QGuiApplication,
    QPainter,
    QPainterPath,
    QPixmap,
    QStandardItem,
    QStandardItemModel,
)
from PySide6.QtWidgets import (
    QHBoxLayout,
    QListView,
    QSizePolicy,
    QStyle,
    QStyledItemDelegate,
    QVBoxLayout,
    QWidget,
)

from .catalog import SAMPLE_BY_KEY
from .samples import PreviewResult


Builder = Callable[[str, QWidget | None], PreviewResult]
_BUILDERS: dict[tuple[str, str], Builder] = {}
SourceSpec = tuple[str, str]


def _apply_root_layout_contract(
    route_id: str,
    sample_id: str,
    source: str,
    widget_name: str,
) -> str:
    """Give source-driven roots the zero-margin C++ sample-group contract."""

    sample = SAMPLE_BY_KEY[(route_id, sample_id)]
    orientation = sample.preview_orientation
    expected_layout_type = {
        "horizontal": "QHBoxLayout",
        "vertical": "QVBoxLayout",
    }.get(orientation)
    if expected_layout_type is None:
        return source
    expected_alignment = {
        "horizontal": (
            "Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter"
        ),
        "vertical": (
            "Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignLeft"
        ),
    }[orientation]
    pattern = re.compile(
        r"^(?P<layout>[A-Za-z_][A-Za-z0-9_]*) = "
        r"Q(?:HBox|VBox)Layout\({0}\)$".format(re.escape(widget_name)),
        re.MULTILINE,
    )
    match = pattern.search(source)
    if match is None:
        return source
    layout_name = match.group("layout")
    replacement = "{0} = {1}({2})\n".format(
        layout_name, expected_layout_type, widget_name
    )
    replacement += "{0}.setContentsMargins(0, 0, 0, 0)\n".format(layout_name)
    replacement += "{0}.setSpacing({1})".format(
        layout_name, sample.preview_spacing
    )
    replacement += "\n{0}.setAlignment({1})".format(
        layout_name, expected_alignment
    )
    qt_core_imports = re.findall(
        r"from\s+PySide6\.QtCore\s+import\s+(?:\([^)]*\)|[^\n]*)",
        source,
        flags=re.MULTILINE,
    )
    if not any(re.search(r"\bQt\b", imported) for imported in qt_core_imports):
        source = "from PySide6.QtCore import Qt\n" + source
    return pattern.sub(replacement, source, count=1)


def _apply_root_parent_contract(source: str, widget_name: str) -> str:
    """Construct QWidget sample roots with the C++ preview parent immediately.

    Reparenting an already-polished widget tree can leave native child effects
    with stale source offsets.  The C++ Gallery passes the preview host into
    every sample factory up front, so source-driven Python samples do the same
    while remaining executable without a Gallery host.
    """

    pattern = re.compile(
        r"^{0} = QWidget\(\)$".format(re.escape(widget_name)),
        re.MULTILINE,
    )
    return pattern.sub(
        "{0} = QWidget(globals().get('gallery_parent'))".format(widget_name),
        source,
        count=1,
    )


def native_samples(route_id: str, *sample_ids: str):
    """Register one builder for exact native SampleCard ids."""

    def decorate(builder: Builder) -> Builder:
        for sample_id in sample_ids:
            key = (route_id, sample_id)
            if key in _BUILDERS:
                raise RuntimeError("duplicate Python Gallery sample builder: {0}".format(key))
            _BUILDERS[key] = builder
        return builder

    return decorate


def _source(*lines: str, imports: str = "") -> str:
    prefix = "import fluentqt\n"
    if imports:
        prefix += imports.rstrip() + "\n"
    return prefix + "\n" + "\n".join(lines) + "\n"


def _load_catalog_names_by_glyph() -> dict[str, str]:
    """Map legacy private-use glyphs to stable public catalog names."""

    path = Path(__file__).with_name("assets") / "icon_aliases.json"
    try:
        aliases = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return {
        chr(int(codepoint, 16)): str(name)
        for codepoint, name in aliases.items()
    }


_CATALOG_NAME_BY_GLYPH = _load_catalog_names_by_glyph()
_SEMANTIC_ICON_EXPRESSION_BY_CATALOG = {}
for _icon_alias, _catalog_name in vars(fluentqt.Typography.Icons).items():
    if _icon_alias.startswith("_") or not isinstance(_catalog_name, str):
        continue
    _SEMANTIC_ICON_EXPRESSION_BY_CATALOG.setdefault(
        _catalog_name,
        "fluentqt.Typography.Icons.{0}".format(_icon_alias),
    )
_PRIVATE_USE_ICON_LITERAL = re.compile(
    r"(?P<quote>['\"])(?P<value>\\u[eEfF][0-9A-Fa-f]{3}|"
    "[\uE000-\uF8FF])(?P=quote)"
)
_CATALOG_ICON_LITERAL = re.compile(
    r"(?P<quote>['\"])(?P<value>ic_fluent_[a-z0-9_]+)(?P=quote)"
)


def _semantic_icon_source(source: str) -> str:
    """Use public semantic icon constants in teaching snippets."""

    def replace_private_use(match: re.Match[str]) -> str:
        value = match.group("value")
        glyph = chr(int(value[2:], 16)) if value.startswith("\\u") else value
        catalog_name = _CATALOG_NAME_BY_GLYPH.get(glyph)
        expression = _SEMANTIC_ICON_EXPRESSION_BY_CATALOG.get(catalog_name, "")
        if not expression:
            return match.group(0)
        return expression

    def replace_catalog_name(match: re.Match[str]) -> str:
        expression = _SEMANTIC_ICON_EXPRESSION_BY_CATALOG.get(
            match.group("value"), ""
        )
        return expression or match.group(0)

    source = _PRIVATE_USE_ICON_LITERAL.sub(replace_private_use, source)
    return _CATALOG_ICON_LITERAL.sub(replace_catalog_name, source)


def _align_cpp_icon_names(route_id: str, sample_id: str, source: str) -> str:
    """Prefer the exact semantic icon names taught by the C++ card."""

    sample = SAMPLE_BY_KEY[(route_id, sample_id)]
    preferred_names = set(
        re.findall(r"Typography::Icons::([A-Za-z0-9_]+)", sample.cpp_snippet)
    )
    preferred_by_catalog: dict[str, str] = {}
    ambiguous_catalogs: set[str] = set()
    for name in preferred_names:
        catalog_name = getattr(fluentqt.Typography.Icons, name, None)
        if not isinstance(catalog_name, str):
            continue
        previous = preferred_by_catalog.get(catalog_name)
        if previous is not None and previous != name:
            ambiguous_catalogs.add(catalog_name)
        else:
            preferred_by_catalog[catalog_name] = name
    for catalog_name in ambiguous_catalogs:
        preferred_by_catalog.pop(catalog_name, None)

    pattern = re.compile(
        r"fluentqt\.Typography\.Icons\.([A-Za-z0-9_]+)"
    )

    def replace(match: re.Match[str]) -> str:
        current_name = match.group(1)
        catalog_name = getattr(
            fluentqt.Typography.Icons,
            current_name,
            None,
        )
        preferred_name = preferred_by_catalog.get(catalog_name, current_name)
        return "fluentqt.Typography.Icons.{0}".format(preferred_name)

    return pattern.sub(replace, source)


def _assigned_names(node: ast.AST) -> set[str]:
    names: set[str] = set()
    for child in ast.walk(node):
        if isinstance(child, ast.Name) and isinstance(child.ctx, ast.Store):
            names.add(child.id)
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
        names.add(node.name)
    return names


def _format_import(node: ast.Import | ast.ImportFrom) -> str:
    rendered = ast.unparse(node)
    if len(rendered) <= 88 or not isinstance(node, ast.ImportFrom):
        return rendered
    module = "." * node.level + (node.module or "")
    names = []
    for alias in node.names:
        value = alias.name
        if alias.asname:
            value += " as " + alias.asname
        names.append("    " + value + ",")
    return "from {0} import (\n{1}\n)".format(module, "\n".join(names))


def _imported_name(alias: ast.alias, from_import: bool) -> str:
    """Return the local name introduced by one import alias."""

    if alias.asname:
        return alias.asname
    if from_import:
        return alias.name
    return alias.name.split(".", 1)[0]


def _normalize_display_imports(source: str) -> str:
    """Keep only used imports and put the FluentQt entry point first."""

    try:
        module = ast.parse(source)
    except SyntaxError:
        return source

    loaded_names = {
        node.id
        for node in ast.walk(module)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }
    future_imports: list[str] = []
    regular_imports: list[str] = []
    import_lines: set[int] = set()
    for statement in module.body:
        if not isinstance(statement, (ast.Import, ast.ImportFrom)):
            continue
        import_lines.update(
            range(statement.lineno - 1, statement.end_lineno or statement.lineno)
        )
        if isinstance(statement, ast.Import):
            aliases: list[ast.alias] = []
            for alias in statement.names:
                if alias.name == "fluentqt" and alias.asname is None:
                    continue
                if _imported_name(alias, False) in loaded_names:
                    aliases.append(alias)
            if aliases:
                regular_imports.append(
                    _format_import(ast.Import(names=aliases))
                )
            continue

        aliases = [
            alias
            for alias in statement.names
            if alias.name == "*"
            or _imported_name(alias, True) in loaded_names
        ]
        if not aliases:
            continue
        rendered = _format_import(
            ast.ImportFrom(
                module=statement.module,
                names=aliases,
                level=statement.level,
            )
        )
        if statement.module == "__future__":
            future_imports.append(rendered)
        else:
            regular_imports.append(rendered)

    body_lines = [
        line
        for index, line in enumerate(source.splitlines())
        if index not in import_lines
    ]
    body = "\n".join(body_lines).strip()
    imports = list(dict.fromkeys(future_imports))
    imports.extend(sorted(dict.fromkeys(regular_imports)))
    return "\n\n".join(
        part for part in ("\n".join(imports), body) if part
    ).rstrip() + "\n"


_COMPACT_DISPLAY_STATEMENTS = (
    ast.AnnAssign,
    ast.Assign,
    ast.Expr,
    ast.Return,
)

_DISPLAY_PREFERRED_LINE_LENGTH = 84
_DISPLAY_HARD_LINE_LENGTH = 88
_CPP_ALIGNMENT_MIN_CALL_LENGTH = 68
_CPP_ALIGNMENT_IGNORED_CALLS = frozenset(("connect",))


def _display_statement_call(statement: ast.stmt) -> ast.Call | None:
    """Return the call taught by a compactable simple statement."""

    if isinstance(statement, ast.Expr) and isinstance(statement.value, ast.Call):
        return statement.value
    if isinstance(statement, (ast.Assign, ast.AnnAssign)) and isinstance(
        statement.value, ast.Call
    ):
        return statement.value
    if isinstance(statement, ast.Return) and isinstance(statement.value, ast.Call):
        return statement.value
    return None


def _same_statement(left: str, right: ast.stmt) -> bool:
    """Check that a formatting candidate preserves the statement AST."""

    try:
        parsed = ast.parse(left)
    except SyntaxError:
        return False
    return (
        len(parsed.body) == 1
        and ast.dump(parsed.body[0], include_attributes=False)
        == ast.dump(right, include_attributes=False)
    )


def _compact_statement_text(
    source: str,
    statement: ast.stmt,
    *,
    maximum_length: int = _DISPLAY_PREFERRED_LINE_LENGTH,
) -> str | None:
    """Collapse a short call without changing its authored string literals."""

    segment = ast.get_source_segment(source, statement)
    if segment is None or "#" in segment:
        return None
    if any(
        isinstance(node, ast.Constant)
        and isinstance(node.value, str)
        and "\n" in node.value
        for node in ast.walk(statement)
    ):
        return None

    joined = " ".join(line.strip() for line in segment.splitlines())
    joined = re.sub(r"([([{])\s+", r"\1", joined)
    joined = re.sub(r"\s+([)\]}])", r"\1", joined)
    without_trailing_commas = re.sub(r",([)\]}])", r"\1", joined)
    candidates = (without_trailing_commas, joined, ast.unparse(statement))
    for candidate in dict.fromkeys(candidates):
        if "\n" in candidate:
            continue
        if statement.col_offset + len(candidate) > maximum_length:
            continue
        if _same_statement(candidate, statement):
            return " " * statement.col_offset + candidate
    return None


def _compact_expression_text(source: str, expression: ast.AST) -> str:
    """Return one semantic expression line while preserving authored quotes."""

    segment = ast.get_source_segment(source, expression)
    candidates: list[str] = []
    if segment is not None and "#" not in segment:
        joined = " ".join(line.strip() for line in segment.splitlines())
        joined = re.sub(r"([([{])\s+", r"\1", joined)
        joined = re.sub(r"\s+([)\]}])", r"\1", joined)
        candidates.extend((re.sub(r",([)\]}])", r"\1", joined), joined))
    candidates.append(ast.unparse(expression))

    expected = ast.dump(expression, include_attributes=False)
    for candidate in dict.fromkeys(candidates):
        if "\n" in candidate:
            continue
        try:
            parsed = ast.parse(candidate, mode="eval")
        except SyntaxError:
            continue
        if ast.dump(parsed.body, include_attributes=False) == expected:
            return candidate
    return ast.unparse(expression)


def _display_statement_prefix(statement: ast.stmt) -> str | None:
    if isinstance(statement, ast.Expr):
        return ""
    if isinstance(statement, ast.Assign):
        return " = ".join(ast.unparse(target) for target in statement.targets) + " = "
    if isinstance(statement, ast.AnnAssign):
        prefix = "{0}: {1}".format(
            ast.unparse(statement.target),
            ast.unparse(statement.annotation),
        )
        return prefix + " = " if statement.value is not None else None
    if isinstance(statement, ast.Return):
        return "return "
    return None


def _display_call_name(call: ast.Call) -> str:
    if isinstance(call.func, ast.Attribute):
        return call.func.attr
    if isinstance(call.func, ast.Name):
        return call.func.id
    return ""


def _mask_cpp_non_code(source: str) -> str:
    """Blank strings/comments while retaining C++ line and column positions."""

    masked = list(source)
    index = 0
    state = "code"
    quote = ""
    while index < len(source):
        current = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if current == "/" and following == "/":
                masked[index] = masked[index + 1] = " "
                state = "line-comment"
                index += 2
                continue
            if current == "/" and following == "*":
                masked[index] = masked[index + 1] = " "
                state = "block-comment"
                index += 2
                continue
            if current in {'"', "'"}:
                quote = current
                masked[index] = " "
                state = "string"
            index += 1
            continue
        if state == "line-comment":
            if current == "\n":
                state = "code"
            else:
                masked[index] = " "
            index += 1
            continue
        if state == "block-comment":
            if current == "*" and following == "/":
                masked[index] = masked[index + 1] = " "
                state = "code"
                index += 2
            else:
                if current != "\n":
                    masked[index] = " "
                index += 1
            continue
        if current == "\\" and following:
            masked[index] = " "
            if following != "\n":
                masked[index + 1] = " "
            index += 2
            continue
        if current == quote:
            masked[index] = " "
            state = "code"
        elif current != "\n":
            masked[index] = " "
        index += 1
    return "".join(masked)


def _cpp_wrapped_call_names(source: str) -> frozenset[str]:
    """Return C++ calls whose canonical card intentionally spans lines."""

    masked = _mask_cpp_non_code(source)
    names: set[str] = set()
    pattern = re.compile(r"\b(?:new\s+)?([A-Za-z_]\w*)\s*\(")
    for match in pattern.finditer(masked):
        name = match.group(1)
        if name in _CPP_ALIGNMENT_IGNORED_CALLS:
            continue
        opening = masked.find("(", match.start(), match.end())
        depth = 0
        closing = -1
        for index in range(opening, len(masked)):
            current = masked[index]
            if current == "(":
                depth += 1
            elif current == ")":
                depth -= 1
                if depth == 0:
                    closing = index
                    break
        if closing >= 0 and "\n" in masked[opening:closing]:
            names.add(name)
    return frozenset(names)


def _format_display_expression(
    source: str,
    expression: ast.AST,
    *,
    indent: int,
    prefix: str = "",
    cpp_wrapped_calls: frozenset[str] = frozenset(),
) -> list[str]:
    compact = _compact_expression_text(source, expression)
    one_line = " " * indent + prefix + compact
    if len(one_line) <= _DISPLAY_PREFERRED_LINE_LENGTH:
        return [one_line]

    if isinstance(expression, ast.Call):
        return _format_display_call(
            source,
            expression,
            indent=indent,
            prefix=prefix,
            cpp_wrapped_calls=cpp_wrapped_calls,
        )
    if isinstance(expression, ast.Lambda) and isinstance(expression.body, ast.Call):
        lambda_head = compact.partition(":")[0] + ": "
        return _format_display_call(
            source,
            expression.body,
            indent=indent,
            prefix=prefix + lambda_head,
            cpp_wrapped_calls=cpp_wrapped_calls,
        )

    segment = ast.get_source_segment(source, expression)
    if segment is not None and "\n" in segment:
        authored = dedent(segment).strip().splitlines()
        return [
            " " * indent + (prefix if index == 0 else "") + line.strip()
            for index, line in enumerate(authored)
        ]
    return [one_line]


def _format_display_call(
    source: str,
    call: ast.Call,
    *,
    indent: int,
    prefix: str = "",
    cpp_wrapped_calls: frozenset[str] = frozenset(),
) -> list[str]:
    compact = _compact_expression_text(source, call)
    one_line = " " * indent + prefix + compact
    call_name = _display_call_name(call)
    align_with_cpp = (
        call_name in cpp_wrapped_calls
        and len(one_line) >= _CPP_ALIGNMENT_MIN_CALL_LENGTH
    )
    if (
        len(one_line) <= _DISPLAY_PREFERRED_LINE_LENGTH
        and not align_with_cpp
    ):
        return [one_line]

    arguments: list[tuple[str, ast.AST]] = [("", value) for value in call.args]
    arguments.extend(
        (
            "**" if keyword_node.arg is None else keyword_node.arg + "=",
            keyword_node.value,
        )
        for keyword_node in call.keywords
    )
    if not arguments:
        return [one_line]

    function = _compact_expression_text(source, call.func)
    lines = [" " * indent + prefix + function + "("]
    for index, (argument_prefix, argument) in enumerate(arguments):
        argument_lines = _format_display_expression(
            source,
            argument,
            indent=indent + 4,
            prefix=argument_prefix,
            cpp_wrapped_calls=cpp_wrapped_calls,
        )
        if index + 1 < len(arguments):
            argument_lines[-1] += ","
        lines.extend(argument_lines)
    lines.append(" " * indent + ")")
    return lines


def _format_display_statement(
    source: str,
    statement: ast.stmt,
    cpp_wrapped_calls: frozenset[str],
) -> str | None:
    call = _display_statement_call(statement)
    prefix = _display_statement_prefix(statement)
    segment = ast.get_source_segment(source, statement)
    if call is None or prefix is None or segment is None or "#" in segment:
        return None
    if any(
        isinstance(node, ast.Constant)
        and isinstance(node.value, str)
        and "\n" in node.value
        for node in ast.walk(statement)
    ):
        return None

    compact = _compact_statement_text(source, statement)
    call_name = _display_call_name(call)
    align_with_cpp = (
        call_name in cpp_wrapped_calls
        and compact is not None
        and len(compact) >= _CPP_ALIGNMENT_MIN_CALL_LENGTH
    )
    if compact is not None and not align_with_cpp:
        return compact

    lines = _format_display_call(
        source,
        call,
        indent=statement.col_offset,
        prefix=prefix,
        cpp_wrapped_calls=cpp_wrapped_calls,
    )
    formatted = "\n".join(lines)
    if max(map(len, lines), default=0) > _DISPLAY_HARD_LINE_LENGTH:
        return None
    if not _same_statement(dedent(formatted), statement):
        return None
    return formatted


def _format_display_calls(
    source: str,
    cpp_wrapped_calls: frozenset[str] = frozenset(),
) -> str:
    """Compact short calls and wrap long calls at semantic argument edges."""

    try:
        module = ast.parse(source)
    except SyntaxError:
        return source

    replacements: list[tuple[int, int, str]] = []
    for statement in ast.walk(module):
        if not isinstance(statement, _COMPACT_DISPLAY_STATEMENTS):
            continue
        if statement.end_lineno is None:
            continue
        formatted = _format_display_statement(
            source,
            statement,
            cpp_wrapped_calls,
        )
        if formatted is not None:
            replacements.append(
                (statement.lineno - 1, statement.end_lineno, formatted)
            )

    lines = source.splitlines()
    for start, end, formatted in sorted(replacements, reverse=True):
        lines[start:end] = formatted.splitlines()
    return "\n".join(lines).rstrip() + "\n"


def _call_family(statement: ast.stmt) -> str:
    if not isinstance(statement, ast.Expr) or not isinstance(statement.value, ast.Call):
        return ""
    return ast.unparse(statement.value.func)


def _normalize_call_group_spacing(source: str) -> str:
    """Remove extraction gaps between repeated calls on the same receiver."""

    try:
        module = ast.parse(source)
    except SyntaxError:
        return source
    lines = source.splitlines()
    remove: set[int] = set()
    for parent in ast.walk(module):
        body = getattr(parent, "body", None)
        if not isinstance(body, list):
            continue
        for previous, current in zip(body, body[1:]):
            previous_family = _call_family(previous)
            if not previous_family or previous_family != _call_family(current):
                continue
            start = previous.end_lineno or previous.lineno
            end = current.lineno - 1
            if start < end and all(not lines[index].strip() for index in range(start, end)):
                remove.update(range(start, end))
    return "\n".join(
        line for index, line in enumerate(lines) if index not in remove
    ).rstrip() + "\n"


def _format_display_source(source: str, canonical_cpp: str = "") -> str:
    """Apply the shared, C++-aligned style used by Python teaching blocks."""

    normalized = _normalize_display_imports(source)
    formatted = _format_display_calls(
        normalized,
        _cpp_wrapped_call_names(canonical_cpp),
    )
    return _normalize_call_group_spacing(formatted)


def _strip_gallery_parent(source: str) -> str:
    parent = r"globals\(\)\.get\((['\"])gallery_parent\1\)"
    source = re.sub(r",\s*" + parent, "", source)
    source = re.sub(parent + r"\s*,\s*", "", source)
    return re.sub(parent, "", source)


def _snake_case(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def _call_receiver(call: ast.Call) -> tuple[str, str] | None:
    if not isinstance(call.func, ast.Attribute):
        return None
    method = call.func.attr
    value: ast.AST = call.func.value
    while isinstance(value, (ast.Attribute, ast.Call)):
        if isinstance(value, ast.Attribute):
            value = value.value
        elif isinstance(value.func, ast.Attribute):
            value = value.func.value
        else:
            break
    if isinstance(value, ast.Name):
        return value.id, method
    return None


def _covered_constructor_names(
    node: ast.AST, covered_types: Sequence[str]
) -> set[str]:
    covered = set(covered_types)
    names: set[str] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign)):
            continue
        value = child.value
        if not isinstance(value, ast.Call):
            continue
        function = value.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr in covered
        ):
            continue
        names.update(_assigned_names(child))
    return names


def _focused_short_function(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
    covered_types: Sequence[str],
    canonical_members: set[str],
) -> ast.FunctionDef | ast.AsyncFunctionDef:
    """Keep only canonical component operations from a long popup helper."""

    covered = set(covered_types)
    local_targets: set[str] = set()
    constructor_statements: set[int] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign)):
            continue
        value = child.value
        if not isinstance(value, ast.Call):
            continue
        function = value.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr in covered
        ):
            continue
        local_targets.update(_assigned_names(child))
        constructor_statements.add(id(child))
    if not local_targets:
        return node

    kept: list[ast.stmt] = []
    seen: set[int] = set()
    for child in ast.walk(node):
        if not isinstance(child, (ast.Assign, ast.AnnAssign, ast.Expr)):
            continue
        if id(child) in seen:
            continue
        names = {
            item.id
            for item in ast.walk(child)
            if isinstance(item, ast.Name)
        }
        attributes = {
            item.attr
            for item in ast.walk(child)
            if isinstance(item, ast.Attribute)
        }
        if id(child) not in constructor_statements and not (
            names & local_targets and attributes & canonical_members
        ):
            continue
        kept.append(child)
        seen.add(id(child))
    kept.sort(key=lambda statement: (statement.lineno, statement.col_offset))
    if not kept:
        return node
    focused = type(node)(
        name=node.name,
        args=node.args,
        body=kept,
        decorator_list=node.decorator_list,
        returns=node.returns,
        type_comment=getattr(node, "type_comment", None),
    )
    ast.copy_location(focused, node)
    if len(ast.unparse(focused).splitlines()) >= len(
        ast.unparse(node).splitlines()
    ):
        return node
    return focused


def _focused_canonical_function(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
    canonical_members: set[str],
) -> ast.FunctionDef | ast.AsyncFunctionDef:
    """Keep the public operations represented by the matching C++ lambda."""

    selected_indexes: set[int] = set()
    definitions: dict[str, int] = {}
    loads: list[set[str]] = []
    for index, statement in enumerate(node.body):
        for name in _assigned_names(statement):
            definitions.setdefault(name, index)
        loads.append(
            {
                child.id
                for child in ast.walk(statement)
                if isinstance(child, ast.Name)
                and isinstance(child.ctx, ast.Load)
            }
        )
        attributes = {
            child.attr
            for child in ast.walk(statement)
            if isinstance(child, ast.Attribute)
        }
        if attributes & canonical_members:
            selected_indexes.add(index)
    if not selected_indexes:
        return node

    changed = True
    while changed:
        changed = False
        dependencies = {
            name
            for index in selected_indexes
            for name in loads[index]
            if name in definitions
        }
        for name in dependencies:
            index = definitions[name]
            if index not in selected_indexes:
                selected_indexes.add(index)
                changed = True
    if len(selected_indexes) == len(node.body):
        return node

    focused = type(node)(
        name=node.name,
        args=node.args,
        body=[
            statement
            for index, statement in enumerate(node.body)
            if index in selected_indexes
        ],
        decorator_list=node.decorator_list,
        returns=node.returns,
        type_comment=getattr(node, "type_comment", None),
    )
    ast.copy_location(focused, node)
    return focused


def _focused_canonical_compound(
    node: ast.stmt,
    covered_types: Sequence[str],
    canonical_members: set[str],
    helper_names: set[str],
    tracked_names: set[str],
    ignored_names: set[str],
) -> ast.stmt:
    """Remove preview-only rows and layouts from a selected compound block."""

    body = getattr(node, "body", None)
    if not isinstance(body, list) or not body:
        return node

    tracked = set(tracked_names)
    if isinstance(node, (ast.For, ast.AsyncFor)):
        iterator_loads = {
            child.id
            for child in ast.walk(node.iter)
            if isinstance(child, ast.Name) and isinstance(child.ctx, ast.Load)
        }
        if iterator_loads & tracked:
            tracked.update(_assigned_names(node.target))
    for statement in body:
        tracked.update(_covered_constructor_names(statement, covered_types))

    definitions: dict[str, int] = {}
    loads: list[set[str]] = []
    calls: list[set[tuple[str, str]]] = []
    attributes: list[set[str]] = []
    helper_calls: list[set[str]] = []
    selected: set[int] = set()
    for index, statement in enumerate(body):
        for name in _assigned_names(statement):
            definitions.setdefault(name, index)
        loads.append(
            {
                child.id
                for child in ast.walk(statement)
                if isinstance(child, ast.Name)
                and isinstance(child.ctx, ast.Load)
            }
        )
        calls.append(
            {
                receiver
                for child in ast.walk(statement)
                if isinstance(child, ast.Call)
                for receiver in [_call_receiver(child)]
                if receiver is not None
            }
        )
        attributes.append(
            {
                child.attr
                for child in ast.walk(statement)
                if isinstance(child, ast.Attribute)
            }
        )
        helper_calls.append(
            {
                child.func.id
                for child in ast.walk(statement)
                if isinstance(child, ast.Call)
                and isinstance(child.func, ast.Name)
                and child.func.id in helper_names
            }
        )
        if _covered_constructor_names(statement, covered_types):
            selected.add(index)

    changed = True
    while changed:
        changed = False
        for index, statement in enumerate(body):
            receiver_match = any(
                receiver in tracked and member in canonical_members
                for receiver, member in calls[index]
            ) or bool(
                attributes[index] & canonical_members
                and loads[index] & tracked
            )
            helper_match = bool(helper_calls[index] and loads[index] & tracked)
            definition_match = bool(_assigned_names(statement) & tracked)
            compound_match = isinstance(
                statement,
                (
                    ast.FunctionDef,
                    ast.AsyncFunctionDef,
                    ast.For,
                    ast.AsyncFor,
                    ast.While,
                    ast.If,
                    ast.With,
                ),
            ) and bool(loads[index] & tracked) and any(
                member in canonical_members
                for _receiver, member in calls[index]
            )
            if (
                receiver_match
                or helper_match
                or definition_match
                or compound_match
            ) and index not in selected:
                selected.add(index)
                changed = True

        dependencies = {
            name
            for index in selected
            for name in loads[index]
            if name in definitions and name not in ignored_names
        }
        for name in dependencies:
            index = definitions[name]
            if index not in selected:
                selected.add(index)
                tracked.add(name)
                changed = True

        selected_definitions = {
            name
            for index in selected
            for name in _assigned_names(body[index])
        }
        for index, statement_calls in enumerate(calls):
            if index in selected or not (loads[index] & selected_definitions):
                continue
            if any(member == "connect" for _receiver, member in statement_calls):
                selected.add(index)
                changed = True

    if not selected or len(selected) == len(body):
        return node

    focused = copy.deepcopy(node)
    focused.body = [
        _focused_canonical_compound(
            body[index],
            covered_types,
            canonical_members,
            helper_names,
            tracked,
            ignored_names,
        )
        for index in sorted(selected)
    ]
    ast.copy_location(focused, node)
    return focused


def _calls_named_helper(statement: ast.stmt, helper_names: set[str]) -> bool:
    if not isinstance(statement, (ast.Assign, ast.AnnAssign)):
        return False
    value = statement.value
    return (
        isinstance(value, ast.Call)
        and isinstance(value.func, ast.Name)
        and value.func.id in helper_names
    )


class _PreviewParentStripper(ast.NodeTransformer):
    """Remove Gallery container parents from public component constructors."""

    def __init__(self, preview_parents: Sequence[str] = ()) -> None:
        super().__init__()
        self._preview_parents = {
            "root",
            "surface",
            "panel",
            *preview_parents,
        }

    def visit_Call(self, node: ast.Call) -> ast.AST:
        self.generic_visit(node)
        function = node.func
        if not (
            isinstance(function, ast.Attribute)
            and isinstance(function.value, ast.Name)
            and function.value.id == "fluentqt"
            and function.attr[:1].isupper()
        ):
            return node
        node.args = [
            argument
            for argument in node.args
            if not (
                isinstance(argument, ast.Name)
                and argument.id in self._preview_parents
            )
        ]
        return node


def _dedent_source_segment(
    segment: str,
    base_indent: int | None = None,
) -> str:
    """Remove AST context indentation while retaining relative wrapping."""

    lines = segment.strip().splitlines()
    if len(lines) <= 1:
        return lines[0].strip() if lines else ""
    if base_indent is None:
        tail = dedent("\n".join(lines[1:])).rstrip()
    else:
        tail = "\n".join(
            line[base_indent:]
            if line[:base_indent].isspace()
            else line
            for line in lines[1:]
        ).rstrip()
    return lines[0].lstrip() + "\n" + tail


def _equivalent_source_statement(source: str, node: ast.stmt) -> str | None:
    """Return the original formatting when it still represents ``node``."""

    segment = ast.get_source_segment(source, node)
    if segment is None:
        return None
    segment = _dedent_source_segment(segment, node.col_offset)
    try:
        parsed = ast.parse(segment)
    except SyntaxError:
        return None
    if len(parsed.body) != 1:
        return None
    if ast.dump(parsed.body[0], include_attributes=False) != ast.dump(
        node, include_attributes=False
    ):
        return None
    return segment


def _render_outline_statement(
    source: str,
    statement: ast.stmt,
    preview_parents: Sequence[str] = (),
) -> str:
    """Render one selected statement without flattening authored wrapping."""

    transformed = _PreviewParentStripper(preview_parents).visit(
        copy.deepcopy(statement)
    )
    ast.fix_missing_locations(transformed)
    generated = ast.unparse(transformed)
    original = _equivalent_source_statement(source, transformed)
    if (
        original is not None
        and max(map(len, generated.splitlines()), default=0) > 88
        and max(map(len, original.splitlines()), default=0) <= 88
    ):
        return original

    if isinstance(transformed, (ast.FunctionDef, ast.AsyncFunctionDef)):
        signature = copy.deepcopy(transformed)
        signature.body = [ast.Pass()]
        ast.fix_missing_locations(signature)
        signature_lines = ast.unparse(signature).splitlines()
        if signature_lines and signature_lines[-1].strip() == "pass":
            signature_lines.pop()
        body = "\n".join(
            "    " + line if line else line
            for child in transformed.body
            for line in _render_outline_statement(
                source, child, preview_parents
            ).splitlines()
        )
        return "\n".join(signature_lines + ([body] if body else []))

    if isinstance(transformed, (ast.For, ast.AsyncFor)):
        iterator = ast.get_source_segment(source, transformed.iter)
        if iterator is None:
            iterator = ast.unparse(transformed.iter)
        iterator = _dedent_source_segment(iterator)
        target = ast.get_source_segment(source, transformed.target)
        if target is None:
            target = ast.unparse(transformed.target)
        keyword = "async for" if isinstance(transformed, ast.AsyncFor) else "for"
        lines = [
            "{0} {1} in {2}:".format(
                keyword,
                target.strip(),
                iterator,
            )
        ]
        for child in transformed.body:
            lines.extend(
                "    " + line if line else line
                for line in _render_outline_statement(
                    source, child, preview_parents
                ).splitlines()
            )
        if transformed.orelse:
            lines.append("else:")
            for child in transformed.orelse:
                lines.extend(
                    "    " + line if line else line
                    for line in _render_outline_statement(
                        source, child, preview_parents
                    ).splitlines()
                )
        return "\n".join(lines)

    return generated


_PREVIEW_LAYOUT_CONSTRUCTORS = {
    "QFormLayout",
    "QGridLayout",
    "QHBoxLayout",
    "QStackedLayout",
    "QVBoxLayout",
}
_PREVIEW_LAYOUT_METHODS = {
    "addLayout",
    "addSpacing",
    "addStretch",
    "addWidget",
    "setAlignment",
    "setContentsMargins",
    "setSpacing",
    "setStretch",
}

# Python ownership facades make lifetime transfer explicit, while the C++
# Gallery generally teaches the host-owned convenience overload.  Treat both
# spellings as the same component operation when reducing executable preview
# source to the user-facing teaching snippet.
_CPP_DISPLAY_MEMBER_ALIASES = {
    "addItem": ("addOwnedItem",),
    "addPage": ("addOwnedPage",),
    "addPane": ("addOwnedPane",),
    "insertItem": ("insertOwnedItem",),
    "insertPage": ("insertOwnedPage", "addOwnedPage"),
    "insertPane": ("insertOwnedPane",),
    "push": ("pushOwnedItem",),
    "replace": ("replaceOwnedItem",),
    "setContentWidget": ("setOwnedContentWidget",),
    "setEditor": ("setOwnedEditor",),
    "setInitialItem": ("setInitialOwnedItem",),
    "setWidget": ("setOwnedContentWidget",),
}


def _canonical_outline(
    module: ast.Module,
    source: str,
    body_index: int,
    route_id: str,
    sample_id: str,
    covered_types: Sequence[str],
) -> str | None:
    """Select the Python statements matching the canonical C++ snippet."""

    sample = SAMPLE_BY_KEY.get((route_id, sample_id))
    if sample is None:
        return None
    cpp = sample.cpp_snippet
    canonical_members = set(
        re.findall(r"(?:->|\.)([A-Za-z_][A-Za-z0-9_]*)\s*\(", cpp)
    )
    canonical_members.update(
        re.findall(r"(?:->|\.)([A-Za-z_][A-Za-z0-9_]*)\s*=", cpp)
    )
    canonical_members.update(
        re.findall(r"::([a-z][A-Za-z0-9_]*)\b", cpp)
    )
    for member in tuple(canonical_members):
        canonical_members.update(
            _CPP_DISPLAY_MEMBER_ALIASES.get(member, ())
        )
    canonical_members.update(
        _snake_case(name) for name in tuple(canonical_members)
    )
    if not canonical_members:
        return None

    cpp_has_widget = bool(re.search(r"\bQWidget\b", cpp))
    cpp_has_layout = bool(
        re.search(
            r"\bQ(?:Form|Grid|HBox|Stacked|VBox)Layout\b",
            cpp,
        )
    )
    preview_parent_names: set[str] = set()
    preview_only_names: set[str] = set()
    for statement in module.body:
        for child in ast.walk(statement):
            if not isinstance(child, (ast.Assign, ast.AnnAssign)):
                continue
            value = child.value
            if not isinstance(value, ast.Call):
                continue
            function = value.func
            if isinstance(function, ast.Name):
                if function.id == "QWidget" and not cpp_has_widget:
                    names = _assigned_names(child)
                    preview_parent_names.update(names)
                    preview_only_names.update(names)
                elif (
                    function.id in _PREVIEW_LAYOUT_CONSTRUCTORS
                    and not cpp_has_layout
                ):
                    preview_only_names.update(_assigned_names(child))
            elif (
                isinstance(function, ast.Attribute)
                and isinstance(function.value, ast.Name)
                and function.value.id == "fluentqt"
                and function.attr not in covered_types
                and not re.search(
                    r"\b{0}\b".format(re.escape(function.attr)), cpp
                )
            ):
                preview_only_names.update(_assigned_names(child))

    body = module.body[body_index:]
    helper_names = {
        name
        for statement in module.body[:body_index]
        for name in _assigned_names(statement)
        if isinstance(
            statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
        )
    }
    target_names: set[str] = set()
    for statement in body:
        target_names.update(
            _covered_constructor_names(statement, covered_types)
        )

    definitions: dict[str, int] = {}
    loads: list[set[str]] = []
    calls: list[set[tuple[str, str]]] = []
    attributes: list[set[str]] = []
    stored_attributes: list[set[str]] = []
    for index, statement in enumerate(body):
        for name in _assigned_names(statement):
            definitions.setdefault(name, index)
        loads.append(
            {
                node.id
                for node in ast.walk(statement)
                if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
            }
        )
        calls.append(
            {
                receiver
                for node in ast.walk(statement)
                if isinstance(node, ast.Call)
                for receiver in [_call_receiver(node)]
                if receiver is not None
            }
        )
        attributes.append(
            {
                node.attr
                for node in ast.walk(statement)
                if isinstance(node, ast.Attribute)
            }
        )
        stored_attributes.append(
            {
                node.attr
                for node in ast.walk(statement)
                if isinstance(node, ast.Attribute)
                and isinstance(node.ctx, ast.Store)
            }
        )

    receiver_hits: dict[str, int] = {}
    for statement_calls in calls:
        for receiver, member in statement_calls:
            if member in canonical_members and receiver in definitions:
                receiver_hits[receiver] = receiver_hits.get(receiver, 0) + 1
    target_names.update(
        receiver for receiver, count in receiver_hits.items() if count >= 2
    )
    if not target_names:
        return None

    selected: set[int] = {
        index
        for index, statement in enumerate(body)
        if _covered_constructor_names(statement, covered_types)
    }
    tracked = set(target_names)
    ignored_dependencies = {
        "root",
        "surface",
        "panel",
        "controls",
        "status",
        "layout",
        *preview_only_names,
    }
    changed = True
    while changed:
        changed = False
        for index, statement in enumerate(body):
            receiver_match = any(
                receiver in tracked and member in canonical_members
                for receiver, member in calls[index]
            ) or bool(
                attributes[index] & canonical_members
                and loads[index] & tracked
            )
            compound_match = isinstance(
                statement,
                (ast.FunctionDef, ast.AsyncFunctionDef, ast.For, ast.While),
            ) and bool(loads[index] & tracked) and any(
                member in canonical_members for _receiver, member in calls[index]
            )
            definition_match = bool(_assigned_names(statement) & tracked)
            attribute_store_match = bool(
                stored_attributes[index] & canonical_members
                and loads[index] & tracked
            )
            if definition_match and _calls_named_helper(statement, helper_names):
                definition_match = False
            if (
                receiver_match
                or compound_match
                or definition_match
                or attribute_store_match
            ):
                if index not in selected:
                    selected.add(index)
                    changed = True

        dependency_names: set[str] = set()
        selected_definitions: set[str] = set()
        for index in selected:
            dependency_names.update(loads[index])
            selected_definitions.update(_assigned_names(body[index]))
        for name in dependency_names:
            lowered = name.lower()
            if name not in definitions:
                continue
            if name in ignored_dependencies or "layout" in lowered:
                continue
            if name not in tracked:
                tracked.add(name)
                changed = True
        for index, statement in enumerate(body):
            if index in selected:
                continue
            if not (loads[index] & selected_definitions):
                continue
            if any(member == "connect" for _receiver, member in calls[index]):
                selected.add(index)
                changed = True

    if not selected:
        return None
    cpp_lines = len(cpp.splitlines())
    selected_nodes: list[ast.stmt] = []
    for index in sorted(selected):
        statement = body[index]
        if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if cpp_lines < 16:
                statement = _focused_short_function(
                    statement, covered_types, canonical_members
                )
            statement = _focused_canonical_function(
                statement, canonical_members
            )
        statement = _focused_canonical_compound(
            statement,
            covered_types,
            canonical_members,
            helper_names,
            tracked,
            preview_only_names,
        )
        selected_nodes.append(statement)

    used_names = {
        node.id
        for statement in selected_nodes
        for node in ast.walk(statement)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }
    imports: list[str] = []
    for statement in module.body:
        if isinstance(statement, ast.Import):
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name.split(".", 1)[0]) in used_names
                or alias.name == "fluentqt"
            ]
            if aliases:
                imports.append(_format_import(ast.Import(names=aliases)))
        elif isinstance(statement, ast.ImportFrom):
            module_name = statement.module or ""
            if (
                module_name.startswith("fluentqt_gallery")
                and module_name != "fluentqt_gallery.metrics"
            ):
                continue
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name) in used_names
            ]
            if aliases:
                imports.append(
                    _format_import(
                        ast.ImportFrom(
                            module=statement.module,
                            names=aliases,
                            level=statement.level,
                        )
                    )
                )
    rendered_statements = [
        _render_outline_statement(
            source, statement, tuple(preview_parent_names)
        )
        for statement in selected_nodes
    ]
    rendered = "\n".join(rendered_statements)
    rendered = re.sub(
        r"([A-Za-z_][A-Za-z0-9_.]*)\(globals\(\)\.get\((['\"])gallery_parent\2\)\)",
        r"\1()",
        rendered,
    )
    rendered = _strip_gallery_parent(rendered)
    return "\n\n".join(
        part
        for part in (
            "\n".join(dict.fromkeys(imports)),
            rendered,
        )
        if part
    ).strip() + "\n"


def _uncanonical_preview_scaffolding_count(source: str, cpp: str) -> int:
    """Count Python preview layout calls that the C++ card does not teach."""

    try:
        module = ast.parse(source)
    except SyntaxError:
        return 0
    cpp_has_layout = bool(
        re.search(
            r"\bQ(?:Form|Grid|HBox|Stacked|VBox)Layout\b",
            cpp,
        )
    )
    cpp_has_widget = bool(re.search(r"\bQWidget\b", cpp))
    unmatched: set[str] = set()
    for node in ast.walk(module):
        if not isinstance(node, ast.Call):
            continue
        if isinstance(node.func, ast.Attribute):
            name = node.func.attr
        elif isinstance(node.func, ast.Name):
            name = node.func.id
        else:
            continue
        if name in _PREVIEW_LAYOUT_METHODS and not re.search(
            r"(?:->|\.){0}\s*\(".format(re.escape(name)), cpp
        ):
            unmatched.add(name)
        elif name in _PREVIEW_LAYOUT_CONSTRUCTORS and not cpp_has_layout:
            unmatched.add(name)
        elif name == "QWidget" and not cpp_has_widget:
            unmatched.add(name)
    return len(unmatched)


def _concise_display_source(
    source: str,
    widget_name: str,
    route_id: str,
    sample_id: str,
    covered_types: Sequence[str],
) -> str:
    """Remove app-only preview infrastructure from a teaching snippet.

    The exact executable source remains in ``PreviewResult.preview_source``.
    C++ Gallery cards likewise show the component-facing setup rather than the
    implementation of reusable sample painters, delegates, and surfaces.
    """

    try:
        module = ast.parse(source)
    except SyntaxError:
        return source

    root_index = None
    for index, statement in enumerate(module.body):
        if widget_name in _assigned_names(statement):
            root_index = index
            break
    if root_index is None:
        return source

    last_leading_helper = -1
    for index, statement in enumerate(module.body[:root_index]):
        if isinstance(
            statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)
        ):
            last_leading_helper = index

    if last_leading_helper >= 0:
        body_index = last_leading_helper + 1
    else:
        body_index = next(
            (
                index
                for index, statement in enumerate(module.body)
                if not isinstance(statement, (ast.Import, ast.ImportFrom))
            ),
            root_index,
        )
    while body_index < len(module.body) and isinstance(
        module.body[body_index], (ast.Import, ast.ImportFrom)
    ):
        body_index += 1
    if body_index >= len(module.body):
        return source

    body_nodes = module.body[body_index:]
    used_names = {
        node.id
        for statement in body_nodes
        for node in ast.walk(statement)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }
    imports: list[str] = []
    omitted_names: set[str] = set()
    for statement in module.body:
        if isinstance(statement, ast.Import):
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name.split(".", 1)[0]) in used_names
                or alias.name == "fluentqt"
            ]
            if aliases:
                imports.append(_format_import(ast.Import(names=aliases)))
        elif isinstance(statement, ast.ImportFrom):
            module_name = statement.module or ""
            aliases = [
                alias
                for alias in statement.names
                if (alias.asname or alias.name) in used_names
            ]
            if (
                module_name.startswith("fluentqt_gallery")
                and module_name != "fluentqt_gallery.metrics"
            ):
                omitted_names.update(alias.asname or alias.name for alias in aliases)
                continue
            if aliases:
                imports.append(
                    _format_import(
                        ast.ImportFrom(
                            module=statement.module,
                            names=aliases,
                            level=statement.level,
                        )
                    )
                )

    for statement in module.body[:body_index]:
        omitted_names.update(_assigned_names(statement))

    lines = source.splitlines()
    body = "\n".join(lines[module.body[body_index].lineno - 1 :]).rstrip()
    body = re.sub(
        r"QWidget\(globals\(\)\.get\((['\"])gallery_parent\1\)\)",
        "QWidget()",
        body,
    )
    body = _strip_gallery_parent(body)
    parts = ["\n".join(dict.fromkeys(imports))]
    if omitted_names & used_names:
        parts.append(
            "# Gallery-only preview drawing/model helpers are omitted."
        )
    parts.append(body)
    concise = "\n\n".join(part for part in parts if part).strip() + "\n"
    cpp_lines = len(SAMPLE_BY_KEY[(route_id, sample_id)].cpp_snippet.splitlines())
    has_public_component = any(
        "fluentqt.{0}".format(covered) in concise
        for covered in covered_types
    )
    if not has_public_component:
        relevant_helpers = [
            index
            for index, statement in enumerate(module.body[:body_index])
            if isinstance(
                statement,
                (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef),
            )
            and _covered_constructor_names(statement, covered_types)
        ]
        if relevant_helpers:
            helper_outline = _canonical_outline(
                module,
                source,
                relevant_helpers[-1],
                route_id,
                sample_id,
                covered_types,
            )
            if helper_outline is not None:
                return helper_outline
    preview_scaffolding = _uncanonical_preview_scaffolding_count(
        concise,
        SAMPLE_BY_KEY[(route_id, sample_id)].cpp_snippet,
    )
    needs_outline = (
        len(concise.splitlines()) > max(14, int(cpp_lines * 1.6))
        or "from fluentqt_gallery" in source
        or preview_scaffolding > 0
    )
    if needs_outline:
        outline = _canonical_outline(
            module,
            source,
            body_index,
            route_id,
            sample_id,
            covered_types,
        )
        if outline is not None:
            outline_scaffolding = _uncanonical_preview_scaffolding_count(
                outline,
                SAMPLE_BY_KEY[(route_id, sample_id)].cpp_snippet,
            )
            outline_lines = len(outline.splitlines())
            concise_lines = len(concise.splitlines())
            if outline_lines < concise_lines or (
                outline_lines == concise_lines
                and outline_scaffolding < preview_scaffolding
            ):
                return outline
    return concise


_DISPLAY_LOOP_LIMIT = 8
_DISPLAY_NAME_FIELDS = (
    "text",
    "label_text",
    "caption",
    "title",
    "header",
    "name",
)


def _fixed_display_values(
    expression: ast.AST,
    sequence_definitions: Mapping[str, ast.Tuple | ast.List],
) -> list[ast.AST] | None:
    """Resolve a small, deterministic iterator used by teaching code."""

    if isinstance(expression, (ast.Tuple, ast.List)):
        return list(expression.elts)
    if isinstance(expression, ast.Name):
        definition = sequence_definitions.get(expression.id)
        return list(definition.elts) if definition is not None else None
    if not isinstance(expression, ast.Call) or not isinstance(
        expression.func, ast.Name
    ):
        return None
    if expression.func.id == "range" and not expression.keywords:
        arguments = []
        for argument in expression.args:
            if not isinstance(argument, ast.Constant) or not isinstance(
                argument.value, int
            ):
                return None
            arguments.append(argument.value)
        try:
            values = range(*arguments)
        except TypeError:
            return None
        if len(values) > _DISPLAY_LOOP_LIMIT:
            return None
        return [ast.Constant(value) for value in values]
    if expression.func.id != "enumerate" or not 1 <= len(expression.args) <= 2:
        return None
    values = _fixed_display_values(expression.args[0], sequence_definitions)
    if values is None:
        return None
    start = 0
    if len(expression.args) == 2:
        start_node = expression.args[1]
        if not isinstance(start_node, ast.Constant) or not isinstance(
            start_node.value, int
        ):
            return None
        start = start_node.value
    return [
        ast.Tuple(
            elts=[ast.Constant(start + index), copy.deepcopy(value)],
            ctx=ast.Load(),
        )
        for index, value in enumerate(values)
    ]


def _display_loop_bindings(
    target: ast.AST,
    value: ast.AST,
) -> dict[str, ast.AST] | None:
    """Pair one destructuring loop target with its fixed iteration value."""

    if isinstance(target, ast.Name):
        return {target.id: value}
    if not isinstance(target, (ast.Tuple, ast.List)) or not isinstance(
        value, (ast.Tuple, ast.List)
    ):
        return None
    if len(target.elts) != len(value.elts):
        return None
    bindings: dict[str, ast.AST] = {}
    for child_target, child_value in zip(target.elts, value.elts):
        child_bindings = _display_loop_bindings(child_target, child_value)
        if child_bindings is None:
            return None
        bindings.update(child_bindings)
    return bindings


class _DisplayLoopLocalCollector(ast.NodeVisitor):
    """Collect loop-body stores without crossing a nested Python scope."""

    def __init__(self) -> None:
        self.names: set[str] = set()

    def visit_Name(self, node: ast.Name) -> None:
        if isinstance(node.ctx, ast.Store):
            self.names.add(node.id)

    def visit_Lambda(self, _node: ast.Lambda) -> None:
        return

    def visit_FunctionDef(self, _node: ast.FunctionDef) -> None:
        return

    def visit_AsyncFunctionDef(self, _node: ast.AsyncFunctionDef) -> None:
        return

    def visit_ClassDef(self, _node: ast.ClassDef) -> None:
        return


def _display_iteration_slug(
    bindings: Mapping[str, ast.AST],
    fallback: int,
) -> str:
    """Choose a readable identifier from a fixed iteration's label."""

    value: object | None = None
    for name in _DISPLAY_NAME_FIELDS:
        candidate = bindings.get(name)
        if isinstance(candidate, ast.Constant) and isinstance(
            candidate.value, str
        ):
            value = candidate.value
            break
    if value is None:
        for candidate in bindings.values():
            if isinstance(candidate, ast.Constant) and isinstance(
                candidate.value, (str, int)
            ):
                value = candidate.value
                break
    if value is None:
        for candidate in bindings.values():
            if isinstance(candidate, ast.Attribute):
                value = candidate.attr
                break
    slug = re.sub(r"[^A-Za-z0-9]+", "_", str(value or fallback)).strip("_")
    slug = re.sub(r"(?<!^)(?=[A-Z])", "_", slug).lower()
    if not slug or slug[0].isdigit() or keyword.iskeyword(slug):
        slug = "item_{0}".format(fallback)
    return slug


def _display_local_name(base: str, slug: str) -> str:
    if base == slug or base.startswith(slug + "_"):
        return base
    if base.endswith("_layout"):
        return "{0}_{1}".format(slug, base)
    return "{0}_{1}".format(slug, base)


def _replace_display_names(
    source: str,
    bindings: Mapping[str, ast.AST],
    local_names: Mapping[str, str],
) -> str:
    """Substitute fixed loop values while retaining authored line wrapping."""

    replacements = {
        name: ast.unparse(value) for name, value in bindings.items()
    }
    replacements.update(local_names)
    lines = source.splitlines()
    try:
        module = ast.parse(source)
    except SyntaxError:
        module = ast.Module(body=[], type_ignores=[])

    spans: dict[int, list[tuple[int, int, str]]] = {}
    covered: dict[int, list[tuple[int, int]]] = {}
    for node in ast.walk(module):
        if not isinstance(node, ast.Subscript) or not isinstance(
            node.value, ast.Name
        ):
            continue
        bound = bindings.get(node.value.id)
        index = node.slice
        if not isinstance(bound, (ast.Tuple, ast.List)) or not isinstance(
            index, ast.Constant
        ) or not isinstance(index.value, int):
            continue
        if not 0 <= index.value < len(bound.elts):
            continue
        if node.lineno != node.end_lineno:
            continue
        row = node.lineno - 1
        spans.setdefault(row, []).append(
            (
                node.col_offset,
                node.end_col_offset,
                ast.unparse(bound.elts[index.value]),
            )
        )
        covered.setdefault(row, []).append(
            (node.col_offset, node.end_col_offset)
        )

    for node in ast.walk(module):
        if not isinstance(node, ast.Name) or node.id not in replacements:
            continue
        if node.lineno != node.end_lineno:
            continue
        row = node.lineno - 1
        if any(
            start <= node.col_offset < end
            for start, end in covered.get(row, ())
        ):
            continue
        spans.setdefault(row, []).append(
            (node.col_offset, node.end_col_offset, replacements[node.id])
        )

    for row, edits in spans.items():
        for start, end, replacement in sorted(edits, reverse=True):
            lines[row] = lines[row][:start] + replacement + lines[row][end:]
    return "\n".join(lines)


def _explicit_display_source(
    route_id: str,
    sample_id: str,
    source: str,
) -> str:
    """Expand fixed Gallery construction loops when C++ is also explicit.

    A teaching card should name a small, fixed set of controls directly.  Data
    driven examples keep their loops when the canonical C++ sample does the
    same, so long models and genuinely dynamic behavior stay concise.
    """

    override = _explicit_display_override(route_id, sample_id)
    if override is not None:
        return override
    sample = SAMPLE_BY_KEY[(route_id, sample_id)]
    if re.search(r"\bfor\s*\(", sample.cpp_snippet):
        return source
    try:
        module = ast.parse(source)
    except SyntaxError:
        return source

    sequence_definitions: dict[str, ast.Tuple | ast.List] = {}
    definition_indexes: dict[str, int] = {}
    load_counts: dict[str, int] = {}
    for node in ast.walk(module):
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load):
            load_counts[node.id] = load_counts.get(node.id, 0) + 1
    for index, statement in enumerate(module.body):
        if not isinstance(statement, ast.Assign) or len(statement.targets) != 1:
            continue
        target = statement.targets[0]
        if isinstance(target, ast.Name) and isinstance(
            statement.value, (ast.Tuple, ast.List)
        ):
            sequence_definitions[target.id] = statement.value
            definition_indexes[target.id] = index

    replacements: list[tuple[int, int, list[str]]] = []
    removable_indexes: set[int] = set()
    for index, statement in enumerate(module.body):
        if not isinstance(statement, ast.For) or statement.orelse:
            continue
        if any(
            isinstance(node, (ast.Break, ast.Continue, ast.Yield, ast.YieldFrom))
            for node in ast.walk(statement)
        ):
            continue
        values = _fixed_display_values(statement.iter, sequence_definitions)
        if not values or len(values) > _DISPLAY_LOOP_LIMIT:
            continue
        bindings = [
            _display_loop_bindings(statement.target, value) for value in values
        ]
        if any(binding is None for binding in bindings):
            continue

        collector = _DisplayLoopLocalCollector()
        for child in statement.body:
            collector.visit(child)
        target_names = _assigned_names(statement.target)
        local_names = collector.names - target_names
        trailing_loads = {
            node.id
            for trailing in module.body[index + 1 :]
            for node in ast.walk(trailing)
            if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
        }
        if local_names & trailing_loads:
            continue

        body_lines = source.splitlines()[
            statement.body[0].lineno - 1 : statement.body[-1].end_lineno
        ]
        body_source = dedent("\n".join(body_lines)).rstrip()
        expanded: list[str] = []
        used_slugs: set[str] = set()
        for ordinal, raw_binding in enumerate(bindings, 1):
            assert raw_binding is not None
            slug = _display_iteration_slug(raw_binding, ordinal)
            if slug in used_slugs:
                slug = "{0}_{1}".format(slug, ordinal)
            used_slugs.add(slug)
            renamed = {
                name: _display_local_name(name, slug)
                for name in local_names
            }
            expanded.append(
                _replace_display_names(body_source, raw_binding, renamed)
            )
        replacements.append(
            (statement.lineno - 1, statement.end_lineno, ["\n\n".join(expanded)])
        )

        if isinstance(statement.iter, ast.Name):
            iterator_name = statement.iter.id
            definition_index = definition_indexes.get(iterator_name)
            if definition_index is not None and load_counts.get(iterator_name) == 1:
                removable_indexes.add(definition_index)

    if not replacements:
        return source
    for definition_index in removable_indexes:
        statement = module.body[definition_index]
        replacements.append((statement.lineno - 1, statement.end_lineno, []))

    lines = source.splitlines()
    for start, end, replacement in sorted(replacements, reverse=True):
        lines[start:end] = replacement
    result = "\n".join(lines)
    result = re.sub(r"\n{3,}", "\n\n", result).strip() + "\n"
    return result


def _explicit_display_override(route_id: str, sample_id: str) -> str | None:
    """Return hand-focused examples where preview layout would hide the API."""

    key = (route_id, sample_id)

    def focused(body: str, imports: str = "") -> str:
        return _source(
            *dedent(body).strip().splitlines(),
            imports=imports,
        )

    if key == ("drawer-view", "drawer-view-basic"):
        return focused(
            """
            drawer = fluentqt.DrawerView(host)
            drawer.setEdge(fluentqt.DrawerView.DrawerEdge.Right)
            drawer.setDrawerLength(260)
            drawer.setOwnedContentWidget(settings_panel)

            open_button = fluentqt.Button("Open drawer", host)
            open_button.clicked.connect(drawer.open)
            """
        )
    if key == ("drawer-view", "drawer-view-close-policy"):
        return focused(
            """
            drawer = fluentqt.DrawerView(host)
            drawer.setAvailableMargins(
                drawer_title_bar_avoidance_margins()
            )
            drawer.setModal(False)
            drawer.setDim(False)
            drawer.setClosePolicy(
                fluentqt.DrawerView.ClosePolicy(
                    fluentqt.DrawerView.NoAutoClose
                )
            )

            panel = QWidget()
            panel_layout = QVBoxLayout(panel)
            panel_layout.setContentsMargins(16, 18, 16, 18)
            drawer.setOwnedContentWidget(panel)
            close_button.clicked.connect(drawer.close)
            """,
            imports=(
                "from PySide6.QtWidgets import QVBoxLayout, QWidget\n"
                "from fluentqt_gallery.metrics import "
                "drawer_title_bar_avoidance_margins"
            ),
        )
    if key == ("list-view", "list-view-basic"):
        return focused(
            """
            def make_initials_avatar(name, background, size=28):
                screen = QGuiApplication.primaryScreen()
                dpr = max(
                    1.0,
                    screen.devicePixelRatio() if screen is not None else 1.0,
                )
                physical = max(1, round(size * dpr))
                pixmap = QPixmap(physical, physical)
                pixmap.setDevicePixelRatio(dpr)
                pixmap.fill(Qt.GlobalColor.transparent)
                painter = QPainter(pixmap)
                painter.setRenderHint(QPainter.RenderHint.Antialiasing)
                painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(background)
                tile = QRectF(0, 0, size, size)
                painter.drawEllipse(tile)
                font = QFont()
                font.setPixelSize(round(size * 0.42))
                font.setWeight(QFont.Weight.DemiBold)
                painter.setFont(font)
                painter.setPen(Qt.GlobalColor.white)
                initials = "".join(word[0].upper() for word in name.split()[:2])
                painter.drawText(tile, Qt.AlignmentFlag.AlignCenter, initials)
                painter.end()
                return pixmap

            accent_palette = (
                QColor(0x00, 0x78, 0xD4), QColor(0x03, 0x83, 0x87),
                QColor(0xCA, 0x50, 0x10), QColor(0x87, 0x64, 0xB8),
                QColor(0xC2, 0x39, 0xB3), QColor(0x49, 0x82, 0x05),
            )
            list_view = fluentqt.ListView()
            list_view.setBackgroundVisible(False)
            list_view.setBorderVisible(False)
            list_view.setProperty("fluentPreserveParentSurface", True)
            list_view.viewport().setProperty("fluentPreserveParentSurface", True)
            list_view.setFixedSize(320, 234)
            list_view.setHeaderText("Contacts")
            list_view.setAccessibleName("Contacts")
            list_view.setIconSize(QSize(28, 28))
            model = QStandardItemModel(list_view)
            contacts = (
                "Kendall Collins", "Henry Ross", "Nicole Wagner",
                "Adam Wolfe", "Stephanie Meyer", "Maya Patel",
                "Alex Chen", "Priya Shah", "Omar Rivera",
                "Elena Rossi", "Jordan Lee", "Riley Brooks",
            )
            for index, contact in enumerate(contacts):
                item = QStandardItem(contact)
                item.setEditable(False)
                item.setData(
                    make_initials_avatar(
                        contact, accent_palette[index % len(accent_palette)]
                    ),
                    Qt.ItemDataRole.DecorationRole,
                )
                model.appendRow(item)
            list_view.setModel(model)
            list_view.setSelectedIndex(0)
            """,
            imports=(
                "from PySide6.QtCore import QRectF, QSize, Qt\n"
                "from PySide6.QtGui import (\n"
                "    QColor, QFont, QGuiApplication, QPainter, QPixmap,\n"
                "    QStandardItem, QStandardItemModel,\n"
                ")"
            ),
        )
    if key == ("list-view", "list-view-multi-select"):
        return focused(
            """
            def make_glyph_pixmap(glyph, background, size):
                screen = QGuiApplication.primaryScreen()
                dpr = max(
                    1.0,
                    screen.devicePixelRatio() if screen is not None else 1.0,
                )
                physical = max(1, round(size * dpr))
                pixmap = QPixmap(physical, physical)
                pixmap.setDevicePixelRatio(dpr)
                pixmap.fill(Qt.GlobalColor.transparent)
                painter = QPainter(pixmap)
                painter.setRenderHint(QPainter.RenderHint.Antialiasing)
                painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
                painter.setPen(Qt.PenStyle.NoPen)
                painter.setBrush(background)
                tile = QRectF(0, 0, size, size)
                painter.drawRoundedRect(tile, size / 4.0, size / 4.0)
                font = QFont("FluentQt Icons")
                font.setPixelSize(round(size * 0.55))
                painter.setFont(font)
                painter.setPen(Qt.GlobalColor.white)
                painter.drawText(tile, Qt.AlignmentFlag.AlignCenter, glyph)
                painter.end()
                return pixmap

            accent_palette = (
                QColor(0x00, 0x78, 0xD4), QColor(0x03, 0x83, 0x87),
                QColor(0xCA, 0x50, 0x10), QColor(0x87, 0x64, 0xB8),
                QColor(0xC2, 0x39, 0xB3), QColor(0x49, 0x82, 0x05),
            )
            list_view = fluentqt.ListView()
            list_view.setBackgroundVisible(False)
            list_view.setBorderVisible(False)
            list_view.setProperty("fluentPreserveParentSurface", True)
            list_view.viewport().setProperty("fluentPreserveParentSurface", True)
            list_view.setFixedSize(320, 234)
            list_view.setHeaderText("Filters")
            list_view.setAccessibleName("Message filters")
            list_view.setIconSize(QSize(24, 24))
            list_view.setSelectionMode(fluentqt.SelectionMode.Multiple)
            filter_model = QStandardItemModel(list_view)
            for index, (text, glyph) in enumerate((
                ("Unread", fluentqt.Typography.Icons.Mail),
                ("Flagged", fluentqt.Typography.Icons.Flag),
                ("Has photos", fluentqt.Typography.Icons.Camera),
                ("From contacts", fluentqt.Typography.Icons.People),
                ("Favorites", fluentqt.Typography.Icons.FavoriteStar),
                ("With documents", fluentqt.Typography.Icons.Document),
                ("Pinned", fluentqt.Typography.Icons.Pin),
                ("Scheduled", fluentqt.Typography.Icons.Calendar),
                ("Archived", fluentqt.Typography.Icons.Folder),
            )):
                item = QStandardItem(text)
                item.setEditable(False)
                item.setData(
                    make_glyph_pixmap(
                        glyph, accent_palette[index % len(accent_palette)], 24
                    ),
                    Qt.ItemDataRole.DecorationRole,
                )
                filter_model.appendRow(item)
            list_view.setModel(filter_model)
            for row in (0, 2):
                list_view.selectionModel().select(
                    filter_model.index(row, 0),
                    QItemSelectionModel.SelectionFlag.Select
                    | QItemSelectionModel.SelectionFlag.Rows,
                )
            """,
            imports=(
                "from PySide6.QtCore import QItemSelectionModel, QRectF, QSize, Qt\n"
                "from PySide6.QtGui import (\n"
                "    QColor, QFont, QGuiApplication, QPainter, QPixmap,\n"
                "    QStandardItem, QStandardItemModel,\n"
                ")"
            ),
        )
    if key == ("list-view", "list-view-horizontal"):
        return focused(
            """
            list_view.setFlow(QListView.Flow.LeftToRight)
            list_view.setModel(model)
            """,
            imports="from PySide6.QtWidgets import QListView",
        )
    if key == ("list-view", "list-view-reorder"):
        return focused(
            """
            list_view = fluentqt.ListView()
            list_view.setHeaderText("Playlist")
            list_view.setModel(model)
            list_view.setCanReorderItems(True)
            """
        )
    if key == ("list-view", "list-view-sections"):
        return focused(
            """
            list_view = fluentqt.ListView()
            list_view.setSectionEnabled(True)

            def section_key(row):
                if row < 3:
                    return "Today"
                if row < 6:
                    return "Yesterday"
                return "Earlier"

            list_view.setSectionKeyFunction(section_key)
            list_view.setModel(model)
            """
        )
    if key == ("list-view", "list-view-scroll-chaining"):
        return focused(
            """
            list_view = fluentqt.ListView()
            list_view.setScrollChainingEnabled(False)
            list_view.setHeaderText("Queue")
            list_view.setModel(model)
            """
        )
    if key == ("list-view", "list-view-placeholder"):
        return focused(
            """
            list_view = fluentqt.ListView()
            list_view.setHeaderText("Downloads")
            list_view.setFooterText("0 items")
            list_view.setPlaceholderText("No downloads yet")
            list_view.setModel(QStandardItemModel(list_view))
            """,
            imports="from PySide6.QtGui import QStandardItemModel",
        )
    if key == ("split-view", "split-view-basic"):
        return focused(
            """
            split_view = fluentqt.SplitView()
            split_view.addOwnedPane(first_pane)
            split_view.addOwnedPane(second_pane)
            split_view.addOwnedPane(third_pane)
            split_view.setPanePreferredSize(0, 150)
            split_view.setPaneFill(2, True)
            """
        )
    if key == ("split-view", "split-view-vertical-constraints"):
        return focused(
            """
            split_view = fluentqt.SplitView()
            split_view.setOrientation(Qt.Orientation.Vertical)
            split_view.addOwnedPane(
                top_pane,
                fluentqt.SplitViewPaneOptions(80, 110, 150, False),
            )
            split_view.addOwnedPane(
                fill_pane,
                fluentqt.SplitViewPaneOptions(90, 160, 500, True),
            )
            split_view.addOwnedPane(
                bottom_pane,
                fluentqt.SplitViewPaneOptions(60, 120, 140, False),
            )
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("split-view", "split-view-hidden-pane"):
        return focused(
            """
            split_view = fluentqt.SplitView()
            split_view.addOwnedPane(
                first_pane,
                fluentqt.SplitViewPaneOptions(60, 120, 240, False),
            )
            split_view.addOwnedPane(
                details_pane,
                fluentqt.SplitViewPaneOptions(60, 150, 260, False),
            )
            split_view.addOwnedPane(
                fill_pane,
                fluentqt.SplitViewPaneOptions(60, 180, 500, True),
            )

            def toggle_details():
                details_pane.setVisible(not details_pane.isVisible())

            toggle.clicked.connect(toggle_details)
            """
        )
    if key == ("stack-view", "stack-view-replace-pop-to-root"):
        return focused(
            """
            stack_view = fluentqt.StackView()
            stack_view.setTransitionAnimationEnabled(False)
            stack_view.setInitialOwnedItem(root_page)
            stack_view.pushOwnedItem(details_page)
            stack_view.replaceOwnedItem(replacement_page)
            stack_view.popToRoot()
            """
        )
    if key == ("tree-view", "tree-view-reorder"):
        return focused(
            """
            tree.setCanReorderItems(True)
            tree.itemReordered.connect(update_external_order)
            """
        )
    if key == ("tree-view", "tree-view-indicator-motion"):
        return focused(
            """
            tree.setSelectionIndicatorVisible(True)
            tree.setIndicatorMotionAnimationEnabled(True)

            parent_index = model.index(0, 0)
            child_index = model.index(0, 0, parent_index)
            sibling_index = model.index(1, 0, parent_index)
            tree.setSelectedItem(parent_index)

            def bind_target(button, index):
                button.clicked.connect(
                    lambda: tree.setSelectedItem(index)
                )

            bind_target(parent_button, parent_index)
            bind_target(child_button, child_index)
            bind_target(sibling_button, sibling_index)
            tree.indicatorHierarchyTransitionChanged.connect(update_status)
            """
        )
    if key == ("accordion", "accordion-single-expansion"):
        return focused(
            """
            accordion = fluentqt.Accordion()
            accordion.setExpansionMode(
                fluentqt.Accordion.ExpansionMode.Single
            )
            accordion.setFixedWidth(520)

            def make_section(title, detail):
                item = fluentqt.Expander()
                item.setHeaderText(title)
                body = QWidget()
                body_layout = QVBoxLayout(body)
                body_layout.setContentsMargins(16, 12, 16, 14)
                body_layout.addWidget(
                    fluentqt.Label("Additional details", body)
                )
                body_layout.addWidget(fluentqt.Label(detail, body))
                item.setOwnedContentWidget(body)
                return item

            account = make_section(
                "Account", "Profile, sign-in, and recovery options."
            )
            notifications = make_section(
                "Notifications",
                "Choose which activity can interrupt you.",
            )
            privacy = make_section(
                "Privacy",
                "Review diagnostics and personalization settings.",
            )
            accordion.addOwnedItem(account)
            accordion.addOwnedItem(notifications)
            accordion.addOwnedItem(privacy)
            account.setExpandedAnimated(True, False)
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout, QWidget",
        )
    if key == ("accordion", "accordion-multiple-expansion"):
        return focused(
            """
            accordion = fluentqt.Accordion()
            accordion.setExpansionMode(
                fluentqt.Accordion.ExpansionMode.Multiple
            )
            accordion.setFixedWidth(520)

            network_item = fluentqt.Expander()
            network_item.setHeaderText("Network")
            network_body = QWidget()
            network_layout = QVBoxLayout(network_body)
            network_layout.setContentsMargins(16, 12, 16, 14)
            network_layout.addWidget(
                fluentqt.Label("Additional details", network_body)
            )
            network_layout.addWidget(
                fluentqt.Label(
                    "Wi-Fi and Ethernet are connected.", network_body
                )
            )
            network_item.setOwnedContentWidget(network_body)

            proxy_item = fluentqt.Expander()
            proxy_item.setHeaderText("Proxy")
            proxy_body = QWidget()
            proxy_layout = QVBoxLayout(proxy_body)
            proxy_layout.setContentsMargins(16, 12, 16, 14)
            proxy_layout.addWidget(
                fluentqt.Label("Additional details", proxy_body)
            )
            proxy_layout.addWidget(
                fluentqt.Label("Use system proxy settings.", proxy_body)
            )
            proxy_item.setOwnedContentWidget(proxy_body)

            accordion.addOwnedItem(network_item)
            accordion.addOwnedItem(proxy_item)
            network_item.setExpandedAnimated(True, False)
            proxy_item.setExpandedAnimated(True, False)
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout, QWidget",
        )
    if key == ("expander", "expander-text-content"):
        return focused(
            """
            expander = fluentqt.Expander()
            expander.setHeaderText("Connection details")

            body = QWidget()
            body_layout = QVBoxLayout(body)
            body_layout.setContentsMargins(16, 12, 16, 14)
            body_layout.setSpacing(4)
            body_layout.addWidget(
                fluentqt.Label("Additional details", body)
            )
            body_layout.addWidget(
                fluentqt.Label(
                    "Server: api.example.com\\nTransport: TLS 1.3", body
                )
            )

            expander.setOwnedContentWidget(body)
            expander.setExpandedAnimated(True, False)
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout, QWidget",
        )
    if key == ("expander", "expander-state-signal"):
        return focused(
            """
            status = fluentqt.Label("Collapsed")

            options = QWidget()
            options_layout = QVBoxLayout(options)
            options_layout.setContentsMargins(16, 12, 16, 14)
            options_layout.addWidget(
                fluentqt.Label("Additional details", options)
            )
            options_layout.addWidget(
                fluentqt.Label(
                    "Diagnostic logging and retry behavior.", options
                )
            )

            expander = fluentqt.Expander()
            expander.setHeaderText("Advanced options")
            expander.setOwnedContentWidget(options)
            expander.expandedChanged.connect(
                lambda expanded: status.setText(
                    "Expanded" if expanded else "Collapsed"
                )
            )
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout, QWidget",
        )
    if key == ("card", "card-surface-appearances"):
        return focused(
            """
            layer = fluentqt.Card()
            layer.setAppearance(fluentqt.Card.Appearance.Layer)
            layer.setFixedSize(170, 88)
            layer_layout = QVBoxLayout(layer)
            layer_layout.setContentsMargins(16, 12, 16, 12)
            layer_layout.addWidget(fluentqt.Label("Layer", layer))
            layer_layout.addWidget(
                fluentqt.Label("Default grouped surface", layer)
            )

            alternate = fluentqt.Card()
            alternate.setAppearance(fluentqt.Card.Appearance.LayerAlt)
            alternate.setFixedSize(170, 88)
            alternate_layout = QVBoxLayout(alternate)
            alternate_layout.setContentsMargins(16, 12, 16, 12)
            alternate_layout.addWidget(
                fluentqt.Label("LayerAlt", alternate)
            )
            alternate_layout.addWidget(
                fluentqt.Label("Alternate layer tone", alternate)
            )

            canvas = fluentqt.Card()
            canvas.setAppearance(fluentqt.Card.Appearance.Canvas)
            canvas.setFixedSize(170, 88)
            canvas_layout = QVBoxLayout(canvas)
            canvas_layout.setContentsMargins(16, 12, 16, 12)
            canvas_layout.addWidget(fluentqt.Label("Canvas", canvas))
            canvas_layout.addWidget(
                fluentqt.Label("Matches the page canvas", canvas)
            )
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout",
        )
    if key == ("card", "card-border-visibility"):
        return focused(
            """
            bordered = fluentqt.Card()
            bordered.setAppearance(fluentqt.Card.Appearance.Layer)
            bordered.setBorderVisible(True)
            bordered.setFixedSize(170, 88)
            bordered_layout = QVBoxLayout(bordered)
            bordered_layout.addWidget(
                fluentqt.Label("Bordered", bordered)
            )
            bordered_layout.addWidget(
                fluentqt.Label("Independent surface", bordered)
            )

            borderless = fluentqt.Card()
            borderless.setAppearance(fluentqt.Card.Appearance.Layer)
            borderless.setBorderVisible(False)
            borderless.setFixedSize(170, 88)
            borderless_layout = QVBoxLayout(borderless)
            borderless_layout.addWidget(
                fluentqt.Label("Borderless", borderless)
            )
            borderless_layout.addWidget(
                fluentqt.Label("Nested composition", borderless)
            )
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout",
        )
    if key == ("menu", "menu-command-shortcuts"):
        return focused(
            """
            button = fluentqt.DropDownButton("File")
            status = fluentqt.Label("Clicked: (none)")
            menu = fluentqt.FluentMenu("", button)

            new_action = menu.addAction("New")
            new_action.setShortcut(QKeySequence.StandardKey.New)
            new_action.triggered.connect(
                lambda: status.setText("Clicked: New")
            )

            open_action = menu.addAction("Open...")
            open_action.setShortcut(QKeySequence.StandardKey.Open)
            save_action = menu.addAction("Save")
            save_action.setShortcut(QKeySequence.StandardKey.Save)
            menu.addSeparator()
            publish = menu.addAction("Publish")
            publish.setEnabled(False)
            button.setMenu(menu)
            """,
            imports="from PySide6.QtGui import QKeySequence",
        )
    if key == ("menu", "menu-cascading-selection"):
        return focused(
            """
            menu = fluentqt.FluentMenu("")
            sort_menu = fluentqt.FluentMenu("Sort by", menu)
            sort_menu.addAction("Name")
            sort_menu.addAction("Date modified")
            menu.addMenu(sort_menu)

            view_group = QActionGroup(menu)
            view_group.setExclusive(True)
            compact = menu.addAction("Compact list")
            compact.setCheckable(True)
            view_group.addAction(compact)
            comfortable = menu.addAction("Comfortable list")
            comfortable.setCheckable(True)
            comfortable.setChecked(True)
            view_group.addAction(comfortable)

            hidden = menu.addAction("Show hidden files")
            hidden.setCheckable(True)
            """,
            imports="from PySide6.QtGui import QActionGroup",
        )
    if key == ("menu-bar", "menu-bar-hosted-surface"):
        return focused(
            """
            menu_bar = fluentqt.FluentMenuBar()
            status = fluentqt.Label("Clicked: (none)")
            menu_bar.setBackgroundVisible(False)

            file_menu = fluentqt.FluentMenu("File", menu_bar)
            new_action = file_menu.addAction("New")
            new_action.triggered.connect(
                lambda: status.setText("Clicked: New")
            )
            file_menu.addAction("Open...")
            menu_bar.addMenu(file_menu)

            edit_menu = fluentqt.FluentMenu("Edit", menu_bar)
            edit_menu.addAction("Undo")
            menu_bar.addMenu(edit_menu)
            """
        )
    if key == ("menu-bar", "menu-bar-access-keys"):
        return focused(
            """
            menu_bar = fluentqt.FluentMenuBar()
            focus_button = fluentqt.Button("Focus")
            status = fluentqt.Label("Command: (none)")
            focus_button.clicked.connect(
                lambda: menu_bar.setFocus(
                    Qt.FocusReason.OtherFocusReason
                )
            )

            file_menu = fluentqt.FluentMenu("&File", menu_bar)
            file_menu.menuAction().setProperty("accessKey", "F")
            save_action = file_menu.addAction("Save")
            save_action.setShortcut(QKeySequence.StandardKey.Save)
            menu_bar.addMenu(file_menu)

            run_action = QAction("Run", menu_bar)
            run_action.triggered.connect(
                lambda: status.setText("Command: Run")
            )
            menu_bar.addAction(run_action)
            """,
            imports=(
                "from PySide6.QtCore import Qt\n"
                "from PySide6.QtGui import QAction, QKeySequence"
            ),
        )
    if key == ("command-bar", "command-bar-responsive-overflow"):
        return focused(
            """
            bar_host = QWidget()
            bar_host.setFixedWidth(536)
            bar_layout = QHBoxLayout(bar_host)
            bar_layout.setContentsMargins(0, 0, 0, 0)
            bar = fluentqt.CommandBar(bar_host)
            bar.setAccessibleName("Document commands")
            bar.setLabelPosition(
                fluentqt.CommandBar.LabelPosition.Right
            )
            bar.setBackgroundVisible(False)
            bar_layout.addWidget(bar)

            add_action = QAction(QIcon(":/icons/add.svg"), "Add", bar)
            edit_action = QAction(QIcon(":/icons/edit.svg"), "Edit", bar)
            edit_action.setPriority(QAction.Priority.HighPriority)
            share_action = QAction(
                QIcon(":/icons/share.svg"), "Share", bar
            )
            separator = QAction(bar)
            separator.setSeparator(True)
            sync_action = QAction(QIcon(":/icons/sync.svg"), "Sync", bar)
            sync_action.setPriority(QAction.Priority.LowPriority)
            pin_action = QAction(QIcon(":/icons/pin.svg"), "Pin", bar)
            pin_action.setCheckable(True)

            bar.addPrimaryAction(add_action)
            bar.addPrimaryAction(edit_action)
            bar.addPrimaryAction(share_action)
            bar.addPrimaryAction(separator)
            bar.addPrimaryAction(sync_action)
            bar.addPrimaryAction(pin_action)
            bar.addSecondaryAction(
                QAction(QIcon(":/icons/settings.svg"), "Settings", bar)
            )
            bar.addSecondaryAction(
                QAction(QIcon(":/icons/help.svg"), "Help", bar)
            )

            compact_button = fluentqt.Button("Compact view")
            labels_button = fluentqt.Button("Labels: Right")
            background_button = fluentqt.Button("Show background")

            def toggle_compact():
                bar_host.setFixedWidth(
                    288 if bar_host.width() > 300 else 536
                )

            def toggle_labels():
                collapsed = (
                    bar.labelPosition()
                    == fluentqt.CommandBar.LabelPosition.Collapsed
                )
                bar.setLabelPosition(
                    fluentqt.CommandBar.LabelPosition.Right
                    if collapsed
                    else fluentqt.CommandBar.LabelPosition.Collapsed
                )

            def toggle_background():
                bar.setBackgroundVisible(not bar.backgroundVisible())

            compact_button.clicked.connect(toggle_compact)
            labels_button.clicked.connect(toggle_labels)
            background_button.clicked.connect(toggle_background)
            """,
            imports=(
                "from PySide6.QtGui import QAction, QIcon\n"
                "from PySide6.QtWidgets import QHBoxLayout, QWidget"
            ),
        )
    if key == ("command-bar", "command-bar-editing-router"):
        return focused(
            """
            editor = fluentqt.LineEdit()
            editor.setText("Review the release notes before Friday")
            router = fluentqt.EditingCommandRouter(window)
            bar = fluentqt.CommandBar()
            bar.setAccessibleName("Editing commands")
            bar.setLabelPosition(
                fluentqt.CommandBar.LabelPosition.Right
            )
            bar.setBackgroundVisible(False)
            Command = fluentqt.EditingCommandRouter.Command

            def command_action(command, icon_path):
                action = router.action(command)
                action.setIcon(QIcon(icon_path))
                return action

            bar.addPrimaryAction(
                command_action(Command.Undo, ":/icons/undo.svg")
            )
            bar.addPrimaryAction(
                command_action(Command.Redo, ":/icons/redo.svg")
            )
            separator = QAction(bar)
            separator.setSeparator(True)
            bar.addPrimaryAction(separator)
            bar.addPrimaryAction(
                command_action(Command.Cut, ":/icons/cut.svg")
            )
            bar.addPrimaryAction(
                command_action(Command.Copy, ":/icons/copy.svg")
            )
            bar.addPrimaryAction(
                command_action(Command.Paste, ":/icons/paste.svg")
            )
            bar.addSecondaryAction(
                command_action(Command.Delete, ":/icons/delete.svg")
            )
            bar.addSecondaryAction(
                command_action(
                    Command.SelectAll, ":/icons/select-all.svg"
                )
            )

            select_text = fluentqt.Button("Select text")
            clear_selection = fluentqt.Button("Clear selection")
            read_only = fluentqt.Button("Read-only: Off")
            read_only.setCheckable(True)

            def select_all():
                editor.setFocus(Qt.FocusReason.OtherFocusReason)
                editor.selectAll()
                router.refresh()

            def deselect():
                editor.setFocus(Qt.FocusReason.OtherFocusReason)
                editor.deselect()
                router.refresh()

            def set_read_only(value):
                editor.setFocus(Qt.FocusReason.OtherFocusReason)
                editor.setReadOnly(value)
                editor.selectAll()
                router.refresh()

            select_text.clicked.connect(select_all)
            clear_selection.clicked.connect(deselect)
            read_only.toggled.connect(set_read_only)
            """,
            imports=(
                "from PySide6.QtCore import Qt\n"
                "from PySide6.QtGui import QAction, QIcon"
            ),
        )
    if key == ("command-bar-flyout", "command-bar-flyout-always-expanded"):
        return focused(
            """
            flyout = fluentqt.CommandBarFlyout()
            flyout.setAlwaysExpanded(True)
            copy_link = QAction(
                QIcon(":/icons/link.svg"), "Copy link", flyout
            )
            favorite = QAction(
                QIcon(":/icons/favorite.svg"), "Favorite", flyout
            )
            favorite.setCheckable(True)
            rename = QAction(
                QIcon(":/icons/edit.svg"), "Rename", flyout
            )
            properties = QAction(
                QIcon(":/icons/info.svg"), "Properties", flyout
            )
            flyout.addPrimaryAction(copy_link)
            flyout.addPrimaryAction(favorite)
            flyout.addSecondaryAction(rename)
            flyout.addSecondaryAction(properties)

            open_button = fluentqt.Button("Open actions")
            always_expanded = fluentqt.Button("Always expanded: On")
            open_button.clicked.connect(
                lambda: flyout.showAt(
                    open_button,
                    fluentqt.CommandBarFlyout.ShowMode.Transient,
                )
            )
            always_expanded.setCheckable(True)
            always_expanded.setChecked(True)
            always_expanded.toggled.connect(flyout.setAlwaysExpanded)
            """,
            imports="from PySide6.QtGui import QAction, QIcon",
        )
    if key == ("breadcrumb", "breadcrumb-size"):
        return focused(
            """
            path = ["Home", "Documents", "Images"]

            standard = fluentqt.Breadcrumb()
            standard.setItems(path)
            standard.setBreadcrumbSize(
                fluentqt.Breadcrumb.BreadcrumbSize.Standard
            )
            standard.setFixedHeight(20)

            large = fluentqt.Breadcrumb()
            large.setItems(path)
            large.setBreadcrumbSize(
                fluentqt.Breadcrumb.BreadcrumbSize.Large
            )
            large.setFixedHeight(40)
            """
        )
    if key == ("navigation-view", "navigation-view-chrome-slots"):
        return focused(
            """
            nav = fluentqt.NavigationView()
            nav.setMinimumWidth(440)
            nav.setMaximumWidth(620)
            nav.setFixedHeight(340)
            nav.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
            nav.setExpandedPaneWidth(180)
            nav.setHeaderChromeWidget(header_section)
            nav.setMainChromeWidget(main_section)
            nav.setFooterChromeWidget(footer_section)
            populate_navigation_pages(nav.contentHost())

            def route_to_page(page_index):
                host = nav.contentHost()
                direction = 1 if page_index >= host.currentIndex() else -1
                host.setCurrentIndex(page_index, direction, True)

            header_section.on_activated = route_to_page
            main_section.on_activated = route_to_page
            footer_section.on_activated = route_to_page
            """,
            imports="from PySide6.QtWidgets import QSizePolicy",
        )
    if key == ("navigation-view", "navigation-view-display-modes"):
        return focused(
            """
            nav = fluentqt.NavigationView()
            nav.setMinimumWidth(440)
            nav.setMaximumWidth(620)
            nav.setFixedHeight(340)
            nav.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            nav.setAnimationEnabled(True)
            nav.setExpandedPaneWidth(180)
            nav.setCompactPaneWidth(52)
            nav.setTopBarHeight(48)

            def apply_display_mode(mode):
                top = mode == fluentqt.NavigationView.DisplayMode.Top
                compact = mode != fluentqt.NavigationView.DisplayMode.Left
                orientation = (
                    Qt.Orientation.Horizontal
                    if top
                    else Qt.Orientation.Vertical
                )
                header_section.set_orientation(orientation)
                main_section.set_orientation(orientation)
                footer_section.set_orientation(orientation)
                header_section.set_compact(compact)
                main_section.set_compact(compact)
                footer_section.set_compact(compact)
                nav.contentHost().setTransitionEffect(
                    fluentqt.StackContentHost.TransitionEffect.SlideFromBottom
                    if top
                    else fluentqt.StackContentHost.TransitionEffect.SlideFromLeft
                )
                nav.setPaneOpen(
                    mode == fluentqt.NavigationView.DisplayMode.Left or top
                )
                nav.setDisplayMode(mode)

            def route_to_page(page_index):
                host = nav.contentHost()
                direction = 1 if page_index >= host.currentIndex() else -1
                host.setCurrentIndex(page_index, direction, True)

            header_section.on_activated = route_to_page
            main_section.on_activated = route_to_page
            footer_section.on_activated = route_to_page
            left_button.clicked.connect(
                lambda: apply_display_mode(
                    fluentqt.NavigationView.DisplayMode.Left
                )
            )
            compact_button.clicked.connect(
                lambda: apply_display_mode(
                    fluentqt.NavigationView.DisplayMode.LeftCompact
                )
            )
            minimal_button.clicked.connect(
                lambda: apply_display_mode(
                    fluentqt.NavigationView.DisplayMode.LeftMinimal
                )
            )
            top_button.clicked.connect(
                lambda: apply_display_mode(
                    fluentqt.NavigationView.DisplayMode.Top
                )
            )
            """,
            imports=(
                "from PySide6.QtCore import Qt\n"
                "from PySide6.QtWidgets import QSizePolicy"
            ),
        )
    if key == ("navigation-view", "navigation-view-content-host"):
        return focused(
            """
            nav = fluentqt.NavigationView()
            nav.setMinimumWidth(440)
            nav.setMaximumWidth(620)
            nav.setFixedHeight(320)
            nav.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            nav.setAnimationEnabled(True)
            nav.setDisplayMode(fluentqt.NavigationView.DisplayMode.Left)
            host = nav.contentHost()
            nav.setExpandedPaneWidth(180)
            populate_navigation_pages(host)
            host.setTransitionEffect(
                fluentqt.StackContentHost.TransitionEffect.SlideFromLeft
            )

            def route_to_page(page_index):
                direction = 1 if page_index >= host.currentIndex() else -1
                host.setCurrentIndex(page_index, direction, True)

            main_section.on_activated = route_to_page
            """,
            imports="from PySide6.QtWidgets import QSizePolicy",
        )
    if key == ("pivot", "pivot-basic"):
        return focused(
            """
            pivot = fluentqt.Pivot()
            pivot.addItem(
                fluentqt.PivotItem(
                    "All", fluentqt.Typography.Icons.Mail
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Unread", fluentqt.Typography.Icons.Filter
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Flagged", fluentqt.Typography.Icons.Flag
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Locked", fluentqt.Typography.Icons.Lock, False
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Mentions", fluentqt.Typography.Icons.Contact
                )
            )

            host = fluentqt.StackContentHost()
            for index in range(pivot.itemCount()):
                host.insertOwnedPage(
                    index,
                    create_page(pivot.itemAt(index).header),
                )

            pivot.currentChanged.connect(
                lambda index: host.setCurrentIndex(index, 0, True)
            )
            """
        )
    if key == ("pivot", "pivot-item-state"):
        return focused(
            """
            pivot = fluentqt.Pivot()
            pivot.addItem(
                fluentqt.PivotItem(
                    "Inbox", fluentqt.Typography.Icons.Mail,
                    True, "inbox", "Inbox view",
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Flagged", fluentqt.Typography.Icons.Flag,
                    True, "flagged", "Flagged mail",
                )
            )
            pivot.addItem(
                fluentqt.PivotItem(
                    "Locked", fluentqt.Typography.Icons.Lock,
                    False, "locked", "Locked view",
                )
            )
            pivot.setSelectedIndex(0)

            def rename_flagged():
                pivot.setItemHeader(1, "Priority")
                pivot.setItemIconGlyph(
                    1, fluentqt.Typography.Icons.ImportantBadge12
                )
                pivot.setItemData(1, "priority")

            rename_button.clicked.connect(rename_flagged)
            unlock_button.clicked.connect(
                lambda: pivot.setItemEnabled(2, True)
            )
            pivot.currentChanged.connect(
                lambda index: pivot.itemAt(index).data
            )
            """
        )
    if key == ("pivot", "pivot-overflow-behavior"):
        return focused(
            """
            items = (
                fluentqt.PivotItem(
                    "All", fluentqt.Typography.Icons.Mail
                ),
                fluentqt.PivotItem(
                    "Unread", fluentqt.Typography.Icons.Filter
                ),
                fluentqt.PivotItem(
                    "Flagged", fluentqt.Typography.Icons.Flag
                ),
                fluentqt.PivotItem(
                    "Mentions", fluentqt.Typography.Icons.Contact
                ),
                fluentqt.PivotItem(
                    "Archive", fluentqt.Typography.Icons.Storage
                ),
                fluentqt.PivotItem(
                    "Long category", fluentqt.Typography.Icons.Folder
                ),
            )

            scroll_buttons = fluentqt.Pivot()
            for item in items:
                scroll_buttons.addItem(item)
            scroll_buttons.setOverflowBehavior(
                fluentqt.Pivot.OverflowBehavior.ScrollButtons
            )
            scroll_buttons.setFixedWidth(420)

            more_button = fluentqt.Pivot()
            for item in items:
                more_button.addItem(item)
            more_button.setOverflowBehavior(
                fluentqt.Pivot.OverflowBehavior.MoreButton
            )
            more_button.setFixedWidth(420)
            more_button.overflowActivated.connect(inspect_hidden_indexes)
            """
        )
    if key == ("selector-bar", "selector-bar-basic"):
        return focused(
            """
            selector = fluentqt.SelectorBar()
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Inbox", fluentqt.Typography.Icons.Mail,
                    True, True, "inbox",
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Calendar", fluentqt.Typography.Icons.Calendar,
                    True, True, "calendar",
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Settings", fluentqt.Typography.Icons.Settings,
                    True, True, "settings",
                )
            )

            host = fluentqt.StackContentHost()
            for index in range(selector.itemCount()):
                host.insertOwnedPage(
                    index,
                    create_page(selector.itemAt(index).text),
                )

            selector.currentChanged.connect(
                lambda index: host.setCurrentIndex(index, 0, True)
            )
            selector.selectionChanged.connect(
                lambda _index, item: item.data
            )
            """
        )
    if key == ("selector-bar", "selector-bar-item-state"):
        return focused(
            """
            selector = fluentqt.SelectorBar()
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Overview", fluentqt.Typography.Icons.Home,
                    True, True, "overview",
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Sample code", fluentqt.Typography.Icons.Document,
                    True, False, "code",
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Disabled", fluentqt.Typography.Icons.Lock,
                    False, True, "disabled",
                )
            )
            selector.addItem(
                fluentqt.SelectorBarItem(
                    "Settings", fluentqt.Typography.Icons.Settings,
                    True, True, "settings",
                )
            )
            selector.setItemVisible(1, False)
            selector.setItemEnabled(2, False)
            selector.setItemSelected(0, True)
            show_code_button.clicked.connect(
                lambda: selector.setItemVisible(1, True)
            )
            """
        )
    if key == ("tab-view", "tab-view-hosted-pages"):
        return focused(
            """
            surface = QWidget()
            surface.setMinimumWidth(360)
            surface.setMaximumWidth(560)
            surface.setFixedHeight(186)
            surface.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            surface_layout = QVBoxLayout(surface)
            surface_layout.setContentsMargins(0, 0, 0, 0)
            surface_layout.setSpacing(0)

            tabs = fluentqt.TabView(surface)
            tabs.setFixedHeight(40)
            tabs.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            tabs.setTabWidthMode(
                fluentqt.TabView.TabWidthMode.SizeToContent
            )
            tabs.setTabReorderEnabled(True)
            tabs.setTabsClosable(False)
            tabs.setAddTabButtonVisible(False)
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Home", fluentqt.Typography.Icons.Home
                )
            )
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Details", fluentqt.Typography.Icons.Document
                )
            )
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Activity", fluentqt.Typography.Icons.Calendar
                )
            )

            host = fluentqt.StackContentHost(surface)
            host.setFixedHeight(146)
            host.setSizePolicy(
                QSizePolicy.Policy.Expanding,
                QSizePolicy.Policy.Fixed,
            )
            for index in range(tabs.tabCount()):
                host.insertOwnedPage(
                    index,
                    create_page(tabs.tabAt(index).text),
                )

            tabs.currentChanged.connect(
                lambda index: host.setCurrentIndex(index, 0, True)
            )
            tabs.tabMoved.connect(host.movePage)
            surface_layout.addWidget(tabs)
            surface_layout.addWidget(host)
            """,
            imports=(
                "from PySide6.QtWidgets import "
                "QSizePolicy, QVBoxLayout, QWidget"
            ),
        )
    if key == ("tab-view", "tab-view-add-close"):
        return focused(
            """
            tabs = fluentqt.TabView()
            tabs.setCloseButtonOverlayMode(
                fluentqt.TabView.CloseButtonOverlayMode.Always
            )
            tabs.setAddTabButtonVisible(True)
            tabs.setTabsClosable(True)
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Home", fluentqt.Typography.Icons.Home, False
                )
            )
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Draft", fluentqt.Typography.Icons.Document
                )
            )
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Review", fluentqt.Typography.Icons.Edit
                )
            )
            tabs.addTab(
                fluentqt.TabViewItem(
                    "Disabled", fluentqt.Typography.Icons.Lock,
                    True, False,
                )
            )

            def add_tab():
                index = tabs.addTab(
                    fluentqt.TabViewItem(
                        "Document", fluentqt.Typography.Icons.Document
                    )
                )
                tabs.setSelectedIndex(index)

            tabs.addTabRequested.connect(add_tab)
            tabs.tabCloseRequested.connect(tabs.closeTab)
            """
        )
    if key == ("tab-view", "tab-view-overflow-reorder"):
        return focused(
            """
            tabs = fluentqt.TabView()
            tabs.setTabWidthMode(
                fluentqt.TabView.TabWidthMode.SizeToContent
            )
            tabs.setTabReorderEnabled(True)
            tabs.setTabsClosable(False)
            tabs.setAddTabButtonVisible(False)
            tabs.setFixedWidth(360)
            for index in range(1, 9):
                tabs.addTab(
                    fluentqt.TabViewItem(
                        f"Document {index} with longer title",
                        fluentqt.Typography.Icons.Document,
                    )
                )
            tabs.tabMoved.connect(keep_page_order_aligned)
            """
        )
    if key == ("pips-pager", "pips-pager-flipview"):
        return focused(
            """
            flip_view = fluentqt.FlipView()
            flip_view.setShowPageIndicator(False)
            for page in pages:
                flip_view.addOwnedPage(page)

            pager = fluentqt.PipsPager()
            pager.setNumberOfPages(flip_view.pageCount())
            pager.setMaxVisiblePips(5)
            pager.selectedPageIndexChanged.connect(
                flip_view.setCurrentIndex
            )
            flip_view.currentIndexChanged.connect(
                pager.setSelectedPageIndex
            )
            """
        )
    if key == ("pips-pager", "pips-pager-orientation"):
        return focused(
            """
            pager = fluentqt.PipsPager()
            pager.setNumberOfPages(7)
            pager.setSelectedPageIndex(3)
            pager.setPreviousButtonVisibility(
                fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
            )
            pager.setNextButtonVisibility(
                fluentqt.PipsPager.PipsPagerButtonVisibility.Visible
            )

            def apply_orientation(orientation):
                pager.setOrientation(orientation)
                pager.setFixedSize(pager.sizeHint())

            horizontal_button.clicked.connect(
                lambda: apply_orientation(Qt.Orientation.Horizontal)
            )
            vertical_button.clicked.connect(
                lambda: apply_orientation(Qt.Orientation.Vertical)
            )
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("annotated-scrollbar", "annotated-scrollbar-scrollview"):
        return focused(
            """
            scroll_view = fluentqt.ScrollView()
            scroll_view.setOwnedContentWidget(color_sections_content)
            scroll_view.setVerticalScrollBarVisibility(
                fluentqt.ScrollView.ScrollBarVisibility.Hidden
            )

            bar = fluentqt.AnnotatedScrollBar()
            bar.setLabels(
                [
                    fluentqt.AnnotatedScrollBarLabel("Azure", 0),
                    fluentqt.AnnotatedScrollBarLabel("Crimson", 900),
                    fluentqt.AnnotatedScrollBarLabel("Cyan", 2430),
                    fluentqt.AnnotatedScrollBarLabel("Fuchsia", 2700),
                    fluentqt.AnnotatedScrollBarLabel("Gold", 4770),
                ]
            )
            bar.setDetailLabelProvider(color_section_for_offset)
            bar.connectToScrollView(scroll_view)
            """
        )
    if key == ("scroll-view", "scroll-view-zoom-aware-content"):
        return focused(
            """
            class ZoomAwareCanvas(fluentqt.ScrollViewZoomAwareWidget):
                def scrollViewUnscaledSize(self):
                    return QSizeF(560, 360)

                def setScrollViewZoomFactor(self, factor):
                    self.resize(round(560 * factor), round(360 * factor))

            scroll_view = fluentqt.ScrollView()
            scroll_view.setZoomMode(
                fluentqt.ScrollView.ZoomMode.Enabled
            )
            scroll_view.setOwnedContentWidget(ZoomAwareCanvas())
            scroll_view.zoomTo(1.5, True)
            """,
            imports="from PySide6.QtCore import QSizeF",
        )
    if key == ("checkbox", "checkbox-two-state"):
        return focused(
            """
            check_box = fluentqt.CheckBox("Accept terms")
            status = fluentqt.Label("State: Unchecked")
            check_box.stateChanged.connect(
                lambda state: status.setText(
                    "State: Checked"
                    if Qt.CheckState(state) == Qt.CheckState.Checked
                    else "State: Unchecked"
                )
            )
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("toggle-switch", "toggle-switch-state"):
        return focused(
            """
            toggle = fluentqt.ToggleSwitch()
            toggle.setOnContent("On")
            toggle.setOffContent("Off")
            toggle.setAccessibleName("Feature toggle")
            status = fluentqt.Label("State: Off")
            toggle.toggled.connect(
                lambda on: status.setText(
                    "State: On" if on else "State: Off"
                )
            )
            """
        )
    if key == ("toggle-switch", "toggle-switch-content"):
        return focused(
            """
            label = fluentqt.Label("Wi-Fi")
            toggle = fluentqt.ToggleSwitch()
            toggle.setOnContent("Connected")
            toggle.setOffContent("Disconnected")
            toggle.setIsOn(True)
            """
        )
    if key == ("flyout", "flyout-command-confirmation"):
        return focused(
            """
            flyout = fluentqt.Flyout(command_button.window())
            flyout.setPlacement(fluentqt.Flyout.Placement.Right)
            confirm = fluentqt.Button("Empty cart", flyout)
            confirm.clicked.connect(flyout.close)
            flyout.showAt(command_button)
            """
        )
    if key == ("dialog", "dialog-owned-content"):
        return focused(
            """
            dialog = fluentqt.Dialog(window)
            dialog.setMinimumSize(480, 280)
            layout = QVBoxLayout(dialog)
            layout.addWidget(title_label)
            layout.addWidget(name_edit)
            layout.addStretch(1)
            layout.addLayout(command_row)
            apply_button.clicked.connect(lambda: dialog.done(1))
            dialog.finished.connect(dialog.deleteLater)
            dialog.open()
            """,
            imports="from PySide6.QtWidgets import QVBoxLayout",
        )
    if key == ("info-badge", "info-badge-custom-metrics"):
        return focused(
            """
            inbox_button = fluentqt.Button("Inbox")
            badge = fluentqt.InfoBadge(inbox_button)
            badge.setDisplayMode(
                fluentqt.InfoBadge.InfoBadgeDisplayMode.Icon
            )
            badge.setIconGlyph(fluentqt.Typography.Icons.Mail)
            badge.setCustomBackgroundColor(QColor("#C42B1C"))
            badge.setCustomTextColor(Qt.GlobalColor.white)
            badge.setBadgeHeight(18)
            badge.setIconGlyphSize(12)
            """,
            imports=(
                "from PySide6.QtCore import Qt\n"
                "from PySide6.QtGui import QColor"
            ),
        )
    if key == ("info-badge", "info-badge-accessibility"):
        return focused(
            """
            inbox = fluentqt.Button("Inbox")
            inbox.setAccessibleName("Inbox")
            badge = fluentqt.InfoBadge(inbox)
            badge.setDisplayMode(
                fluentqt.InfoBadge.InfoBadgeDisplayMode.Value
            )
            badge.setAccessibleName("Unread messages")
            badge.setValue(3)

            increment = fluentqt.Button("Increment")
            toggle = fluentqt.Button("Toggle badge")
            status = fluentqt.Label("Unread value: 3")

            def increment_value():
                badge.setValue(badge.value() + 1)
                status.setText(f"Unread value: {badge.value()}")

            def toggle_badge():
                show_badge = badge.isHidden()
                badge.setVisible(show_badge)
                status.setText(
                    "Badge visible" if show_badge else "Badge hidden"
                )

            increment.clicked.connect(increment_value)
            toggle.clicked.connect(toggle_badge)
            """
        )
    if key == ("info-bar", "info-bar-open-close"):
        return focused(
            """
            status = fluentqt.Label("Visible")
            open_button = fluentqt.Button("Show again")

            info_bar = fluentqt.InfoBar()
            info_bar.setTitle("Draft saved")
            info_bar.setMessage("You can safely leave this page.")
            info_bar.setIsClosable(True)

            def reopen():
                info_bar.setIsOpen(True)
                status.setText("Visible")

            open_button.clicked.connect(reopen)
            info_bar.closed.connect(
                lambda: status.setText("Dismissed")
            )
            """
        )
    if key == ("progress-bar", "progress-bar-determinate-value"):
        return focused(
            """
            progress_bar = fluentqt.ProgressBar()
            progress_bar.setRange(0, 100)
            progress_bar.setBarWidth(320)

            progress_box = fluentqt.NumberBox()
            progress_box.setHeader("Progress")
            progress_box.setRange(0, 100)
            progress_box.setSmallChange(1)
            progress_box.setLargeChange(10)
            progress_box.setDisplayPrecision(0)
            progress_box.setSpinButtonPlacementMode(
                fluentqt.NumberBox.SpinButtonPlacementMode.Inline
            )

            status = fluentqt.Label("Progress: 44%")
            status.setFixedWidth(124)

            def update_progress(value):
                progress_bar.setValue(
                    value if math.isfinite(value) else 0.0
                )
                status.setText(
                    f"Progress: {progress_bar.progressText()}%"
                )

            progress_box.valueChanged.connect(update_progress)
            progress_box.setValue(44)
            """,
            imports="import math",
        )
    if key == ("progress-ring", "progress-ring-determinate-value"):
        return focused(
            """
            ring = fluentqt.ProgressRing()
            ring.setIsIndeterminate(False)
            ring.setIsActive(True)
            ring.setRingSize(
                fluentqt.ProgressRing.ProgressRingSize.Large
            )
            ring.setBackgroundVisible(True)

            progress_box = fluentqt.NumberBox()
            progress_box.setHeader("Progress")
            progress_box.setRange(0, 100)
            progress_box.setSmallChange(1)
            progress_box.setLargeChange(10)
            progress_box.setDisplayPrecision(0)
            progress_box.setSpinButtonPlacementMode(
                fluentqt.NumberBox.SpinButtonPlacementMode.Inline
            )

            status = fluentqt.Label("Value: 44%")
            status.setFixedWidth(124)

            def update_progress(value):
                progress = int(value) if math.isfinite(value) else 0
                ring.setValue(progress)
                status.setText(f"Value: {progress}%")

            progress_box.valueChanged.connect(update_progress)
            progress_box.setValue(44)
            """,
            imports="import math",
        )
    if key == ("toast", "toast-action-lifecycle"):
        return focused(
            """
            retry = QAction("Retry")
            toast = fluentqt.Toast()
            toast.setAction(retry)
            toast.setPauseOnHoverEnabled(True)
            toast.setDuration(5000)

            show_toast = fluentqt.Button("Show actionable toast")
            status = fluentqt.Label("Ready")

            def present():
                toast.setMessage("Upload failed. Retry when ready.")
                toast.setSeverity(fluentqt.Toast.Severity.Error)
                toast.present(show_toast)
                status.setText("Toast open; hover pauses timeout")

            show_toast.clicked.connect(present)
            retry.triggered.connect(
                lambda: status.setText("Retry requested")
            )
            toast.dismissedWithReason.connect(
                lambda reason: status.setText(
                    f"Dismissed: {reason.name}"
                )
            )
            """,
            imports="from PySide6.QtGui import QAction",
        )
    if key == ("toast", "toast-update-key"):
        return focused(
            """
            advance = fluentqt.Button("Advance upload")
            status = fluentqt.Label("Progress: 0%")
            advance.setProperty("progress", 0)

            def advance_upload():
                current = int(advance.property("progress"))
                progress = 25 if current >= 100 else current + 25
                advance.setProperty("progress", progress)
                fluentqt.Toast.showOrUpdateToast(
                    advance,
                    "upload",
                    "Upload complete"
                    if progress == 100
                    else f"Uploading: {progress}%",
                    fluentqt.Toast.Severity.Success
                    if progress == 100
                    else fluentqt.Toast.Severity.Informational,
                    5000,
                    fluentqt.Toast.Placement.TopEnd,
                )
                status.setText(f"Progress: {progress}%")

            advance.clicked.connect(advance_upload)
            """
        )
    if key == ("line-edit", "line-edit-editing-commands"):
        return focused(
            """
            Command = fluentqt.EditingCommandRouter.Command
            router = fluentqt.EditingCommandRouter(window)
            menu_bar = fluentqt.FluentMenuBar()
            menu_bar.setBackgroundVisible(False)
            menu_bar.setFixedWidth(360)

            edit_menu = fluentqt.FluentMenu("Edit", menu_bar)
            edit_menu.addAction(router.action(Command.Undo))
            edit_menu.addAction(router.action(Command.Redo))
            edit_menu.addSeparator()
            edit_menu.addAction(router.action(Command.Cut))
            edit_menu.addAction(router.action(Command.Copy))
            edit_menu.addAction(router.action(Command.Paste))
            edit_menu.addAction(router.action(Command.Delete))
            edit_menu.addSeparator()
            edit_menu.addAction(router.action(Command.SelectAll))
            menu_bar.addMenu(edit_menu)

            line_edit = fluentqt.LineEdit()
            line_edit.setText("Edit this line")
            line_edit.setFixedWidth(360)

            text_edit = fluentqt.TextEdit()
            text_edit.setPlainText(
                "The same actions follow this editor."
            )
            text_edit.setMinVisibleLines(2)
            text_edit.setMaxVisibleLines(2)
            text_edit.setFixedWidth(360)

            status = fluentqt.Label("No editing target")
            router.activeTargetChanged.connect(
                lambda active: status.setText(
                    "Editing target active"
                    if active
                    else "No editing target"
                )
            )
            """
        )
    if key == ("password-box", "password-box-reveal-modes"):
        return focused(
            """
            peek_box = fluentqt.PasswordBox()
            peek_box.setPassword("Peek mode")
            peek_box.setPasswordRevealMode(
                fluentqt.PasswordBox.PasswordRevealMode.Peek
            )

            hidden_box = fluentqt.PasswordBox()
            hidden_box.setPassword("Hidden mode")
            hidden_box.setPasswordRevealMode(
                fluentqt.PasswordBox.PasswordRevealMode.Hidden
            )

            visible_box = fluentqt.PasswordBox()
            visible_box.setPassword("Visible mode")
            visible_box.setPasswordRevealMode(
                fluentqt.PasswordBox.PasswordRevealMode.Visible
            )
            """
        )
    if key == ("title-bar", "title-bar-content-regions"):
        return focused(
            """
            title_bar = fluentqt.TitleBar()
            title_bar.setSystemReservedLeadingWidth(
                platform_leading_width
            )
            title_bar.setSystemReservedTrailingWidth(
                platform_trailing_width
            )

            content = QWidget(title_bar)
            layout = QHBoxLayout(content)
            layout.addWidget(title_label)
            layout.addWidget(search_box)
            layout.addWidget(share_button)
            title_bar.setContentWidget(content)
            """,
            imports="from PySide6.QtWidgets import QHBoxLayout, QWidget",
        )
    if key == ("window", "window-content-host"):
        return focused(
            """
            window = fluentqt.Window()
            window.setAttribute(
                Qt.WidgetAttribute.WA_DeleteOnClose, True
            )
            window.setWindowTitle("Fluent window")
            window.setCustomWindowChromeEnabled(True)
            window.setBackdropEffect(fluentqt.BackdropEffect.Mica)

            content = QWidget()
            content.setAutoFillBackground(False)
            window.setContentWidget(content)
            window.resize(640, 520)
            window.show()
            """,
            imports=(
                "from PySide6.QtCore import Qt\n"
                "from PySide6.QtWidgets import QWidget"
            ),
        )
    if key == ("window", "window-custom-titlebar"):
        return focused(
            """
            window = fluentqt.Window()
            window.setWindowTitle("Custom title bar")
            window.setCustomWindowChromeEnabled(True)
            window.setBackdropEffect(fluentqt.BackdropEffect.Mica)
            window.setCaptionButtonToolTips(
                "Minimize", "Maximize", "Close", "Restore"
            )
            window.setCaptionButtonAccessibleNames(
                "Minimize", "Maximize", "Close", "Restore"
            )

            title_bar_content = QWidget(window.titleBar())
            layout = QHBoxLayout(title_bar_content)
            layout.addWidget(title_label)
            layout.addWidget(search_box)
            layout.addWidget(share_button)
            window.titleBar().setContentWidget(title_bar_content)
            window.titleBar().refreshChromeExclusions()
            window.setContentWidget(page_content)
            window.resize(720, 520)
            window.show()
            """,
            imports="from PySide6.QtWidgets import QHBoxLayout, QWidget",
        )
    if key == ("slider", "slider-range-steps"):
        return focused(
            """
            slider = fluentqt.Slider(Qt.Orientation.Horizontal)
            slider.setRange(500, 1000)
            slider.setSingleStep(10)
            slider.setPageStep(50)
            slider.setValue(800)
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("slider", "slider-tick-marks"):
        return focused(
            """
            slider = fluentqt.Slider(Qt.Orientation.Horizontal)
            slider.setRange(0, 10)
            slider.setTickInterval(1)
            slider.setTickPosition(
                fluentqt.Slider.TickPosition.TicksBelow
            )
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("slider", "slider-vertical"):
        return focused(
            """
            slider = fluentqt.Slider(Qt.Orientation.Vertical)
            slider.setRange(0, 100)
            slider.setValue(25)
            slider.setFixedHeight(160)
            """,
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("button", "button-interaction-state"):
        return _source(
            'rest = fluentqt.Button("Rest")',
            "",
            'hover = fluentqt.Button("Hover")',
            "hover.setInteractionState(",
            "    fluentqt.Button.InteractionState.Hover",
            ")",
            "",
            'pressed = fluentqt.Button("Pressed")',
            "pressed.setInteractionState(",
            "    fluentqt.Button.InteractionState.Pressed",
            ")",
            "",
            'focused = fluentqt.Button("Focus")',
            "focused.setFocusVisual(True)",
            "",
            'disabled = fluentqt.Button("Disabled")',
            "disabled.setInteractionState(",
            "    fluentqt.Button.InteractionState.Disabled",
            ")",
        )
    if key == ("label", "label-elide"):
        return _source(
            'component_label = fluentqt.Label(',
            '    "src/components/textfields/examples/LabelSample.cpp"',
            ")",
            "component_label.setTextElideMode(Qt.TextElideMode.ElideMiddle)",
            "component_label.setFixedWidth(260)",
            "",
            'release_label = fluentqt.Label(',
            '    "Quarterly release summary for the planning review"',
            ")",
            "release_label.setTextElideMode(Qt.TextElideMode.ElideRight)",
            "release_label.setFixedWidth(190)",
            "",
            'artifact_label = fluentqt.Label(',
            '    "fluent-qt-release-textfields-label-preview-bundle"',
            ")",
            "artifact_label.setTextElideMode(Qt.TextElideMode.ElideLeft)",
            "artifact_label.setFixedWidth(240)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("breadcrumb", "breadcrumb-overflow-mode"):
        return _source(
            'path = ["Home", "Projects", "Fluent", "Controls", "Breadcrumb"]',
            "",
            "none = fluentqt.Breadcrumb()",
            "none.setItems(path)",
            "none.setOverflowMode(fluentqt.Breadcrumb.OverflowMode.None_)",
            "none.setFixedWidth(520)",
            "",
            "beginning = fluentqt.Breadcrumb()",
            "beginning.setItems(path)",
            "beginning.setOverflowMode(",
            "    fluentqt.Breadcrumb.OverflowMode.Beginning",
            ")",
            "beginning.setFixedWidth(260)",
            "",
            "middle = fluentqt.Breadcrumb()",
            "middle.setItems(path)",
            "middle.setOverflowMode(fluentqt.Breadcrumb.OverflowMode.Middle)",
            "middle.setFixedWidth(260)",
            "",
            "def inspect_hidden_indexes(hidden_indexes):",
            "    return hidden_indexes",
            "",
            "middle.overflowActivated.connect(inspect_hidden_indexes)",
        )
    if key == ("tab-view", "tab-view-width-modes"):
        return _source(
            "def add_tabs(tabs):",
            "    tabs.addTab(",
            '        fluentqt.TabViewItem("Home", fluentqt.Typography.Icons.Home)',
            "    )",
            "    tabs.addTab(",
            "        fluentqt.TabViewItem(",
            '            "Long document", fluentqt.Typography.Icons.File',
            "        )",
            "    )",
            "    tabs.addTab(",
            "        fluentqt.TabViewItem(",
            '            "Activity", fluentqt.Typography.Icons.Calendar',
            "        )",
            "    )",
            "    tabs.setTabsClosable(False)",
            "    tabs.setAddTabButtonVisible(False)",
            "",
            "equal = fluentqt.TabView()",
            "add_tabs(equal)",
            "equal.setTabWidthMode(fluentqt.TabView.TabWidthMode.Equal)",
            "",
            "size_to_content = fluentqt.TabView()",
            "add_tabs(size_to_content)",
            "size_to_content.setTabWidthMode(",
            "    fluentqt.TabView.TabWidthMode.SizeToContent",
            ")",
            "",
            "compact = fluentqt.TabView()",
            "add_tabs(compact)",
            "compact.setTabWidthMode(fluentqt.TabView.TabWidthMode.Compact)",
            "compact.setSelectedIndex(1)",
        )
    if key == ("checkbox", "checkbox-select-all"):
        return _source(
            'select_all = fluentqt.CheckBox("Select all")',
            "select_all.setTristate(True)",
            'mail = fluentqt.CheckBox("Mail")',
            'calendar = fluentqt.CheckBox("Calendar")',
            'people = fluentqt.CheckBox("People")',
            "",
            "def update_select_all():",
            "    checked = sum(",
            "        item.isChecked() for item in (mail, calendar, people)",
            "    )",
            "    state = (",
            "        Qt.CheckState.Unchecked if checked == 0",
            "        else Qt.CheckState.Checked if checked == 3",
            "        else Qt.CheckState.PartiallyChecked",
            "    )",
            "    select_all.setCheckState(state)",
            "",
            "def apply_select_all():",
            "    checked = select_all.checkState() == Qt.CheckState.Checked",
            "    mail.setChecked(checked)",
            "    calendar.setChecked(checked)",
            "    people.setChecked(checked)",
            "",
            "select_all.clicked.connect(apply_select_all)",
            "mail.clicked.connect(update_select_all)",
            "calendar.clicked.connect(update_select_all)",
            "people.clicked.connect(update_select_all)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("color-picker", "color-picker-rgba"):
        return _source(
            "picker = fluentqt.ColorPicker()",
            "picker.setColor(QColor(0, 120, 212, 180))",
            "picker.colorChanged.connect(apply_color)",
            imports="from PySide6.QtGui import QColor",
        )
    if key == ("flip-view", "flip-view-basic"):
        return _source(
            "flip_view = fluentqt.FlipView()",
            "flip_view.addOwnedPage(sunrise_photo)",
            "flip_view.addOwnedPage(ocean_photo)",
            "flip_view.addOwnedPage(forest_photo)",
            "flip_view.setShowPageIndicator(True)",
        )
    if key == ("flip-view", "flip-view-vertical"):
        return _source(
            "flip_view = fluentqt.FlipView()",
            "flip_view.setOrientation(Qt.Orientation.Vertical)",
            "flip_view.setShowPageIndicator(True)",
            "flip_view.addOwnedPage(first_page)",
            "flip_view.addOwnedPage(second_page)",
            "flip_view.addOwnedPage(third_page)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("flip-view", "flip-view-external-navigation"):
        return _source(
            "flip_view = fluentqt.FlipView()",
            "flip_view.setShowNavigationButtons(False)",
            "flip_view.setShowPageIndicator(False)",
            "previous.clicked.connect(flip_view.goPrevious)",
            "next_button.clicked.connect(flip_view.goNext)",
            "flip_view.currentIndexChanged.connect(update_status)",
        )
    if key == ("stack-view", "stack-view-basic"):
        return _source(
            "stack_view = fluentqt.StackView()",
            "stack_view.setInitialOwnedItem(first_page)",
            "stack_view.pushOwnedItem(next_page)",
            "stack_view.pop()",
            "stack_view.depthChanged.connect(update_status)",
        )
    if key == ("stack-view", "stack-view-transition-type"):
        return _source(
            "stack_view.setTransitionDuration(220)",
            "stack_view.setTransitionType(",
            "    fluentqt.StackView.StackViewTransitionType.ScaleFade",
            ")",
            "stack_view.pushOwnedItem(next_page)",
            "stack_view.pop()",
        )
    if key == ("teaching-tip", "teaching-tip-placement-tail"):
        return focused(
            """
            top = fluentqt.Button("Top")
            right_top = fluentqt.Button("RightTop")
            automatic = fluentqt.Button("Auto")
            tail = fluentqt.ToggleSwitch()
            tail.setAccessibleName("Show TeachingTip tail")
            tail.setIsOn(True)
            tail.setOnContent("Tail")
            tail.setOffContent("No tail")
            status = fluentqt.Label("Placement: none")

            placement_names = {
                fluentqt.TeachingTip.PreferredPlacement.Top: "Top",
                fluentqt.TeachingTip.PreferredPlacement.RightTop: "RightTop",
                fluentqt.TeachingTip.PreferredPlacement.Auto: "Auto",
            }

            def show_tip(anchor, placement):
                name = placement_names[placement]
                tip = fluentqt.TeachingTip(anchor.window())
                tip.setAccessibleName(f"{name} placement tip")
                tip.setPreferredPlacement(placement)
                tip.setTailVisible(tail.isOn())
                tip.setLightDismissEnabled(True)
                tip.setCardSize(QSize(300, 136))
                populate_teaching_tip(
                    tip,
                    f"{name} placement",
                    (
                        "The tail points back to the control that opened "
                        "the tip."
                        if tail.isOn()
                        else "Hide the tail when the surrounding layout "
                        "already makes context clear."
                    ),
                    status,
                )
                tip.opened.connect(
                    lambda: status.setText(
                        f"Placement: {name}, "
                        f"tail {'on' if tail.isOn() else 'off'}"
                    )
                )
                tip.closed.connect(tip.deleteLater)
                tip.showAt(anchor)

            top.clicked.connect(
                lambda: show_tip(
                    top, fluentqt.TeachingTip.PreferredPlacement.Top
                )
            )
            right_top.clicked.connect(
                lambda: show_tip(
                    right_top,
                    fluentqt.TeachingTip.PreferredPlacement.RightTop,
                )
            )
            automatic.clicked.connect(
                lambda: show_tip(
                    automatic,
                    fluentqt.TeachingTip.PreferredPlacement.Auto,
                )
            )
            """,
            imports="from PySide6.QtCore import QSize",
        )
    if key == ("coach-mark", "coach-mark-targeted-glide"):
        return _source(
            "coach = fluentqt.CoachMark(window)",
            "coach.setCardSize(QSize(320, 150))",
            "coach.setPlacement(fluentqt.CoachMark.Placement.Bottom)",
            "coach.setTarget(target_button)",
            "close_button.clicked.connect(coach.close)",
            "coach.open()",
            imports="from PySide6.QtCore import QSize",
        )
    if key == ("flyout", "flyout-placement-anchors"):
        return _source(
            "flyout = fluentqt.Flyout(window)",
            "flyout.setPlacement(fluentqt.Flyout.Placement.Right)",
            "populate_flyout(flyout)",
            "flyout.closed.connect(flyout.deleteLater)",
            "flyout.showAt(anchor_button)",
        )
    if key == ("title-bar", "title-bar-height-exclusions"):
        return _source(
            "title_bar = fluentqt.TitleBar()",
            "title_bar.setTitleBarHeight(48)",
            "title_bar.setContentWidget(toolbar_content)",
            "title_bar.chromeGeometryChanged.connect(update_exclusions)",
            "title_bar.refreshChromeExclusions()",
        )
    if key == ("toast", "toast-stacking"):
        return _source(
            "fluentqt.Toast.setMaximumVisible(3)",
            "",
            "fluentqt.Toast.showToast(",
            '    host, "Draft saved", fluentqt.Toast.Severity.Informational',
            ")",
            "fluentqt.Toast.showToast(",
            '    host, "Upload finished", fluentqt.Toast.Severity.Success',
            ")",
            "fluentqt.Toast.showToast(",
            "    host,",
            '    "Connection is unstable",',
            "    fluentqt.Toast.Severity.Warning,",
            "    2200,",
            "    fluentqt.Toast.Placement.TopEnd,",
            ")",
        )
    if key == ("info-badge", "info-badge-status-colors"):
        return _source(
            "success = fluentqt.InfoBadge()",
            "success.setDisplayMode(",
            "    fluentqt.InfoBadge.InfoBadgeDisplayMode.Value",
            ")",
            "success.setValue(5)",
            "success.setStatus(fluentqt.InfoBadge.InfoBadgeStatus.Success)",
            "",
            "critical = fluentqt.InfoBadge()",
            "critical.setDisplayMode(",
            "    fluentqt.InfoBadge.InfoBadgeDisplayMode.Value",
            ")",
            "critical.setValue(5)",
            "critical.setStatus(",
            "    fluentqt.InfoBadge.InfoBadgeStatus.Critical",
            ")",
        )
    if key == ("scroll-view", "scroll-view-scrollbar-policies"):
        return _source(
            "scroll_view = fluentqt.ScrollView()",
            "",
            "def apply_auto_bars():",
            "    scroll_view.setHorizontalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Auto",
            "    )",
            "    scroll_view.setVerticalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Auto",
            "    )",
            "    scroll_view.setHorizontalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Auto",
            "    )",
            "    scroll_view.setVerticalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Auto",
            "    )",
            "",
            "def apply_visible_bars():",
            "    scroll_view.setHorizontalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Enabled",
            "    )",
            "    scroll_view.setVerticalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Enabled",
            "    )",
            "    scroll_view.setHorizontalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Visible",
            "    )",
            "    scroll_view.setVerticalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Visible",
            "    )",
            "",
            "def apply_hidden_horizontal():",
            "    scroll_view.setHorizontalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Enabled",
            "    )",
            "    scroll_view.setHorizontalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Hidden",
            "    )",
            "",
            "def apply_vertical_disabled():",
            "    scroll_view.setVerticalScrollMode(",
            "        fluentqt.ScrollView.ScrollMode.Disabled",
            "    )",
            "    scroll_view.setVerticalScrollBarVisibility(",
            "        fluentqt.ScrollView.ScrollBarVisibility.Disabled",
            "    )",
        )
    if key == ("tab-view", "tab-view-keyboard-accelerators"):
        return _source(
            "tabs = fluentqt.TabView()",
            "tabs.setAddTabButtonVisible(True)",
            "tabs.setTabsClosable(True)",
            "tabs.setKeyboardAcceleratorsEnabled(True)",
            "tabs.addTab(",
            '    fluentqt.TabViewItem("Shortcut A", fluentqt.Typography.Icons.File)',
            ")",
            "tabs.addTab(",
            '    fluentqt.TabViewItem("Shortcut B", fluentqt.Typography.Icons.File)',
            ")",
            "tabs.addTab(",
            '    fluentqt.TabViewItem("Shortcut C", fluentqt.Typography.Icons.File)',
            ")",
            "",
            "def add_tab():",
            "    index = tabs.addTab(",
            '        fluentqt.TabViewItem("Added", fluentqt.Typography.Icons.File)',
            "    )",
            "    tabs.setSelectedIndex(index)",
            "",
            "tabs.addTabRequested.connect(add_tab)",
            "tabs.tabCloseRequested.connect(tabs.closeTab)",
            "disable_button.clicked.connect(",
            "    lambda: tabs.setKeyboardAcceleratorsEnabled(False)",
            ")",
            "enable_button.clicked.connect(",
            "    lambda: tabs.setKeyboardAcceleratorsEnabled(True)",
            ")",
        )
    if key == ("annotated-scrollbar", "annotated-scrollbar-basic"):
        return _source(
            "bar = fluentqt.AnnotatedScrollBar()",
            "bar.setRange(0, 960)",
            "bar.setPageStep(120)",
            "bar.setLabels([",
            '    fluentqt.AnnotatedScrollBarLabel("2023", 0, "October 2023"),',
            '    fluentqt.AnnotatedScrollBarLabel("2022", 120, "October 2022"),',
            '    fluentqt.AnnotatedScrollBarLabel("2021", 240, "October 2021"),',
            '    fluentqt.AnnotatedScrollBarLabel("2020", 360, "October 2020"),',
            '    fluentqt.AnnotatedScrollBarLabel("2019", 480, "October 2019"),',
            '    fluentqt.AnnotatedScrollBarLabel("2018", 600, "October 2018"),',
            '    fluentqt.AnnotatedScrollBarLabel("2017", 720, "October 2017"),',
            '    fluentqt.AnnotatedScrollBarLabel("2016", 840, "October 2016"),',
            '    fluentqt.AnnotatedScrollBarLabel("2015", 960, "October 2015"),',
            "])",
            "",
            "def detail_for_offset(offset):",
            "    year = 2023 - max(0, min(offset // 120, 8))",
            '    return f"October {year}"',
            "",
            "bar.setDetailLabelProvider(detail_for_offset)",
        )
    if key == ("annotated-scrollbar", "annotated-scrollbar-label-density"):
        return _source(
            "bar = fluentqt.AnnotatedScrollBar()",
            "bar.setRange(0, 1100)",
            "bar.setMinimumLabelSpacing(28)",
            "bar.setLabels(month_labels)",
            "",
            "height_slider = fluentqt.Slider(Qt.Orientation.Horizontal)",
            "height_slider.setRange(180, 360)",
            "height_slider.valueChanged.connect(bar.setFixedHeight)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("selector-bar", "selector-bar-overflow-behavior"):
        return _source(
            "items = [",
            "    fluentqt.SelectorBarItem(",
            '        "Category 1", fluentqt.Typography.Icons.Folder',
            "    ),",
            "    fluentqt.SelectorBarItem(",
            '        "Category 2", fluentqt.Typography.Icons.File',
            "    ),",
            "    fluentqt.SelectorBarItem(",
            '        "Category 3", fluentqt.Typography.Icons.Folder',
            "    ),",
            "    fluentqt.SelectorBarItem(",
            '        "Category 4", fluentqt.Typography.Icons.File',
            "    ),",
            "    fluentqt.SelectorBarItem(",
            '        "Category 5", fluentqt.Typography.Icons.Folder',
            "    ),",
            "    fluentqt.SelectorBarItem(",
            '        "Category 6", fluentqt.Typography.Icons.File',
            "    ),",
            "]",
            "",
            "scroll_buttons = fluentqt.SelectorBar()",
            "for item in items:",
            "    scroll_buttons.addItem(item)",
            "scroll_buttons.setOverflowBehavior(",
            "    fluentqt.SelectorBar.OverflowBehavior.ScrollButtons",
            ")",
            "scroll_buttons.setFixedWidth(360)",
            "",
            "more_button = fluentqt.SelectorBar()",
            "for item in items:",
            "    more_button.addItem(item)",
            "more_button.setOverflowBehavior(",
            "    fluentqt.SelectorBar.OverflowBehavior.MoreButton",
            ")",
            "more_button.setFixedWidth(360)",
            "more_button.overflowActivated.connect(inspect_hidden_indexes)",
        )
    if key == ("avatar", "avatar-initials-sizes"):
        return _source(
            'small = fluentqt.Avatar("Ada Lovelace")',
            "small.setAvatarSize(fluentqt.Avatar.AvatarSize.Small)",
            "",
            'medium = fluentqt.Avatar("Grace Hopper")',
            "medium.setAvatarSize(fluentqt.Avatar.AvatarSize.Medium)",
            "",
            'large = fluentqt.Avatar("Lin Chen")',
            "large.setAvatarSize(fluentqt.Avatar.AvatarSize.Large)",
            "",
            'extra_large = fluentqt.Avatar("Sam Rivera")',
            "extra_large.setAvatarSize(",
            "    fluentqt.Avatar.AvatarSize.ExtraLarge",
            ")",
        )
    if key == ("info-bar", "info-bar-action-layout"):
        return _source(
            'retry_button = fluentqt.Button("Retry")',
            "",
            "info_bar = fluentqt.InfoBar()",
            "info_bar.setSeverity(",
            "    fluentqt.InfoBar.InfoBarSeverity.Warning",
            ")",
            "info_bar.setSingleLine(False)",
            'info_bar.setTitle("Sync paused")',
            "info_bar.setMessage(",
            '    "Some files need attention before the next sync can finish."',
            ")",
            "info_bar.setActionWidget(retry_button)",
            "keep_info_bar_open(info_bar)",
        )
    if key == ("progress-ring", "progress-ring-status"):
        return _source(
            "running = fluentqt.ProgressRing()",
            "running.setIsIndeterminate(False)",
            "running.setIsActive(True)",
            "running.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)",
            "running.setValue(65)",
            "running.setStatus(fluentqt.ProgressRing.ProgressRingStatus.Running)",
            "running.setBackgroundVisible(True)",
            "",
            "paused = fluentqt.ProgressRing()",
            "paused.setIsIndeterminate(False)",
            "paused.setIsActive(True)",
            "paused.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)",
            "paused.setValue(65)",
            "paused.setStatus(fluentqt.ProgressRing.ProgressRingStatus.Paused)",
            "paused.setBackgroundVisible(True)",
            "",
            "error = fluentqt.ProgressRing()",
            "error.setIsIndeterminate(False)",
            "error.setIsActive(True)",
            "error.setRingSize(fluentqt.ProgressRing.ProgressRingSize.Large)",
            "error.setValue(65)",
            "error.setStatus(fluentqt.ProgressRing.ProgressRingStatus.Error)",
            "error.setBackgroundVisible(True)",
        )
    if key == ("tooltip", "tooltip-hover"):
        return _source(
            'button = fluentqt.Button("Archive")',
            "tooltip = fluentqt.ToolTip.attach(",
            "    button,",
            '    "Move the selected message to Archive",',
            "    fluentqt.ToolTip.Placement.Above,",
            ")",
        )
    if key == ("tooltip", "tooltip-animation"):
        return _source(
            'button = fluentqt.Button("Instant tip")',
            "tooltip = fluentqt.ToolTip.attach(",
            '    button, "Animation disabled",',
            "    fluentqt.ToolTip.Placement.Above,",
            ")",
            "tooltip.setAnimationEnabled(False)",
        )
    if key == ("drawer-view", "drawer-view-edges"):
        return _source(
            "drawer = fluentqt.DrawerView(host)",
            "drawer.setDrawerLength(170)",
            "drawer.setModal(False)",
            "drawer.setDim(False)",
            "",
            "def open_from(edge):",
            "    drawer.setEdge(edge)",
            "    drawer.open()",
        )
    if key == ("command-bar-flyout", "command-bar-flyout-show-modes"):
        return _source(
            "flyout = fluentqt.CommandBarFlyout()",
            'share_action = QAction("Share")',
            'save_action = QAction("Save")',
            'delete_action = QAction("Delete")',
            'resize_action = QAction("Resize")',
            'move_action = QAction("Move")',
            "flyout.addPrimaryAction(share_action)",
            "flyout.addPrimaryAction(save_action)",
            "flyout.addPrimaryAction(delete_action)",
            "flyout.addSecondaryAction(resize_action)",
            "flyout.addSecondaryAction(move_action)",
            "",
            "photo.clicked.connect(",
            "    lambda: flyout.showAt(",
            "        photo, fluentqt.CommandBarFlyout.ShowMode.Transient",
            "    )",
            ")",
            "photo.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)",
            "photo.customContextMenuRequested.connect(",
            "    lambda point: flyout.showAtPoint(",
            "        photo,",
            "        point,",
            "        fluentqt.CommandBarFlyout.ShowMode.Standard,",
            "    )",
            ")",
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtGui import QAction",
        )
    if key == ("tab-view", "tab-view-close-button-modes"):
        return _source(
            "def add_tabs(tabs):",
            "    tabs.addTab(",
            '        fluentqt.TabViewItem("Primary", "\uea3a")',
            "    )",
            '    tabs.addTab(fluentqt.TabViewItem("Reference", "\ue8a5"))',
            '    tabs.addTab(fluentqt.TabViewItem("Pinned", "\ue718", False))',
            "    tabs.setAddTabButtonVisible(False)",
            "",
            "auto_tabs = fluentqt.TabView()",
            "add_tabs(auto_tabs)",
            "auto_tabs.setCloseButtonOverlayMode(",
            "    fluentqt.TabView.CloseButtonOverlayMode.Auto",
            ")",
            "",
            "hover_tabs = fluentqt.TabView()",
            "add_tabs(hover_tabs)",
            "hover_tabs.setCloseButtonOverlayMode(",
            "    fluentqt.TabView.CloseButtonOverlayMode.OnHover",
            ")",
            "",
            "always_tabs = fluentqt.TabView()",
            "add_tabs(always_tabs)",
            "always_tabs.setCloseButtonOverlayMode(",
            "    fluentqt.TabView.CloseButtonOverlayMode.Always",
            ")",
        )
    if key == ("pips-pager", "pips-pager-button-visibility"):
        return _source(
            "def configure(pager, visibility):",
            "    pager.setNumberOfPages(7)",
            "    pager.setSelectedPageIndex(3)",
            "    pager.setPreviousButtonVisibility(visibility)",
            "    pager.setNextButtonVisibility(visibility)",
            "",
            "collapsed = fluentqt.PipsPager()",
            "configure(",
            "    collapsed,",
            "    fluentqt.PipsPager.PipsPagerButtonVisibility.Collapsed,",
            ")",
            "visible = fluentqt.PipsPager()",
            "configure(",
            "    visible,",
            "    fluentqt.PipsPager.PipsPagerButtonVisibility.Visible,",
            ")",
            "hover = fluentqt.PipsPager()",
            "configure(",
            "    hover,",
            "    fluentqt.PipsPager.PipsPagerButtonVisibility.VisibleOnPointerOver,",
            ")",
        )
    if key == ("scrollbar", "scrollbar-thickness"):
        return _source(
            "thin = fluentqt.ScrollBar(Qt.Orientation.Horizontal)",
            "thin.setRange(0, 1000)",
            "thin.setPageStep(100)",
            "thin.setValue(220)",
            "thin.setThickness(6)",
            "thin.setOpacity(1.0)",
            "",
            "standard = fluentqt.ScrollBar(Qt.Orientation.Horizontal)",
            "standard.setRange(0, 1000)",
            "standard.setPageStep(100)",
            "standard.setValue(420)",
            "standard.setThickness(7)",
            "standard.setOpacity(1.0)",
            "",
            "large = fluentqt.ScrollBar(Qt.Orientation.Horizontal)",
            "large.setRange(0, 1000)",
            "large.setPageStep(100)",
            "large.setValue(640)",
            "large.setThickness(24)",
            "large.setOpacity(1.0)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("scrollbar", "scrollbar-opacity"):
        return _source(
            "visible = fluentqt.ScrollBar(Qt.Orientation.Horizontal)",
            "visible.setRange(0, 1000)",
            "visible.setPageStep(100)",
            "visible.setValue(420)",
            "visible.setOpacity(1.0)",
            "",
            "subdued = fluentqt.ScrollBar(Qt.Orientation.Horizontal)",
            "subdued.setRange(0, 1000)",
            "subdued.setPageStep(100)",
            "subdued.setValue(420)",
            "subdued.setOpacity(0.45)",
            imports="from PySide6.QtCore import Qt",
        )
    if key == ("info-bar", "info-bar-severities"):
        return _source(
            "info = fluentqt.InfoBar()",
            "info.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Informational)",
            'info.setTitle("Update available")',
            'info.setMessage("Version 3.2 is ready.")',
            "",
            "success = fluentqt.InfoBar()",
            "success.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Success)",
            'success.setTitle("Saved")',
            'success.setMessage("All changes were saved.")',
            "",
            "warning = fluentqt.InfoBar()",
            "warning.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Warning)",
            'warning.setTitle("Storage almost full")',
            'warning.setMessage("Clear space before syncing.")',
            "",
            "error = fluentqt.InfoBar()",
            "error.setSeverity(fluentqt.InfoBar.InfoBarSeverity.Error)",
            'error.setTitle("Upload failed")',
            'error.setMessage("The document could not be saved.")',
        )
    if key == ("shimmer", "shimmer-templates"):
        return _source(
            "image = fluentqt.Shimmer()",
            "image.setShimmerTemplate(",
            "    fluentqt.Shimmer.ShimmerTemplate.ImageCard",
            ")",
            "image.setFixedSize(160, 92)",
            "",
            "avatar_row = fluentqt.Shimmer()",
            "avatar_row.setShimmerTemplate(",
            "    fluentqt.Shimmer.ShimmerTemplate.AvatarTextRow",
            ")",
            "avatar_row.setFixedSize(180, 56)",
            "",
            "text = fluentqt.Shimmer()",
            "text.setShimmerTemplate(",
            "    fluentqt.Shimmer.ShimmerTemplate.TextBlock",
            ")",
            "text.setFixedSize(220, 72)",
        )
    if key == ("toast", "toast-severity"):
        return _source(
            "toast = fluentqt.Toast()",
            "toast.setDuration(2200)",
            "",
            'info_button = fluentqt.Button("Info")',
            "def show_info():",
            '    toast.setMessage("Draft saved locally")',
            "    toast.setSeverity(fluentqt.Toast.Severity.Informational)",
            "    toast.present(info_button)",
            "info_button.clicked.connect(show_info)",
            "",
            'success_button = fluentqt.Button("Success")',
            "def show_success():",
            '    toast.setMessage("Changes published")',
            "    toast.setSeverity(fluentqt.Toast.Severity.Success)",
            "    toast.present(success_button)",
            "success_button.clicked.connect(show_success)",
            "",
            'warning_button = fluentqt.Button("Warning")',
            "def show_warning():",
            '    toast.setMessage("Connection is unstable")',
            "    toast.setSeverity(fluentqt.Toast.Severity.Warning)",
            "    toast.present(warning_button)",
            "warning_button.clicked.connect(show_warning)",
            "",
            'error_button = fluentqt.Button("Error")',
            "def show_error():",
            '    toast.setMessage("Upload could not finish")',
            "    toast.setSeverity(fluentqt.Toast.Severity.Error)",
            "    toast.present(error_button)",
            "error_button.clicked.connect(show_error)",
        )
    if key == ("toast", "toast-title-placement"):
        return _source(
            "toast = fluentqt.Toast()",
            'toast.setTitle("Sync complete")',
            'toast.setMessage("12 files are now available offline.")',
            "toast.setSeverity(fluentqt.Toast.Severity.Success)",
            "toast.setDuration(2600)",
            "",
            "def present(button, placement):",
            "    toast.setPlacement(placement)",
            "    toast.present(button)",
            "",
            'top_start = fluentqt.Button("Top start")',
            "top_start.clicked.connect(",
            "    lambda: present(top_start, fluentqt.Toast.Placement.TopStart)",
            ")",
            'top = fluentqt.Button("Top")',
            "top.clicked.connect(",
            "    lambda: present(top, fluentqt.Toast.Placement.Top)",
            ")",
            'top_end = fluentqt.Button("Top end")',
            "top_end.clicked.connect(",
            "    lambda: present(top_end, fluentqt.Toast.Placement.TopEnd)",
            ")",
            'bottom_start = fluentqt.Button("Bottom start")',
            "bottom_start.clicked.connect(",
            "    lambda: present(",
            "        bottom_start, fluentqt.Toast.Placement.BottomStart",
            "    )",
            ")",
            'bottom = fluentqt.Button("Bottom")',
            "bottom.clicked.connect(",
            "    lambda: present(bottom, fluentqt.Toast.Placement.Bottom)",
            ")",
            'bottom_end = fluentqt.Button("Bottom end")',
            "bottom_end.clicked.connect(",
            "    lambda: present(",
            "        bottom_end, fluentqt.Toast.Placement.BottomEnd",
            "    )",
            ")",
        )
    if key == ("number-box", "number-box-spin-placement"):
        return _source(
            "inline = fluentqt.NumberBox()",
            'inline.setHeader("Inline")',
            "inline.setRange(0, 100)",
            "inline.setSmallChange(5)",
            "inline.setLargeChange(25)",
            "inline.setValue(50)",
            "inline.setSpinButtonPlacementMode(",
            "    fluentqt.NumberBox.SpinButtonPlacementMode.Inline",
            ")",
            "",
            "compact = fluentqt.NumberBox()",
            'compact.setHeader("Compact")',
            "compact.setRange(0, 100)",
            "compact.setSmallChange(5)",
            "compact.setLargeChange(25)",
            "compact.setValue(50)",
            "compact.setSpinButtonPlacementMode(",
            "    fluentqt.NumberBox.SpinButtonPlacementMode.Compact",
            ")",
        )
    if key == ("font-icon", "font-icon-optical-sizes"):
        return _source(
            'compact = fluentqt.FontIcon("ic_fluent_search_20_regular")',
            "compact.setIconSize(fluentqt.Typography.IconSize.Compact)",
            "",
            'standard = fluentqt.FontIcon("ic_fluent_search_20_regular")',
            "standard.setIconSize(fluentqt.Typography.IconSize.Standard)",
            "",
            'medium = fluentqt.FontIcon("ic_fluent_search_20_regular")',
            "medium.setIconSize(24)",
            "",
            'large = fluentqt.FontIcon("ic_fluent_search_20_regular")',
            "large.setIconSize(32)",
        )
    if key == ("font-icon", "font-icon-color-rotation"):
        return _source(
            'inherited = fluentqt.FontIcon("ic_fluent_arrow_right_20_regular")',
            "inherited.setIconSize(24)",
            "",
            'accent = fluentqt.FontIcon("ic_fluent_arrow_right_20_regular")',
            "accent.setIconSize(24)",
            'accent.setColor(QColor("#0F6CBD"))',
            "accent.setRotation(90.0)",
            "",
            'warning = fluentqt.FontIcon("ic_fluent_arrow_right_20_regular")',
            "warning.setIconSize(24)",
            'warning.setColor(QColor("#F7630C"))',
            "warning.setRotation(180.0)",
            imports="from PySide6.QtGui import QColor",
        )
    return None


def _row(parent: QWidget | None = None, spacing: int = 12) -> tuple[QWidget, QHBoxLayout]:
    root = QWidget(parent)
    layout = QHBoxLayout(root)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(spacing)
    layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
    return root, layout


def _column(
    parent: QWidget | None = None, spacing: int = 12
) -> tuple[QWidget, QVBoxLayout]:
    root = QWidget(parent)
    layout = QVBoxLayout(root)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(spacing)
    layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)
    return root, layout


def _hold(owner: QWidget, *values: object) -> None:
    owner._fluentqt_gallery_references = values


def _enum_name(value: object) -> str:
    """Normalize old Shiboken enum byte names for generated Python source."""

    name = value.name
    return name.decode("ascii") if isinstance(name, bytes) else str(name)


def _result(
    widget: QWidget,
    source: str,
    *covered_types: str,
    source_driven: bool = False,
    display_source: str | None = None,
) -> PreviewResult:
    return PreviewResult(
        widget=widget,
        source=display_source if display_source is not None else source,
        covered_types=tuple(covered_types),
        preview_source=source,
        parity_level="native-equivalent",
        source_driven=source_driven,
    )


_DIRECT_TEXT_COLOR = re.compile(
    r"(?:^|[;{\n])\s*color\s*:",
    flags=re.IGNORECASE,
)


def _apply_preview_text_contract(widget: QWidget) -> None:
    """Keep default Fluent labels readable on the styled preview surface.

    The Gallery sample card installs QStyleSheetStyle over its subtree. Qt can
    then ignore a child Label's palette-only WindowText value, although the
    same snippet is correct in a plain application window. Give only labels
    without an explicit semantic role or caller-owned CSS color a Primary role
    while they are hosted by the Gallery. The displayed teaching source stays
    focused on the component API, and custom/semantic colors remain untouched.
    """

    labels = list(widget.findChildren(fluentqt.Label))
    if isinstance(widget, fluentqt.Label):
        labels.insert(0, widget)
    for label in labels:
        if label.textColorRole() != fluentqt.Label.TextColorRole.Default:
            continue
        if _DIRECT_TEXT_COLOR.search(label.styleSheet()):
            continue
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)


def register_source_samples(
    route_id: str,
    covered_types: Sequence[str],
    samples: Mapping[str, SourceSpec],
) -> None:
    """Register source-driven cards whose preview executes displayed code.

    Source-driven cards make semantic drift impossible: the exact Python text
    shown in the Gallery is also what constructs the live preview.
    """

    sample_specs = dict(samples)

    @native_samples(route_id, *sample_specs)
    def build(sample_id: str, parent: QWidget | None) -> PreviewResult:
        widget_name, source = sample_specs[sample_id]
        source = _apply_root_parent_contract(source, widget_name)
        source = _apply_root_layout_contract(
            route_id, sample_id, source, widget_name
        )
        namespace: dict[str, object] = {
            "__name__": "fluentqt_gallery_{0}_{1}".format(
                route_id.replace("-", "_"), sample_id.replace("-", "_")
            ),
            # Source snippets may opt into the real Gallery host when an API
            # resolves behavior at construction time (for example the
            # window-scoped EditingCommandRouter). Standalone execution falls
            # back to None via globals().get("gallery_parent").
            "gallery_parent": parent,
        }
        exec(compile(source, "<{0}/{1}>".format(route_id, sample_id), "exec"), namespace)
        widget = namespace.get(widget_name)
        if not isinstance(widget, QWidget):
            raise TypeError(
                "Source-driven Gallery sample {0}/{1} did not create QWidget {2!r}"
                .format(route_id, sample_id, widget_name)
            )
        if parent is not None and widget.parentWidget() is not parent:
            widget.setParent(parent)
        widget._fluentqt_gallery_source_namespace = namespace
        return _result(
            widget,
            source,
            *covered_types,
            source_driven=True,
            display_source=_concise_display_source(
                source,
                widget_name,
                route_id,
                sample_id,
                covered_types,
            ),
        )


def _materialize_displayed_source(
    route_id: str,
    sample_id: str,
    result: PreviewResult,
    parent: QWidget | None,
) -> PreviewResult:
    """Use the displayed Python text itself as the live preview.

    Legacy hand-authored builders returned a separately constructed widget and
    an executable snippet.  Even when both were intended to match, that left
    two implementations able to drift.  Materializing the snippet here makes
    the user-visible source the single implementation for every SampleCard.
    """

    namespace: dict[str, object] = {
        "__name__": "fluentqt_gallery_{0}_{1}".format(
            route_id.replace("-", "_"), sample_id.replace("-", "_")
        ),
        "gallery_parent": parent,
    }
    preview_source = result.preview_source
    exec(
        compile(
            preview_source,
            "<{0}/{1}>".format(route_id, sample_id),
            "exec",
        ),
        namespace,
    )
    candidates: dict[int, QWidget] = {}
    for name, value in namespace.items():
        if name == "gallery_parent" or value is parent:
            continue
        if not isinstance(value, QWidget):
            continue
        widget_parent = value.parentWidget()
        if widget_parent is None or widget_parent is parent:
            candidates[id(value)] = value
    if len(candidates) != 1:
        for candidate in candidates.values():
            candidate.close()
            candidate.deleteLater()
        raise RuntimeError(
            "Displayed source for {0}/{1} created {2} preview roots; "
            "expected exactly one".format(
                route_id, sample_id, len(candidates)
            )
        )

    widget = next(iter(candidates.values()))
    widget_name = next(
        (
            name
            for name, value in namespace.items()
            if value is widget and name != "gallery_parent"
        ),
        "",
    )
    if parent is not None and widget.parentWidget() is not parent:
        widget.setParent(parent)
    widget._fluentqt_gallery_source_namespace = namespace

    original = result.widget
    if original is not widget:
        original.close()
        original.deleteLater()
    return PreviewResult(
        widget=widget,
        source=(
            result.source
            if result.source != preview_source
            else _concise_display_source(
                preview_source,
                widget_name,
                route_id,
                sample_id,
                result.covered_types,
            )
        ),
        covered_types=result.covered_types,
        preview_source=preview_source,
        parity_level=result.parity_level,
        source_driven=True,
    )


def _expander_item(
    title: str,
    detail: str,
    parent: QWidget | None = None,
) -> fluentqt.Expander:
    body = QWidget()
    body_layout = QVBoxLayout(body)
    body_layout.setContentsMargins(16, 12, 16, 14)
    body_layout.setSpacing(4)
    heading = fluentqt.Label("Additional details", body)
    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.setWordWrap(True)
    body_layout.addWidget(heading)
    description = fluentqt.Label(detail, body)
    description.setFluentTypography(fluentqt.FontRole.Body)
    description.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    description.setWordWrap(True)
    body_layout.addWidget(description)
    item = fluentqt.Expander(parent)
    item.setHeaderText(title)
    item.setOwnedContentWidget(body)
    return item


@native_samples("calendar-date-picker", "calendar-date-picker-basic")
def _calendar_date_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.CalendarDatePicker(parent)
    picker.setPlaceholderText("Pick a date")
    selected_dates: list[QDate] = []
    picker.dateChanged.connect(selected_dates.append)
    _hold(picker, selected_dates)
    return _result(
        picker,
        _source(
            "picker = fluentqt.CalendarDatePicker()",
            'picker.setPlaceholderText("Pick a date")',
            "picker.dateChanged.connect(lambda date: print(date.toString()))",
        ),
        "CalendarDatePicker",
    )


@native_samples("calendar-view", "calendar-view-basic")
def _calendar_view(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    calendar = fluentqt.CalendarView(parent)
    calendar.setSelectedDate(QDate.currentDate())
    return _result(
        calendar,
        _source(
            "calendar = fluentqt.CalendarView()",
            "calendar.setSelectedDate(QDate.currentDate())",
            imports="from PySide6.QtCore import QDate",
        ),
        "CalendarView",
    )


@native_samples("date-picker", "date-picker-basic")
def _date_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.DatePicker(parent)
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Month, "month")
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Day, "day")
    picker.setPlaceholderText(fluentqt.DatePicker.DateField.Year, "year")
    picker.setConfirmButtonAccessibleName("Accept date")
    picker.setCancelButtonAccessibleName("Cancel date")
    picker.setDate(QDate.currentDate())
    return _result(
        picker,
        _source(
            "picker = fluentqt.DatePicker()",
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Month, "month")',
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Day, "day")',
            'picker.setPlaceholderText(fluentqt.DatePicker.DateField.Year, "year")',
            'picker.setConfirmButtonAccessibleName("Accept date")',
            'picker.setCancelButtonAccessibleName("Cancel date")',
            "picker.setDate(QDate.currentDate())",
            imports="from PySide6.QtCore import QDate",
        ),
        "DatePicker",
    )


@native_samples("time-picker", "time-picker-basic")
def _time_picker(_sample_id: str, parent: QWidget | None) -> PreviewResult:
    picker = fluentqt.TimePicker(parent)
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hour")
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "minute")
    picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Period, "AM/PM")
    picker.setConfirmButtonAccessibleName("Accept time")
    picker.setCancelButtonAccessibleName("Cancel time")
    picker.setTime(QTime(9, 30))
    picker.setMinuteIncrement(5)
    return _result(
        picker,
        _source(
            "picker = fluentqt.TimePicker()",
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hour")',
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "minute")',
            'picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Period, "AM/PM")',
            'picker.setConfirmButtonAccessibleName("Accept time")',
            'picker.setCancelButtonAccessibleName("Cancel time")',
            "picker.setTime(QTime(9, 30))",
            "picker.setMinuteIncrement(5)",
            imports="from PySide6.QtCore import QTime",
        ),
        "TimePicker",
    )


@native_samples("accordion", "accordion-single-expansion", "accordion-multiple-expansion")
def _accordion(sample_id: str, parent: QWidget | None) -> PreviewResult:
    multiple = sample_id == "accordion-multiple-expansion"
    spacing = 8 if multiple else 12
    root, layout = _column(parent, spacing)
    accordion = fluentqt.Accordion(root)
    accordion.setExpansionMode(
        fluentqt.Accordion.ExpansionMode.Multiple
        if multiple
        else fluentqt.Accordion.ExpansionMode.Single
    )
    accordion.setFixedWidth(520)
    accordion.setObjectName(
        "galleryAccordionMultiple" if multiple else "galleryAccordionSingle"
    )
    if multiple:
        items = (
            _expander_item("Network", "Wi-Fi and Ethernet are connected.", accordion),
            _expander_item("Proxy", "Use system proxy settings.", accordion),
        )
    else:
        items = (
            _expander_item("Account", "Profile, sign-in, and recovery options.", accordion),
            _expander_item("Notifications", "Choose which activity can interrupt you.", accordion),
            _expander_item("Privacy", "Review diagnostics and personalization settings.", accordion),
        )
    for item in items:
        accordion.addOwnedItem(item)
    items[0].setExpandedAnimated(True, False)
    if multiple:
        items[1].setExpandedAnimated(True, False)
    layout.addWidget(accordion)
    mode = "Multiple" if multiple else "Single"
    source_lines = [
        "def make_section(title, detail):",
        "    body = QWidget()",
        "    body_layout = QVBoxLayout(body)",
        "    body_layout.setContentsMargins(16, 12, 16, 14)",
        "    body_layout.setSpacing(4)",
        '    heading = fluentqt.Label("Additional details", body)',
        "    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)",
        "    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
        "    heading.setWordWrap(True)",
        "    body_layout.addWidget(heading)",
        "    detail_label = fluentqt.Label(detail, body)",
        "    detail_label.setFluentTypography(fluentqt.FontRole.Body)",
        "    detail_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
        "    detail_label.setWordWrap(True)",
        "    body_layout.addWidget(detail_label)",
        "    item = fluentqt.Expander()",
        "    item.setHeaderText(title)",
        "    item.setOwnedContentWidget(body)",
        "    return item",
        "",
        "root = QWidget()",
        "layout = QVBoxLayout(root)",
        "layout.setContentsMargins(0, 0, 0, 0)",
        "layout.setSpacing({0})".format(spacing),
        "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
        "accordion = fluentqt.Accordion(root)",
        "accordion.setExpansionMode(fluentqt.Accordion.ExpansionMode.{0})".format(mode),
        "accordion.setFixedWidth(520)",
    ]
    values = (
        (("Network", "Wi-Fi and Ethernet are connected."), ("Proxy", "Use system proxy settings."))
        if multiple
        else (
            ("Account", "Profile, sign-in, and recovery options."),
            ("Notifications", "Choose which activity can interrupt you."),
            ("Privacy", "Review diagnostics and personalization settings."),
        )
    )
    for index, (title, detail) in enumerate(values):
        source_lines.extend(
            (
                'item_{0} = make_section({1!r}, {2!r})'.format(index, title, detail),
                "accordion.addOwnedItem(item_{0})".format(index),
            )
        )
    source_lines.append("item_0.setExpandedAnimated(True, False)")
    if multiple:
        source_lines.append("item_1.setExpandedAnimated(True, False)")
    source_lines.append("layout.addWidget(accordion)")
    _hold(root, accordion, *items)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
        ),
        "Accordion",
        "Expander",
    )


def _appearance_card(
    parent: QWidget,
    title: str,
    detail: str,
    appearance: fluentqt.Card.Appearance,
    border_visible: bool = True,
) -> fluentqt.Card:
    card = fluentqt.Card(parent)
    card.setAppearance(appearance)
    card.setBorderVisible(border_visible)
    card.setFixedSize(170, 88)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(16, 12, 16, 12)
    layout.setSpacing(3)
    heading = fluentqt.Label(title, card)
    heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
    heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    heading.setWordWrap(True)
    layout.addWidget(heading)
    description = fluentqt.Label(detail, card)
    description.setFluentTypography(fluentqt.FontRole.Caption)
    description.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
    description.setWordWrap(True)
    layout.addWidget(description)
    layout.addStretch(1)
    return card


@native_samples("card", "card-surface-appearances", "card-border-visibility")
def _card(sample_id: str, parent: QWidget | None) -> PreviewResult:
    root, layout = _row(parent)
    if sample_id == "card-surface-appearances":
        values = (
            ("Layer", "Default grouped surface", fluentqt.Card.Appearance.Layer, True),
            ("LayerAlt", "Alternate layer tone", fluentqt.Card.Appearance.LayerAlt, True),
            ("Canvas", "Matches the page canvas", fluentqt.Card.Appearance.Canvas, True),
        )
    else:
        values = (
            ("Bordered", "Independent surface", fluentqt.Card.Appearance.Layer, True),
            ("Borderless", "Nested composition", fluentqt.Card.Appearance.Layer, False),
        )
    cards = []
    source_lines = [
        "root = QWidget()",
        "layout = QHBoxLayout(root)",
        "layout.setContentsMargins(0, 0, 0, 0)",
        "layout.setSpacing(12)",
        "layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)",
    ]
    for index, (title, detail, appearance, border) in enumerate(values):
        card = _appearance_card(root, title, detail, appearance, border)
        layout.addWidget(card)
        cards.append(card)
        source_lines.extend(
            (
                "card_{0} = fluentqt.Card(root)".format(index),
                "card_{0}.setAppearance(fluentqt.Card.Appearance.{1})".format(
                    index, _enum_name(appearance)
                ),
                "card_{0}.setBorderVisible({1})".format(index, border),
                "card_{0}.setFixedSize(170, 88)".format(index),
                "card_{0}_layout = QVBoxLayout(card_{0})".format(index),
                "card_{0}_layout.setContentsMargins(16, 12, 16, 12)".format(index),
                "card_{0}_layout.setSpacing(3)".format(index),
                "card_{0}_title = fluentqt.Label({1!r}, card_{0})".format(index, title),
                "card_{0}_title.setFluentTypography(fluentqt.FontRole.BodyStrong)".format(index),
                "card_{0}_title.setTextColorRole(fluentqt.Label.TextColorRole.Primary)".format(index),
                "card_{0}_title.setWordWrap(True)".format(index),
                "card_{0}_layout.addWidget(card_{0}_title)".format(index),
                "card_{0}_detail = fluentqt.Label({1!r}, card_{0})".format(index, detail),
                "card_{0}_detail.setFluentTypography(fluentqt.FontRole.Caption)".format(index),
                "card_{0}_detail.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)".format(index),
                "card_{0}_detail.setWordWrap(True)".format(index),
                "card_{0}_layout.addWidget(card_{0}_detail)".format(index),
                "card_{0}_layout.addStretch(1)".format(index),
                "layout.addWidget(card_{0})".format(index),
            )
        )
    layout.addStretch()
    source_lines.append("layout.addStretch()")
    _hold(root, *cards)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget",
        ),
        "Card",
    )


@native_samples("divider", "divider-horizontal-insets", "divider-vertical-orientation")
def _divider(sample_id: str, parent: QWidget | None) -> PreviewResult:
    if sample_id == "divider-horizontal-insets":
        card = fluentqt.Card(parent)
        card.setFixedSize(520, 116)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(16, 14, 16, 14)
        layout.setSpacing(10)
        full = fluentqt.Divider(card)
        inset = fluentqt.Divider(card)
        inset.setLeadingInset(24)
        inset.setTrailingInset(48)
        full_label = fluentqt.Label("Full-width separator", card)
        full_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        full_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        full_label.setWordWrap(True)
        inset_label = fluentqt.Label("Inset separator", card)
        inset_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
        inset_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        inset_label.setWordWrap(True)
        layout.addWidget(full_label)
        layout.addWidget(full)
        layout.addWidget(inset_label)
        layout.addWidget(inset)
        return _result(
            card,
            _source(
                "card = fluentqt.Card()",
                "card.setFixedSize(520, 116)",
                "layout = QVBoxLayout(card)",
                "layout.setContentsMargins(16, 14, 16, 14)",
                "layout.setSpacing(10)",
                'full_label = fluentqt.Label("Full-width separator", card)',
                "full_label.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "full_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
                "full_label.setWordWrap(True)",
                "layout.addWidget(full_label)",
                "full = fluentqt.Divider(card)",
                "layout.addWidget(full)",
                'inset_label = fluentqt.Label("Inset separator", card)',
                "inset_label.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "inset_label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
                "inset_label.setWordWrap(True)",
                "layout.addWidget(inset_label)",
                "inset = fluentqt.Divider(card)",
                "inset.setLeadingInset(24)",
                "inset.setTrailingInset(48)",
                "layout.addWidget(inset)",
                imports="from PySide6.QtWidgets import QVBoxLayout",
            ),
            "Divider",
        )

    card = fluentqt.Card(parent)
    card.setFixedSize(420, 76)
    layout = QHBoxLayout(card)
    layout.setContentsMargins(20, 16, 20, 16)
    layout.setSpacing(16)
    dividers = []
    for index, text in enumerate(("Details", "Activity", "History")):
        label = fluentqt.Label(text, card)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        label.setWordWrap(True)
        layout.addWidget(label)
        if index < 2:
            divider = fluentqt.Divider(Qt.Vertical, card)
            divider.setLeadingInset(4)
            divider.setTrailingInset(4)
            divider.setFixedHeight(44)
            layout.addWidget(divider)
            dividers.append(divider)
    layout.addStretch(1)
    _hold(card, *dividers)
    return _result(
        card,
        _source(
            "card = fluentqt.Card()",
            "card.setFixedSize(420, 76)",
            "layout = QHBoxLayout(card)",
            "layout.setContentsMargins(20, 16, 20, 16)",
            "layout.setSpacing(16)",
            'details = fluentqt.Label("Details", card)',
            "details.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "details.setWordWrap(True)",
            "layout.addWidget(details)",
            "first = fluentqt.Divider(Qt.Vertical, card)",
            "first.setLeadingInset(4)",
            "first.setTrailingInset(4)",
            "first.setFixedHeight(44)",
            "layout.addWidget(first)",
            'activity = fluentqt.Label("Activity", card)',
            "activity.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "activity.setWordWrap(True)",
            "layout.addWidget(activity)",
            "second = fluentqt.Divider(Qt.Vertical, card)",
            "second.setLeadingInset(4)",
            "second.setTrailingInset(4)",
            "second.setFixedHeight(44)",
            "layout.addWidget(second)",
            'history = fluentqt.Label("History", card)',
            "history.setTextColorRole(fluentqt.Label.TextColorRole.Primary)",
            "history.setWordWrap(True)",
            "layout.addWidget(history)",
            "layout.addStretch(1)",
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QHBoxLayout",
        ),
        "Divider",
    )


@native_samples("expander", "expander-text-content", "expander-state-signal")
def _expander(sample_id: str, parent: QWidget | None) -> PreviewResult:
    if sample_id == "expander-text-content":
        root, layout = _column(parent, 12)
        expander = fluentqt.Expander(root)
        expander.setObjectName("galleryExpanderTextContent")
        expander.setFixedWidth(520)
        expander.setHeaderText("Connection details")
        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(16, 12, 16, 14)
        body_layout.setSpacing(4)
        heading = fluentqt.Label("Additional details", body)
        heading.setFluentTypography(fluentqt.FontRole.BodyStrong)
        heading.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        heading.setWordWrap(True)
        body_layout.addWidget(heading)
        detail = fluentqt.Label("Server: api.example.com\nTransport: TLS 1.3", body)
        detail.setFluentTypography(fluentqt.FontRole.Body)
        detail.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
        detail.setWordWrap(True)
        body_layout.addWidget(detail)
        expander.setOwnedContentWidget(body)
        expander.setExpandedAnimated(True, False)
        layout.addWidget(expander)
        _hold(root, expander, body, heading, detail)
        return _result(
            root,
            _source(
                "root = QWidget()",
                "layout = QVBoxLayout(root)",
                "layout.setContentsMargins(0, 0, 0, 0)",
                "layout.setSpacing(12)",
                "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
                "body = QWidget()",
                "body_layout = QVBoxLayout(body)",
                "body_layout.setContentsMargins(16, 12, 16, 14)",
                "body_layout.setSpacing(4)",
                'heading = fluentqt.Label("Additional details", body)',
                "heading.setFluentTypography(fluentqt.FontRole.BodyStrong)",
                "body_layout.addWidget(heading)",
                'detail = fluentqt.Label("Server: api.example.com\\nTransport: TLS 1.3", body)',
                "detail.setWordWrap(True)",
                "body_layout.addWidget(detail)",
                "expander = fluentqt.Expander(root)",
                'expander.setHeaderText("Connection details")',
                "expander.setFixedWidth(520)",
                "expander.setOwnedContentWidget(body)",
                "expander.setExpandedAnimated(True, False)",
                "layout.addWidget(expander)",
                imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
            ),
            "Expander",
        )

    root, layout = _column(parent, 8)
    status = fluentqt.Label("Collapsed", root)
    status.setObjectName("galleryExpanderStateLabel")
    status.setFluentTypography(fluentqt.FontRole.Caption)
    status.setTextColorRole(fluentqt.Label.TextColorRole.Primary)
    expander = _expander_item(
        "Advanced options", "Diagnostic logging and retry behavior.", root
    )
    expander.setFixedWidth(520)
    expander.expandedChanged.connect(
        lambda expanded: status.setText("Expanded" if expanded else "Collapsed")
    )
    layout.addWidget(expander)
    layout.addWidget(status)
    _hold(root, expander, status)
    return _result(
        root,
        _source(
            "root = QWidget()",
            "layout = QVBoxLayout(root)",
            "layout.setContentsMargins(0, 0, 0, 0)",
            "layout.setSpacing(8)",
            "layout.setAlignment(Qt.AlignTop | Qt.AlignLeft)",
            "body = QWidget()",
            "body_layout = QVBoxLayout(body)",
            'body_layout.addWidget(fluentqt.Label("Additional details", body))',
            'body_layout.addWidget(fluentqt.Label("Diagnostic logging and retry behavior.", body))',
            "expander = fluentqt.Expander(root)",
            'expander.setHeaderText("Advanced options")',
            "expander.setFixedWidth(520)",
            "expander.setOwnedContentWidget(body)",
            'status = fluentqt.Label("Collapsed", root)',
            "status.setFluentTypography(fluentqt.FontRole.Caption)",
            "expander.expandedChanged.connect(",
            '    lambda expanded: status.setText("Expanded" if expanded else "Collapsed")',
            ")",
            "layout.addWidget(expander)",
            "layout.addWidget(status)",
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtWidgets import QVBoxLayout, QWidget",
        ),
        "Expander",
    )


@native_samples(
    "field",
    "field-helper-text",
    "field-required-error",
    "field-warning-success",
)
def _field(sample_id: str, parent: QWidget | None) -> PreviewResult:
    card = fluentqt.Card(parent)
    layout = QVBoxLayout(card)
    layout.setContentsMargins(16, 14, 16, 14)

    if sample_id == "field-helper-text":
        card.setFixedSize(520, 220)
        layout.setSpacing(12)
        email_field = fluentqt.Field(card)
        email_field.setLabelText("Email")
        email_field.setHelperText("Used only for account recovery.")
        email_editor = fluentqt.LineEdit()
        email_editor.setPlaceholderText("name@example.com")
        email_field.setOwnedEditor(email_editor)
        layout.addWidget(email_field)
        role_field = fluentqt.Field(card)
        role_field.setLabelText("Role")
        role_editor = fluentqt.ComboBox()
        role_editor.addItems(["Designer", "Developer", "Researcher"])
        role_field.setOwnedEditor(role_editor)
        layout.addWidget(role_field)
        _hold(
            card,
            email_field,
            email_editor,
            role_field,
            role_editor,
        )
        return _result(
            card,
            _source(
                "card = fluentqt.Card()",
                "card.setFixedSize(520, 220)",
                "layout = QVBoxLayout(card)",
                "layout.setContentsMargins(16, 14, 16, 14)",
                "layout.setSpacing(12)",
                "email_field = fluentqt.Field(card)",
                'email_field.setLabelText("Email")',
                'email_field.setHelperText("Used only for account recovery.")',
                "email_editor = fluentqt.LineEdit()",
                'email_editor.setPlaceholderText("name@example.com")',
                "email_field.setOwnedEditor(email_editor)",
                "layout.addWidget(email_field)",
                "role_field = fluentqt.Field(card)",
                'role_field.setLabelText("Role")',
                "role_editor = fluentqt.ComboBox()",
                'role_editor.addItems(["Designer", "Developer", "Researcher"])',
                "role_field.setOwnedEditor(role_editor)",
                "layout.addWidget(role_field)",
                imports="from PySide6.QtWidgets import QVBoxLayout",
            ),
            "Field",
        )

    if sample_id == "field-required-error":
        card.setFixedSize(520, 148)
        layout.setSpacing(0)
        field = fluentqt.Field(card)
        field.setLabelText("Password")
        field.setRequired(True)
        field.setValidationState(fluentqt.Field.ValidationState.Error)
        field.setValidationMessage("Password must be at least 8 characters.")
        editor = fluentqt.LineEdit()
        editor.setText("1234")
        field.setOwnedEditor(editor)
        layout.addWidget(field)
        _hold(card, field, editor)
        return _result(
            card,
            _source(
                "card = fluentqt.Card()",
                "card.setFixedSize(520, 148)",
                "layout = QVBoxLayout(card)",
                "layout.setContentsMargins(16, 14, 16, 14)",
                "layout.setSpacing(0)",
                "field = fluentqt.Field(card)",
                'field.setLabelText("Password")',
                "field.setRequired(True)",
                "field.setValidationState(",
                "    fluentqt.Field.ValidationState.Error",
                ")",
                "field.setValidationMessage(",
                '    "Password must be at least 8 characters."',
                ")",
                "editor = fluentqt.LineEdit()",
                'editor.setText("1234")',
                "field.setOwnedEditor(editor)",
                "layout.addWidget(field)",
                imports="from PySide6.QtWidgets import QVBoxLayout",
            ),
            "Field",
        )

    card.setFixedSize(520, 220)
    layout.setSpacing(16)
    warning = fluentqt.Field(card)
    warning.setLabelText("Username")
    warning.setValidationState(fluentqt.Field.ValidationState.Warning)
    warning.setValidationMessage("This name is already taken.")
    warning_editor = fluentqt.LineEdit()
    warning_editor.setText("alex")
    warning.setOwnedEditor(warning_editor)
    layout.addWidget(warning)
    success = fluentqt.Field(card)
    success.setLabelText("Display name")
    success.setValidationState(fluentqt.Field.ValidationState.Success)
    success.setValidationMessage("Looks good")
    success_editor = fluentqt.LineEdit()
    success_editor.setText("Alex Chen")
    success.setOwnedEditor(success_editor)
    layout.addWidget(success)
    _hold(card, warning, warning_editor, success, success_editor)
    return _result(
        card,
        _source(
            "card = fluentqt.Card()",
            "card.setFixedSize(520, 220)",
            "layout = QVBoxLayout(card)",
            "layout.setContentsMargins(16, 14, 16, 14)",
            "layout.setSpacing(16)",
            "warning = fluentqt.Field(card)",
            'warning.setLabelText("Username")',
            "warning.setValidationState(",
            "    fluentqt.Field.ValidationState.Warning",
            ")",
            'warning.setValidationMessage("This name is already taken.")',
            "warning_editor = fluentqt.LineEdit()",
            'warning_editor.setText("alex")',
            "warning.setOwnedEditor(warning_editor)",
            "layout.addWidget(warning)",
            "success = fluentqt.Field(card)",
            'success.setLabelText("Display name")',
            "success.setValidationState(",
            "    fluentqt.Field.ValidationState.Success",
            ")",
            'success.setValidationMessage("Looks good")',
            "success_editor = fluentqt.LineEdit()",
            'success_editor.setText("Alex Chen")',
            "success.setOwnedEditor(success_editor)",
            "layout.addWidget(success)",
            imports="from PySide6.QtWidgets import QVBoxLayout",
        ),
        "Field",
    )


@native_samples("font-icon", "font-icon-optical-sizes", "font-icon-color-rotation")
def _font_icon(sample_id: str, parent: QWidget | None) -> PreviewResult:
    def icon_cell(
        owner: QWidget,
        glyph: str,
        size: int,
        caption: str,
        color: QColor = QColor(),
        rotation: float = 0.0,
    ) -> QWidget:
        cell = QWidget(owner)
        cell_layout = QVBoxLayout(cell)
        cell_layout.setContentsMargins(8, 4, 8, 4)
        cell_layout.setSpacing(8)
        cell_layout.setAlignment(Qt.AlignHCenter | Qt.AlignTop)
        icon = fluentqt.FontIcon(glyph, cell)
        icon.setIconSize(size)
        icon.setColor(color)
        icon.setRotation(rotation)
        icon.setAccessibleName(caption)
        label = fluentqt.Label(caption, cell)
        label.setFluentTypography(fluentqt.FontRole.Caption)
        label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)
        label.setAlignment(Qt.AlignCenter)
        cell_layout.addWidget(icon, 0, Qt.AlignHCenter)
        cell_layout.addWidget(label)
        _hold(cell, icon, label)
        return cell

    cells = []
    if sample_id == "font-icon-optical-sizes":
        root = fluentqt.Card(parent)
        root.setFixedSize(420, 104)
        layout = QHBoxLayout(root)
        layout.setContentsMargins(18, 14, 18, 14)
        layout.setSpacing(22)
        values = (
            (fluentqt.Typography.IconSize.Compact, "12 px", QColor(), 0.0),
            (fluentqt.Typography.IconSize.Standard, "16 px", QColor(), 0.0),
            (24, "24 px", QColor(), 0.0),
            (32, "32 px", QColor(), 0.0),
        )
        glyph = "\ue721"
    else:
        root, layout = _row(parent, 20)
        values = (
            (24, "Inherited", QColor(), 0.0),
            (24, "Accent · 90°", QColor("#0F6CBD"), 90.0),
            (24, "Warning · 180°", QColor("#F7630C"), 180.0),
        )
        glyph = "\ue72a"
    for size, caption, color, rotation in values:
        cell = icon_cell(root, glyph, size, caption, color, rotation)
        layout.addWidget(cell)
        cells.append(cell)
    if sample_id == "font-icon-optical-sizes":
        layout.addStretch(1)
    source_lines = [
        "def make_icon_cell(parent, glyph, size, caption, color=QColor(), rotation=0.0):",
        "    cell = QWidget(parent)",
        "    cell_layout = QVBoxLayout(cell)",
        "    cell_layout.setContentsMargins(8, 4, 8, 4)",
        "    cell_layout.setSpacing(8)",
        "    cell_layout.setAlignment(Qt.AlignHCenter | Qt.AlignTop)",
        "    icon = fluentqt.FontIcon(glyph, cell)",
        "    icon.setIconSize(size)",
        "    icon.setColor(color)",
        "    icon.setRotation(rotation)",
        "    icon.setAccessibleName(caption)",
        "    label = fluentqt.Label(caption, cell)",
        "    label.setFluentTypography(fluentqt.FontRole.Caption)",
        "    label.setTextColorRole(fluentqt.Label.TextColorRole.Secondary)",
        "    label.setAlignment(Qt.AlignCenter)",
        "    cell_layout.addWidget(icon, 0, Qt.AlignHCenter)",
        "    cell_layout.addWidget(label)",
        "    return cell",
        "",
        "root = fluentqt.Card()" if sample_id == "font-icon-optical-sizes" else "root = QWidget()",
        "layout = QHBoxLayout(root)",
    ]
    if sample_id == "font-icon-optical-sizes":
        source_lines.extend(
            (
                "root.setFixedSize(420, 104)",
                "layout.setContentsMargins(18, 14, 18, 14)",
                "layout.setSpacing(22)",
            )
        )
    else:
        source_lines.extend(
            (
                "layout.setContentsMargins(0, 0, 0, 0)",
                "layout.setSpacing(20)",
                "layout.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)",
            )
        )
    if sample_id == "font-icon-optical-sizes":
        source_lines.extend(
            (
                "values = (",
                "    (fluentqt.Typography.IconSize.Compact, '12 px', QColor(), 0.0),",
                "    (fluentqt.Typography.IconSize.Standard, '16 px', QColor(), 0.0),",
                "    (24, '24 px', QColor(), 0.0),",
                "    (32, '32 px', QColor(), 0.0),",
                ")",
            )
        )
    else:
        source_lines.extend(
            (
                "values = (",
                "    (24, 'Inherited', QColor(), 0.0),",
                "    (24, 'Accent \\u00b7 90\\u00b0', QColor('#0F6CBD'), 90.0),",
                "    (24, 'Warning \\u00b7 180\\u00b0', QColor('#F7630C'), 180.0),",
                ")",
            )
        )
    source_lines.extend(
        (
            "for size, caption, color, rotation in values:",
            "    layout.addWidget(make_icon_cell(root, {0!r}, size, caption, color, rotation))".format(glyph),
        )
    )
    if sample_id == "font-icon-optical-sizes":
        source_lines.append("layout.addStretch(1)")
    _hold(root, *cells)
    return _result(
        root,
        _source(
            *source_lines,
            imports="from PySide6.QtCore import Qt\nfrom PySide6.QtGui import QColor\nfrom PySide6.QtWidgets import QHBoxLayout, QVBoxLayout, QWidget",
        ),
        "FontIcon",
    )


@native_samples("shimmer", "shimmer-templates", "shimmer-custom-elements", "shimmer-static-phase")
def _shimmer(sample_id: str, parent: QWidget | None) -> PreviewResult:
    # Reuse the same painted sample surface and caption helpers as every other
    # Status & info route.  The executed source is also the displayed source,
    # so the preview cannot silently diverge from the Python example.
    from .native_samples_status import _status_script

    if sample_id == "shimmer-templates":
        body = """
        root, layout = make_status_surface()
        row, row_layout = horizontal_group(root, 24)
        values = (
            (fluentqt.Shimmer.ShimmerTemplate.ImageCard, QSize(160, 92), "ImageCard"),
            (fluentqt.Shimmer.ShimmerTemplate.AvatarTextRow, QSize(180, 56), "AvatarTextRow"),
            (fluentqt.Shimmer.ShimmerTemplate.TextBlock, QSize(220, 72), "TextBlock"),
        )
        for template, size, caption in values:
            shimmer = fluentqt.Shimmer(row)
            shimmer.setShimmerTemplate(template)
            shimmer.setFixedSize(size)
            row_layout.addWidget(labeled_column(row, caption, shimmer))
        layout.addWidget(row)
        """
        extra_imports = "from PySide6.QtCore import QSize"
    elif sample_id == "shimmer-custom-elements":
        body = """
        root, layout = make_status_surface()
        shimmer = fluentqt.Shimmer(root)
        shimmer.setFixedSize(330, 160)
        shimmer.setElements([
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.RoundedRect,
                QRectF(0, 0, 330, 82),
                8.0,
            ),
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.Circle,
                QRectF(0, 104, 36, 36),
            ),
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.Line,
                QRectF(50, 106, 180, 12),
            ),
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.Line,
                QRectF(50, 128, 124, 12),
            ),
            fluentqt.Shimmer.Element(
                fluentqt.Shimmer.Shape.RoundedRect,
                QRectF(250, 106, 72, 30),
                6.0,
            ),
        ])
        layout.addWidget(shimmer)
        """
        extra_imports = ""
    else:
        body = """
        root, layout = make_status_surface()
        shimmer = fluentqt.Shimmer(root)
        shimmer.setShimmerTemplate(fluentqt.Shimmer.ShimmerTemplate.TextBlock)
        shimmer.setFixedSize(260, 72)
        shimmer.setAnimationEnabled(False)
        shimmer.setShimmerProgress(0.42)
        status = make_status_label(root, "Progress phase: 0.42")
        layout.addWidget(shimmer)
        layout.addWidget(status)
        """
        extra_imports = ""

    source = _status_script(body, extra_imports)
    namespace: dict[str, object] = {
        "__name__": "fluentqt_gallery_shimmer_{0}".format(
            sample_id.replace("-", "_")
        ),
        "gallery_parent": parent,
    }
    exec(compile(source, "<shimmer/{0}>".format(sample_id), "exec"), namespace)
    root = namespace["root"]
    if not isinstance(root, QWidget):
        raise TypeError("Shimmer Gallery source did not construct a QWidget root")
    root._fluentqt_gallery_source_namespace = namespace
    return _result(root, source, "Shimmer")


def _list_model(view: fluentqt.ListView, values: tuple[str, ...]) -> QStandardItemModel:
    model = QStandardItemModel(view)
    for value in values:
        model.appendRow(QStandardItem(value))
    view.setModel(model)
    return model


_ACCENT_PALETTE = (
    QColor(0x00, 0x78, 0xD4),
    QColor(0x03, 0x83, 0x87),
    QColor(0xCA, 0x50, 0x10),
    QColor(0x87, 0x64, 0xB8),
    QColor(0xC2, 0x39, 0xB3),
    QColor(0x49, 0x82, 0x05),
)


def _initials_avatar(name: str, background: QColor, size: int = 28) -> QPixmap:
    screen = QGuiApplication.primaryScreen()
    dpr = max(1.0, screen.devicePixelRatio() if screen is not None else 1.0)
    physical = max(1, round(size * dpr))
    pixmap = QPixmap(physical, physical)
    pixmap.setDevicePixelRatio(dpr)
    pixmap.fill(Qt.GlobalColor.transparent)
    initials = "".join(word[0].upper() for word in name.split()[:2])
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
    painter.setPen(Qt.PenStyle.NoPen)
    painter.setBrush(background)
    painter.drawEllipse(QRectF(0, 0, size, size))
    font = QFont()
    font.setPixelSize(round(size * 0.42))
    font.setWeight(QFont.Weight.DemiBold)
    painter.setFont(font)
    painter.setPen(Qt.GlobalColor.white)
    painter.drawText(
        QRectF(0, 0, size, size),
        Qt.AlignmentFlag.AlignCenter,
        initials,
    )
    painter.end()
    return pixmap


def _glyph_pixmap(glyph: str, background: QColor, size: int) -> QPixmap:
    screen = QGuiApplication.primaryScreen()
    dpr = max(1.0, screen.devicePixelRatio() if screen is not None else 1.0)
    physical = max(1, round(size * dpr))
    pixmap = QPixmap(physical, physical)
    pixmap.setDevicePixelRatio(dpr)
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)
    painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)
    tile = QRectF(0, 0, size, size)
    painter.setPen(Qt.PenStyle.NoPen)
    painter.setBrush(background)
    painter.drawRoundedRect(tile, size / 4.0, size / 4.0)
    icon_font = QFont("FluentQt Icons")
    icon_font.setPixelSize(round(size * 0.55))
    painter.setFont(icon_font)
    painter.setPen(Qt.GlobalColor.white)
    painter.drawText(tile, Qt.AlignmentFlag.AlignCenter, glyph)
    painter.end()
    return pixmap


class _GalleryListRowDelegate(QStyledItemDelegate):
    """Python port of the C++ Gallery's ListRowDelegate."""

    def __init__(self, view: fluentqt.ListView):
        super().__init__(view)
        self._view = view

    def paint(self, painter, option, index):
        if not index.isValid():
            return
        from .foundation_pages import _theme_snapshot

        snapshot = _theme_snapshot(self)
        colors = snapshot["colors"]
        painter.save()
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform)
        selected = bool(option.state & QStyle.StateFlag.State_Selected)
        enabled = bool(option.state & QStyle.StateFlag.State_Enabled)
        hovered = bool(option.state & QStyle.StateFlag.State_MouseOver)
        pressed = bool(option.state & QStyle.StateFlag.State_Sunken) and hovered
        background = QColor(Qt.GlobalColor.transparent)
        if enabled:
            if pressed:
                background = colors["subtleTertiary"]
            elif selected or hovered:
                background = colors["subtleSecondary"]

        background_rect = QRectF(option.rect).adjusted(2.0, 1.0, -2.0, -1.0)
        if background.alpha() > 0:
            path = QPainterPath()
            path.addRoundedRect(background_rect, 4.0, 4.0)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(background)
            painter.drawPath(path)

        cursor_x = background_rect.left() + 14.0
        extent = option.decorationSize
        if not extent.isValid() or extent.isEmpty():
            extent = self._view.iconSize()
        if not extent.isValid() or extent.isEmpty():
            extent = QSize(24, 24)
        decoration = index.data(Qt.ItemDataRole.DecorationRole)
        if isinstance(decoration, QPixmap) and not decoration.isNull():
            icon_rect = QRect(
                round(cursor_x),
                round(background_rect.center().y() - extent.height() / 2.0),
                extent.width(),
                extent.height(),
            )
            painter.drawPixmap(icon_rect, decoration)
            cursor_x = icon_rect.right() + 12.0

        text_rect = QRectF(
            cursor_x,
            background_rect.top(),
            background_rect.right() - cursor_x - 8.0,
            background_rect.height(),
        )
        if not enabled:
            text_color = colors["textDisabled"]
        else:
            text_color = colors["textPrimary"]
        painter.setPen(text_color)
        font = QFont(option.font)
        if selected:
            font.setWeight(QFont.Weight.DemiBold)
        painter.setFont(font)
        text = str(index.data(Qt.ItemDataRole.DisplayRole) or "")
        painter.drawText(
            text_rect,
            Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
            painter.fontMetrics().elidedText(
                text,
                Qt.TextElideMode.ElideRight,
                int(text_rect.width()),
            ),
        )
        painter.restore()

    def sizeHint(self, option, index):
        hint = super().sizeHint(option, index)
        hint.setHeight(max(hint.height(), 36))
        hint.setWidth(hint.width() + 26)
        return hint


@native_samples(
    "list-view",
    "list-view-basic",
    "list-view-multi-select",
    "list-view-horizontal",
    "list-view-reorder",
    "list-view-sections",
    "list-view-scroll-chaining",
    "list-view-placeholder",
)
def _list_view(sample_id: str, parent: QWidget | None) -> PreviewResult:
    view = fluentqt.ListView(parent)
    view.setBackgroundVisible(False)
    view.setBorderVisible(False)
    view.setProperty("fluentPreserveParentSurface", True)
    if view.viewport() is not None:
        view.viewport().setProperty("fluentPreserveParentSurface", True)
    model = QStandardItemModel(view)
    delegate: _GalleryListRowDelegate | None = None

    def populate_glyph_rows(rows: Sequence[tuple[str, str]], icon_size: int) -> None:
        for index, (text, glyph) in enumerate(rows):
            item = QStandardItem(text)
            item.setEditable(False)
            item.setData(
                _glyph_pixmap(
                    glyph,
                    _ACCENT_PALETTE[index % len(_ACCENT_PALETTE)],
                    icon_size,
                ),
                Qt.ItemDataRole.DecorationRole,
            )
            model.appendRow(item)

    source_values: tuple[str, ...] = ()
    source_glyph_rows: tuple[tuple[str, str], ...] = ()
    source_icon_size = 24
    source_lines = [
        "view = fluentqt.ListView(globals().get('gallery_parent'))",
        "model = QStandardItemModel(view)",
        "view.setBackgroundVisible(False)",
        "view.setBorderVisible(False)",
        "view.setProperty('fluentPreserveParentSurface', True)",
        "view.viewport().setProperty('fluentPreserveParentSurface', True)",
    ]
    if sample_id == "list-view-placeholder":
        view.setHeaderText("Downloads")
        view.setFooterText("0 items")
        view.setPlaceholderText("No downloads yet")
        view.setFixedSize(340, 178)
        source_lines.extend(
            (
                "view.setHeaderText('Downloads')",
                "view.setFooterText('0 items')",
                "view.setPlaceholderText('No downloads yet')",
                "view.setFixedSize(340, 178)",
            )
        )
    else:
        delegate = _GalleryListRowDelegate(view)
        view.setItemDelegate(delegate)
        source_lines.extend(
            (
                "delegate = GalleryListRowDelegate(view)",
                "view.setItemDelegate(delegate)",
            )
        )
        if sample_id == "list-view-basic":
            source_values = (
                "Kendall Collins",
                "Henry Ross",
                "Nicole Wagner",
                "Adam Wolfe",
                "Stephanie Meyer",
                "Maya Patel",
                "Alex Chen",
                "Priya Shah",
                "Omar Rivera",
                "Elena Rossi",
                "Jordan Lee",
                "Riley Brooks",
            )
            for index, value in enumerate(source_values):
                item = QStandardItem(value)
                item.setEditable(False)
                item.setData(
                    _initials_avatar(
                        value,
                        _ACCENT_PALETTE[index % len(_ACCENT_PALETTE)],
                    ),
                    Qt.ItemDataRole.DecorationRole,
                )
                model.appendRow(item)
            view.setHeaderText("Contacts")
            view.setAccessibleName("Contacts")
            view.setIconSize(QSize(28, 28))
            view.setFixedSize(320, 234)
            source_lines.extend(
                (
                    "view.setHeaderText('Contacts')",
                    "view.setAccessibleName('Contacts')",
                    "view.setIconSize(QSize(28, 28))",
                    "view.setFixedSize(320, 234)",
                )
            )
        elif sample_id == "list-view-multi-select":
            rows = (
                ("Unread", "\ue715"),
                ("Flagged", "\ue7c1"),
                ("Has photos", "\ue722"),
                ("From contacts", "\ue716"),
                ("Favorites", "\ue734"),
                ("With documents", "\ue8a5"),
                ("Pinned", "\ue718"),
                ("Scheduled", "\ue787"),
                ("Archived", "\ue838"),
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(320, 234)
            view.setHeaderText("Filters")
            view.setAccessibleName("Message filters")
            view.setIconSize(QSize(24, 24))
            view.setSelectionMode(fluentqt.SelectionMode.Multiple)
            source_lines.extend(
                (
                    "view.setFixedSize(320, 234)",
                    "view.setHeaderText('Filters')",
                    "view.setAccessibleName('Message filters')",
                    "view.setIconSize(QSize(24, 24))",
                    "view.setSelectionMode(fluentqt.SelectionMode.Multiple)",
                )
            )
        elif sample_id == "list-view-horizontal":
            rows = (
                ("Home", "\ue80f"),
                ("Music", "\ue8d6"),
                ("Videos", "\ue714"),
                ("Photos", "\ue722"),
                ("Calendar", "\ue787"),
                ("Settings", "\ue713"),
            )
            source_glyph_rows = rows
            source_icon_size = 26
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 26)
            view.setFixedSize(540, 132)
            view.setHeaderText("Library")
            view.setFlow(QListView.Flow.LeftToRight)
            view.setIconSize(QSize(26, 26))
            source_lines.extend(
                (
                    "view.setFixedSize(540, 132)",
                    "view.setHeaderText('Library')",
                    "view.setFlow(QListView.Flow.LeftToRight)",
                    "view.setIconSize(QSize(26, 26))",
                )
            )
        elif sample_id == "list-view-reorder":
            source_values = (
                "Bloom",
                "Northern Lights",
                "Driftwood",
                "Paper Boats",
                "Blue Hour",
                "Signal Fire",
                "Slow Orbit",
                "City Lights",
                "Afterglow",
                "Quiet Roads",
            )
            source_glyph_rows = tuple(
                (text, "\ue8d6") for text in source_values
            )
            source_icon_size = 24
            populate_glyph_rows(source_glyph_rows, source_icon_size)
            view.setFixedSize(320, 220)
            view.setHeaderText("Playlist")
            view.setIconSize(QSize(24, 24))
            view.setCanReorderItems(True)
            source_lines.extend(
                (
                    "view.setFixedSize(320, 220)",
                    "view.setHeaderText('Playlist')",
                    "view.setIconSize(QSize(24, 24))",
                    "view.setCanReorderItems(True)",
                )
            )
        elif sample_id == "list-view-sections":
            rows = (
                ("Build completed", "\ue73e"),
                ("New comment", "\ue8bd"),
                ("Meeting starts soon", "\ue787"),
                ("Pull request updated", "\ue8a5"),
                ("File synced", "\ue895"),
                ("Download ready", "\ue896"),
                ("Reminder", "\ue917"),
                ("Settings changed", "\ue713"),
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(340, 252)
            view.setHeaderText("Notifications")
            view.setIconSize(QSize(24, 24))
            view.setSectionKeyFunction(
                lambda row: "Today" if row < 3 else ("Yesterday" if row < 6 else "Earlier")
            )
            view.setSectionEnabled(True)
            source_lines.extend(
                (
                    "view.setSectionKeyFunction(",
                    "    lambda row: 'Today' if row < 3 else ('Yesterday' if row < 6 else 'Earlier')",
                    ")",
                    "view.setSectionEnabled(True)",
                    "view.setFixedSize(340, 252)",
                    "view.setHeaderText('Notifications')",
                    "view.setIconSize(QSize(24, 24))",
                )
            )
        elif sample_id == "list-view-scroll-chaining":
            rows = tuple(
                (
                    "Queued item {0}".format(index + 1),
                    "\ue8a5" if index % 2 == 0 else "\ue896",
                )
                for index in range(20)
            )
            source_glyph_rows = rows
            source_icon_size = 24
            source_values = tuple(text for text, _glyph in rows)
            populate_glyph_rows(rows, 24)
            view.setFixedSize(340, 238)
            view.setScrollChainingEnabled(False)
            view.setHeaderText("Queue")
            view.setFooterText("Wheel input stays in this ListView")
            view.setIconSize(QSize(24, 24))
            source_lines.extend(
                (
                    "view.setFixedSize(340, 238)",
                    "view.setScrollChainingEnabled(False)",
                    "view.setHeaderText('Queue')",
                    "view.setFooterText('Wheel input stays in this ListView')",
                    "view.setIconSize(QSize(24, 24))",
                )
            )

    if sample_id == "list-view-basic":
        grouped_values = tuple(
            source_values[index : index + 4]
            for index in range(0, len(source_values), 4)
        )
        source_lines.extend(
            (
                "contacts = (",
                *(
                    "    {0},".format(
                        ", ".join(repr(value) for value in values)
                    )
                    for values in grouped_values
                ),
                ")",
                "for index, value in enumerate(contacts):",
                "    item = QStandardItem(value)",
                "    item.setEditable(False)",
                "    item.setData(",
                "        gallery_initials_avatar(",
                "            value, GALLERY_ACCENT_PALETTE[index % len(GALLERY_ACCENT_PALETTE)]",
                "        ),",
                "        Qt.ItemDataRole.DecorationRole,",
                "    )",
                "    model.appendRow(item)",
            )
        )
    elif source_glyph_rows:
        source_lines.extend(
            (
                "for index, (text, glyph) in enumerate({0!r}):".format(
                    source_glyph_rows
                ),
                "    item = QStandardItem(text)",
                "    item.setEditable(False)",
                "    item.setData(",
                "        gallery_glyph_pixmap(",
                "            glyph,",
                "            GALLERY_ACCENT_PALETTE[index % len(GALLERY_ACCENT_PALETTE)],",
                "            {0},".format(source_icon_size),
                "        ),",
                "        Qt.ItemDataRole.DecorationRole,",
                "    )",
                "    model.appendRow(item)",
            )
        )
    view.setModel(model)
    source_lines.append("view.setModel(model)")
    if sample_id == "list-view-basic":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    elif sample_id == "list-view-multi-select":
        selection = view.selectionModel()
        for row in (0, 2):
            selection.select(
                model.index(row, 0),
                QItemSelectionModel.SelectionFlag.Select
                | QItemSelectionModel.SelectionFlag.Rows,
            )
        source_lines.extend(
            (
                "selection = view.selectionModel()",
                "for row in (0, 2):",
                "    selection.select(",
                "        model.index(row, 0),",
                "        QItemSelectionModel.SelectionFlag.Select",
                "        | QItemSelectionModel.SelectionFlag.Rows,",
                "    )",
            )
        )
    elif sample_id == "list-view-horizontal":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    elif sample_id == "list-view-sections":
        view.setSelectedIndex(1)
        source_lines.append("view.setSelectedIndex(1)")
    elif sample_id == "list-view-scroll-chaining":
        view.setSelectedIndex(0)
        source_lines.append("view.setSelectedIndex(0)")
    _hold(view, model, delegate)
    return _result(
        view,
        _source(
            *source_lines,
            imports=(
                "from PySide6.QtCore import QItemSelectionModel, QSize, Qt\n"
                "from PySide6.QtGui import QStandardItem, QStandardItemModel\n"
                "from PySide6.QtWidgets import QListView\n"
                "# Gallery-only helpers reproduce the native card's avatars "
                "and row painting; ListView itself is the public binding API.\n"
                "from fluentqt_gallery.native_samples import (\n"
                "    _ACCENT_PALETTE as GALLERY_ACCENT_PALETTE,\n"
                "    _GalleryListRowDelegate as GalleryListRowDelegate,\n"
                "    _glyph_pixmap as gallery_glyph_pixmap,\n"
                "    _initials_avatar as gallery_initials_avatar,\n"
                ")"
            ),
        ),
        "ListView",
    )


def build_native_sample(
    route_id: str,
    sample_id: str,
    parent: QWidget | None,
) -> PreviewResult | None:
    builder = _BUILDERS.get((route_id, sample_id))
    if builder is None:
        return None
    result = builder(sample_id, parent)
    if not result.source_driven:
        result = _materialize_displayed_source(
            route_id,
            sample_id,
            result,
            parent,
        )
    _apply_preview_text_contract(result.widget)
    result.source = _format_display_source(
        _align_cpp_icon_names(
            route_id,
            sample_id,
            _semantic_icon_source(
                _explicit_display_source(
                    route_id,
                    sample_id,
                    result.source,
                )
            )
        ),
        SAMPLE_BY_KEY[(route_id, sample_id)].cpp_snippet,
    )
    result.route_id = route_id
    result.sample_id = sample_id
    result.parity_level = "native-equivalent"
    return result


def ported_sample_keys() -> frozenset[tuple[str, str]]:
    return frozenset(_BUILDERS)


# Registration modules are imported only after the registry API is complete.
from . import native_samples_basic as _native_samples_basic  # noqa: E402,F401
from . import native_samples_collections as _native_samples_collections  # noqa: E402,F401
from . import native_samples_dialogs as _native_samples_dialogs  # noqa: E402,F401
from . import native_samples_navigation as _native_samples_navigation  # noqa: E402,F401
from . import native_samples_scrolling as _native_samples_scrolling  # noqa: E402,F401
from . import native_samples_status as _native_samples_status  # noqa: E402,F401
from . import native_samples_text_window as _native_samples_text_window  # noqa: E402,F401


__all__ = ["build_native_sample", "ported_sample_keys", "register_source_samples"]
